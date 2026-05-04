/*
 * Copyright (C) 2019-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "edlstorage.h"

#include <fnmatch.h>
#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <utility>
#include <toml++/toml.h>

#include "edlutils.h"
#include "loginternal.h"

SY_DEFINE_LOG_CATEGORY(logEdl, "edl");

using namespace Syntalos;

static const std::string EDL_FORMAT_VERSION = "1";

/**
 * Make filenames multiplatform-safe and simpler.
 */
static std::string edlSanitizeFilename(const std::string &name)
{
    if (name.empty())
        return {};
    std::string tmp;
    tmp.reserve(name.size());
    // strip leading/trailing whitespace
    size_t start = 0, end = name.size();
    while (start < end && (name[start] == ' ' || name[start] == '\t'))
        ++start;
    while (end > start && (name[end - 1] == ' ' || name[end - 1] == '\t'))
        --end;
    for (size_t i = start; i < end; ++i) {
        const char c = name[i];
        if (c == '/' || c == '\\')
            tmp += '_';
        else if (c == ':')
            ; // strip colons
        else
            tmp += c;
    }
    if (tmp == "AUX")
        return "_AUX";
    return tmp;
}

static toml::table createManifestFileSection(EDLDataFile &df)
{
    toml::table dataTab;

    // determine file type from extension if not already set
    if (df.fileType.empty() && !df.parts.empty()) {
        const fs::path p(df.parts.front().filename);
        const auto ext = p.extension().string();
        if (!ext.empty() && ext.front() == '.')
            df.fileType = ext.substr(1);
        // handle double extensions for compressed files
        const auto stem = p.stem();
        const auto stemExt = stem.extension().string();

        if (!stemExt.empty() && !ext.empty()) {
            const auto compressedExt = stemExt.substr(1) + "." + df.fileType;
            if (ext == ".zst" || ext == ".gz" || ext == ".xz")
                df.fileType = compressedExt;
        }
    }

    // guess MIME type, if we can
    if (df.mediaType.empty() && !df.parts.empty())
        df.mediaType = edl::guessContentType(df.parts.front().filename);

    if (!df.fileType.empty())
        dataTab.insert("file_type", df.fileType);
    if (!df.mediaType.empty())
        dataTab.insert("media_type", df.mediaType);
    if (!df.className.empty())
        dataTab.insert("class", df.className);
    if (!df.summary.empty())
        dataTab.insert("summary", df.summary);

    toml::array filePartsArr;
    for (size_t i = 0; i < df.parts.size(); ++i) {
        auto fpart = df.parts[i];
        if (fpart.index < 0)
            fpart.index = static_cast<int>(i);
        toml::table fpartTab;

        if (df.parts.size() > 1)
            fpartTab.insert("index", fpart.index);
        fpartTab.insert("fname", fpart.filename);

        filePartsArr.push_back(std::move(fpartTab));
    }

    dataTab.insert("parts", std::move(filePartsArr));
    return dataTab;
}

EdlDateTime Syntalos::edlCurrentDateTime()
{
    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    return EdlDateTime{std::chrono::current_zone(), now};
}

// ============================================================
// EDLUnit
// ============================================================

class EDLUnit::Private
{
public:
    Private() = default;
    ~Private() = default;

    EDLUnitKind objectKind = EDLUnitKind::UNKNOWN;
    std::string formatVersion;
    EDLUnit *parent = nullptr;

    Uuid collectionId{};
    bool collectionIdSet = false;
    std::optional<EdlDateTime> timeCreated;
    std::string generatorId;
    std::vector<EDLAuthor> authors;

    std::string name;
    fs::path rootPath;

    std::optional<EDLDataFile> dataFile;
    std::vector<EDLDataFile> auxDataFiles;
    std::map<std::string, MetaValue> attrs;

    bool isDetached = false;
    mutable std::mutex mutex;
};

EDLUnit::EDLUnit(EDLUnitKind kind, EDLUnit *parent)
    : d(new EDLUnit::Private)
{
    d->objectKind = kind;
    d->formatVersion = EDL_FORMAT_VERSION;
    d->parent = parent;
}

EDLUnit::~EDLUnit() = default;

EDLUnitKind EDLUnit::objectKind() const
{
    return d->objectKind;
}

std::string EDLUnit::objectKindString() const
{
    switch (d->objectKind) {
    case EDLUnitKind::COLLECTION:
        return "collection";
    case EDLUnitKind::GROUP:
        return "group";
    case EDLUnitKind::DATASET:
        return "dataset";
    default:
        return {};
    }
}

EDLUnit *EDLUnit::parent() const
{
    return d->parent;
}

std::string EDLUnit::name() const
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    return d->name;
}

std::expected<void, std::string> EDLUnit::setName(const std::string &name)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    const auto oldPath = d->rootPath.empty() ? fs::path{} : (d->rootPath / d->name);
    const auto oldName = d->name;

    // we put some restrictions on how people can name objects,
    // as to not mess with the directory layout and to keep names
    // portable between operating systems
    d->name = edlSanitizeFilename(name);
    if (d->name.empty())
        d->name = edl::createRandomString(4);

    if (!oldPath.empty()) {
        const auto newPath = d->rootPath / d->name;
        if (fs::exists(oldPath) && oldPath != newPath) {
            std::error_code ec;
            fs::rename(oldPath, newPath, ec);
            if (ec) {
                d->name = oldName;
                return std::unexpected(
                    std::format(
                        "Unable to rename directory '{}' to '{}': {}",
                        oldPath.string(),
                        newPath.string(),
                        ec.message()));
            }
        }
    }

    return {};
}

EdlDateTime EDLUnit::timeCreated() const
{
    const std::lock_guard<std::mutex> lock(d->mutex);

    // return current time as default
    if (!d->timeCreated.has_value())
        return edlCurrentDateTime();
    return *d->timeCreated;
}

void EDLUnit::setTimeCreated(const EdlDateTime &time)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    d->timeCreated = time;
}

Uuid EDLUnit::collectionId() const
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    return d->collectionId;
}

void EDLUnit::setCollectionId(const Uuid &uuid)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    d->collectionId = uuid;
    d->collectionIdSet = true;
}

std::string EDLUnit::collectionShortTag() const
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    if (!d->collectionIdSet)
        return {};

    // we need to take from the right, because we use UUIDv7 and to get a varying tag
    // from the timestamp component, we would need the first 12+ chars
    const auto hex = d->collectionId.toHex();
    return hex.substr(hex.size() > 8 ? hex.size() - 8 : 0);
}

void EDLUnit::addAuthor(const EDLAuthor &author)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    d->authors.push_back(author);
}

std::vector<EDLAuthor> EDLUnit::authors() const
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    return d->authors;
}

void EDLUnit::setPath(const fs::path &path)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    d->name = path.filename().string();
    d->rootPath = path.parent_path();
}

fs::path EDLUnit::path() const
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    if (d->rootPath.empty())
        return {};
    return d->rootPath / d->name;
}

fs::path EDLUnit::rootPath() const
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    return d->rootPath;
}

void EDLUnit::setRootPath(const fs::path &root)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    d->rootPath = root;
}

std::map<std::string, MetaValue> EDLUnit::attributes() const
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    return d->attrs;
}

void EDLUnit::setAttributes(const std::map<std::string, MetaValue> &attributes)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    d->attrs = attributes;
}

void EDLUnit::insertAttribute(const std::string &key, const MetaValue &value)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    d->attrs.insert_or_assign(key, value);
}

bool EDLUnit::isDetached() const
{
    return d->isDetached;
}

void EDLUnit::setDetached(bool detached)
{
    d->isDetached = detached;
}

std::expected<void, std::string> EDLUnit::save()
{
    if (isDetached())
        return {};
    if (rootPath().empty())
        return std::unexpected("Unable to save experiment data: No root directory is set.");
    auto r = saveManifest();
    if (!r)
        return r;
    return saveAttributes();
}

std::expected<void, std::string> EDLUnit::validate(bool recursive)
{
    return {};
}

void EDLUnit::setObjectKind(const EDLUnitKind &kind)
{
    d->objectKind = kind;
}

void EDLUnit::setParent(EDLUnit *parent)
{
    d->parent = parent;
}

void EDLUnit::setDataObjects(std::optional<EDLDataFile> dataFile, const std::vector<EDLDataFile> &auxDataFiles)
{
    d->dataFile = std::move(dataFile);
    d->auxDataFiles = auxDataFiles;
}

std::string EDLUnit::serializeManifest()
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    toml::table document;

    if (!d->timeCreated.has_value()) {
        // set default creation time, with second-resolution
        d->timeCreated = edlCurrentDateTime();
    }

    document.insert("format_version", d->formatVersion);
    document.insert("type", objectKindString());
    document.insert("time_created", edl::toToml(*d->timeCreated));

    if (d->collectionIdSet)
        document.insert("collection_id", d->collectionId.toHex());
    if (!d->generatorId.empty())
        document.insert("generator", d->generatorId);

    if (!d->authors.empty()) {
        toml::array authorsArr;
        for (const auto &author : d->authors) {
            toml::table authorTab;
            authorTab.insert("name", author.name);
            if (!author.email.empty())
                authorTab.insert("email", author.email);
            for (const auto &[k, v] : author.values)
                authorTab.insert(k, v);
            authorsArr.push_back(std::move(authorTab));
        }
        document.insert("authors", std::move(authorsArr));
    }

    if (d->dataFile.has_value() && !d->dataFile->parts.empty()) {
        auto dataTab = createManifestFileSection(d->dataFile.value());
        document.insert("data", std::move(dataTab));
    }

    // register auxiliary data files (metadata or extra data accompanying the main data files)
    if (!d->auxDataFiles.empty()) {
        toml::array auxDataArr;
        for (auto &adf : d->auxDataFiles) {
            if (adf.parts.empty())
                continue;
            auto dataTab = createManifestFileSection(adf);
            auxDataArr.push_back(dataTab);
        }
        document.insert("data_aux", std::move(auxDataArr));
    }

    // serialize manifest data to TOML
    std::stringstream strData;
    strData << document << "\n";
    return strData.str();
}

std::string EDLUnit::serializeAttributes()
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    // no user-defined attributes means the document is empty
    if (d->attrs.empty())
        return {};

    // serialize user attributes to TOML
    const auto document = edl::toTomlTable(d->attrs);
    std::stringstream strData;
    strData << document << "\n";
    return strData.str();
}

std::expected<void, std::string> EDLUnit::saveManifest()
{
    const auto p = path();
    std::error_code ec;
    fs::create_directories(p, ec);
    if (ec)
        return std::unexpected(std::format("Unable to create EDL directory '{}': {}", p.string(), ec.message()));

    const auto manifestPath = p / "manifest.toml";
    std::ofstream out(manifestPath);
    if (!out)
        return std::unexpected(std::format("Unable to open manifest file for writing in '{}'", p.string()));
    out << serializeManifest();
    return {};
}

std::expected<void, std::string> EDLUnit::saveAttributes()
{
    // do nothing if we have no user-defined attributes to save
    if (d->attrs.empty())
        return {};
    const auto attrStr = serializeAttributes();

    const auto p = path();
    std::error_code ec;
    fs::create_directories(p, ec);
    if (ec)
        return std::unexpected(std::format("Unable to create EDL directory '{}': {}", p.string(), ec.message()));

    const auto attrPath = p / "attributes.toml";
    std::ofstream out(attrPath);
    if (!out)
        return std::unexpected(std::format("Unable to open attributes file for writing in '{}'", p.string()));
    out << attrStr;
    return {};
}

std::string EDLUnit::generatorId() const
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    return d->generatorId;
}

void EDLUnit::setGeneratorId(const std::string &idString)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    d->generatorId = idString;
}

// ============================================================
// EDLDataset
// ============================================================

class EDLDataset::Private
{
public:
    Private() = default;
    ~Private() = default;

    EDLDataFile dataFile;
    std::map<std::string, EDLDataFile> auxFiles;

    std::pair<std::string, EDLDataFile> dataScanPattern;
    std::map<std::string, EDLDataFile> auxDataScanPatterns;

    mutable std::mutex mutex;
};

EDLDataset::EDLDataset(EDLGroup *parent)
    : EDLUnit(EDLUnitKind::DATASET, parent),
      d(new EDLDataset::Private)
{
}

EDLDataset::EDLDataset(const std::string &name, EDLGroup *parent)
    : EDLUnit(EDLUnitKind::DATASET, parent),
      d(new EDLDataset::Private)
{
    (void)EDLDataset::setName(name); // new object, no path set yet - cannot fail
}

EDLDataset::~EDLDataset() = default;

std::expected<void, std::string> EDLDataset::save()
{
    if (rootPath().empty())
        return std::unexpected("Unable to save dataset: No root directory is set.");

    // check if we have to scan for generated files
    // this is *really* ugly, so hopefully this is just a temporary aid
    // to help modules (especially ones where we don't easily control the data generation)
    // to use the EDL scheme.
    // we protect against badly written modules by not having them accidentally register
    // files as both primary data and aux data.
    {
        const std::lock_guard<std::mutex> lock(d->mutex);

        std::set<std::string> auxFileSet;
        for (const auto &[pattern, adf] : d->auxDataScanPatterns) {
            const auto files = findFilesByPattern(pattern);
            if (files.empty()) {
                SY_LOG_WARNING(
                    logEdl,
                    "Dataset '{}' expected aux data matching pattern `{}`, but nothing was found.",
                    name(),
                    pattern);
            } else {
                for (size_t i = 0; i < files.size(); ++i) {
                    auxFileSet.insert(files[i]);
                    EDLDataPart part(files[i]);
                    part.index = static_cast<int>(i);
                    if (i == 0) {
                        EDLDataFile afile;
                        afile.summary = adf.summary;
                        afile.parts.push_back(part);
                        d->auxFiles[pattern] = std::move(afile);
                    } else {
                        d->auxFiles[pattern].parts.push_back(part);
                    }
                }
            }
        }

        if (!d->dataScanPattern.first.empty()) {
            d->dataFile.parts.clear();
            const auto files = findFilesByPattern(d->dataScanPattern.first);
            if (files.empty()) {
                SY_LOG_WARNING(
                    logEdl,
                    "Dataset '{}' expected data matching pattern `{}`, but nothing was found.",
                    name(),
                    d->dataScanPattern.first);
            } else {
                for (const auto &file : files) {
                    if (auxFileSet.count(file))
                        continue;
                    EDLDataPart part(file);
                    d->dataFile.parts.push_back(part);
                }
                d->dataFile.summary = d->dataScanPattern.second.summary;
            }
        }

        // collect aux data values into a vector for setDataObjects
        std::vector<EDLDataFile> auxVec;
        auxVec.reserve(d->auxFiles.size());
        for (const auto &[k, adf] : d->auxFiles)
            auxVec.push_back(adf);
        setDataObjects(d->dataFile, auxVec);
    }

    auto r = saveManifest();
    if (!r)
        return r;
    return saveAttributes();
}

bool EDLDataset::isEmpty() const
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    return d->dataFile.parts.empty() && d->auxFiles.empty() && d->dataScanPattern.first.empty()
           && d->auxDataScanPatterns.empty();
}

fs::path EDLDataset::setDataFile(const std::string &fname, const std::string &summary)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    d->dataFile.parts.clear();
    d->dataFile.summary = summary;

    const auto baseName = fs::path(fname).filename().string();
    EDLDataPart part(baseName);
    d->dataFile.parts.push_back(part);

    // resolve absolute path
    const auto p = path();
    if (p.empty())
        return fs::path{};
    std::error_code ec;
    fs::create_directories(p, ec);
    if (ec)
        SY_LOG_WARNING(logEdl, "Unable to create EDL directory '{}': {}", p.string(), ec.message());
    return p / edlSanitizeFilename(baseName);
}

fs::path EDLDataset::addDataFilePart(const std::string &fname, int index)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    const auto baseName = fs::path(fname).filename().string();
    EDLDataPart part(baseName);
    part.index = index;
    d->dataFile.parts.push_back(part);

    const auto p = path();
    if (p.empty())
        return fs::path{};
    std::error_code ec;
    fs::create_directories(p, ec);
    if (ec)
        SY_LOG_WARNING(logEdl, "Unable to create EDL directory '{}': {}", p.string(), ec.message());
    return p / edlSanitizeFilename(baseName);
}

EDLDataFile EDLDataset::dataFile() const
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    return d->dataFile;
}

std::expected<fs::path, std::string> EDLDataset::addAuxDataFile(
    const std::string &fname,
    const std::string &key,
    const std::string &summary)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    EDLDataFile adf;
    adf.summary = summary;

    const auto actualKey = key.empty() ? fs::path(fname).filename().string() : key;
    d->auxFiles[actualKey] = adf;

    const auto baseName = fs::path(fname).filename().string();
    EDLDataPart part(baseName);
    d->auxFiles[actualKey].parts.push_back(part);

    const auto p = path();
    if (p.empty())
        return fs::path{};
    std::error_code ec;
    fs::create_directories(p, ec);
    if (ec)
        return std::unexpected(std::format("Unable to create EDL directory '{}': {}", p.string(), ec.message()));
    return p / edlSanitizeFilename(baseName);
}

std::expected<fs::path, std::string> EDLDataset::addAuxDataFilePart(
    const std::string &fname,
    const std::string &key,
    int index)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    const auto baseName = fs::path(fname).filename().string();
    const auto actualKey = key.empty() ? baseName : key;

    if (!d->auxFiles.count(actualKey))
        d->auxFiles[actualKey] = EDLDataFile();

    EDLDataPart part(baseName);
    part.index = index;
    d->auxFiles[actualKey].parts.push_back(part);

    const auto p = path();
    if (p.empty())
        return fs::path{};
    std::error_code ec;
    fs::create_directories(p, ec);
    if (ec)
        return std::unexpected(std::format("Unable to create EDL directory '{}': {}", p.string(), ec.message()));
    return p / edlSanitizeFilename(baseName);
}

void EDLDataset::setDataScanPattern(const std::string &wildcard, const std::string &summary)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    EDLDataFile df;
    df.summary = summary;
    d->dataScanPattern = {wildcard, df};
}

void EDLDataset::addAuxDataScanPattern(const std::string &wildcard, const std::string &summary)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    EDLDataFile adf;
    adf.summary = summary;
    d->auxDataScanPatterns[wildcard] = adf;
}

fs::path EDLDataset::pathForDataBasename(const std::string &baseName)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    auto bname = edlSanitizeFilename(fs::path(baseName).filename().string());
    if (bname.empty())
        bname = "data";

    const auto p = path();
    if (p.empty())
        return fs::path{};
    std::error_code ec;
    fs::create_directories(p, ec);
    if (ec)
        SY_LOG_WARNING(logEdl, "Unable to create EDL directory '{}': {}", p.string(), ec.message());
    return p / bname;
}

fs::path EDLDataset::pathForDataPart(const EDLDataPart &dpart)
{
    return path() / dpart.filename;
}

std::vector<std::string> EDLDataset::findFilesByPattern(const std::string &wildcard) const
{
    // Called with d->mutex already held, or internally. Never acquires the dataset mutex itself here.
    const auto p = path();
    if (p.empty() || !fs::is_directory(p))
        return {};

    std::vector<std::string> files;
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(p, ec)) {
        if (!entry.is_regular_file())
            continue;
        const auto fname = entry.path().filename().string();
        if (::fnmatch(wildcard.c_str(), fname.c_str(), 0) == 0)
            files.push_back(fname);
    }
    edl::naturalNumListSort(files);
    return files;
}

// ============================================================
// EDLGroup
// ============================================================

class EDLGroup::Private
{
public:
    Private() = default;
    ~Private() = default;

    std::vector<std::shared_ptr<EDLUnit>> children;
    mutable std::mutex mutex;
};

EDLGroup::EDLGroup(EDLGroup *parent)
    : EDLUnit(EDLUnitKind::GROUP, parent),
      d(new EDLGroup::Private)
{
}

EDLGroup::EDLGroup(const std::string &name, EDLGroup *parent)
    : EDLUnit(EDLUnitKind::GROUP, parent),
      d(new EDLGroup::Private)
{
    (void)EDLGroup::setName(name); // new object, no path yet - cannot fail
}

EDLGroup::~EDLGroup() = default;

std::expected<void, std::string> EDLGroup::setName(const std::string &name)
{
    {
        const std::lock_guard<std::mutex> lock(d->mutex);
        auto r = EDLUnit::setName(name);
        if (!r)
            return r;
        // propagate new path to all children
        const auto p = path();
        for (auto &node : d->children)
            node->setRootPath(p);
    }
    return {};
}

void EDLGroup::setRootPath(const fs::path &root)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    EDLUnit::setRootPath(root);
    // propagate path change through the hierarchy
    const auto p = path();
    for (auto &node : d->children)
        node->setRootPath(p);
}

void EDLGroup::setCollectionId(const Uuid &uuid)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    EDLUnit::setCollectionId(uuid);

    // propagate collection UUID through the DAG
    for (auto &node : d->children)
        node->setCollectionId(uuid);
}

std::vector<std::shared_ptr<EDLUnit>> EDLGroup::children() const
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    return d->children;
}

void EDLGroup::addChild(const std::shared_ptr<EDLUnit> &edlObj)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    _locked_addChild(edlObj);
}

void EDLGroup::_locked_addChild(const std::shared_ptr<EDLUnit> &edlObj)
{
    // Caller must hold d->mutex
    edlObj->setParent(this);
    edlObj->setRootPath(path());
    edlObj->setCollectionId(collectionId());
    d->children.push_back(edlObj);
}

std::expected<std::shared_ptr<EDLGroup>, std::string> EDLGroup::groupByName(const std::string &name, EDLCreateFlag flag)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    for (auto &node : d->children) {
        if (node->name() != name)
            continue;
        if (!std::dynamic_pointer_cast<EDLGroup>(node))
            return std::unexpected(
                std::format("A dataset named '{}' already exists in group '{}'.", name, this->name()));
        if (flag == EDLCreateFlag::MUST_CREATE)
            return std::unexpected(std::format("Group '{}' already exists in group '{}'.", name, this->name()));
        return std::dynamic_pointer_cast<EDLGroup>(node);
    }
    if (flag == EDLCreateFlag::OPEN_ONLY)
        return std::unexpected(std::format("Group '{}' not found in group '{}'.", name, this->name()));

    auto eg = std::make_shared<EDLGroup>(name);
    _locked_addChild(eg);
    return eg;
}

std::expected<std::shared_ptr<EDLDataset>, std::string> EDLGroup::datasetByName(
    const std::string &name,
    EDLCreateFlag flag)
{
    const std::lock_guard<std::mutex> lock(d->mutex);
    for (auto &node : d->children) {
        if (node->name() != name)
            continue;
        if (!std::dynamic_pointer_cast<EDLDataset>(node))
            return std::unexpected(
                std::format("A group named '{}' already exists in group '{}'.", name, this->name()));
        if (flag == EDLCreateFlag::MUST_CREATE)
            return std::unexpected(std::format("Dataset '{}' already exists in group '{}'.", name, this->name()));
        return std::dynamic_pointer_cast<EDLDataset>(node);
    }
    if (flag == EDLCreateFlag::OPEN_ONLY)
        return std::unexpected(std::format("Dataset '{}' not found in group '{}'.", name, this->name()));

    auto ds = std::make_shared<EDLDataset>(name);
    _locked_addChild(ds);
    return ds;
}

std::expected<void, std::string> EDLGroup::save()
{
    // check if we even have a directory to save this data
    if (rootPath().empty())
        return std::unexpected("Unable to save experiment data: No root directory is set.");

    if (auto r = validate(false); !r)
        return r;

    // copy children snapshot to avoid holding lock during child saves
    std::vector<std::shared_ptr<EDLUnit>> snap;
    {
        const std::lock_guard<std::mutex> lock(d->mutex);
        snap = d->children;
    }

    for (auto it = snap.begin(); it != snap.end();) {
        auto &obj = *it;
        if (obj->isDetached()) {
            ++it;
            continue;
        }
        if (obj->parent() != this) {
            SY_LOG_WARNING(
                logEdl, "Unlinking EDL child '{}' that doesn't believe '{}' is its parent.", obj->name(), name());
            const std::lock_guard<std::mutex> lock(d->mutex);
            d->children.erase(std::find(d->children.begin(), d->children.end(), obj));
            it = snap.erase(it);
            continue;
        }
        auto r = obj->save();
        if (!r)
            return std::unexpected(std::format("Saving of '{}' failed: {}", obj->name(), r.error()));
        ++it;
    }

    return EDLUnit::save();
}

std::expected<void, std::string> EDLGroup::validate(bool recursive)
{
    if (rootPath().empty())
        return std::unexpected(std::format("Group '{}' is missing a root path.", name()));

    std::vector<std::shared_ptr<EDLUnit>> snap;
    {
        const std::lock_guard<std::mutex> lock(d->mutex);
        snap = d->children;
    }

    std::set<std::string> seenNames;
    for (const auto &obj : snap) {
        const auto childName = obj->name();
        if (!seenNames.insert(childName).second)
            return std::unexpected(std::format("Duplicate child name '{}' in group '{}'.", childName, name()));

        // externally-owned nodes are saved by their owning worker process
        if (obj->isDetached())
            continue;

        // recursively validate child nodes
        if (recursive) {
            auto r = obj->validate(true);
            if (!r)
                return r;
        }
    }

    return {};
}

// ============================================================
// EDLCollection
// ============================================================

class EDLCollection::Private
{
public:
    Private() = default;
    ~Private() = default;
};

EDLCollection::EDLCollection(const std::string &name)
    : EDLGroup(nullptr),
      d(new EDLCollection::Private)
{
    setObjectKind(EDLUnitKind::COLLECTION);
    setParent(nullptr);
    if (!name.empty()) {
        (void)EDLGroup::setName(name); // new collection, no path yet - cannot fail
        insertAttribute("collection_name", MetaValue{name});
    }

    // A collection must have a unique ID to identify all nodes that belong to it.
    EDLGroup::setCollectionId(newUuid7());
}

EDLCollection::~EDLCollection() = default;

std::string EDLCollection::generatorId() const
{
    return EDLUnit::generatorId();
}

void EDLCollection::setGeneratorId(const std::string &idString)
{
    EDLUnit::setGeneratorId(idString);
}

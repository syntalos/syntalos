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

#pragma once

#include <chrono>
#include <expected>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <datactl/uuid.h>
#include <datactl/streammeta.h>

namespace Syntalos
{

namespace fs = std::filesystem;

using EdlDateTime = std::chrono::zoned_time<std::chrono::system_clock::duration>;

EdlDateTime edlCurrentDateTime();

/**
 * Type of an EDL unit.
 */
enum class EDLUnitKind {
    UNKNOWN,
    COLLECTION,
    GROUP,
    DATASET
};

/**
 * Author information for an EDL collection.
 */
class EDLAuthor
{
public:
    explicit EDLAuthor(const std::string &aName, const std::string &aEmail)
        : name(aName),
          email(aEmail)
    {
    }
    explicit EDLAuthor()
        : name({}),
          email({})
    {
    }

    bool isValid() const
    {
        return !name.empty();
    }

    std::string name;
    std::string email;
    std::map<std::string, std::string> values;
};

class EDLDataPart
{
public:
    explicit EDLDataPart()
        : index(-1),
          filename({})
    {
    }
    explicit EDLDataPart(const std::string &fname)
        : index(-1)
    {
        const auto path = fs::path(fname);
        filename = path.has_filename() ? path.filename().string() : fname;
    }
    explicit EDLDataPart(const fs::path &path)
        : index(-1)
    {
        filename = path.has_filename() ? path.filename().string() : path.string();
    }

    int index;
    std::string filename;
};

class EDLDataFile
{
public:
    explicit EDLDataFile() {}

    std::string className;
    std::string fileType;
    std::string mediaType;
    std::string summary;
    std::vector<EDLDataPart> parts;
};

class EDLGroup;
class EDLDataset;

/**
 * @brief Base class for all EDL entities
 */
class EDLUnit
{
    friend class EDLGroup;

public:
    explicit EDLUnit(EDLUnitKind kind, EDLUnit *parent = nullptr);
    virtual ~EDLUnit();

    EDLUnit(const EDLUnit &) = delete;
    EDLUnit &operator=(const EDLUnit &) = delete;

    EDLUnitKind objectKind() const;
    std::string objectKindString() const;
    EDLUnit *parent() const;

    std::string name() const;
    virtual std::expected<void, std::string> setName(const std::string &name);

    EdlDateTime timeCreated() const;
    void setTimeCreated(const EdlDateTime &time);

    Uuid collectionId() const;
    virtual void setCollectionId(const Uuid &uuid);

    /**
     * @brief Get part of the long collection-id as a short tag in e.g. filenames
     * @return Collection ID fragment string for use as short tag
     */
    std::string collectionShortTag() const;

    void addAuthor(const EDLAuthor &author);
    std::vector<EDLAuthor> authors() const;

    fs::path path() const;
    void setPath(const fs::path &path);

    fs::path rootPath() const;

    std::map<std::string, MetaValue> attributes() const;
    void setAttributes(const std::map<std::string, MetaValue> &attributes);
    void insertAttribute(const std::string &key, const MetaValue &value);

    virtual std::expected<void, std::string> save();
    virtual std::expected<void, std::string> validate(bool recursive = true);

    std::string serializeManifest();
    std::string serializeAttributes();

protected:
    void setObjectKind(const EDLUnitKind &kind);
    void setParent(EDLUnit *parent);

    virtual void setRootPath(const fs::path &root);

    void setDataObjects(std::optional<EDLDataFile> dataFile, const std::vector<EDLDataFile> &auxDataFiles = {});

    std::expected<void, std::string> saveManifest();
    std::expected<void, std::string> saveAttributes();

    std::string generatorId() const;
    void setGeneratorId(const std::string &idString);

private:
    class Private;
    std::unique_ptr<Private> d;
};

/**
 * @brief An EDL dataset
 *
 * A set of data files which belongs together
 * (usually data of the same modality from the same source)
 */
class EDLDataset : public EDLUnit
{
public:
    explicit EDLDataset(EDLGroup *parent = nullptr);
    EDLDataset(const std::string &name, EDLGroup *parent = nullptr);
    ~EDLDataset();

    EDLDataset(const EDLDataset &) = delete;
    EDLDataset &operator=(const EDLDataset &) = delete;

    std::expected<void, std::string> save() override;

    bool isEmpty() const;

    fs::path setDataFile(const std::string &fname, const std::string &summary = {});
    fs::path addDataFilePart(const std::string &fname, int index = -1);
    EDLDataFile dataFile() const;

    std::expected<fs::path, std::string> addAuxDataFile(
        const std::string &fname,
        const std::string &key = {},
        const std::string &summary = {});
    std::expected<fs::path, std::string> addAuxDataFilePart(
        const std::string &fname,
        const std::string &key = {},
        int index = -1);

    /**
     * @brief Set a pattern to find data files
     * @param wildcard Wildcard to find generated data
     *
     * Set a pattern to find generated data when the dataset object
     * is saved, if the data was generated externally and could not
     * be registered properly with addDataPartFilename().
     */
    void setDataScanPattern(const std::string &wildcard, const std::string &summary = {});
    void addAuxDataScanPattern(const std::string &wildcard, const std::string &summary = {});

    /**
     * @brief Get absolute path for data with a given basename
     * Retrieve an absolute path for the given basename in this dataset.
     * The data is *not* registered with the dataset, to add it properly,
     * you need to register a scan pattern.
     * The use of this pattern is discouraged, try to add new data explicitly
     * rather then implicitly.
     *
     * This function ensures the path to the selected data exists, but the data
     * file itself may not be present on disk.
     * An empty path may be returned in case no path could be created.
     */
    fs::path pathForDataBasename(const std::string &baseName);

    /**
     * @brief Return absolute path to data file on disk.
     * This function will just return the absolute path of the given data part
     * on disk. If the data part passed as parameter does not belong to this
     * dataset, an invalid part will be returned.
     * The function will not check for existence of the file!
     *
     * @param dpart The data part to resolve
     * @return Absolute filepath
     */
    fs::path pathForDataPart(const EDLDataPart &dpart);

private:
    class Private;
    std::unique_ptr<Private> d;

    std::vector<std::string> findFilesByPattern(const std::string &wildcard) const;
};

/**
 * Flag to influence how EDL units are loaded.
 */
enum class EDLCreateFlag {
    OPEN_ONLY,      /// Unit will only be opened, never created
    CREATE_OR_OPEN, /// Unit will be opened if it exists, and created otherwise
    MUST_CREATE     /// Unit *must* be created, it is an error if it already exists
};

/**
 * @brief A grouping of groups or datasets
 */
class EDLGroup : public EDLUnit
{
public:
    explicit EDLGroup(EDLGroup *parent = nullptr);
    EDLGroup(const std::string &name, EDLGroup *parent = nullptr);
    ~EDLGroup();

    EDLGroup(const EDLGroup &) = delete;
    EDLGroup &operator=(const EDLGroup &) = delete;

    std::expected<void, std::string> setName(const std::string &name) override;
    void setRootPath(const fs::path &root) override;
    void setCollectionId(const Uuid &uuid) override;

    std::vector<std::shared_ptr<EDLUnit>> children() const;
    void addChild(const std::shared_ptr<EDLUnit> &edlObj);

    std::expected<std::shared_ptr<EDLGroup>, std::string> groupByName(
        const std::string &name,
        EDLCreateFlag flag = EDLCreateFlag::OPEN_ONLY);
    std::expected<std::shared_ptr<EDLDataset>, std::string> datasetByName(
        const std::string &name,
        EDLCreateFlag flag = EDLCreateFlag::OPEN_ONLY);

    std::expected<void, std::string> save() override;
    std::expected<void, std::string> validate(bool recursive = true) override;

private:
    class Private;
    std::unique_ptr<Private> d;

    void _locked_addChild(const std::shared_ptr<EDLUnit> &edlObj);
};

/**
 * @brief A collection of groups and datasets
 */
class EDLCollection : public EDLGroup
{
public:
    explicit EDLCollection(const std::string &name = {});
    ~EDLCollection();

    EDLCollection(const EDLCollection &) = delete;
    EDLCollection &operator=(const EDLCollection &) = delete;

    std::string generatorId() const;
    void setGeneratorId(const std::string &idString);

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace Syntalos

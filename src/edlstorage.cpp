/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include <iostream>
#include <fstream>
#include <chrono>
#include <QDebug>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QDir>
#include <QUuid>
#include <toml++/toml.h>

#include "tomlutils.h"

static const QString EDL_FORMAT_VERSION = QStringLiteral("1");

class EDLObject::Private
{
public:
    Private() {}
    ~Private() {}

    EDLObjectKind objectKind;
    QString formatVersion;
    QUuid collectionId;
    QDateTime timeCreated;
    QString generatorId;
    QList<EDLAuthor> authors;

    QString name;
    QString rootPath;

    std::optional<EDLDataFile> dataFile;
    std::optional<EDLDataFile> auxDataFile;
    QHash<QString, QVariant> attrs;

    QString lastError;
};

EDLObject::EDLObject(EDLObjectKind kind, QObject *parent)
    : QObject(parent),
      d(new EDLObject::Private)
{
    d->objectKind = kind;
    d->formatVersion = EDL_FORMAT_VERSION;

    // set default creation time, with second-resolution (milliseconds are stripped)
    auto cdt = QDateTime::currentDateTime();
    cdt.setTime(QTime(cdt.time().hour(), cdt.time().minute(), cdt.time().second()));
    d->timeCreated = cdt;
}

EDLObject::~EDLObject()
{}

EDLObjectKind EDLObject::objectKind() const
{
    return d->objectKind;
}

QString EDLObject::objectKindString() const
{
    switch (d->objectKind) {
    case EDLObjectKind::COLLECTION:
        return QStringLiteral("collection");
    case EDLObjectKind::GROUP:
        return QStringLiteral("group");
    case EDLObjectKind::DATASET:
        return QStringLiteral("dataset");
    default:
        return QString();
    }
}

QString EDLObject::name() const
{
    return d->name;
}

void EDLObject::setName(const QString &name)
{
    d->name = name;
}

QDateTime EDLObject::timeCreated() const
{
    return d->timeCreated;
}

void EDLObject::setTimeCreated(const QDateTime &time)
{
    d->timeCreated = time;
}

QUuid EDLObject::collectionId() const
{
    return d->collectionId;
}

void EDLObject::setCollectionId(const QUuid &uuid)
{
    d->collectionId = uuid;
}

void EDLObject::addAuthor(const EDLAuthor author)
{
    d->authors.append(author);
}

QList<EDLAuthor> EDLObject::authors() const
{
    return d->authors;
}

void EDLObject::setPath(const QString &path)
{
    QDir dir(path);
    d->name = dir.dirName();
    d->rootPath = QDir::cleanPath(QStringLiteral("%1/..").arg(path));
}

QString EDLObject::path() const
{
    return QDir::cleanPath(d->rootPath + QStringLiteral("/") + name());
}

QString EDLObject::rootPath() const
{
    return d->rootPath;
}

void EDLObject::setRootPath(const QString &root)
{
    d->rootPath = root;
}

QHash<QString, QVariant> EDLObject::attributes() const
{
    return d->attrs;
}

void EDLObject::setAttributes(const QHash<QString, QVariant> &attributes)
{
    d->attrs = attributes;
}

bool EDLObject::save()
{
    if (d->rootPath.isEmpty()) {
        d->lastError = QStringLiteral("Unable to save experiment data: No root directory is set.");
        return false;
    }
    if (!saveManifest())
        return false;
    return saveAttributes();
}

QString EDLObject::lastError() const
{
    return d->lastError;
}

void EDLObject::setObjectKind(const EDLObjectKind &kind)
{
    d->objectKind = kind;
}

void EDLObject::setLastError(const QString &message)
{
    d->lastError = message;
}

void EDLObject::setDataObjects(std::optional<EDLDataFile> dataFile, std::optional<EDLDataFile> auxDataFile)
{
    d->dataFile = dataFile;
    d->auxDataFile = auxDataFile;
}

static toml::table createManifestFileSection(EDLDataFile &df)
{
    toml::table dataTab;

    // try to guess a MIME type in case none is set
    if (df.mediaType.isEmpty()) {
        QMimeDatabase db;
        const auto mime = db.mimeTypeForFile(df.parts.first().fname);
        if (mime.isValid()  && !mime.isDefault())
            df.mediaType = mime.name();
    }

    // if the media type is still empty, we at least want to set a file type
    if (df.mediaType.isEmpty()) {
        if (df.fileType.isEmpty()) {
            QFileInfo fi(df.parts.first().fname);
            df.fileType = fi.completeSuffix();
        }
    }

    if (!df.fileType.isEmpty())
        dataTab.insert("file_type", df.fileType.toLower().toStdString());
    if (!df.mediaType.isEmpty())
        dataTab.insert("media_type", df.mediaType.toStdString());
    if (!df.className.isEmpty())
        dataTab.insert("class", df.className.toLower().toStdString());

    toml::array filePartsArr;
    for (int i = 0; i < df.parts.length(); i++) {
        auto fpart = df.parts[i];
        if (fpart.index < 0)
            fpart.index = i;
        toml::table fpartTab;

        fpartTab.insert("index", fpart.index);
        fpartTab.insert("fname", fpart.fname.toStdString());

        filePartsArr.push_back(std::move(fpartTab));
    }

    dataTab.insert("parts", std::move(filePartsArr));
    return dataTab;
}

QString EDLObject::serializeManifest()
{
    toml::table document;

    document.insert("format_version", d->formatVersion.toStdString());
    document.insert("type", objectKindString().toStdString());
    document.insert("time_created", qDateTimeToToml(d->timeCreated));

    if (!d->collectionId.isNull())
        document.insert("collection_id", d->collectionId.toString(QUuid::WithoutBraces).toStdString());
    if (!d->generatorId.isEmpty())
        document.insert("generator", d->generatorId.toStdString());

    if (!d->authors.isEmpty()) {
        toml::array authorsArr;
        for (const auto& author : d->authors) {
            toml::table authorTab;
            authorTab.insert("name", author.name.toStdString());
            authorTab.insert("email", author.email.toStdString());
            QHashIterator<QString, QString> iter(author.values);
            while (iter.hasNext()) {
                iter.next();
                authorTab.insert(iter.key().toStdString(), iter.value().toStdString());
            }
            authorsArr.push_back(std::move(authorTab));
        }
        document.insert("authors", std::move(authorsArr));
    }

    if (d->dataFile.has_value() && !d->dataFile->parts.isEmpty()) {
        auto dataTab = createManifestFileSection(d->dataFile.value());
        document.insert("data", std::move(dataTab));
    }

    if (d->auxDataFile.has_value() && !d->auxDataFile->parts.isEmpty()) {
        auto dataTab = createManifestFileSection(d->auxDataFile.value());
        document.insert("data", std::move(dataTab));
    }

    // serialize manifest data to TOML
    std::stringstream strData;
    strData << document << "\n";

    return QString::fromStdString(strData.str());
}

QString EDLObject::serializeAttributes()
{
    // no user-defined attributes means the document is empty
    if (d->attrs.isEmpty())
        return QString();

    auto document = qVariantHashToTomlTable(d->attrs);

    // serialize user attributes to TOML
    std::stringstream strData;
    strData << document << "\n";

    return QString::fromStdString(strData.str());
}

bool EDLObject::saveManifest()
{
    QDir dir;
    if (!dir.mkpath(path())) {
        d->lastError = QStringLiteral("Unable to create EDL directory: '%1'").arg(path());
        return false;
    }

    // write the manifest file
    QFile file(QStringLiteral("%1/manifest.toml").arg(path()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        d->lastError = QStringLiteral("Unable to open manifest file for writing (in '%1'): %2").arg(path()).arg(file.errorString());
        return false;
    }

    QTextStream out(&file);
    out << serializeManifest();

    file.close();
    return true;
}

bool EDLObject::saveAttributes()
{
    // do nothing if we have no user-defined attributes to save
    if (d->attrs.isEmpty())
        return true;

    QDir dir;
    if (!dir.mkpath(path())) {
        d->lastError = QStringLiteral("Unable to create EDL directory: '%1'").arg(path());
        return false;
    }

    // write the attributes file
    QFile file(QStringLiteral("%1/attributes.toml").arg(path()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        d->lastError = QStringLiteral("Unable to open attributes file for writing (in '%1'): %2").arg(path()).arg(file.errorString());
        return false;
    }

    QTextStream out(&file);
    out << serializeAttributes();

    file.close();
    return true;
}

QString EDLObject::generatorId() const
{
    return d->generatorId;
}

void EDLObject::setGeneratorId(const QString &idString)
{
    d->generatorId = idString;
}

class EDLDataset::Private
{
public:
    Private() {}
    ~Private() {}

    EDLDataFile dataFile;
    EDLDataFile auxFile;
};

EDLDataset::EDLDataset(EDLObject *parent)
    : EDLObject(EDLObjectKind::DATASET, parent),
      d(new EDLDataset::Private)
{

}

EDLDataset::~EDLDataset()
{}

bool EDLDataset::save()
{
    setDataObjects(d->dataFile, d->auxFile);
    if (rootPath().isEmpty()) {
        setLastError(QStringLiteral("Unable to save dataset: No root directory is set."));
        return false;
    }
    if (!saveManifest())
        return false;
    return saveAttributes();
}

void EDLDataset::addDataFilePart(const QString &fname, int index)
{
    EDLDataPart part(fname);
    part.index = index;
    d->dataFile.parts.append(part);
}

class EDLGroup::Private
{
public:
    Private() {}
    ~Private() {}

    QList<EDLObject*> children;
};

EDLGroup::EDLGroup(EDLObject *parent)
    : EDLObject(EDLObjectKind::GROUP, parent),
      d(new EDLGroup::Private)
{

}

EDLGroup::~EDLGroup()
{}

void EDLGroup::setName(const QString &name)
{
    EDLObject::setName(name);
    // propagate path change through the hierarchy
    for (auto &node : d->children)
        node->setRootPath(path());
}

void EDLGroup::setRootPath(const QString &root)
{
    EDLObject::setRootPath(root);
    // propagate path change through the hierarchy
    for (auto &node : d->children)
        node->setRootPath(path());
}

void EDLGroup::setCollectionId(const QUuid &uuid)
{
    EDLObject::setCollectionId(uuid);
    // propagate collection UUID through the DAG
    for (auto &node : d->children)
        node->setCollectionId(uuid);
}

QList<EDLObject *> EDLGroup::children() const
{
    return d->children;
}

void EDLGroup::addChild(EDLObject *edlObj)
{
    edlObj->setParent(this);
    edlObj->setRootPath(path());
    edlObj->setCollectionId(collectionId());
    d->children.append(edlObj);
}

EDLGroup *EDLGroup::newGroup(const QString &name)
{
    auto eg = new EDLGroup(this);
    eg->setName(name);
    addChild(eg);
    return eg;
}

EDLDataset *EDLGroup::newDataset(const QString &name)
{
    auto ds = new EDLDataset(this);
    ds->setName(name);
    addChild(ds);
    return ds;
}

bool EDLGroup::save()
{
    // check if we even have a directory to save this data
    if (rootPath().isEmpty()) {
        setLastError(QStringLiteral("Unable to save experiment data: No root directory is set."));
        return false;
    }

    // save all our subnodes first
    QMutableListIterator<EDLObject*> i(d->children);
    while (i.hasNext()) {
        auto obj = i.next();
        if (obj->parent() != this) {
            qWarning().noquote() << QStringLiteral("Unlinking EDL child '%1' that doesn't believe '%2' is its parent.").arg(obj->name()).arg(name());
            i.remove();
            continue;
        }
        if (!obj->save()) {
            setLastError(QStringLiteral("Saving of '%1' failed: %2").arg(obj->name()).arg(obj->lastError()));
            return false;
        }
    }

    return EDLObject::save();
}

class EDLCollection::Private
{
public:
    Private() {}
    ~Private() {}

};

EDLCollection::EDLCollection(const QString &name, QObject *parent)
    : d(new EDLCollection::Private)
{
    setObjectKind(EDLObjectKind::COLLECTION);
    setParent(parent);
    setName(name);

    // a collection must have a unique ID to identify all nodes that belong to it
    // by default, we set a version 4 (random) UUID
    setCollectionId(QUuid::createUuid());
}

EDLCollection::~EDLCollection()
{}

QString EDLCollection::generatorId() const
{
    return EDLGroup::generatorId();
}

void EDLCollection::setGeneratorId(const QString &idString)
{
    EDLGroup::setGeneratorId(idString);
}

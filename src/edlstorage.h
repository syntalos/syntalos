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

#pragma once

#include <memory>
#include <QObject>
#include <QHash>
#include <QDateTime>
#include <QFileInfo>
#include <QUuid>

enum class EDLObjectKind
{
    UNKNOWN,
    COLLECTION,
    GROUP,
    DATASET
};

class EDLAuthor
{
public:
    explicit EDLAuthor(const QString &name, const QString &email)
        : name(name), email(email)
    {}
    QString name;
    QString email;
    QHash<QString, QString> values;
};

class EDLDataPart
{
public:
    explicit EDLDataPart()
        : index(-1)
    {}
    explicit EDLDataPart(const QString &fname)
        : index(-1),
          fname(QFileInfo(fname).fileName())
    {}
    int index;
    QString fname;
};

class EDLDataFile
{
public:
    explicit EDLDataFile()
    {}
    QString className;
    QString fileType;
    QString mediaType;
    QList<EDLDataPart> parts;
};

class EDLGroup;
class EDLDataset;

/**
 * @brief Base class for all EDL entities
 */
class EDLObject
{
    friend class EDLGroup;
public:
    explicit EDLObject(EDLObjectKind kind, EDLObject *parent = nullptr);
    ~EDLObject();

    EDLObjectKind objectKind() const;
    QString objectKindString() const;
    EDLObject *parent() const;

    QString name() const;
    virtual bool setName(const QString &name);

    QDateTime timeCreated() const;
    void setTimeCreated(const QDateTime &time);

    QUuid collectionId() const;
    virtual void setCollectionId(const QUuid &uuid);

    void addAuthor(const EDLAuthor author);
    QList<EDLAuthor> authors() const;

    QString path() const;
    void setPath(const QString &path);

    QString rootPath() const;

    QHash<QString, QVariant> attributes() const;
    void setAttributes(const QHash<QString, QVariant> &attributes);

    virtual bool save();

    QString lastError() const;

    QString serializeManifest();
    QString serializeAttributes();

protected:
    void setObjectKind(const EDLObjectKind &kind);
    void setParent(EDLObject *parent);
    void setLastError(const QString &message);

    virtual void setRootPath(const QString &root);

    void setDataObjects(std::optional<EDLDataFile> dataFile,
                        std::optional<EDLDataFile> auxDataFile = std::nullopt);

    bool saveManifest();
    bool saveAttributes();

    QString generatorId() const;
    void setGeneratorId(const QString &idString);

private:
    class Private;
    Q_DISABLE_COPY(EDLObject)
    std::unique_ptr<Private> d;
};

/**
 * @brief An EDL dataset
 *
 * A set of data files which belongs together
 * (usually data of the same modality from the same source)
 */
class EDLDataset : public EDLObject
{
public:
    explicit EDLDataset(EDLGroup *parent = nullptr);
    ~EDLDataset();

    bool save() override;

    QString setDataFile(const QString &fname);
    QString addDataFilePart(const QString &fname, int index = -1);

    QString setAuxDataFile(const QString &fname);
    QString addAuxDataFilePart(const QString &fname, int index = -1);

    /**
     * @brief Set a pattern to find data files
     * @param wildcard Wildcard to find generated data
     *
     * Set a pattern to find generated data when the dataset object
     * is saved, if the data was generated externally and could not
     * be registered properly with addDataPartFilename().
     */
    void setDataScanPattern(const QString &wildcard);
    void setAuxDataScanPattern(const QString &wildcard);

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
     * An empty string may be returned in case no path could be created.
     */
    QString pathForDataBasename(const QString &baseName);

private:
    class Private;
    Q_DISABLE_COPY(EDLDataset)
    std::unique_ptr<Private> d;

    QStringList findFilesByPattern(const QString &wildcard);
};

/**
 * @brief A grouping of groups or datasets
 */
class EDLGroup : public EDLObject
{
public:
    explicit EDLGroup(EDLGroup *parent = nullptr);
    ~EDLGroup();

    bool setName(const QString &name) override;
    void setRootPath(const QString &root) override;
    void setCollectionId(const QUuid &uuid) override;

    QList<std::shared_ptr<EDLObject>> children() const;
    void addChild(std::shared_ptr<EDLObject> edlObj);

    std::shared_ptr<EDLGroup> groupByName(const QString &name, bool create = false);
    std::shared_ptr<EDLDataset> datasetByName(const QString &name, bool create = false);

    bool save() override;

private:
    class Private;
    Q_DISABLE_COPY(EDLGroup)
    std::unique_ptr<Private> d;
};

/**
 * @brief A collection of groups and datasets
 */
class EDLCollection : public EDLGroup
{
public:
    explicit EDLCollection(const QString &name);
    ~EDLCollection();

    QString generatorId() const;
    void setGeneratorId(const QString &idString);

private:
    class Private;
    Q_DISABLE_COPY(EDLCollection)
    std::unique_ptr<Private> d;
};

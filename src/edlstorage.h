/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <QObject>
#include <QUuid>
#include <memory>
#include <optional>

enum class EDLUnitKind {
    UNKNOWN,
    COLLECTION,
    GROUP,
    DATASET
};

class EDLAuthor
{
public:
    explicit EDLAuthor(const QString &aName, const QString &aEmail)
        : name(aName),
          email(aEmail)
    {
    }
    explicit EDLAuthor()
        : name(QString()),
          email(QString())
    {
    }

    bool isValid() const
    {
        return !name.isEmpty();
    }

    QString name;
    QString email;
    QHash<QString, QString> values;
};

class EDLDataPart
{
public:
    explicit EDLDataPart()
        : index(-1)
    {
    }
    explicit EDLDataPart(const QString &filename)
        : index(-1),
          fname(QFileInfo(filename).fileName())
    {
    }
    int index;
    QString fname;
};

class EDLDataFile
{
public:
    explicit EDLDataFile() {}
    QString className;
    QString fileType;
    QString mediaType;
    QString summary;
    QList<EDLDataPart> parts;
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

    EDLUnitKind objectKind() const;
    QString objectKindString() const;
    EDLUnit *parent() const;

    QString name() const;
    virtual bool setName(const QString &name);

    QDateTime timeCreated() const;
    void setTimeCreated(const QDateTime &time);

    QUuid collectionId() const;
    virtual void setCollectionId(const QUuid &uuid);
    QString collectionShortTag() const;

    void addAuthor(const EDLAuthor &author);
    QList<EDLAuthor> authors() const;

    QString path() const;
    void setPath(const QString &path);

    QString rootPath() const;

    QHash<QString, QVariant> attributes() const;
    void setAttributes(const QHash<QString, QVariant> &attributes);
    void insertAttribute(const QString &key, const QVariant &value);

    virtual bool save();

    QString lastError() const;

    QString serializeManifest();
    QString serializeAttributes();

protected:
    void setObjectKind(const EDLUnitKind &kind);
    void setParent(EDLUnit *parent);
    void setLastError(const QString &message);

    virtual void setRootPath(const QString &root);

    void setDataObjects(
        std::optional<EDLDataFile> dataFile,
        const QList<EDLDataFile> &auxDataFiles = QList<EDLDataFile>());

    bool saveManifest();
    bool saveAttributes();

    QString generatorId() const;
    void setGeneratorId(const QString &idString);

private:
    class Private;
    Q_DISABLE_COPY(EDLUnit)
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
    ~EDLDataset();

    bool save() override;

    bool isEmpty() const;

    QString setDataFile(const QString &fname, const QString &summary = QString());
    QString addDataFilePart(const QString &fname, int index = -1);
    EDLDataFile dataFile() const;

    QString addAuxDataFile(const QString &fname, const QString &key = QString(), const QString &summary = QString());
    QString addAuxDataFilePart(const QString &fname, const QString &key = QString(), int index = -1);

    /**
     * @brief Set a pattern to find data files
     * @param wildcard Wildcard to find generated data
     *
     * Set a pattern to find generated data when the dataset object
     * is saved, if the data was generated externally and could not
     * be registered properly with addDataPartFilename().
     */
    void setDataScanPattern(const QString &wildcard, const QString &summary = QString());
    void addAuxDataScanPattern(const QString &wildcard, const QString &summary = QString());

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
    QString pathForDataPart(const EDLDataPart &dpart);

private:
    class Private;
    Q_DISABLE_COPY(EDLDataset)
    std::unique_ptr<Private> d;

    QStringList findFilesByPattern(const QString &wildcard);
};

/**
 * @brief A grouping of groups or datasets
 */
class EDLGroup : public EDLUnit
{
public:
    explicit EDLGroup(EDLGroup *parent = nullptr);
    ~EDLGroup();

    bool setName(const QString &name) override;
    void setRootPath(const QString &root) override;
    void setCollectionId(const QUuid &uuid) override;

    QList<std::shared_ptr<EDLUnit>> children() const;
    void addChild(std::shared_ptr<EDLUnit> edlObj);

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

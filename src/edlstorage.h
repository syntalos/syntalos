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

#include <QObject>
#include <QHash>
#include <QDateTime>

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
          fname(fname)
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

/**
 * @brief Base class for all EDL entities
 */
class EDLObject : public QObject
{
    Q_OBJECT
public:
    explicit EDLObject(EDLObjectKind kind, QObject *parent = nullptr);
    ~EDLObject();

    EDLObjectKind objectKind() const;
    QString objectKindString() const;

    QString name() const;
    virtual void setName(const QString &name);

    QDateTime timeCreated() const;
    void setTimeCreated(const QDateTime &time);

    QUuid collectionId() const;
    virtual void setCollectionId(const QUuid &uuid);

    void addAuthor(const EDLAuthor author);
    QList<EDLAuthor> authors() const;

    QString path() const;
    void setPath(const QString &path);

    QString rootPath() const;
    virtual void setRootPath(const QString &root);

    QHash<QString, QVariant> attrs() const;
    void setAttrs(const QHash<QString, QVariant> &attributes);

    virtual bool save();

    QString lastError() const;

protected:
    void setObjectKind(const EDLObjectKind &kind);
    void setLastError(const QString &message);

    bool saveManifest(const QString &generator = QString(),
                      std::optional<EDLDataFile> dataFile = std::nullopt,
                      std::optional<EDLDataFile> auxDataFile = std::nullopt);
    bool saveAttributes();

private:
    class Private;
    Q_DISABLE_COPY(EDLObject)
    QScopedPointer<Private> d;
};

/**
 * @brief A dataset
 */
class EDLDataset : public EDLObject
{
    Q_OBJECT
public:
    explicit EDLDataset(EDLObject *parent = nullptr);
    ~EDLDataset();

    bool save() override;
    void addDataFilePart(const QString &fname, int index = -1);

private:
    class Private;
    Q_DISABLE_COPY(EDLDataset)
    QScopedPointer<Private> d;
};

/**
 * @brief A grouping of groups or datasets
 */
class EDLGroup : public EDLObject
{
    Q_OBJECT
public:
    explicit EDLGroup(EDLObject *parent = nullptr);
    ~EDLGroup();

    void setName(const QString &name) override;
    void setRootPath(const QString &root) override;
    void setCollectionId(const QUuid &uuid) override;

    QList<EDLObject*> children() const;
    void addChild(EDLObject *edlObj);

    EDLGroup *newGroup(const QString &name);
    EDLDataset *newDataset(const QString &name);

    bool save() override;

private:
    class Private;
    Q_DISABLE_COPY(EDLGroup)
    QScopedPointer<Private> d;
};

/**
 * @brief A collection of groups and datasets
 */
class EDLCollection : public EDLGroup
{
    Q_OBJECT
public:
    explicit EDLCollection(const QString &name, QObject *parent = nullptr);
    ~EDLCollection();

    QString generatorId() const;
    void setGeneratorId(const QString &idString);

    bool save() override;

private:
    class Private;
    Q_DISABLE_COPY(EDLCollection)
    QScopedPointer<Private> d;
};

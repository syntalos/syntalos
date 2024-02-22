/*
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QAbstractListModel>
#include <QList>

#include "edlstorage.h"
#include "moduleapi.h"

class TestSubjectListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit TestSubjectListModel(const QList<TestSubject> &subjects, QObject *parent = nullptr);
    explicit TestSubjectListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool removeRows(int position, int rows, const QModelIndex &parent) override;
    bool removeRow(int row, const QModelIndex &parent = QModelIndex());

    void insertSubject(int row, const TestSubject &subject);
    void addSubject(const TestSubject &subject);
    TestSubject subject(int row) const;

    QVariantHash toVariantHash();
    void fromVariantHash(const QVariantHash &var);

    void clear();

private:
    QList<TestSubject> m_subjects;
};

class ExperimenterListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit ExperimenterListModel(const QList<EDLAuthor> &people, QObject *parent = nullptr);
    explicit ExperimenterListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    bool isEmpty() const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool removeRows(int position, int rows, const QModelIndex &parent) override;
    bool removeRow(int row, const QModelIndex &parent = QModelIndex());

    void insertPerson(int row, const EDLAuthor &person);
    void addPerson(const EDLAuthor &person);
    EDLAuthor person(int row) const;

    QVariantHash toVariantHash() const;
    void fromVariantHash(const QVariantHash &var);
    QStringList toStringList() const;

    void clear();

private:
    QList<EDLAuthor> m_people;
};

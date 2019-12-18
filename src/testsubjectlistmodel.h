/*
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TESTSUBJECTLISTMODEL_H
#define TESTSUBJECTLISTMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QJsonArray>

#include "moduleapi.h"

class TestSubjectListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit TestSubjectListModel(const QList<TestSubject>& subjects,
                                  QObject *parent = nullptr);
    explicit TestSubjectListModel(QObject *parent = nullptr);

    // Basic functionality:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;

    bool removeRows(int position, int rows, const QModelIndex &parent) override;
    bool removeRow(int row, const QModelIndex &parent = QModelIndex());

    void insertSubject(int row, TestSubject subject);
    void addSubject(const TestSubject subject);
    TestSubject subject(int row) const;

    QJsonArray toJson();
    void fromJson(const QJsonArray& json);

    void clear();

private:
    QList<TestSubject> m_subjects;
};

#endif // TESTSUBJECTLISTMODEL_H

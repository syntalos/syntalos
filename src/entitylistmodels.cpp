/*
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include "entitylistmodels.h"

TestSubjectListModel::TestSubjectListModel(const QList<TestSubject> &subjects,
                                           QObject *parent)
    : QAbstractListModel(parent)
{
    m_subjects = subjects;
}

TestSubjectListModel::TestSubjectListModel(QObject *parent)
    : QAbstractListModel(parent)
{}

int TestSubjectListModel::rowCount(const QModelIndex &parent) const
{
    // For list models only the root node (an invalid parent) should return the list's size. For all
    // other (valid) parents, rowCount() should return 0 so that it does not become a tree model.
    if (parent.isValid())
        return 0;

    return m_subjects.count();
}

QVariant TestSubjectListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= m_subjects.size())
        return QVariant();

    if (role == Qt::DisplayRole)
        return m_subjects.at(index.row()).id;
    else
        return QVariant();
}

TestSubject TestSubjectListModel::subject(int row) const
{
    if ((row >= m_subjects.count()) || (row < 0))
        return TestSubject();
    return m_subjects.at(row);
}

bool TestSubjectListModel::removeRows(int position, int rows, const QModelIndex&)
{
    beginRemoveRows(QModelIndex(), position, position+rows-1);

    for (int row = 0; row < rows; ++row) {
        m_subjects.removeAt(position);
    }

    endRemoveRows();
    return true;
}

bool TestSubjectListModel::removeRow(int row, const QModelIndex&)
{
    beginRemoveRows(QModelIndex(), row, row);
    m_subjects.removeAt(row);
    endRemoveRows();
    return true;
}

void TestSubjectListModel::insertSubject(int row, const TestSubject &subject)
{
    beginInsertRows(QModelIndex(), row, row);
    m_subjects.insert(row, subject);
    endInsertRows();
}

void TestSubjectListModel::addSubject(const TestSubject &subject)
{
    beginInsertRows(QModelIndex(), m_subjects.count(), m_subjects.count());
    m_subjects.append(subject);
    endInsertRows();
}

QVariantHash TestSubjectListModel::toVariantHash()
{
    QVariantList list;

    for (auto &sub : m_subjects) {
        QVariantHash vsub;

        vsub["id"] = sub.id;
        vsub["group"] = sub.group;
        vsub["active"] = sub.active;
        vsub["comment"] = sub.comment;

        list.append(vsub);
    }

    QVariantHash var;
    if (!list.isEmpty())
        var.insert("subject", list);
    return var;
}

void TestSubjectListModel::fromVariantHash(const QVariantHash &var)
{
    clear();

    QVariantList vList;
    for (const auto &v : var.values()) {
        if (v.type() == QVariant::List)
            vList = v.toList();
    }
    if (vList.isEmpty())
        return;

    beginInsertRows(QModelIndex(), 0, 0);
    for (const auto &v : vList) {
        const auto vsub = v.toHash();
        if (vsub.isEmpty())
            continue;
        TestSubject sub;

        sub.id = vsub.value("id").toString();
        sub.group = vsub.value("group").toString();
        sub.active = vsub.value("active").toBool();
        sub.comment = vsub.value("comment").toString();

        m_subjects.append(sub);
    }
    endInsertRows();
}

void TestSubjectListModel::clear()
{
    m_subjects.clear();
}

ExperimenterListModel::ExperimenterListModel(const QList<EDLAuthor> &people,
                                           QObject *parent)
    : QAbstractListModel(parent)
{
    m_people = people;
}

ExperimenterListModel::ExperimenterListModel(QObject *parent)
    : QAbstractListModel(parent)
{}

int ExperimenterListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_people.count();
}

bool ExperimenterListModel::isEmpty() const
{
    return m_people.isEmpty();
}

static QString personToDisplayString(const EDLAuthor &p)
{
    if (p.isValid()) {
        if (p.email.isEmpty())
            return p.name;
        else
            return QStringLiteral("%1 <%2>").arg(p.name).arg(p.email);
    } else {
        return QStringLiteral("[Person not set]");
    }
}

QVariant ExperimenterListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= m_people.size())
        return QVariant();

    if (role == Qt::DisplayRole)
        return personToDisplayString(m_people.at(index.row()));
    else
        return QVariant();
}

bool ExperimenterListModel::removeRows(int position, int rows, const QModelIndex&)
{
    beginRemoveRows(QModelIndex(), position, position+rows-1);

    for (int row = 0; row < rows; ++row) {
        m_people.removeAt(position);
    }

    endRemoveRows();
    return true;
}

bool ExperimenterListModel::removeRow(int row, const QModelIndex&)
{
    beginRemoveRows(QModelIndex(), row, row);
    m_people.removeAt(row);
    endRemoveRows();
    return true;
}

void ExperimenterListModel::insertPerson(int row, const EDLAuthor &person)
{
    beginInsertRows(QModelIndex(), row, row);
    m_people.insert(row, person);
    endInsertRows();
}

void ExperimenterListModel::addPerson(const EDLAuthor &person)
{
    beginInsertRows(QModelIndex(), m_people.count(), m_people.count());
    m_people.append(person);
    endInsertRows();
}

EDLAuthor ExperimenterListModel::person(int row) const
{
    if ((row >= m_people.count()) || (row < 0))
        return EDLAuthor();
    return m_people.at(row);
}

QVariantHash ExperimenterListModel::toVariantHash() const
{
    QVariantList list;

    for (auto &person : m_people) {
        QVariantHash vp;

        vp["name"] = person.name;
        vp["email"] = person.email;

        list.append(vp);
    }

    QVariantHash var;
    if (!list.isEmpty())
        var.insert("experimenter", list);
    return var;
}

void ExperimenterListModel::fromVariantHash(const QVariantHash &var)
{
    clear();

    QVariantList vList;
    for (const auto &v : var.values()) {
        if (v.type() == QVariant::List)
            vList = v.toList();
    }
    if (vList.isEmpty())
        return;

    beginInsertRows(QModelIndex(), 0, 0);
    for (const auto &v : vList) {
        const auto vp = v.toHash();
        if (vp.isEmpty())
            continue;
        EDLAuthor person;

        person.name = vp.value("name").toString();
        person.email = vp.value("email").toString();

        m_people.append(person);
    }
    endInsertRows();
}

QStringList ExperimenterListModel::toStringList() const
{
    QStringList list;
    for (auto &person : m_people)
        list.append(personToDisplayString(person));
    return list;
}

void ExperimenterListModel::clear()
{
    m_people.clear();
}

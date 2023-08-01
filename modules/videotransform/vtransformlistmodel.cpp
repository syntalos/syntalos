/*
 * Copyright (C) 2020-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include "vtransformlistmodel.h"

VTransformListModel::VTransformListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int VTransformListModel::rowCount(const QModelIndex &parent) const
{
    // For list models only the root node (an invalid parent) should return the list's size. For all
    // other (valid) parents, rowCount() should return 0 so that it does not become a tree model.
    if (parent.isValid())
        return 0;

    return m_vtfs.count();
}

QVariant VTransformListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= m_vtfs.size())
        return QVariant();

    if (role == Qt::DisplayRole)
        return m_vtfs.at(index.row())->name();
    else if (role == Qt::DecorationRole)
        return m_vtfs.at(index.row())->icon();
    else
        return QVariant();
}

std::shared_ptr<VideoTransform> VTransformListModel::transform(int row) const
{
    if ((row >= m_vtfs.count()) || (row < 0))
        return nullptr;
    return m_vtfs.at(row);
}

bool VTransformListModel::removeRows(int position, int rows, const QModelIndex &)
{
    beginRemoveRows(QModelIndex(), position, position + rows - 1);

    for (int row = 0; row < rows; ++row) {
        m_vtfs.removeAt(position);
    }

    endRemoveRows();
    return true;
}

bool VTransformListModel::removeRow(int row, const QModelIndex &)
{
    beginRemoveRows(QModelIndex(), row, row);
    m_vtfs.removeAt(row);
    endRemoveRows();
    return true;
}

void VTransformListModel::insertTransform(int row, std::shared_ptr<VideoTransform> tf)
{
    beginInsertRows(QModelIndex(), row, row);
    m_vtfs.insert(row, tf);
    endInsertRows();
}

void VTransformListModel::addTransform(std::shared_ptr<VideoTransform> tf)
{
    beginInsertRows(QModelIndex(), m_vtfs.count(), m_vtfs.count());
    m_vtfs.append(tf);
    endInsertRows();
}

QVariantHash VTransformListModel::toVariantHash()
{
    QVariantList list;

    for (auto &tf : m_vtfs) {
        auto vh = tf->toVariantHash();
        vh.insert("type", tf->metaObject()->className());
        list.append(vh);
    }

    QVariantHash var;
    if (!list.isEmpty())
        var.insert("video_transform", list);
    return var;
}

void VTransformListModel::fromVariantHash(const QVariantHash &var)
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
        const auto vh = v.toHash();
        if (vh.isEmpty())
            continue;
        const auto objType = vh.value("type").toString();

        VideoTransform *tf;
        if (objType == CropTransform::staticMetaObject.className())
            tf = new CropTransform;
        else if (objType == ScaleTransform::staticMetaObject.className())
            tf = new ScaleTransform;
        else
            continue;

        tf->fromVariantHash(vh);
        std::shared_ptr<VideoTransform> vtfSPtr(tf);
        addTransform(vtfSPtr);
    }
    endInsertRows();
}

QList<std::shared_ptr<VideoTransform>> VTransformListModel::toList() const
{
    return QList<std::shared_ptr<VideoTransform>>(m_vtfs);
}

void VTransformListModel::clear()
{
    m_vtfs.clear();
}

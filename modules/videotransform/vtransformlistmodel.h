/*
 * Copyright (C) 2020-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include <QAbstractListModel>
#include <QList>
#include <videotransform.h>

class VTransformListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit VTransformListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool removeRows(int position, int rows, const QModelIndex &parent) override;
    bool removeRow(int row, const QModelIndex &parent = QModelIndex());

    void insertTransform(int row, std::shared_ptr<VideoTransform> tf);
    void addTransform(std::shared_ptr<VideoTransform> tf);
    std::shared_ptr<VideoTransform> transform(int row) const;

    QVariantHash toVariantHash();
    void fromVariantHash(const QVariantHash &var);

    QList<std::shared_ptr<VideoTransform>> toList() const;

    void clear();

private:
    QList<std::shared_ptr<VideoTransform>> m_vtfs;
};

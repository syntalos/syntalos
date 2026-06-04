/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "datatypeselector.h"

#include "datactl/datatypes.h"

namespace Syntalos
{

DataTypeSelector::DataTypeSelector(QWidget *parent)
    : QComboBox(parent)
{
    connect(this, &QComboBox::currentIndexChanged, this, [this]() {
        Q_EMIT selectionChanged();
    });
}

void DataTypeSelector::addNoneEntry(const QString &label)
{
    addItem(label, static_cast<int>(BaseDataType::Unknown));
}

void DataTypeSelector::addDataType(int typeId, const QString &label)
{
    const auto text = label.isEmpty() ? QString::fromStdString(BaseDataType::typeIdToString(typeId)) : label;
    addItem(text, typeId);
}

void DataTypeSelector::addAllDataTypes()
{
    for (const auto &[name, id] : streamTypeIdIndex())
        addItem(QString::fromStdString(name), id);
}

int DataTypeSelector::selectedTypeId() const
{
    return currentData().toInt();
}

void DataTypeSelector::setSelectedTypeId(int typeId)
{
    const int idx = findData(typeId);
    setCurrentIndex(idx >= 0 ? idx : 0);
}

QString DataTypeSelector::selectedTypeName() const
{
    return QString::fromStdString(BaseDataType::typeIdToString(selectedTypeId()));
}

void DataTypeSelector::setSelectedTypeName(const QString &typeName)
{
    setSelectedTypeId(static_cast<int>(BaseDataType::typeIdFromString(typeName.toStdString())));
}

} // namespace Syntalos

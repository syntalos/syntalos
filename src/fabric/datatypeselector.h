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

#pragma once

#include <QComboBox>

namespace Syntalos
{

/**
 * @brief Combo box for selecting a single stream data type.
 *
 * Many modules can only consume one input stream type at a time (writers,
 * meters, ...) and let the user pick which one. This widget centralizes that
 * selection: it offers an optional "None" entry, lets the module advertise
 * either a restricted set of supported types or every registered stream type,
 * and (de)serializes the choice via the type's canonical name.
 */
class DataTypeSelector : public QComboBox
{
    Q_OBJECT
public:
    explicit DataTypeSelector(QWidget *parent = nullptr);

    /**
     * @brief Add a "None" entry, mapping to BaseDataType::Unknown.
     *
     * Add this first for modules that may be left unconfigured (no input port).
     */
    void addNoneEntry(const QString &label = QStringLiteral("None"));

    /**
     * @brief Add a single supported stream type.
     * @param typeId The BaseDataType::TypeId of the stream type.
     * @param label Optional display label; defaults to the type's canonical name.
     */
    void addDataType(int typeId, const QString &label = QString());

    /**
     * @brief Add every registered stream type, labelled by its type name.
     */
    void addAllDataTypes();

    /**
     * @brief TypeId of the current selection (BaseDataType::Unknown if "None").
     */
    int selectedTypeId() const;
    void setSelectedTypeId(int typeId);

    /**
     * @brief Canonical type name of the current selection, for serialization.
     */
    QString selectedTypeName() const;
    void setSelectedTypeName(const QString &typeName);

Q_SIGNALS:
    void selectionChanged();
};

} // namespace Syntalos

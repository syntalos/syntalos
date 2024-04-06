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

#include "datatypes.h"
#include "frametype.h"

#ifndef NO_TID_PORTCONSTRUCTORS
#include "moduleapi.h"
#endif

#define CHECK_RETURN_INPUT_PORT(T) \
    if (typeId == BaseDataType::T) \
        return new StreamInputPort<T>(mod, id, title);

#define CHECK_RETURN_STREAM(T)     \
    if (typeId == BaseDataType::T) \
        return new DataStream<T>();

static QMap<QString, int> g_streamTypeIdMap;

void registerStreamMetaTypes()
{
    // only register the types if we have not created the global registry yet
    if (!g_streamTypeIdMap.isEmpty())
        return;

    for (auto i = BaseDataType::Unknown + 1; i < BaseDataType::Last; ++i) {
        const auto typeId = static_cast<BaseDataType::TypeId>(i);
        g_streamTypeIdMap[BaseDataType::typeIdToString(typeId)] = typeId;
    }

    // register some Qt types
    qRegisterMetaType<ModuleState>();
}

QMap<QString, int> streamTypeIdMap()
{
    return g_streamTypeIdMap;
}

VarStreamInputPort *Syntalos::newInputPortForType(
    int typeId,
    AbstractModule *mod,
    const QString &id,
    const QString &title = QString())
{
    CHECK_RETURN_INPUT_PORT(ControlCommand)
    CHECK_RETURN_INPUT_PORT(TableRow)
    CHECK_RETURN_INPUT_PORT(FirmataControl)
    CHECK_RETURN_INPUT_PORT(FirmataData)
    CHECK_RETURN_INPUT_PORT(Frame)
    CHECK_RETURN_INPUT_PORT(IntSignalBlock)
    CHECK_RETURN_INPUT_PORT(FloatSignalBlock)

    qCritical() << "Unable to create input port for unknown type ID" << typeId;
    return nullptr;
}

VariantDataStream *Syntalos::newStreamForType(int typeId)
{
    CHECK_RETURN_STREAM(ControlCommand)
    CHECK_RETURN_STREAM(TableRow)
    CHECK_RETURN_STREAM(FirmataControl)
    CHECK_RETURN_STREAM(FirmataData)
    CHECK_RETURN_STREAM(Frame)
    CHECK_RETURN_STREAM(IntSignalBlock)
    CHECK_RETURN_STREAM(FloatSignalBlock)

    qCritical() << "Unable to create data stream for unknown type ID" << typeId;
    return nullptr;
}

QString connectionHeatToHumanString(ConnectionHeatLevel heat)
{
    switch (heat) {
    case ConnectionHeatLevel::NONE:
        return QStringLiteral("none");
    case ConnectionHeatLevel::LOW:
        return QStringLiteral("low");
    case ConnectionHeatLevel::MEDIUM:
        return QStringLiteral("medium");
    case ConnectionHeatLevel::HIGH:
        return QStringLiteral("high");
    }

    return QStringLiteral("unknown");
}

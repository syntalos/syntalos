/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include "datatypes.h"
#include "frametype.h"

#ifndef NO_TID_PORTCONSTRUCTORS
#include "moduleapi.h"
#endif

#define CHECK_RETURN_INPUT_PORT( T ) \
    if (typeId == qMetaTypeId<T>()) \
        return new StreamInputPort<T>(mod, id, title);

#define CHECK_RETURN_STREAM( T ) \
    if (typeId == qMetaTypeId<T>()) \
        return new DataStream<T>();

static QMap<QString, int> g_streamTypeIdMap;

template<typename T, bool isPrimary = true>
static void registerStreamType()
{
    qRegisterMetaType<T>();
    if constexpr(isPrimary) {
        auto id = qMetaTypeId<T>();
        g_streamTypeIdMap[QMetaType::typeName(id)] = id;
    }
}

void registerStreamMetaTypes()
{
    // only register the types if we have not created the global registry yet
    if (!g_streamTypeIdMap.isEmpty())
        return;

    registerStreamType<ModuleState, false>();
    registerStreamType<ControlCommand>();
    registerStreamType<TableRow>();
    registerStreamType<FirmataControl>();
    registerStreamType<FirmataData>();
    registerStreamType<Frame>();
    registerStreamType<SignalDataType, false>();
    registerStreamType<IntSignalBlock>();
    registerStreamType<FloatSignalBlock>();
}

QMap<QString, int> streamTypeIdMap()
{
    return g_streamTypeIdMap;
}

#ifndef NO_TID_PORTCONSTRUCTORS

VarStreamInputPort *newInputPortForType(int typeId, AbstractModule *mod, const QString &id, const QString &title = QString())
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

VariantDataStream *newStreamForType(int typeId)
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

#endif

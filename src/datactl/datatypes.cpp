/*
 * Copyright (C) 2019-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "datactl/datatypes.h"
#include "datactl/frametype.h"

namespace Syntalos
{

static std::vector<std::pair<std::string, int>> g_streamTypeIdIndex;

std::string toString(ModuleState state)
{
    switch (state) {
    case ModuleState::UNKNOWN:
        return "unknown";
    case ModuleState::INITIALIZING:
        return "initializing";
    case ModuleState::IDLE:
        return "idle";
    case ModuleState::PREPARING:
        return "preparing";
    case ModuleState::DORMANT:
        return "dormant";
    case ModuleState::READY:
        return "ready";
    case ModuleState::RUNNING:
        return "running";
    case ModuleState::ERROR:
        return "error";
    default:
        return "invalid-state";
    }
}

void registerStreamMetaTypes()
{
    // only register the types if we have not created the global registry yet
    if (!g_streamTypeIdIndex.empty())
        return;

    g_streamTypeIdIndex.reserve(static_cast<size_t>(BaseDataType::Last) - 1);
    for (auto i = BaseDataType::Unknown + 1; i < BaseDataType::Last; ++i) {
        const auto typeId = static_cast<BaseDataType::TypeId>(i);
        g_streamTypeIdIndex.emplace_back(BaseDataType::typeIdToString(typeId), typeId);
    }
}

std::vector<std::pair<std::string, int>> streamTypeIdIndex()
{
    return g_streamTypeIdIndex;
}

std::string BaseDataType::typeIdToString(int value)
{
    if (!typeIdIsValid(value))
        return "<<unknown>>";
    return typeIdToString(static_cast<TypeId>(value));
}

std::string BaseDataType::typeIdToString(TypeId value)
{
    if (value == Unknown)
        return "Unknown";
    std::string result = "<<unknown>>";
    forEachStreamType([&](auto tag) {
        using T = typename decltype(tag)::type;
        if (T::staticTypeId() != value)
            return false;
        result = T::staticTypeName();
        return true;
    });
    return result;
}

BaseDataType::TypeId BaseDataType::typeIdFromString(const std::string &str)
{
    if (str == "Unknown")
        return TypeId::Unknown;
    TypeId result = TypeId::Unknown;
    forEachStreamType([&](auto tag) {
        using T = typename decltype(tag)::type;
        if (str != T::staticTypeName())
            return false;
        result = T::staticTypeId();
        return true;
    });
    return result;
}

SignalBlockI32::SignalBlockI32(struct SignalBlockU16 &&src)
{
    // The data matrix has a different scalar type, so the cast must read every
    // element anyway; only the timestamps can actually be moved.
    timestamps = std::move(src.timestamps);
    data = src.data.cast<int32_t>();
}

SignalBlockU16::SignalBlockU16(struct SignalBlockI32 &&src)
{
    timestamps = std::move(src.timestamps);
    data = src.data.cast<uint16_t>();
}

} // namespace Syntalos

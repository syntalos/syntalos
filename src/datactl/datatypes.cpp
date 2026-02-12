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

static std::vector<std::pair<std::string, int>> g_streamTypeIdIndex;

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

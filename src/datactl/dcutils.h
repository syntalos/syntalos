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

#include <array>
#include <cstdint>
#include <string>
#include <QStringList>

namespace Syntalos
{

/**
 * @brief A 16-byte UUID (Universally Unique Identifier).
 */
using Uuid = std::array<uint8_t, 16>;

/**
 * @brief Generate a Version 7 UUID (UUIDv7) based on a timestamp and random data.
 *
 * The function generates a UUID conforming to Version 7 of the UUID format.
 *
 * @return A new UUIDv7.
 */
Uuid newUuid7();

/**
 * @brief Convert a UUID to a canonical lowercase string representation.
 *
 * @param uuid The UUID to be converted.
 * @return The UUID in 8-4-4-4-12 format.
 */
std::string toHex(const Uuid &uuid);

/**
 * @brief Naturally sort the give string list (sorting "10" after "9")
 **/
QStringList stringListNaturalSort(QStringList &list);

} // namespace Syntalos

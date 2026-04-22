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
#include <optional>

namespace Syntalos
{

/**
 * @brief A 16-byte UUID (Universally Unique Identifier).
 */
struct Uuid {
    std::array<uint8_t, 16> bytes{};

    explicit Uuid() {};
    explicit Uuid(const std::array<uint8_t, 16> &bytes)
        : bytes(bytes)
    {
    }

    bool operator==(const Uuid &other) const
    {
        return bytes == other.bytes;
    }

    uint8_t &operator[](size_t i)
    {
        return bytes[i];
    }
    const uint8_t &operator[](size_t i) const
    {
        return bytes[i];
    }

    /**
     * @brief Convert UUID to a canonical lowercase string representation.
     *
     * @return The UUID in 8-4-4-4-12 format.
     */
    std::string toHex() const;

    /**
     * Create new UUID from a hex string.
     * @param str The UUID string.
     *
     * @return A new UUID, or std::nullopt if conversion failed.
     */
    static std::optional<Uuid> fromHex(const std::string &str);
};

/**
 * @brief Generate a Version 7 UUID (UUIDv7) based on a timestamp and random data.
 *
 * The function generates a UUID conforming to Version 7 of the UUID format.
 *
 * @return A new UUIDv7.
 */
Uuid newUuid7();

} // namespace Syntalos

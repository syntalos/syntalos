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

#include "uuid.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <random>

namespace Syntalos
{

std::string Uuid::toHex() const
{
    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(36);

    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            out += '-';

        const auto b = bytes[i];
        out += hex[(b >> 4) & 0x0F];
        out += hex[b & 0x0F];
    }

    return out;
}

std::optional<Uuid> Uuid::fromHex(const std::string &s)
{
    Uuid uuid{};
    size_t byteIdx = 0;
    for (size_t i = 0; i < s.size() && byteIdx < 16; ++i) {
        const char c = s[i];
        if (c == '-')
            continue;
        auto hexVal = [](char ch) -> uint8_t {
            if (ch >= '0' && ch <= '9')
                return static_cast<uint8_t>(ch - '0');
            if (ch >= 'a' && ch <= 'f')
                return static_cast<uint8_t>(ch - 'a' + 10);
            if (ch >= 'A' && ch <= 'F')
                return static_cast<uint8_t>(ch - 'A' + 10);
            return 0;
        };
        if (i + 1 < s.size() && s[i + 1] != '-') {
            uuid.bytes[byteIdx++] = static_cast<uint8_t>((hexVal(c) << 4) | hexVal(s[i + 1]));
            ++i;
        }
    }

    return uuid;
}

Uuid newUuid7()
{
    std::array<uint8_t, 16> value;

    // random bytes
    std::random_device rd;
    std::uniform_int_distribution<uint16_t> dist(0, 0xFF);
    std::generate(value.begin(), value.end(), [&]() {
        return static_cast<uint8_t>(dist(rd));
    });

    // RFC 9562 UUIDv7 uses a 48-bit Unix timestamp in milliseconds.
    auto now = std::chrono::system_clock::now();
    const auto millis = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());

    // timestamp
    value[0] = (millis >> 40) & 0xFF;
    value[1] = (millis >> 32) & 0xFF;
    value[2] = (millis >> 24) & 0xFF;
    value[3] = (millis >> 16) & 0xFF;
    value[4] = (millis >> 8) & 0xFF;
    value[5] = millis & 0xFF;

    // version and variant
    value[6] = (value[6] & 0x0F) | 0x70;
    value[8] = (value[8] & 0x3F) | 0x80;

    return Uuid(value);
}

} // namespace Syntalos

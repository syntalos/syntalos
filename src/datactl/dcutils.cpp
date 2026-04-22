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

#include "dcutils.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <QCollator>

namespace Syntalos
{

Uuid newUuid7()
{
    Uuid value;

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

    return value;
}

std::string toHex(const Uuid &uuid)
{
    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(36);

    for (size_t i = 0; i < uuid.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            out += '-';

        const auto b = uuid[i];
        out += hex[(b >> 4) & 0x0F];
        out += hex[b & 0x0F];
    }

    return out;
}

QStringList stringListNaturalSort(QStringList &list)
{
    if (list.isEmpty())
        return list;

    // prefer en_DK unless that isn't available.
    // we previously defaulted to "C", but doing that
    // will produce the wrong sorting order
    QCollator collator(QLocale("en_DK.UTF-8"));
    if (collator.locale().language() == QLocale::C) {
        collator = QCollator();
        if (collator.locale().language() == QLocale::C) {
            collator = QCollator(QLocale("en"));
            if (collator.locale().language() == QLocale::C)
                qWarning() << "Unable to find a non-C locale for collator.";
        }
    }
    collator.setNumericMode(true);

    std::sort(list.begin(), list.end(), collator);

    return list;
}

} // namespace Syntalos

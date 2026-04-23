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

#include "streammeta.h"

#include <sstream>

namespace Syntalos
{

static void writeJsonString(std::ostream &os, const std::string &s)
{
    os << '"';
    for (unsigned char c : s) {
        switch (c) {
        case '"':
            os << "\\\"";
            break;
        case '\\':
            os << "\\\\";
            break;
        case '\b':
            os << "\\b";
            break;
        case '\f':
            os << "\\f";
            break;
        case '\n':
            os << "\\n";
            break;
        case '\r':
            os << "\\r";
            break;
        case '\t':
            os << "\\t";
            break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                os << buf;
            } else {
                os << static_cast<char>(c);
            }
        }
    }
    os << '"';
}

void writeJson(std::ostream &os, const MetaValue &val)
{
    std::visit(
        [&os](const auto &v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                os << "null";
            } else if constexpr (std::is_same_v<T, bool>) {
                os << (v ? "true" : "false");
            } else if constexpr (std::is_same_v<T, int64_t>) {
                os << v;
            } else if constexpr (std::is_same_v<T, double>) {
                os << v;
            } else if constexpr (std::is_same_v<T, std::string>) {
                writeJsonString(os, v);
            } else if constexpr (std::is_same_v<T, MetaSize>) {
                os << "{\"width\":" << v.width << ",\"height\":" << v.height << '}';
            } else if constexpr (std::is_same_v<T, MetaArray>) {
                os << '[';
                for (size_t i = 0; i < v.size(); ++i) {
                    if (i > 0)
                        os << ',';
                    writeJson(os, v[i]);
                }
                os << ']';
            } else if constexpr (std::is_same_v<T, MetaStringMap>) {
                writeJsonObject(os, v);
            }
        },
        static_cast<const MetaValue::Base &>(val));
}

void writeJsonObject(std::ostream &os, const std::map<std::string, MetaValue> &obj)
{
    os << '{';
    bool first = true;
    for (const auto &[k, v] : obj) {
        if (!first)
            os << ',';
        first = false;
        writeJsonString(os, k);
        os << ':';
        writeJson(os, v);
    }
    os << '}';
}

std::string toJsonString(const MetaValue &v)
{
    std::ostringstream os;
    writeJson(os, v);
    return os.str();
}

std::string toJsonObjectString(const MetaStringMap &m)
{
    std::ostringstream os;
    writeJsonObject(os, m);
    return os.str();
}

std::expected<MetaStringMap, std::string> stringMapFromJson(const std::string &json)
{
    size_t i = 0;
    auto skip = [&]() {
        while (i < json.size() && (json[i] == ' ' || json[i] == '\t' || json[i] == '\n' || json[i] == '\r'))
            ++i;
    };
    auto readStr = [&]() -> std::string {
        if (i >= json.size() || json[i] != '"')
            return {};
        ++i;
        std::string s;
        while (i < json.size() && json[i] != '"') {
            if (json[i] == '\\') {
                ++i;
            }
            s += json[i++];
        }
        ++i; // closing quote
        return s;
    };

    MetaStringMap result;
    skip();
    if (i < json.size() && json[i] == '{') {
        ++i;
        while (i < json.size() && json[i] != '}') {
            skip();
            if (json[i] == '"') {
                const auto key = readStr();
                skip();
                if (i < json.size() && json[i] == ':')
                    ++i;
                skip();
                if (i < json.size() && json[i] == '"') {
                    result[key] = MetaValue{readStr()};
                } else {
                    // number or boolean
                    size_t start = i;
                    while (i < json.size() && json[i] != ',' && json[i] != '}' && json[i] != ' ')
                        ++i;
                    const auto numStr = json.substr(start, i - start);
                    try {
                        result[key] = MetaValue{static_cast<int64_t>(std::stoll(numStr))};
                    } catch (...) {
                        try {
                            result[key] = MetaValue{std::stod(numStr)};
                        } catch (...) {
                        }
                    }
                }
            }
            skip();
            if (i < json.size() && json[i] == ',')
                ++i;
        }
    }

    return result;
}

} // namespace Syntalos

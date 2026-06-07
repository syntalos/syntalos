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

#include "edlutils.h"

#include <gio/gio.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace Syntalos::edl
{

std::string createRandomString(size_t len)
{
    static constexpr std::string_view chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::random_device rd;
    std::uniform_int_distribution<size_t> dist(0, chars.size() - 1);
    std::string result;
    result.reserve(len);
    for (size_t i = 0; i < len; ++i)
        result += chars[dist(rd)];
    return result;
}

toml::date_time toToml(const EdlDateTime &dt)
{
    // Extract the local_time and utc offset from the zoned_time
    const auto sysTime = dt.get_sys_time();
    const auto localTime = dt.get_local_time();

    // Compute UTC offset in minutes
    const auto offset = std::chrono::duration_cast<std::chrono::minutes>(
        localTime.time_since_epoch() - sysTime.time_since_epoch());
    const auto offsetMinutes = static_cast<int16_t>(offset.count());

    // Decompose local time into calendar fields
    const auto localDays = std::chrono::floor<std::chrono::days>(localTime);
    const auto tod = localTime - localDays;

    const std::chrono::year_month_day ymd{localDays};
    const std::chrono::hh_mm_ss<std::chrono::system_clock::duration> hms{tod};

    toml::date d;
    d.year = static_cast<int>(ymd.year());
    d.month = static_cast<uint8_t>(static_cast<unsigned>(ymd.month()));
    d.day = static_cast<uint8_t>(static_cast<unsigned>(ymd.day()));

    toml::time t;
    t.hour = static_cast<uint8_t>(hms.hours().count());
    t.minute = static_cast<uint8_t>(hms.minutes().count());
    t.second = static_cast<uint8_t>(hms.seconds().count());
    t.nanosecond = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(hms.subseconds()).count());

    toml::time_offset off;
    off.minutes = offsetMinutes;

    return toml::date_time{d, t, off};
}

static toml::array metaArrayToToml(const MetaArray &arr)
{
    toml::array result;
    for (const auto &elem : arr) {
        std::visit(
            [&result](const auto &v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::nullptr_t>) {
                    result.push_back(toml::value<std::string>{"null"});
                } else if constexpr (std::is_same_v<T, bool>) {
                    result.push_back(v);
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    result.push_back(v);
                } else if constexpr (std::is_same_v<T, double>) {
                    result.push_back(v);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    result.push_back(v);
                } else if constexpr (std::is_same_v<T, MetaSize>) {
                    toml::table sz;
                    sz.insert("width", v.width);
                    sz.insert("height", v.height);
                    result.push_back(std::move(sz));
                } else if constexpr (std::is_same_v<T, MetaArray>) {
                    result.push_back(metaArrayToToml(v));
                } else if constexpr (std::is_same_v<T, MetaStringMap>) {
                    // Nested map in array - recurse
                    toml::table inner;
                    for (const auto &[k2, v2] : v) {
                        std::visit(
                            [&inner, &k2](const auto &iv) {
                                using IT = std::decay_t<decltype(iv)>;
                                if constexpr (std::is_same_v<IT, std::nullptr_t>) {
                                    inner.insert(k2, std::string{"null"});
                                } else if constexpr (std::is_same_v<IT, bool>) {
                                    inner.insert(k2, iv);
                                } else if constexpr (std::is_same_v<IT, int64_t>) {
                                    inner.insert(k2, iv);
                                } else if constexpr (std::is_same_v<IT, double>) {
                                    inner.insert(k2, iv);
                                } else if constexpr (std::is_same_v<IT, std::string>) {
                                    inner.insert(k2, iv);
                                } else if constexpr (std::is_same_v<IT, MetaSize>) {
                                    toml::table sz;
                                    sz.insert("width", iv.width);
                                    sz.insert("height", iv.height);
                                    inner.insert(k2, std::move(sz));
                                } else if constexpr (std::is_same_v<IT, MetaArray>) {
                                    inner.insert(k2, metaArrayToToml(iv));
                                } else if constexpr (std::is_same_v<IT, MetaStringMap>) {
                                    // Deep nesting - handled recursively via toTomlTable
                                    inner.insert(k2, toTomlTable(iv));
                                }
                            },
                            static_cast<const MetaValue::Base &>(v2));
                    }
                    result.push_back(std::move(inner));
                }
            },
            static_cast<const MetaValue::Base &>(elem));
    }
    return result;
}

toml::table toTomlTable(const std::map<std::string, MetaValue> &attrs)
{
    toml::table tab;
    for (const auto &[key, val] : attrs) {
        std::visit(
            [&tab, &key](const auto &v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::nullptr_t>) {
                    // TOML has no null; skip or emit empty string
                } else if constexpr (std::is_same_v<T, bool>) {
                    tab.insert(key, v);
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    tab.insert(key, v);
                } else if constexpr (std::is_same_v<T, double>) {
                    tab.insert(key, v);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    if (!v.empty())
                        tab.insert(key, v);
                } else if constexpr (std::is_same_v<T, MetaSize>) {
                    toml::table sz;
                    sz.insert("width", v.width);
                    sz.insert("height", v.height);
                    tab.insert(key, std::move(sz));
                } else if constexpr (std::is_same_v<T, MetaArray>) {
                    if (!v.empty())
                        tab.insert(key, metaArrayToToml(v));
                } else if constexpr (std::is_same_v<T, MetaStringMap>) {
                    if (!v.empty())
                        tab.insert(key, toTomlTable(v));
                }
            },
            static_cast<const MetaValue::Base &>(val));
    }
    return tab;
}

void naturalNumListSort(std::vector<std::string> &list)
{
    std::sort(list.begin(), list.end(), [](const std::string &a, const std::string &b) {
        return strverscmp(a.c_str(), b.c_str()) < 0;
    });
}

std::string guessContentType(const fs::path &filePath, bool onlyCertain)
{
    gboolean resultUncertain = FALSE;
    g_autofree gchar *guess = g_content_type_guess(filePath.c_str(), nullptr, 0, &resultUncertain);
    if (resultUncertain && onlyCertain)
        return {};

    return (guess == nullptr ? std::string() : std::string(guess));
}

[[nodiscard]] static constexpr char asciiToLower(char c) noexcept
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

[[nodiscard]] static constexpr bool isAsciiWhitespace(char c) noexcept
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/**
 * Replace every non-overlapping occurrence of @p from with @p to, in a single pass.
 */
static void replaceAll(std::string &s, std::string_view from, std::string_view to)
{
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

/**
 * Largest byte length <= @p limit that does not fall inside a multi-byte UTF-8 sequence,
 * so truncating @p s to it can never produce invalid UTF-8.
 */
[[nodiscard]] static std::size_t utf8BoundedLength(std::string_view s, std::size_t limit) noexcept
{
    if (limit >= s.size())
        return s.size();
    // Back up over UTF-8 continuation bytes (10xxxxxx) so we cut on a codepoint boundary.
    std::size_t n = limit;
    while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80)
        --n;
    return n;
}

[[nodiscard]] std::string makeCompactName(std::string_view input, const CompactNameOptions &opts)
{
    std::string result;
    result.reserve(input.size());

    bool pendingSeparator = false;
    for (const char c : input) {
        // Optionally drop anything outside the 7-bit ASCII range
        if (opts.asciiOnly && static_cast<unsigned char>(c) > 0x7F)
            continue;

        // Drop characters that are structural in URLs (query/fragment/parameter markers),
        // as well as other characters we never want in filenames (globs, quotes, pipes, ...).
        static constexpr std::string_view droppedChars = "#?&*`\"'<>|";
        if (droppedChars.contains(c))
            continue;

        if (isAsciiWhitespace(c)) {
            // Collapse whitespace runs; emission is deferred so we never lead with a
            // separator and a trailing run is dropped for free.
            pendingSeparator = true;
            continue;
        }

        if (pendingSeparator) {
            pendingSeparator = false;
            if (opts.wordSeparator != '\0' && !result.empty())
                result.push_back(opts.wordSeparator);
        }

        // Map path/drive separators to underscores, and also replace dots
        // (to make file-extension handling more robust for naive
        // post-processing tools). Keep all other characters as-is.
        if (c == '/' || c == '\\' || c == ':')
            result.push_back('_');
        else if (c == '.')
            result.push_back('-');
        else
            result.push_back(c);
    }

    // Clean up separator/underscore artifacts, mirroring the historical replace chain.
    replaceAll(result, "_-", "-");
    replaceAll(result, "-_", "-");

    if (opts.lowercase) {
        for (char &c : result)
            c = asciiToLower(c);
    }

    // Truncate, preferring the last word boundary that still fits. maxLength is a byte
    // count; a hard cut is backed up to a UTF-8 codepoint boundary to stay valid UTF-8.
    if (result.size() > opts.maxLength) {
        const auto cut = result.find_last_of("-_", opts.maxLength);
        result.resize(cut == std::string::npos ? utf8BoundedLength(result, opts.maxLength) : cut);
        while (result.ends_with('-') || result.ends_with('_'))
            result.pop_back();
    }

    if (result.empty()) {
        if (opts.fallback.empty())
            return {};
        // Normalize the fallback through the same rules (without its own fallback, to
        // avoid recursion); if it still collapses to nothing, hard-truncate it.
        CompactNameOptions fallbackOpts = opts;
        fallbackOpts.fallback = {};
        auto fallbackResult = makeCompactName(opts.fallback, fallbackOpts);
        if (fallbackResult.empty())
            fallbackResult = std::string{opts.fallback.substr(0, utf8BoundedLength(opts.fallback, opts.maxLength))};
        return fallbackResult;
    }

    return result;
}

} // namespace Syntalos::edl

/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
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

// Private helper utilities for the datactl EDL implementation.
// Not part of the public API.

#pragma once

#include <map>
#include <ostream>
#include <string>
#include <vector>
#include <filesystem>
#include <toml++/toml.h>

#include "streammeta.h"

// EdlDateTime is defined in edlstorage.h, but we only need the underlying type here.
// We declare it manually to avoid a circular include.
#include <chrono>
namespace Syntalos
{
using EdlDateTime = std::chrono::zoned_time<std::chrono::system_clock::duration>;
}

namespace Syntalos::edl
{

namespace fs = std::filesystem;

/**
 * @brief Create a random alphanumeric string.
 */
std::string createRandomString(size_t len);

/**
 * @brief Convert an EdlDateTime to a TOML date-time with local UTC offset.
 */
toml::date_time toToml(const EdlDateTime &dt);

/**
 * @brief Convert a MetaValue map to a TOML table.
 */
toml::table toTomlTable(const std::map<std::string, MetaValue> &attrs);

/**
 * @brief Sort a vector of strings, with any numbers sorted numerically / "naturally" for humans.
 *
 * This sorts text alphabetically, but treats sequences of digits as numbers.
 * For example, "file2" will come before "file10".
 */
void naturalNumListSort(std::vector<std::string> &list);

/**
 * @brief Guess the content type (MIME type) of a file based on its contents or extension.
 *
 * This function attempts to determine the content type of a file.
 * If the result is uncertain, it can optionally return an empty string depending
 * on the `onlyCertain` parameter.
 *
 * @param filePath The path of the file for which the content type is to be determined.
 * @param onlyCertain If true, the function returns an empty string when the content type guess is uncertain.
 * @return The guessed content type as a string, or an empty string if uncertain and `onlyCertain` is set to true.
 */
std::string guessContentType(const fs::path &filePath, bool onlyCertain = true);

} // namespace Syntalos::edl

/*
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include <expected>
#include <QStringList>

namespace Syntalos
{
// Conversion helpers to convert a bunch of random stuff into QString
// This is especially useful for supporting Qt versions < 6.9, which
// didn't accept QUtf8StringView-style arguments yet in some places.

inline const QString &qstr(const QString &s) noexcept
{
    return s;
}

inline QString qstr(QStringView s)
{
    return s.toString();
}

inline QString qstr(const char *s)
{
    return QString::fromUtf8(s);
}

inline QString qstr(std::string_view s)
{
    return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size()));
}

inline QString qstr(const std::string &s)
{
    return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size()));
}

} // namespace Syntalos

/**
 * @brief Create a random alphanumeric string with the given length.
 */
QString createRandomString(int len);

/**
 * @brief Simplify a string for use as a module name
 */
QString simplifyStrForModuleName(const QString &s);

/**
 * @brief Simplify a string for use in file basenames.
 */
QString simplifyStrForFileBasename(const QString &s);

/**
 * @brief Simplify a string for use in file basenames, and return a lowercased version.
 */
QString simplifyStrForFileBasenameLower(const QString &s);

/**
 * @brief Split a string, limiting the amount of splits made
 */
QStringList qStringSplitLimit(
    const QString &str,
    const QChar &sep,
    int maxSplit,
    Qt::CaseSensitivity cs = Qt::CaseSensitive);

/**
 * @brief Naturally sort the give string list (sorting "10" after "9")
 **/
QStringList stringListNaturalSort(QStringList &list);

/**
 * @brief Return the complete current Syntalos version number.
 */
QString syntalosVersionFull();

/**
 * @brief Check if Syntalos is running in a Flatpak sandbox.
 */
bool isInFlatpakSandbox();

/**
 * @brief Get path to the OSes default temporary directory.
 * @return An absolute path to temp dir.
 */
QString tempDirRoot();

/**
 * @brief Get path to a temporary directory that can hold large files.
 * @return An absolute path to temp dir.
 */
QString tempDirLargeRoot();

/**
 * @brief Find file on the host system (if running in a sandbox)
 * @param path Path of the file to check for
 * @return The absolute path to the requested file, or an empty string if not found.
 */
QString findHostFile(const QString &path);

/**
 * @brief Check if a udev rule exists.
 * @param ruleFilename Name of the udev rule file
 * @return true if the rule existed, false otherwise.
 */
bool hostUdevRuleExists(const QString &ruleFilename);

/**
 * @brief Check if a binary is in the current environment PATH.
 * @param binaryName Name of the binary to check for
 * @return true if the binary is in the PATH, false otherwise.
 */
bool isBinaryInPath(const QString &binaryName);

/**
 * @brief Delay execution by approximately the given time, not blocking the main event loop.
 * @param waitMsec Time to wait.
 */
void delay(int waitMsec);

/**
 * @brief Calculate the BLAKE3 hash for the given data.
 */
QByteArray blake3HashForData(const QByteArray &data);

/**
 * @brief Calculate the BLAKE3 hash for the given file.
 */
auto blake3HashForFile(const QString &filename) -> std::expected<QByteArray, QString>;

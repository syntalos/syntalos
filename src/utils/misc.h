/*
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QStringList>

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
QStringList qStringSplitLimit(const QString &str,
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
 * @brief Find file on the host system (if running in a sandbox)
 * @param path Path of the file to check for
 * @return The absolute path to the requested file, or an empty string if not found.
 */
QString findHostFile(const QString &path);

#endif // UTILS_H

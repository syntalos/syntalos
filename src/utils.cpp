/*
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include "utils.h"

#include <QDebug>
#include <QCollator>
#include <QRandomGenerator>

QString createRandomString(int len)
{
    const auto possibleChars = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");

    QString str;
    for (int i=0; i < len; i++) {
        int index = QRandomGenerator::global()->generate() % possibleChars.length();
        QChar nextChar = possibleChars.at(index);
        str.append(nextChar);
    }

    return str;
}

QString simplifyStrForModuleName(const QString &s)
{
    return s.simplified()
            .replace("/", "-")
            .replace("\\", "-");
}

QString simplifyStrForFileBasename(const QString &s)
{
    return simplifyStrForModuleName(s)
            .replace(" ", "")
            .replace(":", "_");
}

QString simplifyStrForFileBasenameLower(const QString &s)
{
    return simplifyStrForFileBasename(s).toLower();
}

QStringList qStringSplitLimit(const QString &str, const QChar &sep, int maxSplit, Qt::CaseSensitivity cs)
{
    QStringList list;
    int start = 0;
    int end;
    while ((end = str.indexOf(sep, start, cs)) != -1) {
        if (start != end)
            list.append(str.mid(start, end - start));
        start = end + 1;
        if (maxSplit > 0) {
            if (list.length() > maxSplit)
                break;
        }
    }
    if (start != str.size())
        list.append(str.mid(start));
    return list;
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

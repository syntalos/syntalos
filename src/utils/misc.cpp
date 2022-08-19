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

#include "config.h"
#include "misc.h"

#include <QDebug>
#include <QCollator>
#include <QRandomGenerator>
#include <QCoreApplication>

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
    const auto tmp = s.simplified()
                      .replace("/", "_")
                      .replace("\\", "_");
    if (tmp.isEmpty())
        return QStringLiteral("Unnamed");
    return tmp;
}

QString simplifyStrForFileBasename(const QString &s)
{
    return simplifyStrForModuleName(s)
            .replace(" ", "")
            .replace(":", "_")
            .replace("_-", "-")
            .replace("-_", "-");
}

QString simplifyStrForFileBasenameLower(const QString &s)
{
    return simplifyStrForModuleName(s)
            .replace(" ", "-") // use dash to make resulting name easier to read (possible camelcasing won't work in the resulting all-lowercase string)
            .replace(":", "_")
            .replace("_-", "-")
            .replace("-_", "-").toLower();
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

/**
 * Retrieve the full Syntalos version, including any VCS information.
 */
QString syntalosVersionFull()
{
    auto syVersion = QCoreApplication::applicationVersion();
    auto syVcs = QStringLiteral(SY_VCS_TAG).replace(syVersion, "");
    if (syVcs.contains("-"))
        syVcs = syVcs.section('-', 1);
    if (syVcs.startsWith("v"))
        syVcs.remove(0, 1);
    if (syVcs == QStringLiteral("+")) {
        syVersion = syVersion + QStringLiteral("+");
        syVcs = "";
    }

    return syVcs.isEmpty()? syVersion : QStringLiteral("%1 (%2)").arg(syVersion, syVcs);
}

bool isInFlatpakSandbox()
{
    return qEnvironmentVariable("container") == QStringLiteral("flatpak");
}

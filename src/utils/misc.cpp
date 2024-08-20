/*
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "misc.h"
#include "config.h"

#include <filesystem>
#include <linux/magic.h>
#include <sys/vfs.h>

#include <QCollator>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QThread>
#include <QTime>
#include <QProcessEnvironment>

namespace fs = std::filesystem;

QString createRandomString(int len)
{
    const auto possibleChars = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");

    QString str;
    for (int i = 0; i < len; i++) {
        int index = QRandomGenerator::global()->generate() % possibleChars.length();
        QChar nextChar = possibleChars.at(index);
        str.append(nextChar);
    }

    return str;
}

QString simplifyStrForModuleName(const QString &s)
{
    const auto tmp = s.simplified().replace("/", "_").replace("\\", "_");
    if (tmp.isEmpty())
        return QStringLiteral("Unnamed");
    return tmp;
}

QString simplifyStrForFileBasename(const QString &s)
{
    return simplifyStrForModuleName(s).replace(" ", "").replace(":", "_").replace("_-", "-").replace("-_", "-");
}

QString simplifyStrForFileBasenameLower(const QString &s)
{
    return simplifyStrForModuleName(s)
        .replace(" ", "-") // use dash to make resulting name easier to read (possible camelcasing won't work in the
                           // resulting all-lowercase string)
        .replace(":", "_")
        .replace("_-", "-")
        .replace("-_", "-")
        .toLower();
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

    return syVcs.isEmpty() ? syVersion : QStringLiteral("%1 (%2)").arg(syVersion, syVcs);
}

bool isInFlatpakSandbox()
{
    if (qEnvironmentVariable("container") == QStringLiteral("flatpak"))
        return true;

    // We check for FLATPAK_ID as well to make this function work for older versions
    // of Flatpak. 1.14.4 or higher is confirmed to not need this check.
    if (qEnvironmentVariable("FLATPAK_ID").startsWith("org.syntalos"))
        return true;
    return false;
}

QString findHostFile(const QString &path)
{
    if (isInFlatpakSandbox()) {
        const auto hostPath = fs::path(QStringLiteral("/run/host/%1").arg(path).toStdString()).lexically_normal();
        if (fs::exists(hostPath))
            return QString::fromStdString(hostPath.string());
    } else {
        if (fs::exists(fs::path(path.toStdString())))
            return path;
    }
    return QString();
}

bool hostUdevRuleExists(const QString &ruleFilename)
{
    QStringList udevPaths = {"/lib/udev/rules.d", "/usr/lib/udev/rules.d", "/etc/udev/rules.d"};
    for (const auto &root : udevPaths) {
        if (!findHostFile(root + "/" + ruleFilename).isEmpty())
            return true;
    }
    return false;
}

QString tempDirRoot()
{
    return QDir::tempPath();
}

static bool isFileOnTmpfs(const QString &fname)
{
    struct statfs info;
    statfs(qPrintable(fname), &info);

    if (info.f_type == TMPFS_MAGIC)
        return true;
    return false;
}

QString tempDirLargeRoot()
{
    QString tmpDir = QStringLiteral("/var/tmp");
    if (fs::exists(tmpDir.toStdString()) && !isFileOnTmpfs(tmpDir))
        return tmpDir;

    tmpDir = tempDirRoot();
    if (!isFileOnTmpfs(tmpDir))
        return tmpDir;

    return QStandardPaths::writableLocation(QStandardPaths::TempLocation);
}

void delay(int waitMsec)
{
    if (waitMsec <= 54) {
        // if it's just a short wait, we don't bother with the event loop
        QThread::usleep(waitMsec * 1000);
        return;
    }

    QTime doneTime = QTime::currentTime().addMSecs(waitMsec);
    while (QTime::currentTime() < doneTime) {
        QThread::usleep(500);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
}

bool isBinaryInPath(const QString &binaryName)
{
    QString path = QProcessEnvironment::systemEnvironment().value("PATH");
    QStringList directories = path.split(QDir::listSeparator());

    // check for the binary in all directories in PATH
    for (const QString &dir : directories) {
        QFileInfo fileInfo(QDir(dir), binaryName);
        if (fileInfo.exists() && fileInfo.isExecutable()) {
            return true;
        }
    }

    return false;
}

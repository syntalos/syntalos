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

#include "executils.h"

#include <QDebug>
#include <QFile>
#include <QApplication>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QEventLoop>
#include <glib.h>

#include "simpleterminal.h"
#include "sysinfo.h"
#include "utils/misc.h"

namespace Syntalos
{
QString shellQuote(const QString &str)
{
    auto cs = g_shell_quote(qPrintable(str));
    const auto res = QString::fromUtf8(cs);
    g_free(cs);
    return res;
}

/**
 * @brief Find executable on the host system (if running in a sandbox)
 * @param exe Name of the executable to look for
 * @return Executable name, or empty string if not found
 */
QString findHostExecutable(const QString &exe)
{
    auto sysInfo = SysInfo::get();
    if (sysInfo->inFlatpakSandbox()) {
        QStringList exeLocations = QStringList() << "/usr/bin"
                                                 << "/usr/local/bin"
                                                 << "/usr/sbin"
                                                 << "";
        for (const auto &loc : exeLocations) {
            QString exeHost = QStringLiteral("/run/host%1/%2").arg(loc, exe);
            QFileInfo fi(exeHost);
            if (fi.isExecutable())
                return loc + "/" + exe;
        }
        return QString();
    }

    // no sandbox
    return QStandardPaths::findExecutable(exe);
}

/**
 * @brief Run command on the host
 * @param exe The program to run
 * @param args Program arguments
 * @param waitForFinished Wait for the command to finish
 * @return Exit status of the program (if waiting for finished)
 */
int runHostExecutable(const QString &exe, const QStringList &args, bool waitForFinished)
{
    auto sysInfo = SysInfo::get();
    QProcess proc;
    proc.setProcessChannelMode(QProcess::ForwardedChannels);
    if (sysInfo->inFlatpakSandbox()) {
        // in sandbox, go via flatpak-spawn
        QStringList fpsArgs = QStringList() << "--host";
        fpsArgs << exe;
        if (waitForFinished)
            proc.start("flatpak-spawn", fpsArgs + args);
        else
            proc.startDetached("flatpak-spawn", fpsArgs + args);
    } else {
        // no sandbox, we can run the command directly
        if (waitForFinished)
            proc.start(exe, args);
        else
            proc.startDetached(exe, args);
    }
    if (waitForFinished) {
        proc.waitForFinished(-1);
        return proc.exitCode();
    } else {
        return 0;
    }
}

int runInTerminal(const QString &cmd, const QStringList &args, const QString &wdir, const QString &title)
{
    auto termWin = std::make_unique<SimpleTerminal>();
    if (!title.isEmpty())
        termWin->setWindowTitle(title);

    if (!wdir.isEmpty())
        termWin->setWorkingDirectory(wdir);

    // create our helper script
    auto rtdDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (rtdDir.isEmpty())
        rtdDir = "/tmp";
    auto exitFname = QStringLiteral("%1/sy-termexit-%2").arg(rtdDir, createRandomString(6));
    auto shHelperFname = QStringLiteral("%1/sy-termrun-%2").arg(rtdDir, createRandomString(6));

    QFile shFile(shHelperFname);
    if (!shFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning().noquote() << "Unable to open temporary file" << shHelperFname << "for writing.";
        return -255;
    }

    QString cmdShell = shellQuote(cmd);
    for (const auto &a : args)
        cmdShell += QStringLiteral(" %1").arg(shellQuote(a));
    QTextStream out(&shFile);
    out << "#!/bin/sh\n" << cmdShell << "\n" << QStringLiteral("echo $? > %1\n").arg(exitFname);
    shFile.flush();
    shFile.setPermissions(QFileDevice::ExeUser | QFileDevice::ReadUser | QFileDevice::WriteUser);
    shFile.close();

    // show the terminal window
    termWin->show();
    termWin->raise();
    termWin->activateWindow();

    // run our helper script as shell
    termWin->setShellProgram(shHelperFname);
    termWin->startShell();

    // wait for the terminal to close
    QEventLoop loop;
    bool finished = false;

    QObject::connect(termWin.get(), &SimpleTerminal::finished, [&finished, &loop]() {
        finished = true;
        loop.quit();
    });
    QObject::connect(termWin.get(), &SimpleTerminal::windowClosed, [&loop]() {
        loop.quit();
    });

    // run the event loop until the command is done or the window is closed
    loop.exec();
    QApplication::processEvents();

    if (finished) {
        // read the exit code from the temp file
        QFile file(exitFname);
        int exitCode = 255;
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            exitCode = QString::fromUtf8(file.readAll()).trimmed().toInt();
            file.close();
        }
        file.remove();

        return exitCode;
    } else {
        // window was closed before command finished
        QFile::remove(exitFname);
        return std::numeric_limits<int>::max();
    }
}

} // namespace Syntalos

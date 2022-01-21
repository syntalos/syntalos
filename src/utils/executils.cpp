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

#include "executils.h"

#include <QDebug>
#include <QProcess>
#include <QStandardPaths>
#include <QFile>
#include <glib.h>

#include "utils/misc.h"


QString shellQuote(const QString &str)
{
    auto cs = g_shell_quote(qPrintable(str));
    const auto res = QString::fromUtf8(cs);
    g_free(cs);
    return res;
}

int runInExternalTerminal(const QString &cmd, const QStringList &args, const QString &wdir)
{
    QString terminalExe;
    QStringList extraTermArgs;
    if (terminalExe.isEmpty()) {
        terminalExe = QStandardPaths::findExecutable("konsole");
        extraTermArgs.append("--hide-menubar");
    }
    if (terminalExe.isEmpty()) {
        terminalExe = QStandardPaths::findExecutable("gnome-terminal");
        extraTermArgs.append("--hide-menubar");
    }
    if (terminalExe.isEmpty())
        terminalExe = QStandardPaths::findExecutable("xterm");
    if (terminalExe.isEmpty())
        terminalExe = QStandardPaths::findExecutable("x-terminal-emulator");

    if (terminalExe.isEmpty())
        return -255;

    auto rtdDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (rtdDir.isEmpty())
        rtdDir = "/tmp";
    auto exitFname = QStringLiteral("%1/sy-termexit-%2").arg(rtdDir, createRandomString(6));

    QString cmdShell = shellQuote(cmd);
    for (const auto &a : args)
        cmdShell += QStringLiteral(" %1").arg(shellQuote(a));
    cmdShell += QStringLiteral( "; echo $? > %1").arg(exitFname);

    const auto termArgs = QStringList() << "-e" << "sh" << "-c" << cmdShell;
    QProcess proc;
    if (!wdir.isEmpty())
        proc.setWorkingDirectory(wdir);
    proc.start(terminalExe, extraTermArgs + termArgs);
    proc.waitForFinished(-1);

    // the terminal itself failed, so the command can't have worked
    if (proc.exitStatus() != 0) {
        QFile::remove(exitFname);
        return proc.exitStatus();
    }

    QFile file(exitFname);
    if (!file.open(QIODevice::ReadWrite | QIODevice::Text))
        return -255;

    int ret = QString::fromUtf8(file.readAll()).toInt();
    file.remove();
    return ret;
}

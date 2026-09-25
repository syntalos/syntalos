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

#include <QObject>

namespace Syntalos
{

/**
 * @brief Shell-escape a string
 */
QString shellQuote(const QString &str);

QString findHostExecutable(const QString &exe);

/**
 * @brief Run command on the host
 * @param exe The program to run
 * @param args Program arguments
 * @param waitForFinished Wait for the command to finish
 * @return Exit status of the program (if waiting for finished)
 */
int runHostExecutable(const QString &exe, const QStringList &args, bool waitForFinished);

/**
 * @brief Find one of Syntalos' own helper executables.
 *
 * When Syntalos runs from its build tree, the tool is looked up relative to the
 * build directory (e.g. "tools/crashreport/syntalos-crashreport"), otherwise at
 * its installed location.
 *
 * @return The absolute path of the executable, or an empty string if it is not available.
 */
QString findToolExecutable(const QString &buildTreeRelPath, const QString &installedPath);

int runInTerminal(
    const QString &cmd,
    const QStringList &args = QStringList(),
    const QString &wdir = QString(),
    const QString &title = QString());

bool launchProgramPidFd(const QString &exePath, int *pidfd_out);
bool isProcessRunning(int pidfd);

} // namespace Syntalos

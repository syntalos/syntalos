/*
 * Copyright (C) 2022 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "utils.h"

#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <vector>

#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Get the initial PID that matches a name.
 *
 * Modified from https://proswdev.blogspot.com/2012/02/get-process-id-by-name-in-linux-using-c.html
 */
int findFirstProcIdByName(std::string procName)
{
    int pid = -1;

    // open the /proc directory
    DIR *dp = opendir("/proc");
    if (dp != NULL) {
        // enumerate all entries in directory until process found
        struct dirent *dirp;
        while (pid < 0 && (dirp = readdir(dp))) {
            // skip non-numeric entries
            int id = atoi(dirp->d_name);
            if (id > 0) {
                // read contents of virtual /proc/{pid}/cmdline file
                std::string cmdPath = std::string("/proc/") + dirp->d_name + "/cmdline";
                std::ifstream cmdFile(cmdPath.c_str());
                std::string cmdLine;
                std::getline(cmdFile, cmdLine);
                if (!cmdLine.empty()) {
                    // keep first cmdline item which contains the program path
                    size_t pos = cmdLine.find('\0');
                    if (pos != std::string::npos)
                        cmdLine = cmdLine.substr(0, pos);
                    // keep program name only, removing the path
                    pos = cmdLine.rfind('/');
                    if (pos != std::string::npos)
                        cmdLine = cmdLine.substr(pos + 1);
                    // compare against requested process name
                    if (procName == cmdLine)
                        pid = id;
                }
            }
        }
    }

    closedir(dp);

    return pid;
}

PtraceScopeManager::PtraceScopeManager()
{
    m_prevScope = readPtraceScope();
    m_pkexecExe = QStandardPaths::findExecutable("pkexec");
}

void PtraceScopeManager::ensureAllowed()
{
    if (m_prevScope.isEmpty()) {
        qWarning().noquote() << "Unable to determine the state of yama/ptrace_scope!";
        return;
    }
    if (m_prevScope == "0")
        return;
    changePtraceScope(false);
}

void PtraceScopeManager::reset()
{
    if (m_prevScope.isEmpty())
        return;
    if (readPtraceScope() == m_prevScope)
        return;
    changePtraceScope(m_prevScope != "0");
}

QString PtraceScopeManager::readPtraceScope()
{
    QFile file(QLatin1String("/proc/sys/kernel/yama/ptrace_scope"));
    if (file.open(QIODevice::ReadOnly | QFile::Text)) {
        QTextStream stream(&file);
        return stream.readAll().trimmed();
    }
    return QString();
}

void PtraceScopeManager::changePtraceScope(bool state)
{
    if (m_pkexecExe.isEmpty()) {
        qWarning().noquote() << "Unable to change yama/ptrace_scope - pkexec is missing.";
        return;
    }

    QProcess pkProc;
    pkProc.setProgram(m_pkexecExe);
    pkProc.setArguments(
        QStringList() << "/bin/sh"
                      << "-c" << QStringLiteral("echo %1 > /proc/sys/kernel/yama/ptrace_scope").arg(state ? 1 : 0));
    pkProc.start();
    bool ret = pkProc.waitForFinished(120 * 1000);
    if (!ret)
        qWarning().noquote() << "Unable to change yama/ptrace_scope - pkexec timed out.";
}

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

#include <QFile>
#include <QApplication>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QEventLoop>
#include <glib.h>
#include <sched.h>
#include <linux/sched.h>
#include <sys/syscall.h>
#include <sys/wait.h>

#include "fabric/logging.h"
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
        LOG_WARNING(logRoot, "Unable to open temporary file {} for writing.", shHelperFname);
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

/**
 * Fallback for launchProgram() if the clone3() syscall was not available or is blocked
 * by seccomp filters.
 */
static bool launchProgramNC3Fallback(const QString &exePath, int *pidfd_out)
{
    // this may allocate, so we do it before vfork to avoid memory allocation in the child process
    const auto exePathBytes = exePath.toLocal8Bit();

    pid_t pid = vfork();
    if (pid < 0) {
        perror("vfork");
        return false;
    }

    if (pid == 0) {
        // child process
        char *const argv[] = {const_cast<char *>(exePathBytes.data()), nullptr};

        execv(argv[0], argv);
        perror("execvp");
        _exit(EXIT_FAILURE);
    }

    // parent process
    int pidfd = syscall(SYS_pidfd_open, pid, 0);
    if (pidfd < 0) {
        perror("pidfd_open");
        return false;
    }

    if (pidfd_out)
        *pidfd_out = pidfd;

    return true;
}

/**
 * @brief Launch a program in a new process, and return the PID of the new process as pidfd.
 *
 * This function is used to launch a new program in a new process, and return the PID as pidfd.
 *
 * @param exePath The path to the executable to launch.
 * @param pidfd_out A pointer to an integer, which will be set to the PID of the new process.
 * @return true if the program was successfully launched, false otherwise.
 */
bool launchProgramPidFd(const QString &exePath, int *pidfd_out)
{
    struct clone_args cl_args = {0};
    int pidfd;
    pid_t parent_tid = -1;

    cl_args.parent_tid = __u64((uintptr_t)&parent_tid);
    cl_args.pidfd = __u64((uintptr_t)&pidfd);
    cl_args.flags = CLONE_PIDFD | CLONE_PARENT_SETTID;
    cl_args.exit_signal = SIGCHLD;

    char *const argv[] = {const_cast<char *>(qPrintable(exePath)), nullptr};

    const auto pid = (pid_t)syscall(SYS_clone3, &cl_args, sizeof(cl_args));
    if (pid < 0) {
        // clone3 was blocked or is not available, try our fallback
        if (errno == ENOSYS)
            return launchProgramNC3Fallback(exePath, pidfd_out);
        return false;
    }

    if (pid == 0) { // Child process
        execvp(qPrintable(exePath), argv);
        perror("execvp"); // execvp only returns on error
        exit(EXIT_FAILURE);
    }

    if (pidfd_out)
        *pidfd_out = pidfd;

    return true;
}

/**
 * @brief Check if a process is still running using a pidfd.
 *
 * This function checks if a process is still running, by checking if the process
 * has exited or not.
 *
 * @param pidfd The PIDFD of the process to check.
 * @return true if the process is still running, false otherwise.
 */
bool isProcessRunning(int pidfd)
{
    siginfo_t si;
    int result;

    si.si_pid = 0;

    result = waitid(P_PIDFD, pidfd, &si, WEXITED | WNOHANG);
    if (result == -1) {
        return false; // Assuming failure means the process is not running
    }

    return si.si_pid == 0;
}

} // namespace Syntalos

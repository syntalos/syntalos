/*
 * Copyright (C) 2020-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "pyvenvmanager.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

#include "executils.h"
#include "globalconfig.h"
#include "utils/misc.h"

namespace Syntalos
{

Q_LOGGING_CATEGORY(logVEnv, "pyvenv")

QString pythonVEnvDirForName(const QString &venvName)
{
    GlobalConfig gconf;
    return QStringLiteral("%1/%2").arg(gconf.virtualEnvDir(), venvName);
}

bool pythonVirtualEnvExists(const QString &venvName)
{
    return QFile::exists(QStringLiteral("%1/bin/python").arg(pythonVEnvDirForName(venvName)));
}

static void injectSystemPyModule(const QString &venvDir, const QString &pyModName)
{
    QDir dir(QStringLiteral("%1/lib/").arg(venvDir));
    QStringList vpDirs = dir.entryList(QStringList() << QStringLiteral("python*"));

    QStringList systemPyModPaths = QStringList()
                                   << QStringLiteral("/usr/lib/python3/dist-packages/%1/").arg(pyModName)
                                   << QStringLiteral("/usr/local/lib/python3/dist-packages/%1/").arg(pyModName)
                                   << QStringLiteral("/app/lib/python/site-packages/%1/").arg(pyModName);

    for (const auto &systemPyQtPath : systemPyModPaths) {
        QDir sysPyQtDir(systemPyQtPath);
        if (!sysPyQtDir.exists())
            continue;

        for (const auto &pyDir : vpDirs) {
            qDebug().noquote() << "Adding system Python module to venv:" << systemPyQtPath;
            QFile::link(systemPyQtPath, QStringLiteral("%1/lib/%2/site-packages/%3").arg(venvDir, pyDir, pyModName));
        }
    }
}

static bool injectSystemPyQtBindings(const QString &venvDir)
{
    // the PyQt6/PySide modules must be for the same version as Syntalos was compiled for,
    // as the pyworker binary is linked against Qt as well (and trying to load a different
    // version will fail)
    // therefore, we add this hack and inject just the system Qt Python bindings into the
    // virtual environment.
    injectSystemPyModule(venvDir, QStringLiteral("PyQt6"));
    return true;
}

bool createPythonVirtualEnv(const QString &venvName, const QString &requirementsFile)
{
    auto rtdDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (rtdDir.isEmpty())
        rtdDir = "/tmp";
    const auto venvDir = pythonVEnvDirForName(venvName);
    QDir().mkpath(venvDir);

    const auto tmpCommandFile = QStringLiteral("%1/sy-venv-%2.sh").arg(rtdDir, createRandomString(6));
    QFile shFile(tmpCommandFile);
    if (!shFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(logVEnv).noquote() << "Unable to open temporary file" << tmpCommandFile << "for writing.";
        return false;
    }

    const auto tmpRequirementsFname = QStringLiteral("%1/%2-requirements_%3.txt")
                                          .arg(rtdDir, venvName, createRandomString(4));
    QFile tmpReqFile(tmpRequirementsFname);
    if (!tmpReqFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(logVEnv).noquote() << "Unable to open temporary file" << tmpRequirementsFname << "for writing.";
        return false;
    }

    if (requirementsFile.isEmpty()) {
        // we have no requirements file and want to create a blank venv, so we just create an empty dummy file
        QTextStream rqfOut(&tmpReqFile);
        rqfOut << "\n";
        tmpReqFile.close();
        qCDebug(logVEnv) << "Creating empty virtualenv" << venvName;
    } else {
        QFile reqFile(requirementsFile);
        if (!reqFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCWarning(logVEnv).noquote() << "Unable to open file" << requirementsFile << "for reading.";
            return false;
        }

        QTextStream rqfIn(&reqFile);
        QTextStream rqfOut(&tmpReqFile);
        while (!rqfIn.atEnd()) {
            QString line = rqfIn.readLine();
            if (!line.startsWith("PyQt6"))
                rqfOut << line << "\n";
        }
        tmpReqFile.close();

        qCDebug(logVEnv).noquote().nospace()
            << "Creating virtualenv \"" << venvName << "\" using requirements file: " << requirementsFile;
    }

    qCDebug(logVEnv).noquote() << "Creating new Python virtualenv in:" << venvDir;
    QTextStream out(&shFile);
    out << "#!/bin/bash\n\n"
        << "run_check() {\n"
        << "    echo -e \"\\033[1;33m-\\033[0m \\033[1m$@\\033[0m\"\n"
        << "    $@\n"
        << "    if [ $? -ne 0 ]\n"
        << "    then\n"
        << "        echo \"\"\n"
        << "        read -p \"Command failed to run. Press enter to exit.\"\n"
        << "        exit 1\n"
        << "    fi\n"
        << "}\n"
        << "export PATH=$PATH:/app/bin\n\n"

        << "cd " << shellQuote(venvDir) << "\n"
        << "run_check virtualenv ."
        << "\n"
        << "run_check source " << shellQuote(QStringLiteral("%1/bin/activate").arg(venvDir)) << "\n"
        << "run_check pip install -r " << shellQuote(tmpRequirementsFname) << "\n"

        << "echo \"\"\n"
        << "read -p \"Success! Press any key to exit.\""
        << "\n";
    shFile.flush();
    shFile.setPermissions(QFileDevice::ExeUser | QFileDevice::ReadUser | QFileDevice::WriteUser);
    shFile.close();

    int ret = runInTerminal(
        tmpCommandFile, QStringList(), venvDir, QStringLiteral("Creating virtual Python environment"));

    shFile.remove();
    tmpReqFile.remove();
    if (ret == 0)
        return injectSystemPyQtBindings(venvDir);

    // failure, let's try to remove the bad virtualenv (failures to do so are ignored)
    QDir(venvDir).removeRecursively();
    return false;
}

} // namespace Syntalos

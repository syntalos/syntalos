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

#include "config.h"
#include "pyvenvmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>

#include "logging.h"
#include "executils.h"
#include "globalconfig.h"
#include "utils/misc.h"

namespace Syntalos
{

#define logVEnv getLogger("pyvenv")

QString pythonVEnvDirForName(const QString &venvName)
{
    GlobalConfig gconf;
    return QStringLiteral("%1/%2").arg(gconf.virtualEnvDir(), venvName);
}

static bool pythonVirtualEnvExists(const QString &venvName)
{
    return QFile::exists(QStringLiteral("%1/bin/python").arg(pythonVEnvDirForName(venvName)));
}

static QString requirementsHashMetadataPath(const QString &venvDir)
{
    return QStringLiteral("%1/.requirements.b3sum").arg(venvDir);
}

static bool writeRequirementsHashMetadata(const QString &venvDir, const QByteArray &requirementsHash)
{
    const auto metadataPath = requirementsHashMetadataPath(venvDir);
    if (requirementsHash.isEmpty()) {
        QFile::remove(metadataPath);
        return true;
    }

    QFile metadataFile(metadataPath);
    if (!metadataFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_WARNING(logVEnv, "Unable to write venv requirements metadata: {}", metadataPath);
        return false;
    }

    QTextStream out(&metadataFile);
    out << QString::fromLatin1(requirementsHash.toHex()) << "\n";
    return true;
}

static bool readRequirementsHashMetadata(const QString &venvDir, QString &requirementsHashOut)
{
    requirementsHashOut = QString();

    QFile metadataFile(requirementsHashMetadataPath(venvDir));
    if (!metadataFile.exists())
        return false;

    if (!metadataFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_WARNING(logVEnv, "Unable to read venv requirements metadata for: {}", venvDir);
        return false;
    }

    const auto rawHash = QString::fromUtf8(metadataFile.readAll()).trimmed();
    if (rawHash.isEmpty())
        return false;

    requirementsHashOut = rawHash;
    return true;
}

static QString interpreterPathFromCfg(const QString &venvDir)
{
    QFile cfgFile(QStringLiteral("%1/pyvenv.cfg").arg(venvDir));
    if (!cfgFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    QString executablePath;
    QString home;

    QTextStream in(&cfgFile);
    while (!in.atEnd()) {
        const auto line = in.readLine();
        const auto eqPos = line.indexOf('=');
        if (eqPos <= 0)
            continue;

        const auto key = line.left(eqPos).trimmed();
        const auto value = line.mid(eqPos + 1).trimmed();
        if (key == QStringLiteral("executable"))
            executablePath = value;
        else if (key == QStringLiteral("home"))
            home = value;
    }

    if (!executablePath.isEmpty())
        return executablePath;

    if (!home.isEmpty()) {
        const auto py3Path = QStringLiteral("%1/python3").arg(home);
        if (QFileInfo(py3Path).isAbsolute())
            return py3Path;

        const auto pyPath = QStringLiteral("%1/python").arg(home);
        if (QFileInfo(pyPath).isAbsolute())
            return pyPath;
    }

    return QString();
}

PyVirtualEnvStatus pythonVirtualEnvStatus(const QString &venvName, const QString &requirementsFile)
{
    if (!pythonVirtualEnvExists(venvName))
        return PyVirtualEnvStatus::MISSING;

    const auto venvDir = pythonVEnvDirForName(venvName);
    const auto interpreterPath = interpreterPathFromCfg(venvDir);
    if (!interpreterPath.isEmpty() && QFileInfo(interpreterPath).isAbsolute() && !QFileInfo::exists(interpreterPath)) {
        LOG_WARNING(
            logVEnv, "Base Python interpreter for virtual environment {} is missing: {}", venvName, interpreterPath);
        return PyVirtualEnvStatus::INTERPRETER_MISSING;
    }

    if (requirementsFile.isEmpty())
        return PyVirtualEnvStatus::VALID;

    const auto hashResult = blake3HashForFile(requirementsFile);
    QString currentHashStr;
    if (hashResult.has_value())
        currentHashStr = QString::fromLatin1(hashResult->toHex());
    else
        LOG_WARNING(
            logVEnv,
            "Unable to compute BLAKE3 hash for requirements file: {} - treating as changed requirements.",
            requirementsFile);

    QString storedReqHash;
    if (!readRequirementsHashMetadata(venvDir, storedReqHash)) {
        LOG_WARNING(
            logVEnv,
            "Unable to read stored requirements hash metadata for virtual environment {} - treating as changed "
            "requirements.",
            venvName);
        return PyVirtualEnvStatus::REQUIREMENTS_CHANGED;
    }

    if (storedReqHash != currentHashStr)
        return PyVirtualEnvStatus::REQUIREMENTS_CHANGED;

    return PyVirtualEnvStatus::VALID;
}

static void injectSyntalosMlinkIntoVenv(const QString &venvDir)
{
    // Locate the syntalos_mlink extension: prefer a dev-build next to the app binary,
    // fall back to the installed system path.
    const QString devDir = QCoreApplication::applicationDirPath() + QStringLiteral("/python");
    const QString installedDir = QStringLiteral(PYTHON_INSTALL_PATH);

    QString soDir;
    for (const auto &dir : {devDir, installedDir}) {
        if (!QDir(dir).entryList({"syntalos_mlink*.so"}, QDir::Files).isEmpty()) {
            soDir = dir;
            break;
        }
    }

    if (soDir.isEmpty()) {
        LOG_WARNING(logVEnv, "Could not find syntalos_mlink extension to inject into venv");
        return;
    }

    const auto soFiles = QDir(soDir).entryList({"syntalos_mlink*.so"}, QDir::Files);
    const QString pyiSrc = QStringLiteral("%1/syntalos_mlink.pyi").arg(soDir);

    QDir libDir(QStringLiteral("%1/lib").arg(venvDir));
    const auto pyDirs = libDir.entryList({"python*"}, QDir::Dirs);
    for (const auto &pyDir : pyDirs) {
        const auto sitePkgs = QStringLiteral("%1/lib/%2/site-packages").arg(venvDir, pyDir);
        if (!QDir(sitePkgs).exists())
            continue;

        for (const auto &soFile : soFiles) {
            const auto dst = QStringLiteral("%1/%2").arg(sitePkgs, soFile);
            if (QFile::exists(dst))
                QFile::remove(dst);
            QFile::link(QStringLiteral("%1/%2").arg(soDir, soFile), dst);
        }

        if (QFile::exists(pyiSrc)) {
            const auto dst = QStringLiteral("%1/syntalos_mlink.pyi").arg(sitePkgs);
            if (QFile::exists(dst))
                QFile::remove(dst);
            QFile::link(pyiSrc, dst);
        }
    }
}

auto createPythonVirtualEnv(const QString &venvName, const QString &requirementsFile, bool recreate)
    -> std::expected<QString, QString>
{
    auto rtdDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (rtdDir.isEmpty())
        rtdDir = "/tmp";
    const auto venvDir = pythonVEnvDirForName(venvName);

    if (QDir(venvDir).exists()) {
        if (recreate) {
            LOG_INFO(logVEnv, "Removing existing virtualenv before recreation: {}", venvDir);
            if (!QDir(venvDir).removeRecursively())
                return std::unexpected(QStringLiteral("Unable to remove existing virtualenv: %1").arg(venvDir));
        } else {
            // nothing to do, the venv already exists
            return venvDir;
        }
    }

    QDir().mkpath(venvDir);

    const auto tmpCommandFile = QStringLiteral("%1/sy-venv-%2.sh").arg(rtdDir, createRandomString(6));
    QFile shFile(tmpCommandFile);
    if (!shFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_WARNING(logVEnv, "Unable to open temporary file {} for writing.", tmpCommandFile);
        return std::unexpected(
            QStringLiteral("Unable to open temporary file for virtualenv creation: %1").arg(shFile.errorString()));
    }

    const auto tmpRequirementsFname = QStringLiteral("%1/%2-requirements_%3.txt")
                                          .arg(rtdDir, venvName, createRandomString(4));
    QFile tmpReqFile(tmpRequirementsFname);
    if (!tmpReqFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_WARNING(logVEnv, "Unable to open temporary file {} for writing.", tmpRequirementsFname);
        return std::unexpected(
            QStringLiteral("Unable to open temporary file for virtualenv creation: %1").arg(tmpReqFile.errorString()));
    }

    if (requirementsFile.isEmpty()) {
        // we have no requirements file and want to create a blank venv, so we just create an empty dummy file
        QTextStream rqfOut(&tmpReqFile);
        rqfOut << "\n";
        tmpReqFile.close();
        LOG_INFO(logVEnv, "Creating empty virtualenv {}", venvName);
    } else {
        QFile reqFile(requirementsFile);
        if (!reqFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            LOG_WARNING(logVEnv, "Unable to open file {} for reading.", requirementsFile);
            return std::unexpected(QStringLiteral("Unable to open requirements file \"%1\": %2")
                                       .arg(requirementsFile, reqFile.errorString()));
        }

        QTextStream rqfIn(&reqFile);
        QTextStream rqfOut(&tmpReqFile);
        while (!rqfIn.atEnd()) {
            QString line = rqfIn.readLine();
            // We can filter requirements here, but currently don't need to do so
            rqfOut << line << "\n";
        }
        tmpReqFile.close();

        LOG_INFO(logVEnv, "Creating virtualenv \"{}\" using requirements file: {}", venvName, requirementsFile);
    }

    LOG_INFO(logVEnv, "Creating new Python virtualenv in: {}", venvDir);
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
    if (ret == 0) {
        injectSyntalosMlinkIntoVenv(venvDir);

        if (!requirementsFile.isEmpty()) {
            const auto b3sumResult = blake3HashForFile(requirementsFile);

            if (!b3sumResult.has_value())
                return std::unexpected(
                    QStringLiteral("Unable to compute requirements checksum: %1").arg(b3sumResult.error()));

            if (!writeRequirementsHashMetadata(venvDir, b3sumResult.value()))
                LOG_WARNING(logVEnv, "Unable to persist requirements metadata for virtualenv: {}", venvName);
        }

        return venvDir;
    }

    // failure, let's try to remove the bad virtualenv (failures to do so are ignored)
    QDir(venvDir).removeRecursively();

    return std::unexpected(
        QStringLiteral("Environment creation failed - refer to the terminal log for more information."));
}

} // namespace Syntalos

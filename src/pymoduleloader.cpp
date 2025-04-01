/*
 * Copyright (C) 2020-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "pymoduleloader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMessageBox>
#include <QStandardPaths>
#include <iostream>

#include "globalconfig.h"
#include "mlinkmodule.h"
#include "fabric/executils.h"
#include "utils/misc.h"

class PythonModule : public MLinkModule
{
    Q_OBJECT
public:
    explicit PythonModule(QObject *parent = nullptr)
        : MLinkModule(parent),
          m_features(ModuleFeature::NONE)
    {
        // we use the generic Python OOP worker process for this
        setModuleBinary(findSyntalosPyWorkerBinary());

        setOutputCaptured(false); // don't capture stdout until we are running
        connect(this, &MLinkModule::processOutputReceived, this, [&](const QString &data) {
            std::cout << "py." << id().toStdString() << "(" << name().toStdString() << "): " << data.toStdString()
                      << std::endl;
        });
    }

    ~PythonModule() override {}

    ModuleFeatures features() const override
    {
        return m_features;
    }

    void setFeatures(ModuleFeatures features)
    {
        m_features = features;
    }

    void setupPorts(const QVariantList &varInPorts, const QVariantList &varOutPorts)
    {
        for (const auto &pv : varInPorts) {
            const auto po = pv.toHash();
            registerInputPortByTypeId(
                BaseDataType::typeIdFromString(po.value("data_type").toString()),
                po.value("id").toString(),
                po.value("title").toString());
        }

        for (const auto &pv : varOutPorts) {
            const auto po = pv.toHash();
            registerOutputPortByTypeId(
                BaseDataType::typeIdFromString(po.value("data_type").toString()),
                po.value("id").toString(),
                po.value("title").toString());
        }
    }

    QString virtualEnvDir() const
    {
        GlobalConfig gconf;
        return QStringLiteral("%1/%2").arg(gconf.virtualenvDir(), id());
    }

    bool virtualEnvExists()
    {
        return QFile::exists(QStringLiteral("%1/bin/python").arg(virtualEnvDir()));
    }

    void injectSystemPyModule(const QString &venvDir, const QString &pyModName)
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
                QFile::link(
                    systemPyQtPath, QStringLiteral("%1/lib/%2/site-packages/%3").arg(venvDir, pyDir, pyModName));
            }
        }
    }

    bool injectSystemPyQtBindings(const QString &venvDir)
    {
        // the PyQt6/PySide modules must be for the same version as Syntalos was compiled for,
        // as the pyworker binary is linked against Qt as well (and trying to load a different
        // version will fail)
        // therefore, we add this hack and inject just the system Qt Python bindings into the
        // virtual environment.
        injectSystemPyModule(venvDir, QStringLiteral("PyQt6"));
        return true;
    }

    bool installVirtualEnv()
    {
        GlobalConfig gconf;
        auto rtdDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        if (rtdDir.isEmpty())
            rtdDir = "/tmp";
        const auto venvDir = virtualEnvDir();
        QDir().mkpath(venvDir);

        const auto tmpCommandFile = QStringLiteral("%1/sy-venv-%2.sh").arg(rtdDir, createRandomString(6));
        QFile shFile(tmpCommandFile);
        if (!shFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning().noquote() << "Unable to open temporary file" << tmpCommandFile << "for writing.";
            return false;
        }

        const auto origRequirementsFname = QStringLiteral("%1/requirements.txt").arg(m_pyModDir);
        const auto tmpRequirementsFname = QStringLiteral("%1/%2-requirements_%3.txt")
                                              .arg(rtdDir, id(), createRandomString(4));
        QFile tmpReqFile(tmpRequirementsFname);
        if (!tmpReqFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning().noquote() << "Unable to open temporary file" << tmpRequirementsFname << "for writing.";
            return false;
        }

        QFile reqFile(origRequirementsFname);
        if (!reqFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning().noquote() << "Unable to open file" << origRequirementsFname << "for reading.";
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

        qDebug().noquote() << "Creating new Python virtualenv in:" << venvDir;
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

        int ret = runInExternalTerminal(tmpCommandFile, QStringList(), venvDir);

        shFile.remove();
        tmpReqFile.remove();
        if (ret == 0)
            return injectSystemPyQtBindings(venvDir);
        // failure, let's try to remove the bad virtualenv (failures to do so are ignored)
        QDir(venvDir).removeRecursively();
        return false;
    }

    bool ensurePythonCodeRunning()
    {
        if (isProcessRunning())
            return true;

        setPythonVirtualEnv(virtualEnvDir());
        setOutputCaptured(true);
        if (!runProcess())
            return false;

        // run script immediately
        if (!setScriptFromFile(m_mainPyFname, m_pyModDir)) {
            raiseError(QStringLiteral("Unable to open Python script file: %1").arg(m_mainPyFname));
            return false;
        }
        if (!sendPortInformation())
            return false;
        if (!loadCurrentScript())
            return false;

        return true;
    }

    bool initialize() override
    {
        if (moduleBinary().isEmpty()) {
            raiseError("Unable to find Python worker binary. Is Syntalos installed correctly?");
            return false;
        }

        if (m_useVEnv && !virtualEnvExists() && QFile::exists(QStringLiteral("%1/requirements.txt").arg(m_pyModDir))) {
            auto reply = QMessageBox::question(
                nullptr,
                QStringLiteral("Create virtual environment for %1?").arg(id()),
                QStringLiteral(
                    "The '%1' module requested to run its Python code in a virtual environment, however "
                    "a virtual Python environment for modules of type '%2' does not exist yet. "
                    "Should Syntalos attempt to set up the environment automatically? "
                    "(This will open a system terminal and run the necessary commands, which may take some time)")
                    .arg(name(), id()),
                QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::No)
                return false;

            QCoreApplication::processEvents();
            if (!installVirtualEnv()) {
                QMessageBox::warning(
                    nullptr,
                    QStringLiteral("Failed to create virtual environment for %1?").arg(id()),
                    QStringLiteral(
                        "Failed to set up the virtual environment - refer to the terminal log for more information."));
                return false;
            }
        }

        // native Python modules have their main function launched immediately
        if (!ensurePythonCodeRunning())
            return false;

        setInitialized();
        return true;
    }

    bool prepare(const TestSubject &testSubject) override
    {
        setOutputCaptured(true);

        if (!isProcessRunning()) {
            ensurePythonCodeRunning();
        } else {
            // reload Python script if it was changed meanwhile
            if (isScriptModified()) {
                qDebug().noquote().nospace() << "py." << id() << "(" << name() << "): "
                                             << "=> Reloading Python script";
                if (!setScriptFromFile(m_mainPyFname, m_pyModDir)) {
                    raiseError(QStringLiteral("Unable to open Python script file: %1").arg(m_mainPyFname));
                    return false;
                }
                if (!sendPortInformation())
                    return false;
                if (!loadCurrentScript())
                    return false;
            }
        }

        return MLinkModule::prepare(testSubject);
    }

    void showSettingsUi() override
    {
        if (!ensurePythonCodeRunning())
            return;

        MLinkModule::showSettingsUi();
    }

    void showDisplayUi() override
    {
        if (!ensurePythonCodeRunning())
            return;

        MLinkModule::showDisplayUi();
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &extraData) override
    {
        Q_UNUSED(settings)
        extraData = settingsData();
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &extraData) override
    {
        Q_UNUSED(settings)
        setSettingsData(extraData);
        return true;
    }

    void setPythonInfo(const QString &fname, const QString wdir, bool useVEnv)
    {
        m_mainPyFname = fname;
        m_pyModDir = wdir;
        m_useVEnv = useVEnv;
    }

private:
    QString m_mainPyFname;
    QString m_pyModDir;
    bool m_useVEnv;
    ModuleFeatures m_features;
};

class PyModuleInfo : public ModuleInfo
{
public:
    explicit PyModuleInfo(QString id, QString name, QString description, QIcon icon, ModuleCategories categories)
        : m_id(std::move(id)),
          m_name(std::move(name)),
          m_description(std::move(description)),
          m_icon(std::move(icon)),
          m_categories(categories),
          m_useVEnv(false)
    {
    }

    QString id() const override
    {
        return m_id;
    }

    QString name() const override
    {
        return m_name;
    };

    QString description() const override
    {
        return m_description;
    };

    QIcon icon() const override
    {
        return m_icon;
    };

    ModuleCategories categories() const final
    {
        return m_categories;
    }

    AbstractModule *createModule(QObject *parent = nullptr) override
    {
        auto mod = new PythonModule(parent);
        mod->setPythonInfo(m_pyFname, rootDir(), m_useVEnv);
        mod->setupPorts(m_portDefInput, m_portDefOutput);
        mod->setFeatures(m_features);
        return mod;
    }

    void setMainPyScriptFname(const QString &pyFname)
    {
        m_pyFname = pyFname;
    }

    void setFeatures(ModuleFeatures features)
    {
        m_features = features;
    }

    void setUseVEnv(bool enabled)
    {
        m_useVEnv = enabled;
    }

    void setPortDef(const QVariantList &defInput, const QVariantList &defOutput)
    {
        m_portDefInput = defInput;
        m_portDefOutput = defOutput;
    }

private:
    QString m_id;
    QString m_name;
    QString m_description;
    QIcon m_icon;
    ModuleCategories m_categories;
    ModuleFeatures m_features;

    QString m_pyFname;
    bool m_useVEnv;

    QVariantList m_portDefInput;
    QVariantList m_portDefOutput;
};

ModuleInfo *loadPythonModuleInfo(const QString &modId, const QString &modDir, const QVariantHash &modData)
{
    PyModuleInfo *modInfo = nullptr;
    const auto modDef = modData["syntalos_module"].toHash();
    const auto name = modDef.value("name").toString();
    const auto desc = modDef.value("description").toString();
    const auto iconName = modDef.value("icon").toString();
    const auto categories = moduleCategoriesFromString(modDef.value("categories", QString()).toString());
    const auto useVEnv = modDef.value("use_venv", false).toBool();
    const auto featuresList = modDef.value("features", QStringList()).toStringList();

    if (name.isEmpty())
        throw std::runtime_error("Required 'name' key not found in module metadata.");
    if (desc.isEmpty())
        throw std::runtime_error("Required 'description' key not found in module metadata.");

    QIcon icon = QIcon(":/module/generic");
    if (!iconName.isEmpty()) {
        const auto iconFname = QDir(modDir).filePath(iconName);
        if (QFileInfo::exists(iconFname))
            icon = QIcon(iconFname);
        else
            icon = QIcon::fromTheme(iconName, QIcon(":/module/generic"));
    }

    const auto pyFile = QDir(modDir).filePath(modDef.value("main").toString());
    if (!QFileInfo::exists(pyFile))
        throw std::runtime_error(
            qPrintable(QStringLiteral("Main entrypoint Python file %1 does not exist").arg(pyFile)));

    ModuleFeatures features;
    for (const auto &f : featuresList) {
        if (f == QStringLiteral("show-settings"))
            features |= ModuleFeature::SHOW_SETTINGS;
        else if (f == QStringLiteral("show-display"))
            features |= ModuleFeature::SHOW_DISPLAY;
    }

    modInfo = new PyModuleInfo(modId, name, desc, icon, categories);
    modInfo->setRootDir(modDir);
    modInfo->setMainPyScriptFname(pyFile);
    modInfo->setUseVEnv(useVEnv);
    modInfo->setFeatures(features);

    const auto portsDef = modData.value("ports").toHash();
    if (!portsDef.isEmpty())
        modInfo->setPortDef(portsDef.value("in").toList(), portsDef.value("out").toList());

    return modInfo;
}

#include "pymoduleloader.moc"

/*
 * Copyright (C) 2020-2021 Matthias Klumpp <matthias@tenstral.net>
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

#include <iostream>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QStandardPaths>
#include <QMessageBox>
#include <QCoreApplication>

#include "oopmodule.h"
#include "globalconfig.h"
#include "utils/executils.h"
#include "utils/misc.h"

class PythonModule : public OOPModule
{
    Q_OBJECT
public:

    explicit PythonModule(QObject *parent = nullptr)
        : OOPModule(parent),
          m_settingsOpened(false)
    {
        // we use the generic Python OOP worker process for this
        setWorkerBinaryPyWorker();

        setCaptureStdout(false);  // don't capture stdout until we are running
        connect(this, &OOPModule::processStdoutReceived, this, [&](const QString& data) {
            std::cout << "py." << id().toStdString() << "(" << name().toStdString() << "): "
                      << data.toStdString() << std::endl;
        });
    }

    ~PythonModule() override
    {}

    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_SETTINGS;
    }

    void setupPorts(const QVariantList &varInPorts, const QVariantList &varOutPorts)
    {
        for (const auto &pv : varInPorts) {
            const auto po = pv.toHash();
            registerInputPortByTypeId(QMetaType::type(qPrintable(po.value("data_type").toString())),
                                      po.value("id").toString(),
                                      po.value("title").toString());
        }

        for (const auto &pv : varOutPorts) {
            const auto po = pv.toHash();
            registerOutputPortByTypeId(QMetaType::type(qPrintable(po.value("data_type").toString())),
                                       po.value("id").toString(),
                                       po.value("title").toString());
        }
    }

    QString virtualEnvDir() const
    {
        GlobalConfig gconf;
        return QStringLiteral("%1/%2").arg(gconf.virtualenvDir()).arg(id());
    }

    bool virtualEnvExists()
    {
        return QFile::exists(QStringLiteral("%1/bin/python").arg(virtualEnvDir()));
    }

    bool installVirtualEnv()
    {
        GlobalConfig gconf;
        auto rtdDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        if (rtdDir.isEmpty())
            rtdDir = "/tmp";
        const auto venvDir = virtualEnvDir();
        QDir().mkpath(venvDir);

        const auto tmpCommandFile = QStringLiteral("%1/sy-venv-%2.sh").arg(rtdDir).arg(createRandomString(6));
        QFile file(tmpCommandFile);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning().noquote() << "Unable to open temporary file" << tmpCommandFile << "for writing.";
            return false;
        }

        qDebug().noquote() << "Creating new Python virtualenv in:" << venvDir;
        QTextStream out(&file);
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
            << "}\n\n"

            << "cd " << shellQuote(venvDir) << "\n"
            << "run_check virtualenv ." << "\n"
            << "run_check source " << shellQuote(QStringLiteral("%1/bin/activate").arg(venvDir)) << "\n"
            << "run_check pip install --ignore-requires-python -r " << shellQuote(QStringLiteral("%1/requirements.txt").arg(m_pyModDir)) << "\n"

            << "echo \"\"\n"
            << "read -p \"Success! Press any key to exit.\"" << "\n";
        file.flush();
        file.setPermissions(QFileDevice::ExeUser | QFileDevice::ReadUser | QFileDevice::WriteUser);
        file.close();

        int ret = runInExternalTerminal(tmpCommandFile, QStringList(), venvDir);

        //file.remove();
        if (ret == 0)
            return true;
        // failure, let's try to remove the bad virtualenv (failures to do so are ignored)
        QDir(venvDir).removeRecursively();
        return false;
    }

    bool initialize() override
    {
        if (workerBinary().isEmpty()) {
            raiseError("Unable to find Python worker binary. Is Syntalos installed correctly?");
            return false;
        }

        if (m_useVEnv && !virtualEnvExists() && QFile::exists(QStringLiteral("%1/requirements.txt").arg(m_pyModDir))) {
            auto reply = QMessageBox::question(nullptr,
                                               QStringLiteral("Create virtual environment for %1?").arg(id()),
                                               QStringLiteral("The '%1' module requested to run its Python code in a virtual environment, however "
                                                              "a virtual Python environment for modules of type '%2' does not exist yet. "
                                                              "Should Syntalos attempt to set up the environment automatically? "
                                                              "(This will open a system terminal and run the necessary command, which may take some time)")
                                                              .arg(name()).arg(id()),
                                               QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::No)
                return false;

            QCoreApplication::processEvents();
            if (!installVirtualEnv()) {
                QMessageBox::warning(nullptr,
                                     QStringLiteral("Failed to create virtual environment for %1?").arg(id()),
                                     QStringLiteral("Failed to set up the virtual environment - refer to the terminal log for more information."));
                return false;
            }
        }

        setInitialized();
        return true;
    }

    bool prepare(const TestSubject &testSubject) override
    {
        setCaptureStdout(true);
        setPythonFile(m_mainPyFname, m_pyModDir, m_pyVEnv);

        OOPModule::prepare(testSubject);

        // recover any adjusted settings
        if (m_pendingSettings.isFinished())
            setSettingsData(m_pendingSettings.returnValue());
        m_settingsOpened = false;
        return true;
    }

    void showSettingsUi() override
    {
        if (m_settingsOpened && !m_pendingSettings.isFinished())
            return;
        if (m_settingsOpened)
            setSettingsData(m_pendingSettings.returnValue());

        setCaptureStdout(false);
        setPythonFile(m_mainPyFname, m_pyModDir, m_pyVEnv);
        auto res = showSettingsChangeUi(settingsData());
        if (res.has_value())
            m_pendingSettings = res.value();
        m_settingsOpened = true;
    }

    void serializeSettings(const QString&, QVariantHash &settings, QByteArray &extraData) override
    {
        Q_UNUSED(settings)
        extraData = settingsData();
    }

    bool loadSettings(const QString&, const QVariantHash &settings, const QByteArray &extraData) override
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
    QString m_pyVEnv;
    QString m_pyModDir;
    bool m_useVEnv;

    bool m_settingsOpened;
    QRemoteObjectPendingReply<QByteArray> m_pendingSettings;
};

class PyModuleInfo : public ModuleInfo
{
public:
    explicit PyModuleInfo(const QString &id, const QString &name,
                          const QString &description, const QPixmap &icon,
                          bool isDevModule)
        : m_id(id),
          m_name(name),
          m_description(description),
          m_icon (icon),
          m_isDevModule(isDevModule) {}

    ~PyModuleInfo() override {}

    QString id() const override { return m_id; }
    QString name() const override { return m_name; };
    QString description() const override { return m_description; };
    QPixmap pixmap() const override { return m_icon; };
    bool devel() const override { return m_isDevModule; }
    AbstractModule *createModule(QObject *parent = nullptr) override
    {
        auto mod = new PythonModule(parent);
        mod->setPythonInfo(m_pyFname, m_pyModDir, m_useVEnv);
        mod->setupPorts(m_portDefInput, m_portDefOutput);
        return mod;
    }

    void setMainPyScriptFname(const QString &pyFname) { m_pyFname = pyFname; }
    void setModSourceDir(const QString &wdir) { m_pyModDir = wdir; }
    void setUseVEnv(bool enabled) { m_useVEnv = enabled; }
    void setPortDef(const QVariantList &defInput, const QVariantList &defOutput)
    {
        m_portDefInput = defInput;
        m_portDefOutput = defOutput;
    }

private:
    QString m_id;
    QString m_name;
    QString m_description;
    QPixmap m_icon;
    bool m_isDevModule;

    QString m_pyFname;
    QString m_pyModDir;
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
    const auto isDevMod = modDef.value("devel", false).toBool();
    const auto useVEnv = modDef.value("use_venv", false).toBool();

    if (name.isEmpty())
        throw std::runtime_error("Required 'name' key not found in module metadata.");
    if (desc.isEmpty())
        throw std::runtime_error("Required 'description' key not found in module metadata.");

    QPixmap icon = QPixmap(":/module/generic");
    if (!iconName.isEmpty()) {
        if (!icon.load(QDir(modDir).filePath(iconName)))
            icon = QIcon::fromTheme(iconName, QIcon(":/module/generic")).pixmap(64, 64);
    }

    const auto pyFile = QDir(modDir).filePath(modDef.value("main").toString());
    if (!QFileInfo::exists(pyFile))
        throw std::runtime_error(qPrintable(QStringLiteral("Main entrypoint Python file %1 does not exist").arg(pyFile)));

    modInfo = new PyModuleInfo(modId, name, desc, icon, isDevMod);
    modInfo->setMainPyScriptFname(pyFile);
    modInfo->setModSourceDir(modDir);
    modInfo->setUseVEnv(useVEnv);

    const auto portsDef = modData.value("ports").toHash();
    if (!portsDef.isEmpty())
        modInfo->setPortDef(portsDef.value("in").toList(),
                            portsDef.value("out").toList());

    return modInfo;
}

#include "pymoduleloader.moc"

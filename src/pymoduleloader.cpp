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

#include "pymoduleloader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMessageBox>
#include <QTimer>
#include <iostream>

#include "globalconfig.h"
#include "mlinkmodule.h"
#include "pyvenvmanager.h"

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
                BaseDataType::typeIdFromString(po.value("data_type").toString().toStdString()),
                po.value("id").toString(),
                po.value("title").toString());
        }

        for (const auto &pv : varOutPorts) {
            const auto po = pv.toHash();
            registerOutputPortByTypeId(
                BaseDataType::typeIdFromString(po.value("data_type").toString().toStdString()),
                po.value("id").toString(),
                po.value("title").toString());
        }
    }

    bool virtualEnvExists() const
    {
        return pythonVirtualEnvExists(id());
    }

    bool installVirtualEnv()
    {
        const auto reqFile = QStringLiteral("%1/requirements.txt").arg(m_pyModDir);
        return createPythonVirtualEnv(id(), reqFile);
    }

    bool ensurePythonCodeRunning()
    {
        if (isProcessRunning())
            return true;

        setPythonVirtualEnv(pythonVEnvDirForName(id()));
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

    void stop() override
    {
        // If the process is already dead when stop() is called, it crashed during
        // the run.
        // The error has already been raised and will end the run; schedule an
        // async restart so the module is immediately ready again afterwards.
        const bool didCrash = !isProcessRunning() && state() != ModuleState::DORMANT;
        if (didCrash) {
            QTimer::singleShot(0, this, [this]() {
                ensurePythonCodeRunning();
            });
        } else {
            MLinkModule::stop();
        }
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

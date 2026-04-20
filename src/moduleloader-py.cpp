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

#include "moduleloader-py.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QTimer>
#include <iostream>

#include "config.h"
#include "globalconfig.h"
#include "mlinkmodule.h"
#include "pyvenvmanager.h"

/**
 * Get a directory to prepend to PYTHONPATH so the syntalos_mlink C extension
 * can be imported even if we are running from a not-installed copy of Syntalos.
 */
static QString syntalosLocalPyMlinkPath()
{
    // In dev / uninstalled builds the extension is built alongside the application
    // binary in a "python/" subdirectory.
    const QString devDir = QCoreApplication::applicationDirPath() + QStringLiteral("/python");

    // prefer the dev-build directory when it contains the extension
    if (!QDir(devDir).entryList({"syntalos_mlink*.so"}, QDir::Files).isEmpty())
        return devDir;

    return {};
}

/**
 * Find the system python3 interpreter.
 */
static QString findSystemPython3Interpreter()
{
    const QStringList candidates = {
        QStringLiteral("/usr/bin/python3"),
        QStringLiteral("/usr/local/bin/python3"),
    };

    for (const auto &p : candidates) {
        if (QFileInfo::exists(p))
            return p;
    }
    return QStringLiteral("python3"); // rely on PATH
}

class PythonModule : public MLinkModule
{
    Q_OBJECT
public:
    explicit PythonModule(QObject *parent = nullptr)
        : MLinkModule(parent),
          m_features(ModuleFeature::NONE),
          m_useVEnv(false),
          m_runGdb(false)
    {
        // Python modules run persistently: the process is started at initialization
        // and kept alive between runs so settings UI and port connections remain
        // available without relaunching the script.
        setWorkerMode(ModuleWorkerMode::PERSISTENT);

        setOutputCaptured(false); // don't capture stdout until we are running
        connect(this, &MLinkModule::processOutputReceived, this, [&](const QString &data) {
            std::cout << "py." << id().toStdString() << "(" << name().toStdString() << "): " << data.toStdString()
                      << std::endl;
        });
    }

    ~PythonModule() override = default;

    ModuleFeatures features() const override
    {
        return m_features;
    }

    void setFeatures(ModuleFeatures features)
    {
        m_features = features;
    }

    bool installVirtualEnv(bool recreate)
    {
        const auto reqFile = QStringLiteral("%1/requirements.txt").arg(m_pyModDir);
        const auto result = createPythonVirtualEnv(id(), reqFile, recreate);
        if (result.has_value())
            return true;

        QMessageBox::warning(
            nullptr, QStringLiteral("Failed to create virtual environment for %1").arg(id()), result.error());
        return false;
    }

    /**
     * Configure and launch the Python script as a direct subprocess.
     *
     * Sets the Python interpreter as the module binary (the venv's interpreter if
     * applicable), passes the script path as its sole argument, and prepends the
     * syntalos_mlink search directories to PYTHONPATH so the extension is always
     * importable, even when not using a venv (in venvs, we symlink the extension
     * into the environment).
     */
    bool ensurePythonCodeRunning()
    {
        if (isProcessRunning())
            return true;

        // Build PYTHONPATH: syntalos_mlink search paths take priority so the
        // extension is always importable.
        const auto localPyModPath = syntalosLocalPyMlinkPath();
        if (!localPyModPath.isEmpty()) {
            auto penv = QProcessEnvironment::systemEnvironment();
            QStringList pythonPath;
            pythonPath << localPyModPath;
            const QString existingPythonPath = penv.value(QStringLiteral("PYTHONPATH"));
            if (!existingPythonPath.isEmpty())
                pythonPath << existingPythonPath;
            penv.insert(QStringLiteral("PYTHONPATH"), pythonPath.join(':'));
            setModuleBinaryEnv(penv);
        }

        // Resolve interpreter: use the venv's python when a venv is requested.
        // setPythonVirtualEnv() tells runProcess() to set VIRTUAL_ENV and prepend
        // the venv's bin/ directory to PATH, which fully activates the environment.
        const QString venvDir = m_useVEnv ? pythonVEnvDirForName(id()) : QString();
        if (!venvDir.isEmpty()) {
            const QString venvPython = venvDir + QStringLiteral("/bin/python");
            setModuleBinary(QFileInfo::exists(venvPython) ? venvPython : findSystemPython3Interpreter());
            setPythonVirtualEnv(venvDir);
        } else {
            setModuleBinary(findSystemPython3Interpreter());
        }

        setModuleBinaryArgs({m_mainPyFname});
        if (m_runGdb) {
            // NOTE: This is currently for debugging during development only!
            setModuleBinary("/usr/bin/gdb");
            setModuleBinaryArgs(
                {"-q",
                 "-batch",
                 "-ex",
                 "run",
                 "-ex",
                 "bt full",
                 "--args",
                 findSystemPython3Interpreter(),
                 m_mainPyFname});
        }

        setModuleBinaryWorkDir(m_pyModDir);
        setOutputCaptured(true);

        // run!
        if (!runProcess())
            return false;

        // Drain all InputPortChange / OutputPortChange ADD messages emitted during
        // script-level port registration so ports are available on the master side
        // before we return. This lets project loading restore connections immediately
        // after initialize() completes.
        handleIncomingControl();

        return true;
    }

    bool initialize() override
    {
        if (m_mainPyFname.isEmpty()) {
            raiseError("No Python script file set for this module.");
            return false;
        }

        if (m_useVEnv && QFile::exists(QStringLiteral("%1/requirements.txt").arg(m_pyModDir))) {
            const auto reqFile = QStringLiteral("%1/requirements.txt").arg(m_pyModDir);
            const auto venvStatus = pythonVirtualEnvStatus(id(), reqFile);

            if (venvStatus == PyVirtualEnvStatus::MISSING) {
                const auto reply = QMessageBox::question(
                    nullptr,
                    QStringLiteral("Create virtual environment for %1?").arg(id()),
                    QStringLiteral(
                        "The '%1' module requested to run its Python code in a virtual environment, however "
                        "a virtual Python environment for modules of type '%2' does not exist yet. "
                        "Should Syntalos attempt to set up the environment automatically? "
                        "(This will open a terminal and run the necessary commands, which may take some time)")
                        .arg(name(), id()),
                    QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::No)
                    return false;

                processUiEvents();
                if (!installVirtualEnv(false))
                    return false;
            } else if (venvStatus == PyVirtualEnvStatus::REQUIREMENTS_CHANGED) {
                const auto reply = QMessageBox::question(
                    nullptr,
                    QStringLiteral("Recreate virtual environment for %1?").arg(id()),
                    QStringLiteral(
                        "The requirements for '%1' have changed since this virtual environment was created. "
                        "Should Syntalos recreate the environment now? "
                        "(This will open a terminal and reinstall the module dependencies)")
                        .arg(name()),
                    QMessageBox::Yes | QMessageBox::No);

                // we reuse the old environment if the user selected "No"
                if (reply == QMessageBox::No) {
                    setInitialized();
                    return true;
                }

                processUiEvents();
                if (!installVirtualEnv(true))
                    return false;
            } else if (venvStatus == PyVirtualEnvStatus::INTERPRETER_MISSING) {
                QMessageBox::information(
                    nullptr,
                    QStringLiteral("Recreating virtual environment for %1").arg(id()),
                    QStringLiteral(
                        "The Python interpreter used to create the '%1' environment is no longer available. "
                        "Syntalos must recreate the virtual environment before this module can start.")
                        .arg(name()));

                processUiEvents();
                if (!installVirtualEnv(true))
                    return false;
            }
        }

        if (!ensurePythonCodeRunning())
            return false;

        setInitialized();
        return true;
    }

    bool prepare(const TestSubject &testSubject) override
    {
        setOutputCaptured(true);

        if (!isProcessRunning())
            ensurePythonCodeRunning();

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
        // the run. The error has already been raised and will end the run; schedule
        // an async restart so the module is immediately ready again afterwards.
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
        const auto sd = settingsData();
        extraData = QByteArray(reinterpret_cast<const char *>(sd.data()), static_cast<qsizetype>(sd.size()));
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &extraData) override
    {
        Q_UNUSED(settings)
        ByteVector bv(
            reinterpret_cast<const std::byte *>(extraData.constData()),
            reinterpret_cast<const std::byte *>(extraData.constData()) + extraData.size());
        setSettingsData(bv);
        return true;
    }

    void setPythonInfo(const QString &fname, const QString &wdir, bool useVEnv)
    {
        m_mainPyFname = fname;
        m_pyModDir = wdir;
        m_useVEnv = useVEnv;
    }

private:
    QString m_mainPyFname;
    QString m_pyModDir;
    ModuleFeatures m_features;
    bool m_useVEnv;
    bool m_runGdb;
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

private:
    QString m_id;
    QString m_name;
    QString m_description;
    QIcon m_icon;
    ModuleCategories m_categories;
    ModuleFeatures m_features;

    QString m_pyFname;
    bool m_useVEnv;
};

ModuleInfo *loadPythonModuleInfo(const QString &modId, const QString &modDir, const QVariantHash &modData)
{
    PyModuleInfo *modInfo = nullptr;
    const auto modDef = modData["syntalos_module"].toHash();
    const auto name = modDef.value("name").toString();
    const auto desc = modDef.value("description").toString();
    const auto iconName = modDef.value("icon").toString();
    const auto categories = moduleCategoriesFromVariant(modDef.value("categories"));
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

    return modInfo;
}

#include "moduleloader-py.moc"

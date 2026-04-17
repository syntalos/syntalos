/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "moduleloader-ext.h"

#include <QDir>
#include <QIcon>
#include <QTimer>
#include <iostream>

#include "mlinkmodule.h"

class ExternalModule : public MLinkModule
{
    Q_OBJECT
public:
    explicit ExternalModule(QObject *parent = nullptr)
        : MLinkModule(parent),
          m_features(ModuleFeature::NONE)
    {
        // Executable MLink modules run persistently:
        // Their process is started on initialization and kept alive between runs
        // so that the settings/display UI and port connections remain available.
        setWorkerMode(ModuleWorkerMode::PERSISTENT);

        setOutputCaptured(false); // don't capture stdout until we are running
        connect(this, &MLinkModule::processOutputReceived, this, [&](const QString &data) {
            std::cout << id().toStdString() << "(" << name().toStdString() << "): " << data.toStdString() << std::endl;
        });
    }

    ~ExternalModule() override = default;

    ModuleFeatures features() const override
    {
        return m_features;
    }

    void setFeatures(ModuleFeatures features)
    {
        m_features = features;
    }

    bool ensureModuleRunning()
    {
        if (isProcessRunning())
            return true;

        setOutputCaptured(true);
        return runProcess();
    }

    bool initialize() override
    {
        if (moduleBinary().isEmpty()) {
            raiseError("Unable to find module binary. Is the module installed correctly?");
            return false;
        }

        if (!ensureModuleRunning())
            return false;

        return MLinkModule::initialize();
    }

    bool prepare(const TestSubject &testSubject) override
    {
        setOutputCaptured(true);

        if (!isProcessRunning())
            ensureModuleRunning();

        return MLinkModule::prepare(testSubject);
    }

    void showSettingsUi() override
    {
        if (!ensureModuleRunning())
            return;

        MLinkModule::showSettingsUi();
    }

    void showDisplayUi() override
    {
        if (!ensureModuleRunning())
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
                ensureModuleRunning();
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

private:
    ModuleFeatures m_features;
};

class ExtModuleInfo : public ModuleInfo
{
public:
    explicit ExtModuleInfo(QString id, QString name, QString description, QIcon icon, ModuleCategories categories)
        : m_id(std::move(id)),
          m_name(std::move(name)),
          m_description(std::move(description)),
          m_icon(std::move(icon)),
          m_categories(categories)
    {
    }

    QString id() const override
    {
        return m_id;
    }

    QString name() const override
    {
        return m_name;
    }

    QString description() const override
    {
        return m_description;
    }

    QIcon icon() const override
    {
        return m_icon;
    }

    ModuleCategories categories() const final
    {
        return m_categories;
    }

    AbstractModule *createModule(QObject *parent = nullptr) override
    {
        auto mod = new ExternalModule(parent);
        mod->setModuleBinary(m_binaryPath);
        mod->setFeatures(m_features);
        return mod;
    }

    void setBinaryPath(const QString &path)
    {
        m_binaryPath = path;
    }

    void setFeatures(ModuleFeatures features)
    {
        m_features = features;
    }

private:
    QString m_id;
    QString m_name;
    QString m_description;
    QIcon m_icon;
    ModuleCategories m_categories;
    ModuleFeatures m_features;
    QString m_binaryPath;
};

ModuleInfo *loadExtModuleInfo(const QString &modId, const QString &modDir, const QVariantHash &modData)
{
    const auto modDef = modData["syntalos_module"].toHash();
    const auto name = modDef.value("name").toString();
    const auto desc = modDef.value("description").toString();
    const auto iconName = modDef.value("icon").toString();
    const auto categories = moduleCategoriesFromVariant(modDef.value("categories"));
    const auto featuresList = modDef.value("features", QStringList()).toStringList();
    const auto binaryName = modDef.value("binary").toString();

    if (name.isEmpty())
        throw std::runtime_error("Required 'name' key not found in module metadata.");
    if (desc.isEmpty())
        throw std::runtime_error("Required 'description' key not found in module metadata.");
    if (binaryName.isEmpty())
        throw std::runtime_error("Required 'binary' key not found in executable module metadata.");

    QIcon icon = QIcon(":/module/generic");
    if (!iconName.isEmpty()) {
        const auto iconFname = QDir(modDir).filePath(iconName);
        if (QFileInfo::exists(iconFname))
            icon = QIcon(iconFname);
        else
            icon = QIcon::fromTheme(iconName, QIcon(":/module/generic"));
    }

    const auto binaryPath = QDir(modDir).filePath(binaryName);
    if (!QFileInfo::exists(binaryPath))
        throw std::runtime_error(qPrintable(QStringLiteral("Module binary '%1' does not exist").arg(binaryPath)));

    ModuleFeatures features;
    for (const auto &f : featuresList) {
        if (f == QStringLiteral("show-settings"))
            features |= ModuleFeature::SHOW_SETTINGS;
        else if (f == QStringLiteral("show-display"))
            features |= ModuleFeature::SHOW_DISPLAY;
    }

    auto modInfo = new ExtModuleInfo(modId, name, desc, icon, categories);
    modInfo->setRootDir(modDir);
    modInfo->setBinaryPath(binaryPath);
    modInfo->setFeatures(features);

    return modInfo;
}

#include "moduleloader-ext.moc"

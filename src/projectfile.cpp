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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "projectfile.h"

#include <KTar>
#include <QDir>
#include <QTemporaryFile>
#include <QMessageBox>
#include <utility>

#include "engine.h"
#include "flowgraphview.h"

#include "utils/tomlutils.h"
#include "utils/misc.h"

namespace Syntalos
{

// config format API level
static const QString CONFIG_FILE_FORMAT_VERSION = QStringLiteral("1");

static void setStatusText(StatusMessageFn fn, const QString &msg)
{
    if (!fn)
        return;
    fn(msg);
}

static bool saveProjectConfigInternal(
    Engine *engine,
    TestSubjectListModel *subjectList,
    ExperimenterListModel *experimenterList,
    FlowGraphView *graphView,
    const ProjectStorageSettings &storage,
    QFileDevice *file,
    StatusMessageFn statusFn)
{
    auto log = getLogger("board.save");

    KTar tar(file);
    if (!tar.open(QIODevice::WriteOnly)) {
        LOG_WARNING(log, "Unable to open new configuration file for writing.");
        return false;
    }
    setStatusText(statusFn, "Saving configuration to file...");

    QDir confBaseDir(QStringLiteral("%1/..").arg(file->fileName()));

    // save basic settings
    QVariantHash settings;
    settings.insert("version_format", CONFIG_FILE_FORMAT_VERSION);
    settings.insert("version_app", QCoreApplication::applicationVersion());
    settings.insert("time_created", QDateTime::currentDateTime());

    settings.insert("export_base_dir", engine->exportBaseDir());
    settings.insert("experiment_id", engine->experimentId().isEmpty() ? storage.experimentId : engine->experimentId());

    QVariantHash storageSettings;
    QStringList exportDirOrder;
    for (const auto &part : storage.exportDirLayout)
        exportDirOrder.append(exportDirPathComponentToKey(part));
    storageSettings.insert("order", exportDirOrder);
    storageSettings.insert("clock_time_in_dir", storage.clockTimeInDir);
    storageSettings.insert("simple_names", storage.simpleNames);
    storageSettings.insert("flat_root", storage.flatRoot);

    settings.insert("storage", storageSettings);

    // basic configuration
    tar.writeFile("main.toml", qVariantHashToTomlData(settings));

    // save list of subjects
    tar.writeFile("subjects.toml", qVariantHashToTomlData(subjectList->toVariantHash()));

    // save list of experimenters
    tar.writeFile("experimenters.toml", qVariantHashToTomlData(experimenterList->toVariantHash()));

    // save graph settings
    graphView->saveState();
    tar.writeFile("graph.toml", qVariantHashToTomlData(graphView->settings()));

    // save module settings
    auto modIndex = 0;
    for (auto &mod : engine->presentModules()) {
        if (!tar.writeDir(QString::number(modIndex)))
            return false;

        QVariantHash modSettings;
        QByteArray modExtraData;
        setStatusText(statusFn, QStringLiteral("Saving data for '%1'...").arg(mod->name()));

        mod->serializeSettings(confBaseDir.absolutePath(), modSettings, modExtraData);
        if (!modSettings.isEmpty())
            tar.writeFile(
                QStringLiteral("%1/%2.toml").arg(modIndex).arg(mod->id()), qVariantHashToTomlData(modSettings));
        if (!modExtraData.isEmpty())
            tar.writeFile(QStringLiteral("%1/%2.dat").arg(modIndex).arg(mod->id()), modExtraData);

        QVariantHash modInfo;
        modInfo.insert("id", mod->id());
        modInfo.insert("name", mod->name());
        modInfo.insert("ui_display_geometry", mod->serializeDisplayUiGeometry());
        modInfo.insert("enabled", mod->modifiers().testFlag(ModuleModifier::ENABLED));
        modInfo.insert("stop_on_failure", mod->modifiers().testFlag(ModuleModifier::STOP_ON_FAILURE));

        // save info about port subscriptions in
        // the form inPortId -> sourceModuleName
        QVariantHash modSubs;
        for (const auto &iport : mod->inPorts()) {
            if (!iport->hasSubscription())
                continue;
            QVariantList srcVal = {iport->outPort()->owner()->name(), iport->outPort()->id()};
            modSubs.insert(iport->id(), srcVal);
        }

        modInfo.insert("subscriptions", modSubs);
        tar.writeFile(QStringLiteral("%1/info.toml").arg(modIndex), qVariantHashToTomlData(modInfo));

        modIndex++;
    }

    setStatusText(statusFn, "Saving configuration to file...");
    tar.close();

    setStatusText(statusFn, QStringLiteral("Board saved at %1.").arg(QTime::currentTime().toString(Qt::TextDate)));
    return true;
}

bool saveProjectConfiguration(
    Engine *engine,
    FlowGraphView *graphView,
    TestSubjectListModel *subjectList,
    ExperimenterListModel *experimenterList,
    const ProjectStorageSettings &storage,
    const QString &fileName,
    StatusMessageFn statusFn)
{
    auto log = getLogger("board.save");
    if (fileName.isEmpty()) {
        LOG_WARNING(log, "Tried to save board, but filename was empty.");
        return false;
    }

    auto realFileName = fileName;
    if (!realFileName.endsWith(".syct"))
        realFileName = QStringLiteral("%1.syct").arg(fileName);

    LOG_INFO(log, "Saving board as: {}", realFileName);
    QTemporaryFile file(realFileName + ".XXXXXX.tmp");
    file.setAutoRemove(false);

    if (!saveProjectConfigInternal(
            engine, subjectList, experimenterList, graphView, storage, &file, std::move(statusFn))) {
        return false;
    }

    file.close();
    if (::rename(qPrintable(file.fileName()), qPrintable(realFileName)) != 0) {
        QFile::remove(file.fileName());
        LOG_ERROR(log, "Failed to save '{}': Final rename failed.", realFileName);
        return false;
    }

    return true;
}

bool loadProjectConfigurationInteractive(
    Engine *engine,
    FlowGraphView *graphView,
    TestSubjectListModel *subjectList,
    ExperimenterListModel *experimenterList,
    ProjectStorageSettings &outStorage,
    const QString &fileName,
    QWidget *parent,
    std::function<void(void)> preLoadFn,
    StatusMessageFn statusFn)
{
    auto log = getLogger("board.load");

    KTar tar(fileName);
    if (!tar.open(QIODevice::ReadOnly)) {
        LOG_ERROR(log, "Unable to open settings file for reading: {}", tar.errorString());
        return false;
    }

    auto rootDir = tar.directory();

    // load main settings
    auto globalSettingsFile = rootDir->file("main.toml");
    if (globalSettingsFile == nullptr) {
        QMessageBox::critical(
            parent,
            QStringLiteral("Can not load settings"),
            QStringLiteral("The settings file is damaged or is no valid Syntalos configuration bundle."));
        setStatusText(statusFn, "");
        return false;
    }

    QString parseError;
    const auto rootObj = parseTomlData(globalSettingsFile->data(), parseError);
    if (!parseError.isEmpty()) {
        QMessageBox::critical(
            parent,
            QStringLiteral("Can not load settings"),
            QStringLiteral("The settings file is damaged or is no valid Syntalos configuration file. %1")
                .arg(parseError));
        setStatusText(statusFn, "");
        return false;
    }

    if (rootObj.value("version_format").toString() != CONFIG_FILE_FORMAT_VERSION) {
        auto reply = QMessageBox::question(
            parent,
            "Incompatible configuration",
            QStringLiteral(
                "The settings file you want to load was created with a different, possibly older version of "
                "Syntalos and may not work correctly in this version.\n"
                "Should we attempt to load it anyway? (This may result in unexpected behavior)"),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            setStatusText(statusFn, "Aborted configuration loading.");
            return true;
        }
    }

    // from this point on, we will be making permanent changes
    if (preLoadFn)
        preLoadFn();

    auto storageObj = rootObj.value("storage").toHash();
    outStorage.clockTimeInDir = storageObj.value("clock_time_in_dir", false).toBool();
    outStorage.simpleNames = storageObj.value("simple_names", true).toBool();
    outStorage.flatRoot = storageObj.value("flat_root", false).toBool();

    QList<ExportPathComponent> exportDirLayout;
    const auto exportDirLayoutValues = storageObj.value("order").toList();
    for (const auto &value : exportDirLayoutValues) {
        const auto maybePart = exportDirPathComponentFromKey(value.toString().trimmed().toLower());
        if (maybePart.has_value() && !exportDirLayout.contains(maybePart.value()))
            exportDirLayout.append(maybePart.value());
    }
    outStorage.exportDirLayout = exportDirLayout;

    outStorage.exportBaseDir = rootObj.value("export_base_dir").toString();
    outStorage.experimentId = rootObj.value("experiment_id").toString();

    // load list of subjects
    subjectList->clear();
    auto subjectsFile = rootDir->file("subjects.toml");
    if (subjectsFile != nullptr) {
        // not having a list of subjects is totally fine

        setStatusText(statusFn, "Loading subject information...");
        const auto subjData = parseTomlData(subjectsFile->data(), parseError);
        if (parseError.isEmpty())
            subjectList->fromVariantHash(subjData);
        else
            LOG_WARNING(log, "Unable to load test-subject data: {}", parseError);
    }

    // load list of experimenters
    experimenterList->clear();

    auto experimentersFile = rootDir->file("experimenters.toml");
    if (experimentersFile != nullptr) {
        // not having a list of subjects is totally fine

        setStatusText(statusFn, "Loading experimenter data...");
        const auto peopleData = parseTomlData(experimentersFile->data(), parseError);
        if (parseError.isEmpty())
            experimenterList->fromVariantHash(peopleData);
        else
            LOG_WARNING(log, "Unable to load experimenter data: {}", parseError);
    }

    setStatusText(statusFn, "Destroying old modules...");
    engine->removeAllModules();
    auto rootEntries = rootDir->entries();
    rootEntries.sort();

    // load graph settings
    auto graphFile = rootDir->file("graph.toml");
    if (graphFile != nullptr) {
        setStatusText(statusFn, "Caching graph settings...");
        const auto graphConfig = parseTomlData(graphFile->data(), parseError);
        if (parseError.isEmpty()) {
            // the graph view will apply stored settings to new nodes automatically
            // from here on.
            graphView->setSettings(graphConfig);
            graphView->restoreState();
        } else {
            LOG_WARNING(log, "Unable to parse graph configuration: {}", parseError);
        }
    }

    // we load the modules in two passes, to ensure they can all register
    // their interdependencies correctly.
    QList<QPair<AbstractModule *, QPair<QVariantHash, QByteArray>>> modSettingsList;
    QList<QPair<AbstractModule *, QVariantHash>> modDisplayGeometryList;

    // add modules
    QList<QPair<AbstractModule *, QVariantHash>> jSubInfo;
    for (auto &ename : rootEntries) {
        auto e = rootDir->entry(ename);
        if (!e->isDirectory())
            continue;
        auto ifile = rootDir->file(QStringLiteral("%1/info.toml").arg(ename));
        if (ifile == nullptr)
            continue;

        auto iobj = parseTomlData(ifile->data(), parseError);
        if (!parseError.isEmpty())
            LOG_WARNING(log, "Issue while loading module info: {}", parseError);

        const auto modId = iobj.value("id").toString();
        const auto modName = iobj.value("name").toString();
        const auto uiDisplayGeometry = iobj.value("ui_display_geometry").toHash();
        const auto jSubs = iobj.value("subscriptions").toHash();

        setStatusText(statusFn, QStringLiteral("Instantiating module: %1(%2)").arg(modId, modName));
        auto mod = engine->createModule(modId, modName);
        if (mod == nullptr) {
            QMessageBox::critical(
                parent,
                QStringLiteral("Can not load settings"),
                QStringLiteral(
                    "Unable to find module '%1' - please install the module first, then "
                    "attempt to load this configuration again.")
                    .arg(modId));
            setStatusText(statusFn, "Failed to load settings.");

            const auto reply = QMessageBox::question(
                parent,
                QStringLiteral("Ignore missing module?"),
                QStringLiteral(
                    "While installing the missing module is the right solution to load this board, "
                    "you can also enforce loading it. Please be aware that loading may fail. Load anyway?"),
                QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                LOG_WARNING(log, "Module {}[{}] was missing, but trying to load board anyway.", modId, modName);
                continue;
            }
            return false;
        }

        // load module modifiers
        auto modModifiers = mod->modifiers();
        modModifiers.setFlag(ModuleModifier::ENABLED, iobj.value("enabled", true).toBool());
        modModifiers.setFlag(ModuleModifier::STOP_ON_FAILURE, iobj.value("stop_on_failure", true).toBool());
        mod->setModifiers(modModifiers);

        // load module-specific configuration
        auto sfile = rootDir->file(QStringLiteral("%1/%2.toml").arg(ename).arg(modId));
        QVariantHash modSettings;
        if (sfile != nullptr) {
            modSettings = parseTomlData(sfile->data(), parseError);
            if (!parseError.isEmpty())
                LOG_WARNING(log, "Issue while loading module configuration for {}: {}", mod->name(), parseError);
        }
        sfile = rootDir->file(QStringLiteral("%1/%2.dat").arg(ename).arg(modId));
        QByteArray modSettingsEx;
        if (sfile != nullptr)
            modSettingsEx = sfile->data();

        // save display geometries - we apply them after settings have been loaded,
        // as some modules do odd things in their settings loading phase which impact
        // display UI geometry loading
        if (!uiDisplayGeometry.isEmpty())
            modDisplayGeometryList.append(qMakePair(mod, uiDisplayGeometry));

        // store subscription info to connect modules later
        jSubInfo.append(qMakePair(mod, jSubs));

        // store module-owned configuration for later
        modSettingsList.append(qMakePair(mod, qMakePair(modSettings, modSettingsEx)));
    }

    QDir confBaseDir(QString("%1/..").arg(fileName));

    // load module-owned configurations
    for (auto &pair : modSettingsList) {
        const auto mod = pair.first;
        const auto settings = pair.second;
        setStatusText(statusFn, QStringLiteral("Loading settings for module: %1(%2)").arg(mod->id()).arg(mod->name()));
        if (!mod->loadSettings(confBaseDir.absolutePath(), settings.first, settings.second)) {
            auto ret = QMessageBox::critical(
                parent,
                QStringLiteral("Can not load settings"),
                QStringLiteral("Unable to load module settings for '%1'. Continue loading this project anyway?")
                    .arg(mod->name()),
                QMessageBox::Yes | QMessageBox::No);
            setStatusText(statusFn, QStringLiteral("Failed to load settings for '%1'").arg(mod->name()));

            if (ret != QMessageBox::Yes) {
                setStatusText(statusFn, "Failed to load project settings.");
                return false;
            }
        }
    }

    // apply module view geometries
    for (auto &pair : modDisplayGeometryList)
        pair.first->restoreDisplayUiGeometry(pair.second);

    // create module connections
    setStatusText(statusFn, "Restoring streams and subscriptions...");
    for (auto &pair : jSubInfo) {
        auto mod = pair.first;
        const auto jSubs = pair.second;
        for (const QString &iPortId : jSubs.keys()) {
            const auto modPortPair = jSubs.value(iPortId).toList();
            if (modPortPair.size() != 2) {
                LOG_WARNING(log, "Malformed project data: Invalid project port pair in {} settings.", mod->name());
                continue;
            }
            const auto srcModName = modPortPair[0].toString();
            const auto srcModOutPortId = modPortPair[1].toString();
            const auto srcMod = engine->moduleByName(srcModName);
            if (srcMod == nullptr) {
                LOG_WARNING(
                    log,
                    "Error when loading project: Source module {} plugged into {} of {} was not found. Skipped "
                    "connection.",
                    srcModName,
                    iPortId,
                    mod->name());
                continue;
            }
            auto inPort = mod->inPortById(iPortId);
            if (inPort.get() == nullptr) {
                LOG_WARNING(
                    log, "Error when loading project: Module {} has no input port with ID", mod->name(), iPortId);
                continue;
            }
            auto outPort = srcMod->outPortById(srcModOutPortId);
            if (outPort.get() == nullptr) {
                LOG_WARNING(
                    log,
                    "Error when loading project: Module {} has no output port with ID {}",
                    srcMod->name(),
                    srcModOutPortId);
                continue;
            }
            inPort->setSubscription(outPort.get(), outPort->subscribe());
        }
    }

    // we are ready now
    setStatusText(statusFn, "Board successfully loaded from file.");
    return true;
}

} // namespace Syntalos

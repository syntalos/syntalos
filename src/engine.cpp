/*
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "engine.h"

#include <QDebug>
#include <QMessageBox>
#include <QStorageInfo>
#include <QCoreApplication>
#include <QDateTime>
#include <QStandardPaths>
#include <QThread>
#include <QVector>
#include <QTemporaryDir>
#include <QTimer>
#include <filesystem>

#include "oop/oopmodule.h"
#include "edlstorage.h"
#include "modulelibrary.h"
#include "moduleeventthread.h"
#include "globalconfig.h"
#include "sysinfo.h"
#include "meminfo.h"
#include "syclock.h"
#include "rtkit.h"
#include "cpuaffinity.h"
#include "utils.h"

namespace Syntalos {
    Q_LOGGING_CATEGORY(logEngine, "engine")
}

using namespace Syntalos;

class ThreadDetails
{
public:
    explicit ThreadDetails()
        : name(createRandomString(8)),
          niceness(0),
          allowedRTPriority(0)
    {}

    QString name;
    int niceness;
    int allowedRTPriority;
    std::vector<uint> cpuAffinity;
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
class Engine::Private
{
public:
    Private() { }
    ~Private() { }

    SysInfo *sysInfo;
    GlobalConfig *gconf;
    QWidget *parentWidget;
    QList<AbstractModule*> activeModules;
    ModuleLibrary *modLibrary;
    std::shared_ptr<SyncTimer> timer;

    QString exportBaseDir;
    QString exportDir;
    bool exportDirIsTempDir;
    bool exportDirIsValid;

    EDLAuthor experimenter;
    TestSubject testSubject;
    QString experimentId;
    bool simpleStorageNames;

    std::atomic_bool active;
    std::atomic_bool running;
    std::atomic_bool failed;
    QString runFailedReason;

    bool saveInternal;
    std::shared_ptr<EDLGroup> edlInternalData;
    QHash<QString, std::shared_ptr<TimeSyncFileWriter>> internalTSyncWriters;
};
#pragma GCC diagnostic pop

Engine::Engine(QWidget *parentWidget)
    : QObject(parentWidget),
      d(new Engine::Private)
{
    d->saveInternal = false;
    d->gconf = new GlobalConfig(this);
    d->sysInfo = new SysInfo(this);
    d->exportDirIsValid = false;
    d->active = false;
    d->running = false;
    d->simpleStorageNames = true;
    d->modLibrary = new ModuleLibrary(this);
    d->parentWidget = parentWidget;
    d->timer.reset(new SyncTimer);

    // allow sending states via Qt queued connections,
    // and also register all other transmittable data types
    // with the meta object system
    registerStreamMetaTypes();

    // for time synchronizers - this metatype can not be registered in the
    // synchronizers, as they may run in arbitrary threads and some may even
    // try to register simultaneously
    qRegisterMetaType<TimeSyncStrategies>();
}

Engine::~Engine()
{}

ModuleLibrary *Engine::library() const
{
    return d->modLibrary;
}

SysInfo *Engine::sysInfo() const
{
    return d->sysInfo;
}

QString Engine::exportBaseDir() const
{
    return d->exportBaseDir;
}

void Engine::setExportBaseDir(const QString &dataDir)
{
    d->exportBaseDir = dataDir;

    if (dataDir.isEmpty())
        return;

    d->exportDirIsValid = QDir().exists(d->exportBaseDir);
    d->exportDirIsTempDir = false;
    if (d->exportBaseDir.startsWith(QStandardPaths::writableLocation(QStandardPaths::TempLocation)) ||
        d->exportBaseDir.startsWith(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))) {
        d->exportDirIsTempDir = true;
    }

    // update the actual export directory location, using the test subject data and the
    // current date
    refreshExportDirPath();
}

bool Engine::exportDirIsTempDir() const
{
    return d->exportDirIsTempDir;
}

bool Engine::exportDirIsValid() const
{
    return d->exportDirIsValid;
}

TestSubject Engine::testSubject() const
{
    return d->testSubject;
}

void Engine::setTestSubject(const TestSubject &ts)
{
    d->testSubject = ts;
    refreshExportDirPath();
}

QString Engine::experimentId() const
{
    return d->experimentId;
}

void Engine::setExperimentId(const QString &id)
{
    d->experimentId = id;
    refreshExportDirPath();
}

EDLAuthor Engine::experimenter() const
{
    return d->experimenter;
}

void Engine::setExperimenter(const EDLAuthor &person)
{
    d->experimenter = person;
}

bool Engine::simpleStorageNames() const
{
    return d->simpleStorageNames;
}

void Engine::setSimpleStorageNames(bool enabled)
{
    d->simpleStorageNames = enabled;
}

QString Engine::exportDir() const
{
    return d->exportDir;
}

bool Engine::isRunning() const
{
    return d->running;
}

bool Engine::isActive() const
{
    return d->active;
}

bool Engine::hasFailed() const
{
    return d->failed;
}

milliseconds_t Engine::currentRunElapsedTime() const
{
    if (!d->running)
        return milliseconds_t(0);
    return d->timer->timeSinceStartMsec();
}

AbstractModule *Engine::createModule(const QString &id, const QString &name)
{
    auto modInfo = d->modLibrary->moduleInfo(id);
    if (modInfo == nullptr)
        return nullptr;

    // Ensure we don't register a module twice that should only exist once
    if (modInfo->singleton()) {
        for (auto &emod : d->activeModules) {
            if (emod->id() == id)
                return nullptr;
        }
    }

    auto mod = modInfo->createModule();
    assert(mod);
    mod->setId(modInfo->id());
    mod->setIndex(modInfo->count() + 1);
    modInfo->setCount(mod->index());
    if (name.isEmpty()) {
        if (modInfo->count() > 1)
            mod->setName(QStringLiteral("%1 %2").arg(modInfo->name()).arg(modInfo->count()));
        else
            mod->setName(simplifyStrForModuleName(modInfo->name()));
    } else {
        mod->setName(simplifyStrForModuleName(name));
    }

    d->activeModules.append(mod);
    emit moduleCreated(modInfo.get(), mod);

    // the module has been created and registered, we can
    // safely initialize it now.
    mod->setState(ModuleState::INITIALIZING);
    QCoreApplication::processEvents();
    if (!mod->initialize()) {
        QMessageBox::critical(d->parentWidget, QStringLiteral("Module initialization failed"),
                              QStringLiteral("Failed to initialize module '%1', it can not be added. %2").arg(mod->id()).arg(mod->lastError()),
                              QMessageBox::Ok);
        removeModule(mod);
        return nullptr;
    }

    // now listen to errors emitted by this module
    connect(mod, &AbstractModule::error, this, &Engine::receiveModuleError);

    // connect synchronizer details callbacks
    connect(mod, &AbstractModule::synchronizerDetailsChanged, this, &Engine::onSynchronizerDetailsChanged, Qt::QueuedConnection);
    connect(mod, &AbstractModule::synchronizerOffsetChanged, this, &Engine::onSynchronizerOffsetChanged, Qt::QueuedConnection);

    mod->setState(ModuleState::IDLE);
    return mod;
}

bool Engine::removeModule(AbstractModule *mod)
{
    auto id = mod->id();
    if (d->activeModules.removeOne(mod)) {
        // Update module info
        auto modInfo = d->modLibrary->moduleInfo(id);
        modInfo->setCount(modInfo->count() - 1);

        emit modulePreRemove(mod);
        delete mod;
        return true;
    }

    return false;
}

void Engine::removeAllModules()
{
    if (d->running)
        stop();
    if (d->active) {
        qCInfo(logEngine).noquote() << "Requested to remove all modules, but engine is still active. Waiting for it to shut down.";
        for (int i = 0; i < 800; ++i) {
            if (!d->active)
                break;
            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
            QThread::msleep(50);
        };
        if (d->active) {
            qFatal("Requested to remove all modules on an active engine that did not manage to shut down in time. This must not happen.");
            assert(0);
        }
    }

    foreach (auto mod, d->activeModules)
        removeModule(mod);
}

QList<AbstractModule *> Engine::activeModules() const
{
    return d->activeModules;
}

AbstractModule *Engine::moduleByName(const QString &name) const
{
    // FIXME: In case we grow projects with huge module counts, we
    // will actually want a QHash index of modules here, so speed up
    // this function (which may otherwise slow down project loading times)
    for (const auto &mod : d->activeModules) {
        if (mod->name() == name)
            return mod;
    }
    return nullptr;
}

bool Engine::saveInternalDiagnostics() const
{
    return d->saveInternal;
}

void Engine::setSaveInternalDiagnostics(bool save)
{
    d->saveInternal = save;
}

bool Engine::makeDirectory(const QString &dir)
{
    if (!QDir().mkpath(dir)) {
        const auto message = QStringLiteral("Unable to create directory '%1'.").arg(dir);
        QMessageBox::critical(d->parentWidget,
                              QStringLiteral("Error"),
                              message);
        emitStatusMessage("OS error.");
        return false;
    }

    return true;
}

void Engine::refreshExportDirPath()
{
    auto time = QDateTime::currentDateTime();
    auto currentDate = time.date().toString("yyyy-MM-dd");

    d->exportDir = QDir::cleanPath(QStringLiteral("%1/%2/%3/%4")
                                   .arg(d->exportBaseDir)
                                   .arg(d->testSubject.id)
                                   .arg(currentDate)
                                   .arg(d->experimentId));
}

void Engine::emitStatusMessage(const QString &message)
{
    qCDebug(logEngine).noquote() << message;
    emit statusMessage(message);
}

/**
 * @brief Return a list of active modules that have been sorted in the order they
 * should be prepared, run and overall be handled in (but not stopped in!).
 */
QList<AbstractModule *> Engine::createModuleExecOrderList()
{
    // While modules could in theory be initialized in arbitrary order,
    // it is more efficient and more predicatble if we initialize data-generating
    // modules and modules which do not receive input first, and the initialize
    // the ones which rely on data created by those modules.
    // Proper dependency resolution would be needed for a perfect solution,
    // but we only need one that's "good enough" here for now. So this algorithm
    // will not produce a perfect result, especially if there are cycles in the
    // module graph.
    QList<AbstractModule*> orderedActiveModules;
    QSet<AbstractModule*> assignedMods;

    const auto modCount = d->activeModules.length();
    assignedMods.reserve(modCount);
    orderedActiveModules.reserve(modCount);
    for (const auto &mod : d->activeModules) {
        if (assignedMods.contains(mod))
            continue;

        // modules with no input ports go first
        if (mod->inPorts().isEmpty()) {
            orderedActiveModules.prepend(mod);
            assignedMods.insert(mod);
            continue;
        }

        auto anySubscribed = false;
        for (const auto &iport : mod->inPorts()) {
            if (iport->hasSubscription()) {
                anySubscribed = true;
                const auto upstreamMod = iport->outPort()->owner();
                if (!assignedMods.contains(upstreamMod)) {
                    orderedActiveModules.append(upstreamMod);
                    assignedMods.insert(upstreamMod);
                }
            }
        }

        // just stop if all modules have been assigned
        if (assignedMods.size() == modCount)
            break;

        if (assignedMods.contains(mod))
            continue;

        if (!anySubscribed)
            orderedActiveModules.prepend(mod);
        else
            orderedActiveModules.append(mod);
        assignedMods.insert(mod);
    }

    if (orderedActiveModules.length() != modCount)
        qCCritical(logEngine).noquote() << "Invalid count of ordered modules:" << orderedActiveModules.length() << "!=" << modCount;
    assert(orderedActiveModules.length() == modCount);

    auto debugText = QStringLiteral("Running modules in order: ");
    for (auto &mod : orderedActiveModules)
        debugText.append(mod->name() + QStringLiteral("; "));
    qCDebug(logEngine).noquote() << debugText;

    return orderedActiveModules;
}

/**
 * @brief Create new module stop order from their exec order.
 */
QList<AbstractModule *> Engine::createModuleStopOrderFromExecOrder(const QList<AbstractModule *> &modExecList)
{
    QList<AbstractModule*> stopOrderMods;
    QSet<AbstractModule*> assignedMods;
    stopOrderMods.reserve(modExecList.length());

    for (auto &mod : modExecList) {
        if (assignedMods.contains(mod))
            continue;

        // FIXME: This is very ugly special-casing of a single module type, but we want to give users
        // a chance to still send Firmata commands when the system is terminating.
        // Possibly replace this with module-defined declarative StartupOrder/TerminateOrder later?
        if (mod->id() == QStringLiteral("firmata-io")) {
            for (const auto &iport : mod->inPorts()) {
                if (iport->hasSubscription()) {
                    const auto upstreamMod = iport->outPort()->owner();
                    if (upstreamMod->id() == QStringLiteral("pyscript")) {
                        if (!assignedMods.contains(upstreamMod)) {
                            stopOrderMods.append(upstreamMod);
                            assignedMods.insert(upstreamMod);

                            stopOrderMods.append(mod);
                            assignedMods.insert(mod);
                        } else {
                            // inefficiently search for the module we should stop after
                            for (ssize_t i = 0; i < stopOrderMods.length(); i++) {
                                if (upstreamMod == stopOrderMods[i]) {
                                    stopOrderMods.insert(i + 1, mod);
                                    assignedMods.insert(mod);
                                    break;
                                }
                            }
                        }

                        break;
                    }
                }
            }
        }

        // we need to check the set again here, as modules may have been
        // added while we were in this loop
        if (!assignedMods.contains(mod)) {
            stopOrderMods.append(mod);
            assignedMods.insert(mod);
        }
    }

    if (stopOrderMods.length() != modExecList.length())
        qCCritical(logEngine).noquote() << "Invalid count of stop-ordered modules:" << stopOrderMods.length() << "!=" << modExecList.length();
    assert(stopOrderMods.length() == modExecList.length());

    return stopOrderMods;
}

/**
 * @brief Main entry point for engine-managed module threads.
 */
static void executeModuleThread(const ThreadDetails td, AbstractModule *mod, OptionalWaitCondition *waitCondition)
{
    pthread_setname_np(pthread_self(), qPrintable(td.name.mid(0, 15)));

    // set higher niceness for this thread
    if (td.niceness != 0)
        setCurrentThreadNiceness(td.niceness);

    // set CPU affinity
    if (!td.cpuAffinity.empty())
        thread_set_affinity_from_vec(pthread_self(), td.cpuAffinity);

    if (mod->features().testFlag(ModuleFeature::REALTIME)) {
        if (setCurrentThreadRealtime(td.allowedRTPriority))
            qCDebug(logEngine).noquote().nospace() << "Module thread for '" << mod->name() << "' set to realtime mode.";
    }

    mod->runThread(waitCondition);
}

/**
 * @brief Main entry point for threads used to manage out-of-process worker modules.
 */
static void executeOOPModuleThread(const ThreadDetails td, QList<OOPModule*> mods,
                                   OptionalWaitCondition *waitCondition, std::atomic_bool &running)
{
    pthread_setname_np(pthread_self(), qPrintable(td.name.mid(0, 15)));

    // set higher niceness for this thread
    if (td.niceness != 0)
        setCurrentThreadNiceness(td.niceness);

    // set CPU affinity
    if (!td.cpuAffinity.empty())
        thread_set_affinity_from_vec(pthread_self(), td.cpuAffinity);

    QEventLoop loop;

    // prepare all OOP modules in their new thread
    {
        QList<OOPModule*> readyMods;
        bool threadIsRealtime = false;
        for (auto &mod : mods) {

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            const auto qvCpuAffinity = QVector<uint>(td.cpuAffinity.begin(), td.cpuAffinity.end());
#else
            const auto qvCpuAffinity = QVector<uint>::fromStdVector(td.cpuAffinity);
#endif

            if (!mod->oopPrepare(&loop, qvCpuAffinity)) {
                // deininitialize modules we already have prepared
                for (auto &reMod : readyMods)
                    reMod->oopFinalize(&loop);
                mod->oopFinalize(&loop);

                qCDebug(logEngine).noquote().nospace() << "Failed to prepare OOP module " << mod->name() << ": " << mod->lastError();
                return;
            }

            // FIXME: if only one of the modules requests realtime prority, the whole thread goes RT at the moment.
            // we do actually want to split out such modules to their of thread (currently, this situation never happens,
            // because OOP modules aren't realtime).
            if (!threadIsRealtime && mod->features().testFlag(ModuleFeature::REALTIME)) {
                if (setCurrentThreadRealtime(td.allowedRTPriority)) {
                    threadIsRealtime = true;
                    qCDebug(logEngine).noquote().nospace() << "OOP thread " << td.name << " set to realtime mode (requested by '" << mod->name() << "').";
                }
            }

            // ensure we are ready - the engine has reset ourselves to "PREPARING"
            // to make this possible before launching this thread
            mod->setStateReady();

            readyMods.append(mod);
        }
    }

    // wait for us to start
    waitCondition->wait();

    for (auto &mod : mods)
        mod->oopStart(&loop);

    while (running) {
        loop.processEvents();

        for (auto &mod : mods)
            mod->oopRunEvent(&loop);
    }

    for (auto &mod : mods)
        mod->oopFinalize(&loop);
}

bool Engine::run()
{
    if (d->running)
        return false;

    d->failed = true; // if we exit before this is reset, initialization has failed

    if (d->activeModules.isEmpty()) {
        QMessageBox::warning(d->parentWidget,
                             QStringLiteral("Configuration error"),
                             QStringLiteral("You did not add a single module to be run.\nPlease add a module to the board to continue."));
        return false;
    }

    if (!exportDirIsValid() || d->exportBaseDir.isEmpty() || d->exportDir.isEmpty()) {
        QMessageBox::critical(d->parentWidget,
                             QStringLiteral("Configuration error"),
                             QStringLiteral("Data export directory was not properly set. Can not continue."));
        return false;
    }

    // persistent data recording can be initialized!
    qCDebug(logEngine, "Initializing new persistent recording run");

    // test for available disk space and readyness of device
    QStorageInfo storageInfo(d->exportBaseDir);
    if (storageInfo.isValid() && storageInfo.isReady()) {
        auto mbAvailable = storageInfo.bytesAvailable() / 1000 / 1000;
        qCDebug(logEngine).noquote() << mbAvailable << "MB available in data export location";
        // TODO: Make the warning level configurable in global settings
        if (mbAvailable < 8000) {
            auto reply = QMessageBox::question(d->parentWidget,
                                               QStringLiteral("Disk is almost full - Continue anyway?"),
                                               QStringLiteral("The disk '%1' is located on has low amounts of space available (< 8 GB). "
                                                              "If this run generates more data than we have space for, it will fail (possibly corrupting data). Continue anyway?")
                                                              .arg(d->exportBaseDir),
                                               QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::No)
                return false;
        }
    } else {
        QMessageBox::critical(d->parentWidget,
                              QStringLiteral("Disk not ready"),
                              QStringLiteral("The disk device at '%1' is either invalid (not mounted) or not ready for operation. Can not continue.").arg(d->exportBaseDir));
        return false;
    }

    // safeguard against accidental data removals
    QDir deDir(d->exportDir);
    if (deDir.exists()) {
        auto reply = QMessageBox::question(d->parentWidget,
                                           QStringLiteral("Existing data found - Continue anyway?"),
                                           QStringLiteral("The directory '%1' already contains data (likely from a previous run). "
                                                          "If you continue, the old data will be deleted. Continue and delete data?")
                                                          .arg(d->exportDir),
                                           QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No)
            return false;

        emitStatusMessage(QStringLiteral("Removing data from an old run..."));
        deDir.removeRecursively();
    }

    // perform the actual run, now that all error checking is done
    return runInternal(d->exportDir);
}

bool Engine::runEphemeral()
{
    if (d->running)
        return false;

    d->failed = true; // if we exit before this is reset, initialization has failed
    if (d->activeModules.isEmpty()) {
        QMessageBox::warning(d->parentWidget,
                             QStringLiteral("Configuration error"),
                             QStringLiteral("You did not add a single module to be run.\nPlease add a module to the board to continue."));
        return false;
    }

    QTemporaryDir tempDir(QStringLiteral("%1/syntalos-tmprun-XXXXXX").arg(QDir::tempPath()));
    if (!tempDir.isValid()) {
        QMessageBox::warning(d->parentWidget,
                             QStringLiteral("Unable to run"),
                             QStringLiteral("Unable to perform ephemeral run: Temporary data storage could not be created. %s").arg(tempDir.errorString()));
        return false;
    }

    qCDebug(logEngine, "Initializing new ephemeral recording run");

    auto tempExportDir = tempDir.filePath("edl");

    // perform the actual run, in a temporary directory
    auto ret = runInternal(tempExportDir);

    qCDebug(logEngine, "Removing temporary storage directory");
    if (!tempDir.remove())
        qCDebug(logEngine, "Unable to remove temporary directory: %s", qPrintable(tempDir.errorString()));

    if (ret)
        qCDebug(logEngine, "Ephemeral run completed (result: success)");
    else
        qCDebug(logEngine, "Ephemeral run completed (result: failure)");
    return ret;
}

/**
 * @brief Actually run an experiment module board
 * @return true on succees
 *
 * This function runs an experiment with the given path,
 * doing *no* error checking on the data export path anymore.
 * It may never be called from anything but internal engine functions.
 */
bool Engine::runInternal(const QString &exportDirPath)
{
    QDir edlDir(exportDirPath);
    if (edlDir.exists()) {
        QMessageBox::critical(d->parentWidget,
                             QStringLiteral("Internal Error"),
                             QStringLiteral("Directory '%1' was expected to be nonexistent, but the directory exists. "
                                            "Stopped run to prevent potential data loss. This condition should never happen.").arg(exportDirPath));
        return false;
    }

    if (!makeDirectory(exportDirPath))
        return false;

    // the engine is actively doing stuff with modules now
    d->active = true;

    // reset failure reason, in case one was set from a previous run
    d->runFailedReason = QString();

    // tell listeners that we are preparing a run
    emit preRunStart();

    // cache default thread RT and niceness values
    const auto defaultThreadNice = d->gconf->defaultThreadNice();
    const auto defaultRTPriority = d->gconf->defaultRTThreadPriority();

    // cache number of online CPUs
    const auto cpuCoreCount = get_online_cores_count();

    // set main thread niceness for the current run
    setCurrentThreadNiceness(defaultThreadNice);

    // set CPU core affinities base setting
    const auto explicitCoreAffinities = d->gconf->explicitCoreAffinities();
    if (explicitCoreAffinities) {
        qCDebug(logEngine).noquote().nospace() << "Explicit CPU core affinity is enabled.";
        // tie main thread to first CPU
        thread_set_affinity(pthread_self(), 0);
    } else {
        qCDebug(logEngine).noquote().nospace() << "Explicit CPU core affinity is disabled.";
    }

    // create new experiment directory layout (EDL) collection to store
    // all data modules generate in
    std::shared_ptr<EDLCollection> storageCollection(new EDLCollection(QStringLiteral("%1_%2_%3")
                                                                       .arg(d->testSubject.id)
                                                                       .arg(d->experimentId)
                                                                       .arg(QDateTime::currentDateTime().toString("yy-MM-dd+hh.mm"))));
    storageCollection->setPath(exportDirPath);

    // if we should save internal diagnostic data, create a group for it!
    if (d->saveInternal) {
        d->edlInternalData = std::make_shared<EDLGroup>();
        d->edlInternalData->setName("syntalos_internal");
        storageCollection->addChild(d->edlInternalData);
        qCDebug(logEngine).noquote().nospace() << "Writing some internal data to datasets for debugging and analysis";
    }
    d->internalTSyncWriters.clear();

    // fetch list of modules in their activation order
    auto orderedActiveModules = createModuleExecOrderList();

    // create a new master timer for synchronization
    d->timer.reset(new SyncTimer);

    auto lastPhaseTimepoint = currentTimePoint();
    // assume success until a module actually fails
    bool initSuccessful = true;
    d->failed = false;

    // perform module name sanity check
    {
        QSet<QString> modNameSet;
        for (auto &mod : orderedActiveModules) {
            const auto expectedName = simplifyStrForModuleName(mod->name());
            if (mod->name() != expectedName) {
                qCWarning(logEngine).noquote() << "Module" << mod->name() << "has invalid name. Expected:" << expectedName << "(The module has been renamed)";
                mod->setName(expectedName);
            }

            const auto uniqName = simplifyStrForFileBasenameLower(mod->name());
            if (modNameSet.contains(uniqName)) {
                QMessageBox::critical(d->parentWidget,
                                      QStringLiteral("Can not run this board"),
                                      QStringLiteral("A module with the name '%1' exists twice in this board, or another module has a very similar name. "
                                                     "Please give the duplicate a unique name in order to execute this board.").arg(mod->name()));
                d->active = false;
                d->failed = true;
                return false;
            }
            modNameSet.insert(uniqName);
        }
    }

    // the dedicated threads our modules run in, references owned by the vector
    std::vector<std::thread> dThreads;
    QList<AbstractModule*> threadedModules;

    // special event threads and their assigned modules, with a specific identifier string as hash key
    QHash<QString, QList<AbstractModule*>> eventModules;
    QHash<QString, std::shared_ptr<ModuleEventThread>> evThreads;

    // out-of-process modules need a thread to handle communication in the master application, so
    // we provide one here (and possibly more in future in case this doesn't scale well).
    QList<OOPModule*> oopModules;
    std::vector<std::thread> oopThreads;

    // filter out dedicated-thread modules, those get special treatment
    for (auto &mod : orderedActiveModules) {
        auto oopMod = qobject_cast<OOPModule*>(mod);
        if (oopMod != nullptr) {
            oopModules.append(oopMod);
            continue;
        }

        if (mod->driver() == ModuleDriverKind::THREAD_DEDICATED)
            threadedModules.append(mod);
    }
    const auto threadedModulesTotalN = threadedModules.size() + oopModules.size();

    // give modules a hint as to how many CPU cores they themselves may use additionally
    uint potentialNoaffinityCPUCount = 0;
    if (threadedModulesTotalN <= (cpuCoreCount - 1))
        potentialNoaffinityCPUCount = cpuCoreCount - threadedModulesTotalN - 1;
    qCDebug(logEngine).noquote() << "Predicted amount of CPU cores with no (explicitly known) occupation:" << potentialNoaffinityCPUCount;
    for (auto &mod : orderedActiveModules)
        mod->setPotentialNoaffinityCPUCount(potentialNoaffinityCPUCount);

    QCoreApplication::processEvents();

    // prepare modules
    for (auto &mod : orderedActiveModules) {
        // Prepare module. At this point it should have a timer,
        // the location where data is saved and be in the PREPARING state.
        emitStatusMessage(QStringLiteral("Preparing '%1'...").arg(mod->name()));
        lastPhaseTimepoint = currentTimePoint();

        const auto modInfo = d->modLibrary->moduleInfo(mod->id());

        mod->setStatusMessage(QString());
        mod->setTimer(d->timer);
        mod->setState(ModuleState::PREPARING);

        mod->setSimpleStorageNames(d->simpleStorageNames);
        if ((modInfo != nullptr) && (!modInfo->storageGroupName().isEmpty())) {
            auto storageGroup = storageCollection->groupByName(modInfo->storageGroupName(), true);
            if (storageGroup.get() == nullptr) {
                qCCritical(logEngine) << "Unable to create data storage group with name" << modInfo->storageGroupName();
                mod->setStorageGroup(storageCollection);
            } else {
                mod->setStorageGroup(storageGroup);
            }
        } else {
            mod->setStorageGroup(storageCollection);
        }

        if (!mod->prepare(d->testSubject)) {
            initSuccessful = false;
            d->failed = true;
            d->runFailedReason = QStringLiteral("Prepare step failed for: %1(%2)").arg(mod->id()).arg(mod->name());
            emitStatusMessage(QStringLiteral("Module '%1' failed to prepare.").arg(mod->name()));
            break;
        }
        // if the module hasn't set itself to ready yet, assume it is idle
        if (mod->state() != ModuleState::READY)
            mod->setState(ModuleState::IDLE);

        qCDebug(logEngine).noquote().nospace() << "Module '" << mod->name() << "' prepared in " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
    }

    // wait condition for all threads to block them until we have actually started (or not block them, in case
    // the thread was really slow to initialize and we are already running)
    std::unique_ptr<OptionalWaitCondition> startWaitCondition(new OptionalWaitCondition());

    // Only actually launch if preparation didn't fail.
    // we still call stop() on all modules afterwards though,
    // as some might need a stop call to clean up resources ther were
    // set up during preparations.
    // Modules are expected to deal with multiple calls to stop().
    if (initSuccessful) {
        emitStatusMessage(QStringLiteral("Initializing launch..."));
        lastPhaseTimepoint = currentTimePoint();

        // prepare pinning threads to CPU cores
        QHash<AbstractModule*, std::vector<uint>> modCPUMap;
        if (explicitCoreAffinities) {
            if (threadedModulesTotalN <= (cpuCoreCount - 1)) {
                // we have enough cores and can tie each thread to a dedicated core, to (ideally) prevent
                // the scheduler from moving them around between CPUs too much once they go idle
                auto availableCores = cpuCoreCount - 1; // all cores minus the one our main thread is running on

                for (auto &mod : threadedModules) {
                    if (availableCores > 0) {
                        modCPUMap[mod] = std::vector<uint>{(uint)availableCores};
                        availableCores--;
                    }
                }
                for (auto &mod : oopModules) {
                    if (availableCores > 0) {
                        modCPUMap[mod] = std::vector<uint>{(uint)availableCores};
                        availableCores--;
                    }
                }
            } else {
                // we don't have enough cores - in this case, prefer modules which requested to be run on a dedicated core
                // OOP modules will get their own core if at all possible in a sensible way
                auto availableCores = cpuCoreCount - 1;
                for (auto &mod : threadedModules) {
                    if (!mod->features().testFlag(ModuleFeature::CORE_AFFINITY))
                        continue;
                    if (availableCores > 0) {
                        modCPUMap[mod] = std::vector<uint>{(uint)availableCores};
                        availableCores--;
                    }
                }

                bool oopDedicatedThreads = false;
                if (availableCores > 0) {
                    // give OOP modules their own core if at least two remain
                    if ((availableCores - oopModules.size()) >= 2) {
                        for (auto &mod : oopModules) {
                            if (availableCores > 0) {
                                modCPUMap[mod] = std::vector<uint>{(uint)availableCores};
                                availableCores--;
                            }
                        }
                        oopDedicatedThreads = true;
                    }
                }

                // give the remaining cores to other modules
                std::vector<uint> remainingCores;
                for (uint i = availableCores; i > 0; i--)
                    remainingCores.push_back(i);
                for (auto &mod : threadedModules) {
                    if (modCPUMap.contains(mod))
                        continue;
                    modCPUMap[mod] = remainingCores;
                }
                // treat OOP modules the same if we are low on threads
                if (!oopDedicatedThreads) {
                    for (auto &mod : oopModules) {
                        if (modCPUMap.contains(mod))
                            continue;
                        modCPUMap[mod] = remainingCores;
                    }
                }
            }
        }

        // only emit a resource warning if we are using way more threads than we probably should
        if (threadedModulesTotalN > (cpuCoreCount + (cpuCoreCount / 2)))
            Q_EMIT resourceWarning(CpuCores, false, QStringLiteral("Likely not enough CPU cores available for optimal operation."));

        // launch threads for threaded modules, except for out out-of-process
        // modules - they get special treatment
        for (int i = 0; i < threadedModules.size(); i++) {
            auto mod = threadedModules[i];

            // we are preparing again, this time for threading!
            // this is important, as we will only start when the module
            // signalled that it is ready now.
            mod->setState(ModuleState::PREPARING);

            ThreadDetails td;
            td.niceness = defaultThreadNice;
            td.allowedRTPriority = defaultRTPriority;

            if (modCPUMap.contains(mod)) {
                td.cpuAffinity = modCPUMap[mod];
                std::ostringstream oss;
                std::copy(td.cpuAffinity.begin(), td.cpuAffinity.end() - 1, std::ostream_iterator<uint>(oss, ","));
                oss << td.cpuAffinity.back();

                qCDebug(logEngine).noquote().nospace() << "Module '" << mod->name() << "' thread will prefer CPU core(s) " << QString::fromStdString(oss.str());
            }

            // the thread name shouldn't be longer than 16 chars (inlcuding NULL)
            td.name = QStringLiteral("%1-%2").arg(mod->id().midRef(0, 12)).arg(i);
            dThreads.push_back(std::thread(executeModuleThread,
                                           td,
                                           mod,
                                           startWaitCondition.get()));
        }
        assert(dThreads.size() == (size_t)threadedModules.size());

        // collect all modules which do some kind of event-based execution
        {
            QHash<QString, int> remainingEvModCountById;
            for (auto &mod : orderedActiveModules) {
                if ((mod->driver() == ModuleDriverKind::EVENTS_SHARED) ||
                    (mod->driver() == ModuleDriverKind::EVENTS_DEDICATED)) {
                    if (!remainingEvModCountById.contains(mod->id()))
                        remainingEvModCountById[mod->id()] = 0;
                    remainingEvModCountById[mod->id()] += 1;
                }
            }

            // assign modules to their threads and give the groups an ID
            for (auto &mod : orderedActiveModules) {
                QString evGroupId;
                if (mod->driver() == ModuleDriverKind::EVENTS_SHARED) {
                    evGroupId = QStringLiteral("shared_0");
                } else if (mod->driver() == ModuleDriverKind::EVENTS_DEDICATED) {
                    if (mod->eventsMaxModulesPerThread() <= 0) {
                        evGroupId = QStringLiteral("m:%1").arg(mod->id());
                        remainingEvModCountById[mod->id()] -= 1;
                    } else {
                        // reduce number first to get "0 / eventsMaxModulesPerThread" last
                        remainingEvModCountById[mod->id()] -= 1;
                        int evGroupPerModIdx = static_cast<int>(remainingEvModCountById[mod->id()] / mod->eventsMaxModulesPerThread());
                        evGroupId = QStringLiteral("m:%1_%2").arg(mod->id(), evGroupPerModIdx);
                    }
                } else {
                    // not an event-driven module
                    continue;
                }

                // add module to hash map
                if (!eventModules.contains(evGroupId))
                    eventModules[evGroupId] = QList<AbstractModule*>();
                eventModules[evGroupId].append(mod);
            }
        }

        // prepare out-of-process modules
        // NOTE: We currently throw them all into one thread, which may not
        // be the most performant thing to do if there are a lot of OOP modules.
        // But let's address that case when we actually run into performance issues
        if (!oopModules.isEmpty()) {
            for (auto &mod : oopModules)
                mod->setState(ModuleState::PREPARING);

            ThreadDetails td;
            td.niceness = defaultThreadNice;
            td.allowedRTPriority = defaultRTPriority;
            td.name = QStringLiteral("oopc:shared");

            if (modCPUMap.contains(oopModules[0])) {
                td.cpuAffinity = modCPUMap[oopModules[0]];
                std::ostringstream oss;
                std::copy(td.cpuAffinity.begin(), td.cpuAffinity.end() - 1, std::ostream_iterator<uint>(oss, ","));
                oss << td.cpuAffinity.back();

                qCDebug(logEngine).noquote().nospace() << "OOP thread '" << td.name << "' will prefer CPU core(s) " << QString::fromStdString(oss.str());
            }

            oopThreads.push_back(std::thread(executeOOPModuleThread,
                                             td,
                                             oopModules,
                                             startWaitCondition.get(),
                                             std::ref(d->running)));
        }

        // run special threads with built-in event loops for modules that selected an event-based driver
        for (const auto evThreadKey : eventModules.keys()) {
            std::shared_ptr<ModuleEventThread> evThread(new ModuleEventThread(evThreadKey));
            evThread->run(eventModules[evThreadKey], startWaitCondition.get());
            evThreads[evThreadKey] = evThread;
            qCDebug(logEngine).noquote().nospace() << "Started event thread '" << evThreadKey << "' with " << eventModules[evThreadKey].length() << " participating modules";
        }

        qCDebug(logEngine).noquote().nospace() << "Module and engine threads created in " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
        lastPhaseTimepoint = currentTimePoint();

        // ensure all modules are in the READY state
        // (modules may take a bit of time to prepare their threads)
        // FIXME: Maybe add a timeout on this, in case a module doesn't
        // behave and never ever leaves its preparation phase?
        for (auto &mod : orderedActiveModules) {
            if (mod->state() == ModuleState::READY)
                continue;
            // IDLE is also a valid state at this point, the module may not
            // have had additional setup to do
            if (mod->state() == ModuleState::IDLE)
                continue;
            emitStatusMessage(QStringLiteral("Waiting for '%1' to get ready...").arg(mod->name()));
            while (mod->state() != ModuleState::READY) {
                QThread::msleep(500);
                QCoreApplication::processEvents();
                if (mod->state() == ModuleState::ERROR) {
                    emitStatusMessage(QStringLiteral("Module '%1' failed to initialize.").arg(mod->name()));
                    initSuccessful = false;
                    break;
                }
                if (d->failed) {
                    // we failed elsewhere
                    initSuccessful = false;
                    break;
                }
            }
        }

        qCDebug(logEngine).noquote().nospace() << "Waited for modules to get ready for " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
    }

    // Meanwhile, threaded modules may have failed, so let's check again if we are still
    // good on initialization
    if (initSuccessful) {
        emitStatusMessage(QStringLiteral("Launch setup completed."));

        // set up resource watchers

        // watcher for disk space
        bool diskSpaceWarningEmitted = false;
        QTimer diskSpaceCheckTimer;
        diskSpaceCheckTimer.setInterval(60 * 1000); // check every 60sec
        connect(&diskSpaceCheckTimer, &QTimer::timeout, [&]() {
            std::filesystem::space_info ssi;
            try {
                ssi = std::filesystem::space(d->exportBaseDir.toStdString());
            } catch (const std::filesystem::filesystem_error &e) {
                qCWarning(logEngine).noquote() << "Could not determine remaining free disk space:" << e.what();
                return;
            }

            const double mibAvailable = ssi.available / 1024.0 / 1024.0;
            if (mibAvailable < 8192) {
                Q_EMIT resourceWarning(StorageSpace, false, QStringLiteral("Disk space is very low. Less than %1 GiB remaining.").arg(mibAvailable / 1024.0, 0, 'f', 1));
                diskSpaceWarningEmitted = true;
            } else {
                if (diskSpaceWarningEmitted) {
                    Q_EMIT resourceWarning(StorageSpace, true, QStringLiteral("%1 GiB of disk space remaining.").arg(mibAvailable / 1024.0, 0, 'f', 1));
                    diskSpaceWarningEmitted = false;
                }
            }
        });

        // watcher for remaining system memory
        bool memoryWarningEmitted = false;
        QTimer memCheckTimer;
        memCheckTimer.setInterval(10 * 1000); // check every 10sec
        connect(&memCheckTimer, &QTimer::timeout, [&]() {
            const auto memInfo = read_meminfo();
            if (memInfo.memAvailablePercent < 5) {
                // when we have less than 5% memory remaining, there usually still is (slower) swap space available,
                // this is why 5% is relatively low.
                // NOTE: Be more clever here in future and check available swap space in advance for this warning?
                Q_EMIT resourceWarning(Memory, false, QStringLiteral("System memory is low. Only %1% remaining.").arg(memInfo.memAvailablePercent, 0, 'f', 1));
                memoryWarningEmitted = true;
            } else {
                if (memoryWarningEmitted) {
                    Q_EMIT resourceWarning(Memory, true, QStringLiteral("%1% of system memory remaining.").arg(memInfo.memAvailablePercent, 0, 'f', 1));
                    memoryWarningEmitted = true;
                }
            }
        });

        // watcher for subscription buffer
        std::vector<std::shared_ptr<VariantStreamSubscription>> monitoredSubscriptions;
        for (auto& mod : orderedActiveModules) {
            for (auto &port : mod->inPorts()) {
                if (!port->hasSubscription())
                    continue;
                monitoredSubscriptions.push_back(port->subscriptionVar());
            }
        }
        bool subBufferWarningEmitted = false;
        QTimer subBufferCheckTimer;
        subBufferCheckTimer.setInterval(10 * 1000); // check every 10sec
        connect(&subBufferCheckTimer, &QTimer::timeout, [&]() {
            bool issueFound = false;
            for (auto& sub : monitoredSubscriptions) {
                if (sub->approxPendingCount() > 100) {
                    Q_EMIT resourceWarning(StreamBuffers, false, QStringLiteral("A module is overwhelmed with its input and not fast enough."));
                    subBufferWarningEmitted = true;
                    issueFound = true;
                    break;
                }
            }

            if (!issueFound && subBufferWarningEmitted)
                Q_EMIT resourceWarning(StreamBuffers, false, QStringLiteral("All modules appear to run fast enough."));
        });

        // start resource watchers
        diskSpaceCheckTimer.start();
        memCheckTimer.start();
        subBufferCheckTimer.start();

        // we officially start now, launch the timer
        d->timer->start();
        d->running = true;

        // first, launch all threaded and evented modules
        for (auto& mod : orderedActiveModules) {
            if ((mod->driver() != ModuleDriverKind::THREAD_DEDICATED) &&
                (mod->driver() != ModuleDriverKind::EVENTS_DEDICATED) &&
                (mod->driver() != ModuleDriverKind::EVENTS_SHARED))
                continue;

            mod->start();

            // ensure modules are in their "running" state now, or
            // have themselves declared "idle" (meaning they won't be used at all)
            mod->m_running = true;
            if (mod->state() != ModuleState::IDLE)
                mod->setState(ModuleState::RUNNING);
        }

        // wake that thundering herd and hope all threaded modules awoken by the
        // start signal behave properly
        // (Threads *must* only be unlocked after we've sent start() to the modules, as they
        // may prepare stuff in start() that the threads need, like timestamp syncs)
        startWaitCondition->wakeAll();

        qCDebug(logEngine).noquote().nospace() << "Threaded/evented module startup completed, took " << d->timer->timeSinceStartMsec().count() << "msec";
        lastPhaseTimepoint = d->timer->currentTimePoint();

        // tell all non-threaded modules individuall now that we started
        for (auto& mod : orderedActiveModules) {
            // ignore threaded & evented
            if ((mod->driver() == ModuleDriverKind::THREAD_DEDICATED) ||
                (mod->driver() == ModuleDriverKind::EVENTS_DEDICATED) ||
                (mod->driver() == ModuleDriverKind::EVENTS_SHARED))
                continue;

            mod->start();

            // work around bad modules which don't set this on their own in start()
            mod->m_running = true;
            mod->setState(ModuleState::RUNNING);
        }

        qCDebug(logEngine).noquote().nospace() << "Startup phase completed, all modules are running. Took additional " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";

        // tell listeners that we are running now
        emit runStarted();

        emitStatusMessage(QStringLiteral("Running..."));

        // run the main loop and process UI events
        // modules may have injected themselves into the UI event loop
        // as well via QTimer callbacks, in case they need to modify UI elements.
        while (d->running) {
            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
            if (d->failed)
                break;
        }

        // stop resource watcher timers
        diskSpaceCheckTimer.stop();
        memCheckTimer.stop();
    }

    auto finishTimestamp = static_cast<long long>(d->timer->timeSinceStartMsec().count());
    emitStatusMessage(QStringLiteral("Run stopped, finalizing..."));

    // Wake all threads again if we have failed, because some module may have
    // failed so early that other modules may not even have made it through their
    // startup phase, and in this case are stuck waiting.
    // We wake threads again later shortly before joining them (just in case),
    // so you may thing this early wakeup call isn't necessary.
    // Some modules though may actually wait for the thread to go down first
    // (by setting m_running to false) and wait on that event in their stop() function.
    // And this won't ever happen in case the thread is still idling on the start wait condition.
    // So we set every module that has its own thread to "not running" and then ring the wakeup bell
    if (d->failed) {
        for (auto &mod : threadedModules)
            mod->m_running = false;
        startWaitCondition->wakeAll();
    }

    // join all threads running evented modules, therefore stop
    // processing any new events
    lastPhaseTimepoint = d->timer->currentTimePoint();
    for (const auto &evThread : evThreads.values()) {
        emitStatusMessage(QStringLiteral("Waiting for event thread `%1`...").arg(evThread->threadName()));
        evThread->stop();
    }
    qCDebug(logEngine).noquote().nospace() << "Waited " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec for event threads to stop.";

    // send stop command to all modules
    for (auto &mod : createModuleStopOrderFromExecOrder(orderedActiveModules)) {
        emitStatusMessage(QStringLiteral("Stopping '%1'...").arg(mod->name()));
        lastPhaseTimepoint = d->timer->currentTimePoint();

        // wait a little bit for modules to process remaining data from their
        // stream subscriptions - we don't wait too long here, simply because
        // the upstream module may still be generating data (and in that case
        // we would never be able to stop, especially if there are cycles in
        // the module graph
        for (const auto &iport : mod->inPorts()) {
            if (!iport->hasSubscription())
                continue;

            // give the module 1.2sec to clear pending elements for this subscription
            const auto startPortWaitTS = d->timer->timeSinceStartMsec();
            size_t remainingElements = 0;
            do {
                remainingElements = iport->subscriptionVar()->approxPendingCount();
                if (remainingElements == 0)
                    break;
                qApp->processEvents();
            } while ((d->timer->timeSinceStartMsec() - startPortWaitTS).count() <= 1600);

            if (remainingElements != 0)
                qCDebug(logEngine).noquote().nospace() << "Module '" << mod->name() << "' "
                                             << "subscription `" << iport->id() << "` "
                                             << "possibly lost " << remainingElements << " element(s)";
        }

        // send the stop command
        mod->stop();
        QCoreApplication::processEvents();

        // safeguard against bad modules which don't stop running their
        // thread loops on their own
        mod->m_running = false;

        // ensure modules really have terminated all their outgoing streams,
        // because if they didn't do that, connected modules may not be able to exit
        for (auto const port : mod->outPorts())
            port->stopStream();

        // ensure modules display the correct state after we stopped a run
        if ((mod->state() != ModuleState::IDLE) && (mod->state() != ModuleState::ERROR))
            mod->setState(ModuleState::IDLE);

        // all module data must be written by this point, so we "steal" its storage group,
        // so the module will trigger an error message if is still tries to access the final
        // data.
        mod->setStorageGroup(nullptr);

        qCDebug(logEngine).noquote().nospace() << "Module '" << mod->name() << "' stopped in " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
    }

    lastPhaseTimepoint = d->timer->currentTimePoint();

    // join all dedicated module threads with the main thread again, waiting for them to terminate
    startWaitCondition->wakeAll(); // wake up all threads again, just in case one is stuck waiting
    for (size_t i = 0; i < dThreads.size(); i++) {
        auto &thread = dThreads[i];
        auto mod = threadedModules[i];
        emitStatusMessage(QStringLiteral("Waiting for '%1'...").arg(mod->name()));
        qApp->processEvents();
        thread.join();
    }

    // join all out-of-process module communication threads
    for (auto &oopThread : oopThreads) {
        emitStatusMessage(QStringLiteral("Waiting for external processes and their relays..."));
        qApp->processEvents();
        oopThread.join();
    }

    qCDebug(logEngine).noquote().nospace() << "All (non-event) engine threads joined in " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
    lastPhaseTimepoint = d->timer->currentTimePoint();

    if (d->saveInternal) {
        emitStatusMessage(QStringLiteral("Finalizing internal dataset..."));
        for (auto &tsw : d->internalTSyncWriters.values())
            tsw->close();
    }

    if (!initSuccessful) {
        // if we failed to prepare this run, don't save the manifest and also
        // remove any data that we might have already created, as well as the
        // export directory.
        emitStatusMessage(QStringLiteral("Removing broken data..."));
        edlDir.removeRecursively();
    } else {
        emitStatusMessage(QStringLiteral("Writing experiment metadata..."));

        // write collection metadata with information about this experiment
        storageCollection->setTimeCreated(QDateTime::currentDateTime());

        storageCollection->setGeneratorId(QStringLiteral("%1 %2")
                                          .arg(QCoreApplication::applicationName())
                                          .arg(QCoreApplication::applicationVersion()));
        if (d->experimenter.isValid())
            storageCollection->addAuthor(d->experimenter);

        QVariantHash extraData;
        extraData.insert("subject_id", d->testSubject.id.isEmpty()? QVariant() : d->testSubject.id);
        extraData.insert("subject_group", d->testSubject.group.isEmpty()? QVariant() : d->testSubject.group);
        extraData.insert("subject_comment", d->testSubject.comment.isEmpty()? QVariant() : d->testSubject.comment);
        extraData.insert("recording_length_msec", finishTimestamp);
        extraData.insert("success", !d->failed);
        if (d->failed && !d->runFailedReason.isEmpty()) {
            extraData.insert("failure_reason", d->runFailedReason);
        }
        extraData.insert("machine_node", QStringLiteral("%1 [%2 %3]").arg(d->sysInfo->machineHostName())
                                                                     .arg(d->sysInfo->osType())
                                                                     .arg(d->sysInfo->osVersion()));

        QVariantList attrModList;
        for (auto &mod : orderedActiveModules) {
            QVariantHash info;
            info.insert(QStringLiteral("id"), mod->id());
            info.insert(QStringLiteral("name"), mod->name());
            attrModList.append(info);
        }
        extraData.insert("modules", attrModList);
        storageCollection->setAttributes(extraData);

        qCDebug(logEngine) << "Saving experiment metadata in:" << storageCollection->path();

        if (!storageCollection->save()) {
            QMessageBox::critical(d->parentWidget,
                                  QStringLiteral("Unable to finish recording"),
                                  QStringLiteral("Unable to save experiment metadata: %1").arg(storageCollection->lastError()));
            d->failed = true;
        }

        qCDebug(logEngine).noquote().nospace() << "Manifest and additional data saved in " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
    }

    // reset main thread niceness, we are not important anymore of no experiment is running
    setCurrentThreadNiceness(0);

    // clear main thread CPU affinity
    thread_clear_affinity(pthread_self());

    // we have stopped doing things with modules
    d->active = false;

    // tell listeners that we are stopped now
    emit runStopped();

    emitStatusMessage(QStringLiteral("Ready."));
    return true;
}

void Engine::stop()
{
    d->running = false;
}

void Engine::receiveModuleError(const QString& message)
{
    auto mod = qobject_cast<AbstractModule*>(sender());
    if (mod != nullptr)
        d->runFailedReason = QStringLiteral("%1(%2): %3").arg(mod->id()).arg(mod->name()).arg(message);
    else
        d->runFailedReason = QStringLiteral("?(?): %1").arg(message);

    const bool wasRunning = d->running;
    d->failed = true;
    d->running = false;
    if (mod != nullptr)
        emit moduleError(mod, message);

    if (wasRunning)
        emit runFailed(mod, message);
}

void Engine::onSynchronizerDetailsChanged(const QString &id, const TimeSyncStrategies &, const microseconds_t &)
{
    if (!d->saveInternal)
        return;
    if (d->internalTSyncWriters.value(id).get() != nullptr)
        return;
    QString modId;
    auto mod = qobject_cast<AbstractModule*>(sender());
    if (mod != nullptr)
        modId = mod->id();

    std::shared_ptr<EDLDataset> ds(new EDLDataset);
    ds->setName(QStringLiteral("%1-%2").arg(modId).arg(id));
    d->edlInternalData->addChild(ds);

    std::shared_ptr<TimeSyncFileWriter> tsw(new TimeSyncFileWriter);
    tsw->setFileName(ds->setDataFile("offsets.tsync"));
    tsw->setTimeUnits(TSyncFileTimeUnit::MICROSECONDS, TSyncFileTimeUnit::MICROSECONDS);
    tsw->setTimeDataTypes(TSyncFileDataType::INT64, TSyncFileDataType::INT64);
    tsw->setTimeNames(QStringLiteral("approx-master-time"), QStringLiteral("sync-offset"));
    tsw->open(QStringLiteral("SyntalosInternal::%1").arg(modId).arg(mod->name()), ds->collectionId());
    d->internalTSyncWriters[id] = tsw;
}

void Engine::onSynchronizerOffsetChanged(const QString &id, const microseconds_t &currentOffset)
{
    if (!d->saveInternal)
        return;

    auto tsw = d->internalTSyncWriters.value(id);
    if (tsw.get() == nullptr)
        return;

    tsw->writeTimes(d->timer->timeSinceStartUsec(), currentOffset);
}

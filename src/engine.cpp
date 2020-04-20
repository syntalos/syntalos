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
#include <QTemporaryDir>

#include "oop/oopmodule.h"
#include "edlstorage.h"
#include "modulelibrary.h"
#include "moduleeventthread.h"
#include "syclock.h"
#include "sysinfo.h"
#include "utils.h"

using namespace Syntalos;

#pragma GCC diagnostic ignored "-Wpadded"
class Engine::Private
{
public:
    Private() { }
    ~Private() { }

    SysInfo *sysInfo;
    QWidget *parentWidget;
    QList<AbstractModule*> activeModules;
    ModuleLibrary *modLibrary;
    std::shared_ptr<SyncTimer> timer;

    TestSubject testSubject;
    QString exportBaseDir;
    QString exportDir;
    bool exportDirIsTempDir;
    bool exportDirIsValid;

    QString experimentId;

    std::atomic_bool running;
    std::atomic_bool failed;
    QString runFailedReason;
};
#pragma GCC diagnostic pop

Engine::Engine(QWidget *parentWidget)
    : QObject(parentWidget),
      d(new Engine::Private)
{
    d->sysInfo = new SysInfo(this);
    d->exportDirIsValid = false;
    d->running = false;
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

QString Engine::exportDir() const
{
    return d->exportDir;
}

bool Engine::isRunning() const
{
    return d->running;
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
            mod->setName(modInfo->name());
    } else {
        mod->setName(name);
    }

    d->activeModules.append(mod);
    emit moduleCreated(modInfo.get(), mod);

    // the module has been created and registered, we can
    // safely initialize it now.
    mod->setState(ModuleState::INITIALIZING);
    QCoreApplication::processEvents();
    if (!mod->initialize()) {
        QMessageBox::critical(d->parentWidget, QStringLiteral("Module initialization failed"),
                              QStringLiteral("Failed to initialize module %1, it can not be added. Message: %2").arg(mod->id()).arg(mod->lastError()),
                              QMessageBox::Ok);
        removeModule(mod);
        return nullptr;
    }

    // now listen to errors emitted by this module
    connect(mod, &AbstractModule::error, this, &Engine::receiveModuleError);

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
    qDebug().noquote() << "Engine:" << message;
    emit statusMessage(message);
}

/**
 * @brief Return a list of active modules that have been sorted in the order they
 * should be prepared, run and stopped in.
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
        qCritical().noquote() << "Invalid count of ordered modules:" << orderedActiveModules.length() << "!=" << modCount;
    assert(orderedActiveModules.length() == modCount);

    auto debugText = QStringLiteral("Running modules in order: ");
    for (auto &mod : orderedActiveModules)
        debugText.append(mod->name() + QStringLiteral("; "));
    qDebug().noquote() << debugText;

    return orderedActiveModules;
}

/**
 * @brief Main entry point for engine-managed module threads.
 */
static void executeModuleThread(const QString& threadName, AbstractModule *mod, OptionalWaitCondition *waitCondition)
{
    pthread_setname_np(pthread_self(), qPrintable(threadName.mid(0, 15)));

    mod->runThread(waitCondition);
}

/**
 * @brief Main entry point for threads used to manage out-of-process worker modules.
 */
static void executeOOPModuleThread(const QString& threadName, QList<OOPModule*> mods,
                                   OptionalWaitCondition *waitCondition, std::atomic_bool &running)
{
    pthread_setname_np(pthread_self(), qPrintable(threadName.mid(0, 15)));

    QEventLoop loop;

    // prepare all OOP modules in their new thread
    {
        QList<OOPModule*> readyMods;
        for (auto &mod : mods) {
            if (!mod->oopPrepare(&loop)) {
                // deininitialize modules we already have prepared
                for (auto &reMod : readyMods)
                    reMod->oopFinalize(&loop);
                mod->oopFinalize(&loop);

                qDebug().noquote().noquote().nospace() << "Failed to prepare OOP module " << mod->name() << ": " << mod->lastError();
                return;
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
    qDebug("Initializing new persistent recording run");

    // test for available disk space and readyness of device
    QStorageInfo storageInfo(d->exportBaseDir);
    if (storageInfo.isValid() && storageInfo.isReady()) {
        auto mbAvailable = storageInfo.bytesAvailable() / 1000 / 1000;
        qDebug().noquote() << mbAvailable << "MB available in data export location";
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

    qDebug("Initializing new ephemeral recording run");

    auto tempExportDir = tempDir.filePath("edl");

    // perform the actual run, in a temporary directory
    auto ret = runInternal(tempExportDir);

    qDebug("Removing temporary storage directory");
    if (!tempDir.remove())
        qDebug("Unable to remove temporary directory: %s", qPrintable(tempDir.errorString()));

    if (ret)
        qDebug("Ephemeral run completed (result: success)");
    else
        qDebug("Ephemeral run completed (result: failure)");
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

    // reset failure reason, in case one was set from a previous run
    d->runFailedReason = QString();

    // tell listeners that we are preparing a run
    emit preRunStart();

    // create new experiment directory layout (EDL) collection to store
    // all data modules generate in
    std::shared_ptr<EDLCollection> storageCollection(new EDLCollection(QStringLiteral("%1_%2_%3")
                                                                       .arg(d->testSubject.id)
                                                                       .arg(d->experimentId)
                                                                       .arg(QDateTime::currentDateTime().toString("yy-MM-dd+hh.mm"))));
    storageCollection->setPath(exportDirPath);

    // fetch list of modules in their activation order
    auto orderedActiveModules = createModuleExecOrderList();

    // create a new master timer for synchronization
    d->timer.reset(new SyncTimer);

    auto lastPhaseTimepoint = currentTimePoint();
    // assume success until a module actually fails
    bool initSuccessful = true;
    d->failed = false;

    // prepare modules
    for (auto &mod : orderedActiveModules) {
        // Prepare module. At this point it should have a timer,
        // the location where data is saved and be in the PREPARING state.
        emitStatusMessage(QStringLiteral("Preparing %1...").arg(mod->name()));
        lastPhaseTimepoint = currentTimePoint();

        const auto modInfo = d->modLibrary->moduleInfo(mod->id());

        mod->setStatusMessage(QString());
        mod->setTimer(d->timer);
        mod->setState(ModuleState::PREPARING);

        if ((modInfo != nullptr) && (!modInfo->storageGroupName().isEmpty())) {
            auto storageGroup = storageCollection->groupByName(modInfo->storageGroupName(), true);
            if (storageGroup.get() == nullptr) {
                qCritical() << "Unable to create data storage group with name" << modInfo->storageGroupName();
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

        qDebug().noquote().nospace() << "Engine: " << "Module '" << mod->name() << "' prepared in " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
    }

    // threads our modules run in, as module name/thread pairs
    std::vector<std::thread> dThreads;
    QHash<size_t, AbstractModule*> threadIdxModMap;
    std::unique_ptr<OptionalWaitCondition> startWaitCondition(new OptionalWaitCondition());

    QList<AbstractModule*> idleEventModules;
    QList<AbstractModule*> idleUiEventModules;
    std::vector<std::shared_ptr<ModuleEventThread>> evThreads;

    QList<OOPModule*> oopModules;
    std::vector<std::thread> oopThreads;

    // Only actually launch if preparation didn't fail.
    // we still call stop() on all modules afterwards though,
    // as some might need a stop call to clean up resources ther were
    // set up during preparations.
    // Modules are expected to deal with multiple calls to stop().
    if (initSuccessful) {
        emitStatusMessage(QStringLiteral("Initializing launch..."));

        lastPhaseTimepoint = currentTimePoint();

        // launch threads for threaded modules, but filter out out-of-process
        // modules - they get special treatment
        for (int i = 0; i < orderedActiveModules.size(); i++) {
            auto mod = orderedActiveModules[i];
            auto oopMod = qobject_cast<OOPModule*>(mod);
            if (oopMod != nullptr) {
                oopModules.append(oopMod);
                continue;
            }

            if (!mod->features().testFlag(ModuleFeature::RUN_THREADED))
                continue;

            // we are preparing again, this time for threading!
            // this is important, as we will only start when the module
            // signalled that it is ready now.
            mod->setState(ModuleState::PREPARING);

            // the thread name shouldn't be longer than 16 chars (inlcuding NULL)
            auto threadName = QStringLiteral("%1-%2").arg(mod->id().midRef(0, 12)).arg(i);
            dThreads.push_back(std::thread(executeModuleThread,
                                           threadName,
                                           mod,
                                           startWaitCondition.get()));
            threadIdxModMap[dThreads.size() - 1] = mod;
        }

        // collect all modules which do idle event execution
        for (auto &mod : orderedActiveModules)
            if (mod->features().testFlag(ModuleFeature::RUN_EVENTS))
                idleEventModules.append(mod);

        // prepare out-of-process modules
        // NOTE: We currently throw them all into one thread, which may not
        // be the most performant thing to do if there are a lot of OOP modules.
        // But let's address that case when we actually run into performance issues
        if (!oopModules.isEmpty()) {
            for (auto &mod : oopModules)
                mod->setState(ModuleState::PREPARING);
            oopThreads.push_back(std::thread(executeOOPModuleThread,
                                             "oop_comm_1",
                                             oopModules,
                                             startWaitCondition.get(),
                                             std::ref(d->running)));
        }

        // create new thread for idle-event processing modules, if needed
        // TODO: on systems with low CPU count we could also move the execution of those
        // to the main thread
        if (!idleEventModules.isEmpty()) {
            std::shared_ptr<ModuleEventThread> evThread(new ModuleEventThread);
            evThread->run(idleEventModules, startWaitCondition.get());
            evThreads.push_back(evThread);
        }

        qDebug().noquote().nospace() << "Engine: " << "Module and engine threads created in " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
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
            emitStatusMessage(QStringLiteral("Waiting for %1 to become ready...").arg(mod->name()));
            while (mod->state() != ModuleState::READY) {
                QThread::msleep(500);
                QCoreApplication::processEvents();
                if (mod->state() == ModuleState::ERROR) {
                    emitStatusMessage(QStringLiteral("Module '%1' failed to initialize.").arg(mod->name()));
                    initSuccessful = false;
                    break;
                }
            }
        }

        qDebug().noquote().nospace() << "Engine: " << "Waited for modules to become ready for " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
    }

    // Meanwhile, threaded modules may have failed, so let's check again if we are still
    // good on initialization
    if (initSuccessful) {
        emitStatusMessage(QStringLiteral("Launch setup completed."));

        // we officially start now, launch the timer
        d->timer->start();
        d->running = true;

        // first, launch all threaded and evented modules
        for (auto& mod : orderedActiveModules) {
            if ((!mod->features().testFlag(ModuleFeature::RUN_THREADED)) &&
                (!mod->features().testFlag(ModuleFeature::RUN_EVENTS)))
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

        qDebug().noquote().nospace() << "Engine: " << "Threaded/evented module startup completed, took " << d->timer->timeSinceStartMsec().count() << "msec";
        lastPhaseTimepoint = d->timer->currentTimePoint();

        // tell all non-threaded modules individuall now that we started
        for (auto& mod : orderedActiveModules) {
            // ignore threaded & evented
            if ((mod->features().testFlag(ModuleFeature::RUN_THREADED)) ||
                (mod->features().testFlag(ModuleFeature::RUN_EVENTS)))
                continue;

            mod->start();

            // work around bad modules which don't set this on their own in start()
            mod->m_running = true;
            mod->setState(ModuleState::RUNNING);
        }

        qDebug().noquote().nospace() << "Engine: " << "Startup phase completed, all modules are running. Took additional " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";

        // tell listeners that we are running now
        emit runStarted();

        emitStatusMessage(QStringLiteral("Running..."));

        // run the main loop and process UI events
        // modules may have injected themselves into the UI event loop
        // as well via QTimer callbacks, in case they need to modify UI elements.
        while (d->running) {
            QCoreApplication::processEvents();
            if (d->failed)
                break;
        }
    }

    auto finishTimestamp = static_cast<long long>(d->timer->timeSinceStartMsec().count());

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
        for (auto &mod : threadIdxModMap.values())
            mod->m_running = false;
        startWaitCondition->wakeAll();
    }

    // join all threads running evented modules, therefore stop
    // processing any new events
    for (const auto &evThread : evThreads) {
        emitStatusMessage(QStringLiteral("Waiting for event thread `%1`...").arg(evThread->threadName()));
        evThread->stop();
    }

    // send stop command to all modules
    for (auto &mod : orderedActiveModules) {
        emitStatusMessage(QStringLiteral("Stopping %1...").arg(mod->name()));
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
            } while ((d->timer->timeSinceStartMsec() - startPortWaitTS).count() <= 1200);

            if (remainingElements != 0)
                qDebug().noquote().nospace() << "Engine: "
                                             << "Module '" << mod->name() << "' "
                                             << "subscription `" << iport->id() << "` "
                                             << "possibly lost " << remainingElements << " element(s)";
        }

        // send the stop command
        mod->stop();

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

        qDebug().noquote().nospace() << "Engine: " << "Module '" << mod->name() << "' stopped in " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
    }

    lastPhaseTimepoint = d->timer->currentTimePoint();

    // join all dedicated module threads with the main thread again, waiting for them to terminate
    startWaitCondition->wakeAll(); // wake up all threads again, just in case one is stuck waiting
    for (size_t i = 0; i < dThreads.size(); i++) {
        auto &thread = dThreads[i];
        auto mod = threadIdxModMap[i];
        emitStatusMessage(QStringLiteral("Waiting for %1...").arg(mod->name()));
        qApp->processEvents();
        thread.join();
    }

    // join all out-of-process module communication threads
    for (auto &oopThread : oopThreads) {
        emitStatusMessage(QStringLiteral("Waiting for external processes and their relays..."));
        qApp->processEvents();
        oopThread.join();
    }

    qDebug().noquote().nospace() << "Engine: " << "All (non-event) engine threads joined in " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
    lastPhaseTimepoint = d->timer->currentTimePoint();

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

        qDebug() << "Saving experiment metadata in:" << storageCollection->path();

        if (!storageCollection->save()) {
            QMessageBox::critical(d->parentWidget,
                                  QStringLiteral("Unable to finish recording"),
                                  QStringLiteral("Unable to save experiment metadata: %1").arg(storageCollection->lastError()));
            d->failed = true;
        }

        qDebug().noquote().nospace() << "Engine: " << "Manifest and additional data saved in " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
    }

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
    if (mod != nullptr) {
        emit moduleError(mod, message);
        d->runFailedReason = QStringLiteral("%1(%2): %3").arg(mod->id()).arg(mod->name()).arg(message);
    } else {
        d->runFailedReason = QStringLiteral("?(?): %1").arg(message);
    }

    d->failed = true;
    if (d->running) {
        d->running = false;
        emit runFailed(mod, message);
    }
}

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
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QThread>

#include "oop/oopmodule.h"
#include "modulelibrary.h"
#include "hrclock.h"
#include "utils.h"

#pragma GCC diagnostic ignored "-Wpadded"
class Engine::Private
{
public:
    Private() { }
    ~Private() { }

    QWidget *parentWidget;
    QList<AbstractModule*> activeModules;
    ModuleLibrary *modLibrary;
    std::shared_ptr<HRTimer> timer;

    TestSubject testSubject;
    QString exportBaseDir;
    QString exportDir;
    bool exportDirIsTempDir;
    bool exportDirIsValid;

    QString experimentId;

    std::atomic_bool running;
    std::atomic_bool failed;
};
#pragma GCC diagnostic pop

Engine::Engine(QWidget *parentWidget)
    : QObject(parentWidget),
      d(new Engine::Private)
{
    d->exportDirIsValid = false;
    d->running = false;
    d->modLibrary = new ModuleLibrary(this);
    d->parentWidget = parentWidget;
    d->timer.reset(new HRTimer);

    // allow sending states via Qt queued connections,
    // and also register all other transmittable data types
    // with the meta object system
    registerStreamMetaTypes();
}

Engine::~Engine()
{}

ModuleLibrary *Engine::library() const
{
    return d->modLibrary;
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
        QMessageBox::critical(d->parentWidget,
                              QStringLiteral("Error"),
                              QStringLiteral("Unable to create directory '%1'.").arg(dir));
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
    qDebug().noquote() << "EngineStatus:" << message;
    emit statusMessage(message);
}

/**
 * @brief Return a list of active modules that have been sorted in the order they
 * should be initialized in.
 */
QList<AbstractModule *> Engine::createModuleExecOrderList()
{
    // we need to do some ordering so modules which consume data from other modules get
    // activated last.
    // this implicit sorting isn't great, and we might - if MazeAmaze becomes more complex than
    // it already is - replace this with declarative ordering a proper dependency resolution
    // at some point.
    // we don't use Qt sorting facilities here, because we want at least some of the original
    // module order to be kept.
    QList<AbstractModule*> orderedActiveModules;
    QList<AbstractModule*> scriptModList;
    //int firstImgSinkModIdx = -1;

    orderedActiveModules.reserve(d->activeModules.length());
    for (auto &mod : d->activeModules) {
#if 0
        if (qobject_cast<ImageSourceModule*>(mod) != nullptr) {
            if (firstImgSinkModIdx >= 0) {
                // put in before the first sink
                orderedActiveModules.insert(firstImgSinkModIdx, mod);
                continue;
            } else {
                // just add the module
                orderedActiveModules.append(mod);
                continue;
            }
        } else if (qobject_cast<ImageSinkModule*>(mod) != nullptr) {
            if (firstImgSinkModIdx < 0) {
                // we have the first sink module
                orderedActiveModules.append(mod);
                firstImgSinkModIdx = orderedActiveModules.length() - 1;
                continue;
            } else {
                // put in after the first sink
                orderedActiveModules.insert(firstImgSinkModIdx + 1, mod);
                continue;
            }
        } else if (qobject_cast<PyScriptModule*>(mod) != nullptr) {
            // scripts are always initialized last, as they may arbitrarily connect
            // to the other modules to control them.
            // and we rather want that to happen when everything is prepared to run.
            scriptModList.append(mod);
            continue;
        } else {
            orderedActiveModules.append(mod);
        }
#endif
        orderedActiveModules.append(mod);
    }

    for (auto mod : scriptModList)
        orderedActiveModules.append(mod);

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
    for (auto &mod : mods) {
        if (!mod->oopPrepare(&loop)) {
            qDebug().noquote() << "Failed to prepare OOP module" << mod->name() << ":" << mod->lastError();
            return;
        }
        // ensure we are ready - the engine has reset ourselves to "PREPARING"
        // to make this possible before launching this thread
        mod->setStateReady();
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

/**
 * @brief Main entry point for engine-managed module threads.
 */
static void executeIdleEventModuleThread(const QString& threadName, QList<AbstractModule*> mods,
                                         OptionalWaitCondition *waitCondition, std::atomic_bool &running, std::atomic_bool &failed)
{
    pthread_setname_np(pthread_self(), qPrintable(threadName.mid(0, 15)));

    // wait for us to start
    waitCondition->wait();

    // check if any module signals that it will actually not be doing anything
    // (if so, we don't need to call it and can maybe even terminate this thread)
    QMutableListIterator<AbstractModule*> i(mods);
    while (i.hasNext()) {
        if (i.next()->state() == ModuleState::IDLE)
            i.remove();
    }

    if (mods.isEmpty()) {
        qDebug() << "All evented modules are idle, shutting down their thread.";
        return;
    }

    while (running) {
        for (auto &mod : mods) {
            if (!mod->runEvent()){
                qDebug() << QStringLiteral("Module %1 failed in event loop.").arg(mod->name());
                failed = true;
                return;
            }
        }
    }
}

bool Engine::run()
{
    if (d->running)
        return false;

    d->failed = true; // if we exit before this is reset, initialization has failed

    if (d->activeModules.isEmpty()) {
        QMessageBox::warning(d->parentWidget,
                             QStringLiteral("Configuration error"),
                             QStringLiteral("You did not add a single module to be run.\nPlease add a module to the experiment to continue."));
        return false;
    }

    if (d->exportBaseDir.isEmpty() || d->exportDir.isEmpty()) {
        QMessageBox::critical(d->parentWidget,
                             QStringLiteral("Configuration error"),
                             QStringLiteral("Data export directory was not properly set. Can not continue."));
        return false;
    }

    // determine and create the directory for ephys data
    qDebug() << "Initializing new recording run";

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

    if (!makeDirectory(d->exportDir))
        return false;

    // fetch list of modules in their activation order
    auto orderedActiveModules = createModuleExecOrderList();

    bool initSuccessful = true;
    d->failed = false; // assume success until a module actually fails
    for (auto &mod : orderedActiveModules) {
        // Prepare module. At this point it should have a timer,
        // the location where data is saved and be in the PREPARING state.
        emitStatusMessage(QStringLiteral("Preparing %1...").arg(mod->name()));
        mod->setStatusMessage(QString());
        mod->setTimer(d->timer);
        mod->setState(ModuleState::PREPARING);
        mod->setDataStorageRootDir(d->exportDir);
        if (!mod->prepare(d->testSubject)) {
            initSuccessful = false;
            d->failed = true;
            emitStatusMessage(QStringLiteral("Module %1 failed to prepare.").arg(mod->name()));
            break;
        }
        // if the module hasn't set itself to ready yet, assume it is idle
        if (mod->state() != ModuleState::READY)
            mod->setState(ModuleState::IDLE);
    }

    // threads our modules run in, as module name/thread pairs
    std::vector<std::thread> dThreads;
    QHash<size_t, AbstractModule*> threadIdxModMap;
    std::unique_ptr<OptionalWaitCondition> startWaitCondition(new OptionalWaitCondition());

    QList<AbstractModule*> idleEventModules;
    QList<AbstractModule*> idleUiEventModules;
    std::vector<std::thread> evThreads;

    QList<OOPModule*> oopModules;
    std::vector<std::thread> oopThreads;

    // Only actually launch if preparation didn't fail.
    // we still call stop() on all modules afterwards though,
    // as some might need a stop call to clean up resources ther were
    // set up during preparations.
    // Modules are expected to deal with multiple calls to stop().
    if (initSuccessful) {
        emitStatusMessage(QStringLiteral("Initializing launch..."));

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
            evThreads.push_back(std::thread(executeIdleEventModuleThread,
                                            "evmod_loop_1",
                                            idleEventModules,
                                            startWaitCondition.get(),
                                            std::ref(d->running),
                                            std::ref(d->failed)));
        }

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
                    emitStatusMessage(QStringLiteral("Module %1 failed to initialize.").arg(mod->name()));
                    initSuccessful = false;
                    break;
                }
            }
        }
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
        startWaitCondition->wakeAll();

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

    // send stop command to all modules
    for (auto &mod : orderedActiveModules) {
        emitStatusMessage(QStringLiteral("Stopping %1...").arg(mod->name()));
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

        // all module data must be written by this point, so we "steal" its storage root path,
        // so the module is less tempted to write data into old experiment locations.
        mod->setDataStorageRootDir(QString());
    }

    // join all dedicated module threads with the main thread again, waiting for them to terminate
    startWaitCondition->wakeAll(); // wake up all threads again, just in case one is stuck waiting
    for (size_t i = 0; i < dThreads.size(); i++) {
        auto &thread = dThreads[i];
        auto mod = threadIdxModMap[i];
        emitStatusMessage(QStringLiteral("Waiting for %1...").arg(mod->name()));
        thread.join();
    }

    // join all out-of-process module communication threads
    for (auto &oopThread : oopThreads) {
        emitStatusMessage(QStringLiteral("Waiting for external processes and their relays..."));
        oopThread.join();
    }

    // join all threads running evented modules
    for (auto &evThread : evThreads) {
        emitStatusMessage(QStringLiteral("Waiting for event executor..."));
        evThread.join();
    }

    if (!initSuccessful) {
        // if we failed to prepare this run, don't save the manifest and also
        // remove any data that we might have already created, as well as the
        // export directory.
        emitStatusMessage(QStringLiteral("Removing broken data..."));
        deDir.removeRecursively();
    } else {
        emitStatusMessage(QStringLiteral("Writing manifest & metadata..."));

        // store metadata from all modules, in case they want us to store data for them
        for (auto &mod : orderedActiveModules) {
            auto mdata = mod->experimentMetadata();
            if (mdata.isEmpty())
                continue;
            mdata.insert(QStringLiteral("_modName"), mod->name());
            mdata.insert(QStringLiteral("_modType"), mod->id());

            const auto modMetaFilename = QStringLiteral("%1/%2.meta.json").arg(d->exportDir).arg(simplifyStringForFilename(mod->name()));
            qDebug() << "Saving module metadata:" << modMetaFilename;

            QFile modMetaFile(modMetaFilename);
            if (!modMetaFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QMessageBox::critical(d->parentWidget,
                                      QStringLiteral("Unable to finish recording"),
                                      QStringLiteral("Unable to open module metadata of %1 file for writing.").arg(mod->name()));
                d->failed = true;
                return false;
            }

            QTextStream modMetaFileOut(&modMetaFile);
            modMetaFileOut << QJsonDocument::fromVariant(mdata).toJson();
        }

        // write manifest with misc metadata about the experiment run
        QDateTime curDateTime(QDateTime::currentDateTime());

        QJsonObject manifest;
        manifest.insert("AppVersion", QCoreApplication::applicationVersion());
        manifest.insert("SubjectId", d->testSubject.id);
        manifest.insert("SubjectGroup", d->testSubject.group);
        manifest.insert("SubjectComment", d->testSubject.comment);
        manifest.insert("RecordingLengthMsec", finishTimestamp);
        manifest.insert("Date", curDateTime.toString(Qt::ISODate));
        manifest.insert("Success", !d->failed);

        QJsonArray jActiveModules;
        for (auto &mod : orderedActiveModules) {
            QJsonObject info;
            info.insert(QStringLiteral("id"), mod->id());
            info.insert(QStringLiteral("name"), mod->name());
            jActiveModules.append(info);
        }
        manifest.insert("ActiveModules", jActiveModules);

        const auto manifestFilename = QStringLiteral("%1/manifest.json").arg(d->exportDir);
        qDebug() << "Saving manifest as:" << manifestFilename;
        QFile manifestFile(manifestFilename);
        if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::critical(d->parentWidget,
                                  QStringLiteral("Unable to finish recording"),
                                  QStringLiteral("Unable to open manifest file for writing."));
            d->failed = true;
            return false;
        }

        QTextStream manifestFileOut(&manifestFile);
        manifestFileOut << QJsonDocument(manifest).toJson();
    }

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
        emit moduleError(mod, message);

    d->failed = true;
    if (d->running) {
        d->running = false;
        emit runFailed(mod, message);
    }
}

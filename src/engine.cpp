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
#include <QSharedData>
#include <QMessageBox>
#include <QStorageInfo>
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QThread>

#include "modulemanager.h"
#include "hrclock.h"

#pragma GCC diagnostic ignored "-Wpadded"
class Engine::EData : public QSharedData
{
public:
    EData() { }
    ~EData() { }

    QWidget *parentWidget;
    ModuleManager *modManager;
    std::shared_ptr<HRTimer> timer;

    TestSubject testSubject;
    QString exportBaseDir;
    QString exportDir;
    bool exportDirIsTempDir;
    bool exportDirIsValid;

    QString experimentId;

    bool running;
    bool failed;
};
#pragma GCC diagnostic pop

Engine::Engine(ModuleManager *modManager, QObject *parent)
    : QObject(parent),
      d(new EData)
{
    d->exportDirIsValid = false;
    d->running = false;
    d->modManager = modManager;
    d->parentWidget = d->modManager->parentWidget();
    d->timer.reset(new HRTimer);

    // react to error events emitted by a module
    connect(d->modManager, &ModuleManager::moduleError, [&](AbstractModule *mod, const QString &message) {
        d->failed = true;
        d->running = false;
        emit runFailed(mod, message);
    });

    // allow sending states via Qt queued connections
    qRegisterMetaType<ModuleState>();
}

Engine::~Engine()
{

}

ModuleManager *Engine::modManager() const
{
    return d->modManager;
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

    orderedActiveModules.reserve(d->modManager->activeModules().length());
    for (auto &mod : d->modManager->activeModules()) {
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

bool Engine::run()
{
    if (d->running)
        return false;

    d->failed = true; // if we exit before this is reset, initialization has failed

    if (d->modManager->activeModules().isEmpty()) {
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

    bool prepareStepFailed = false;
    d->failed = false; // assume success until a module actually fails
    for (auto &mod : orderedActiveModules) {
        emitStatusMessage(QStringLiteral("Preparing %1...").arg(mod->name()));
        mod->setStatusMessage(QString());
        mod->setTimer(d->timer);
        mod->setState(ModuleState::PREPARING);
        if (!mod->prepare(d->exportDir, d->testSubject)) {
            prepareStepFailed = true;
            d->failed = true;
            emitStatusMessage(QStringLiteral("Module %1 failed to prepare.").arg(mod->name()));
            break;
        }
        mod->setState(ModuleState::IDLE);
    }

    // threads our modules run in, as module name/thread pairs
    std::vector<std::thread> threads;
    QHash<size_t, AbstractModule*> threadIdxModMap;
    std::unique_ptr<OptionalWaitCondition> startWaitCondition(new OptionalWaitCondition());

    // Only actually launch if preparation didn't fail.
    // we still call stop() on all modules afterwards though,
    // as some might need a stop call to clean up resources ther were
    // set up during preparations.
    // Modules are expected to deal with multiple calls to stop().
    if (!prepareStepFailed) {
        emitStatusMessage(QStringLiteral("Initializing launch..."));

        // launch threads for threaded modules
        for (int i = 0; i < orderedActiveModules.size(); i++) {
            auto mod = orderedActiveModules[i];
            if (!mod->features().testFlag(ModuleFeature::RUN_THREADED))
                continue;

            // we are preparing again, this time for threading!
            // this is important, as we will only start when the module
            // signalled that it is ready now.
            mod->setState(ModuleState::PREPARING);

            // the thread name shouldn't be longer than 16 chars (inlcuding NULL)
            auto threadName = QStringLiteral("%1-%2").arg(mod->id().midRef(0, 12)).arg(i);
            threads.push_back(std::thread(executeModuleThread,
                                          threadName,
                                          mod,
                                          startWaitCondition.get()));
            threadIdxModMap[threads.size() - 1] = mod;
        }

        // collect all modules which do idle event execution
        QList<AbstractModule*> idleEventModules;
        for (auto &mod : orderedActiveModules) {
            if (!mod->features().testFlag(ModuleFeature::RUN_EVENTS))
                continue;
            idleEventModules.append(mod);
        }

        // ensure all modules are in the READY state
        // (modules may take a bit of time to prepare their threads)
        // FIXME: Maybe add a timeout on this, in case a module doesn't
        // behave and never ever leaves its preparation phase?
        for (auto &mod : orderedActiveModules) {
            if (mod->state() == ModuleState::READY)
                continue;
            emitStatusMessage(QStringLiteral("Waiting for %1 to become ready...").arg(mod->name()));
            while (mod->state() != ModuleState::READY) {
                QThread::msleep(500);
                QCoreApplication::processEvents();
            }
        }

        emitStatusMessage(QStringLiteral("Launch setup completed."));

        // we officially start now, launch the timer
        d->timer->start();

        // first, launch all threaded modules
        for (auto& mod : orderedActiveModules) {
            if (!mod->features().testFlag(ModuleFeature::RUN_THREADED))
                continue;

            mod->start();

            // ensure modules are in their "running" state now
            mod->m_running = true;
            mod->setState(ModuleState::RUNNING);
        }

        // wake that thundering herd and hope all threaded modules awoken by the
        // start signal behave properly
        startWaitCondition->wakeAll();

        // tell all non-threaded modules individuall now that we started
        for (auto& mod : orderedActiveModules) {
            // ignore threaded
            if (mod->features().testFlag(ModuleFeature::RUN_THREADED))
                continue;

            mod->start();

            // work around bad modules which don't set this on their own in start()
            mod->m_running = true;
            mod->setState(ModuleState::RUNNING);
        }

        d->running = true;
        emitStatusMessage(QStringLiteral("Running..."));

        while (d->running) {
            for (auto &mod : idleEventModules) {
                if (!mod->runEvent()){
                    emitStatusMessage(QStringLiteral("Module %1 failed.").arg(mod->name()));
                    d->failed = true;
                    break;
                }
            }
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

        // safeguard against bad modules which don't stop themselves from running their
        // thread loops on their own
        mod->m_running = false;

        // ensure modules really have terminated all their outgoing streams,
        // because if they didn't do that, connected modules may not be able to exit
        for (auto const port : mod->outPorts())
            port->stopStream();

        // ensure modules display the correct state after we stopped a run
        if ((mod->state() != ModuleState::IDLE) && (mod->state() != ModuleState::ERROR))
            mod->setState(ModuleState::IDLE);
    }

    // join all module threads with the main thread again, waiting for them to terminate
    startWaitCondition->wakeAll(); // wake up all threads again, just in case one is stuck waiting
    for (size_t i = 0; i < threads.size(); i++) {
        auto &thread = threads[i];
        auto mod = threadIdxModMap[i];
        emitStatusMessage(QStringLiteral("Waiting for %1...").arg(mod->name()));
        thread.join();
    }

    if (prepareStepFailed) {
        // if we failed to prepare this run, don't save the manifest and also
        // remove any data that we might have already created, as well as the
        // export directory.
        emitStatusMessage(QStringLiteral("Removing broken data..."));
        deDir.removeRecursively();
    } else {
        emitStatusMessage(QStringLiteral("Writing manifest..."));

        // write manifest with misc information
        QDateTime curDateTime(QDateTime::currentDateTime());

        QJsonObject manifest;
        manifest.insert("appVersion", QCoreApplication::applicationVersion());
        manifest.insert("subjectId", d->testSubject.id);
        manifest.insert("subjectGroup", d->testSubject.group);
        manifest.insert("subjectComment", d->testSubject.comment);
        manifest.insert("recordingLengthMsec", finishTimestamp);
        manifest.insert("date", curDateTime.toString(Qt::ISODate));
        manifest.insert("success", !d->failed);

        QJsonArray jActiveModules;
        for (auto &mod : orderedActiveModules) {
            QJsonObject info;
            info.insert(mod->id(), mod->name());
            jActiveModules.append(info);
        }
        manifest.insert("activeModules", jActiveModules);

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

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
        emit statusMessage("OS error.");
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

        emit statusMessage(QStringLiteral("Removing data from an old run..."));
        deDir.removeRecursively();
    }

    if (!makeDirectory(d->exportDir))
        return false;

    // fetch list of modules in their activation order
    auto orderedActiveModules = d->modManager->createOrderedModuleList();

    bool prepareStepFailed = false;
    d->failed = false; // assume success until a module actually fails
    Q_FOREACH(auto mod, orderedActiveModules) {
        emit statusMessage(QStringLiteral("Preparing %1...").arg(mod->name()));
        mod->setStatusMessage(QString());
        mod->setTimer(d->timer);
        if (!mod->prepare(d->exportDir, d->testSubject)) {
            prepareStepFailed = true;
            d->failed = true;
            emit statusMessage(QStringLiteral("Module %1 failed to prepare.").arg(mod->name()));
            break;
        }
    }

    // Only actually launch if preparation didn't fail.
    // we still call stop() on all modules afterwards though,
    // as some might need a stop call to clean up resources ther were
    // set up during preparations.
    // Modules are expected to deal with multiple calls to stop().
    if (!prepareStepFailed) {
        emit statusMessage(QStringLiteral("Initializing launch..."));

        d->timer->start();
        Q_FOREACH(auto mod, orderedActiveModules)
            mod->start();

        d->running = true;
        emit statusMessage(QStringLiteral("Running..."));

        while (d->running) {
            Q_FOREACH(auto mod, orderedActiveModules) {
                if (!mod->runCycle()){
                    emit statusMessage(QStringLiteral("Module %1 failed.").arg(mod->name()));
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

    Q_FOREACH(auto mod, orderedActiveModules) {
        emit statusMessage(QStringLiteral("Stopping %1...").arg(mod->name()));
        mod->stop();

        // ensure modules display the correct state after we stopped a run
        if (mod->state() == ModuleState::RUNNING || mod->state() == ModuleState::WAITING)
            mod->setState(ModuleState::READY);
    }

    if (prepareStepFailed) {
        // if we failed to prepare this run, don't save the manifest and also
        // remove any data that we might have already created, as well as the
        // export directory.
        deDir.removeRecursively();
    } else {
        emit statusMessage(QStringLiteral("Writing manifest..."));

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
        Q_FOREACH(auto mod, orderedActiveModules) {
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

    emit statusMessage(QStringLiteral("Ready."));
    return true;
}

void Engine::stop()
{
    d->running = false;
}

/*
 * Copyright (C) 2024-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "dbuscontrol.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QTimer>

#include "engine.h"
#include "logging.h"
#include "mainwindow.h"
#include "uiprompts.h"
#include "fabric/runstatistics.h"

namespace Syntalos
{

static quill::Logger *dbcLogger()
{
    static quill::Logger *const log = getLogger("dbusctl");
    return log;
}

DBusControlAdaptor::DBusControlAdaptor(MainWindow *window)
    : QDBusAbstractAdaptor(window),
      m_window(window),
      m_engine(window->engine())
{
    connect(m_engine, &Engine::preRunPrepare, this, [this]() {
        // a new run is starting (possibly not via this interface), so its outcome is not reported yet
        m_runStoppedEmitted = false;
        if (!m_runPending)
            m_failMessage.clear();
    });
    connect(m_engine, &Engine::runStarted, this, &DBusControlAdaptor::RunStarted);
    connect(m_engine, &Engine::runFailed, this, [this](AbstractModule *mod, const QString &message) {
        onEngineRunFailed(mod == nullptr ? QString() : mod->name(), message);
    });
    connect(m_engine, &Engine::runStopped, this, &DBusControlAdaptor::onEngineRunStopped);
}

bool DBusControlAdaptor::registerOnSessionBus(MainWindow *window)
{
    new DBusControlAdaptor(window);
    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerObject(QString::fromLatin1(SY_DBUS_OBJECT_PATH), window, QDBusConnection::ExportAdaptors)) {
        LOG_WARNING(dbcLogger(), "Unable to register D-Bus control interface: {}", bus.lastError().message());
        return false;
    }
    return true;
}

QString DBusControlAdaptor::state() const
{
    if (m_window->isProjectLoadInProgress())
        return QStringLiteral("loading");
    if (m_engine->isActive() && m_engine->isStopRequested())
        return QStringLiteral("stopping");
    if (m_engine->isRunning())
        return QStringLiteral("running");
    if (m_runPending || m_engine->isActive())
        return QStringLiteral("preparing");
    return QStringLiteral("idle");
}

bool DBusControlAdaptor::nonInteractive() const
{
    return isNonInteractive();
}

void DBusControlAdaptor::setNonInteractive(bool enabled)
{
    LOG_INFO(dbcLogger(), "Non-interactive mode {} via remote control", enabled ? "enabled" : "disabled");
    setNonInteractiveMode(enabled);
}

QString DBusControlAdaptor::exportDirectory() const
{
    return m_engine->exportBaseDir();
}

QString DBusControlAdaptor::SetExportDirectory(const QString &path)
{
    LOG_INFO(dbcLogger(), "Setting export directory on remote request: {}", path);
    const auto res = m_window->setExportDirectory(path);
    if (!res) {
        LOG_ERROR(dbcLogger(), "Failed to set export directory '{}': {}", path, res.error());
        return res.error();
    }
    return {};
}

QString DBusControlAdaptor::LoadProject(const QString &path)
{
    LOG_INFO(dbcLogger(), "Loading project on remote request: {}", path);
    if (m_runPending)
        return QStringLiteral("A run is about to start, can not load a project now.");

    const auto res = m_window->loadProject(path);
    if (!res) {
        LOG_ERROR(dbcLogger(), "Failed to load project '{}': {}", path, res.error());
        Q_EMIT ProjectLoaded(false, res.error());
        return res.error();
    }

    Q_EMIT ProjectLoaded(true, {});
    return {};
}

QString DBusControlAdaptor::StartRun(bool ephemeral, int maxDurationSec)
{
    if (m_runPending)
        return QStringLiteral("A run is already about to start.");
    if (m_window->isProjectLoadInProgress())
        return QStringLiteral("A project is currently being loaded, can not start a run.");
    if (m_engine->isActive())
        return QStringLiteral("A run is already active.");
    if (m_engine->presentModules().isEmpty())
        return QStringLiteral("No modules are present, there is nothing to run.");

    LOG_INFO(
        dbcLogger(),
        "Starting {} run on remote request (max. duration: {} s)",
        ephemeral ? "ephemeral" : "persistent",
        maxDurationSec);
    m_runPending = true;
    m_runStoppedEmitted = false;
    m_failMessage.clear();
    m_statsBeforeRun = m_engine->lastRunStatistics();

    // the run blocks until it is finished, so we start it once the reply has been sent
    QTimer::singleShot(0, this, [this, ephemeral, maxDurationSec]() {
        const auto res = m_window->startRun(ephemeral, maxDurationSec);
        m_runPending = false;
        if (m_runStoppedEmitted)
            return;

        // the run never got to the point where the engine reports it as stopped
        QString message = res ? m_failMessage : res.error();
        if (message.isEmpty())
            message = QStringLiteral("The run could not be started.");
        LOG_ERROR(dbcLogger(), "Run failed to start: {}", message);
        m_runStoppedEmitted = true;
        Q_EMIT RunStopped(false, message);
    });

    return {};
}

void DBusControlAdaptor::StopRun()
{
    LOG_INFO(dbcLogger(), "Stopping run on remote request");
    m_window->stopRun();
}

void DBusControlAdaptor::Quit()
{
    LOG_INFO(dbcLogger(), "Quitting on remote request");
    QTimer::singleShot(0, m_window, &MainWindow::requestQuit);
}

QString DBusControlAdaptor::RunStatisticsJson()
{
    const auto stats = m_engine->lastRunStatistics();
    if (!stats || stats == m_statsBeforeRun)
        return {};
    return QString::fromUtf8(stats->toJsonData());
}

void DBusControlAdaptor::onEngineRunFailed(const QString &moduleName, const QString &message)
{
    if (moduleName.isEmpty())
        m_failMessage = message;
    else
        m_failMessage = QStringLiteral("%1: %2").arg(moduleName, message);
}

void DBusControlAdaptor::onEngineRunStopped()
{
    if (m_runStoppedEmitted)
        return;
    m_runStoppedEmitted = true;

    const bool failed = m_engine->hasFailed();
    QString message = m_failMessage;
    if (failed && message.isEmpty()) {
        const auto stats = m_engine->lastRunStatistics();
        if (stats)
            message = stats->failReason;
    }
    Q_EMIT RunStopped(!failed, message);
}

} // namespace Syntalos

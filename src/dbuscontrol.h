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

#pragma once

#include <QDBusAbstractAdaptor>
#include <memory>

#include "dbuscontrol-defs.h"

class MainWindow;

namespace Syntalos
{
class Engine;
class RunStatistics;

/**
 * @brief D-Bus interface to control the running Syntalos instance.
 *
 * The adaptor is attached to the main window and exposes project loading and
 * run control on the session bus, so other tools on the machine can drive Syntalos.
 */
class DBusControlAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.syntalos.syntalos")
    Q_PROPERTY(QString State READ state)
    Q_PROPERTY(bool NonInteractive READ nonInteractive WRITE setNonInteractive)
    Q_PROPERTY(QString ExportDirectory READ exportDirectory)

public:
    explicit DBusControlAdaptor(MainWindow *window);

    /**
     * @brief Create the adaptor and register it on the session bus.
     * @return false if the object could not be registered.
     */
    static bool registerOnSessionBus(MainWindow *window);

    /**
     * One of: idle, loading, preparing, running, stopping
     */
    QString state() const;

    bool nonInteractive() const;
    void setNonInteractive(bool enabled);

    /**
     * The data export base directory of the loaded project
     */
    QString exportDirectory() const;

public Q_SLOTS:
    /**
     * Load a project file. Returns an empty string on success, or an error message.
     */
    QString LoadProject(const QString &path);

    /**
     * Override the data export base directory of the loaded project (created if missing).
     * Returns an empty string on success, or an error message.
     */
    QString SetExportDirectory(const QString &path);

    /**
     * Start a run of the loaded project, stopped automatically after maxDurationSec if > 0.
     * Returns an empty string if the run was accepted, or an error message.
     */
    QString StartRun(bool ephemeral, int maxDurationSec);

    /**
     * Stop the current run, if any.
     */
    void StopRun();

    /**
     * Quit Syntalos. Will stop any active run first.
     */
    void Quit();

    /**
     * Retrieve statistics of the last run that was started via StartRun as JSON,
     * or an empty string if there are none.
     */
    QString RunStatisticsJson();

Q_SIGNALS:
    void ProjectLoaded(bool success, const QString &message);
    void RunStarted();
    void RunStopped(bool success, const QString &message);

private:
    void onEngineRunFailed(const QString &moduleName, const QString &message);
    void onEngineRunStopped();

    MainWindow *m_window;
    Engine *m_engine;

    bool m_runPending{false};
    bool m_runStoppedEmitted{false};
    QString m_failMessage;
    std::shared_ptr<const RunStatistics> m_statsBeforeRun;
};

} // namespace Syntalos

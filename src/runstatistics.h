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
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <vector>

#include "datactl/datatypes.h"
#include "fabric/moduleapi.h"
#include "utils/resourceinfo.h"

namespace Syntalos
{

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

/**
 * @brief Statistics for a single module that took part in a run.
 */
struct ModuleRunStats {
    QString id;
    QString name;
    ModuleDriverKind driver = ModuleDriverKind::NONE;
    ModuleState finalState = ModuleState::UNKNOWN;
    bool outOfProcess = false;
    QString eventThreadKey; /// event thread this module ran on, if it has an event-based driver
    QString errorMessage;

    // priority elevation granted for this run
    bool realtimeRequested = false;
    int niceness = 0;
    /// whether the requested elevation took effect on the module's thread, event thread or worker
    /// process; nothing if no elevation was requested or we could not find out
    std::optional<bool> priorityApplied;
    std::vector<uint> cpuAffinity;

    /// usage of the dedicated in-process thread (if the module has one)
    std::optional<ThreadUsageStats> thread;

    /// usage of the out-of-process worker during this run (if the module has one)
    qint64 workerPid = 0;
    std::optional<ThreadUsageStats> worker;

    /// measurements the module reported itself, via AbstractModule::setRunStatistic()
    QVariantHash moduleStats;
};

/**
 * @brief Statistics for a shared event thread.
 */
struct EventThreadRunStats {
    QString key;
    QStringList moduleNames;
    bool realtimeRequested = false;
    int niceness = 0;
    bool priorityApplied = false; /// whether the requested elevation took effect
    std::optional<ThreadUsageStats> thread;
};

/**
 * @brief Statistics for a single connection (stream subscription) between two ports.
 */
struct ConnectionRunStats {
    QString srcModule;
    QString srcPort;
    QString dstModule;
    QString dstPort;
    QString dataType;
    quint64 itemsReceived = 0;
    quint64 peakPending = 0;   /// largest queue backlog observed by the engine's monitor
    quint64 pendingAtStop = 0; /// items still queued when the receiving module was stopped
    ConnectionHeatLevel maxHeat = ConnectionHeatLevel::NONE;

    /// data flows directly between two worker processes; the engine does not see it,
    /// so no counts are available for this connection
    bool directIpc = false;
};

/**
 * @brief Statistics collected by the engine over one experiment run.
 */
struct RunStatistics {
    static constexpr int FormatVersion = 1;

    QDateTime started;
    QString runId;
    QString experimentId;
    QString exportDir;
    bool ephemeral = false;
    bool failed = false;
    QString failReason;
    double durationSec = 0.0;    /// recording length (Syntalos Master Clock)
    double usageWindowSec = 0.0; /// wall-clock length of the window the usage deltas were measured over
    qint64 bytesWritten = -1;

    int cpuCoreCount = 0;
    uint priorityBudget = 0; /// RtKit elevation budget the engine distributed
    int threadsElevated = 0; /// threads and worker processes running with raised priority (realtime or nice < 0)
    int threadsTotal = 0;    /// dedicated module threads + event threads

    std::optional<ThreadUsageStats> mainThread; /// usage delta of the engine (GUI) thread over the run
    std::optional<ThreadUsageStats> process;    /// usage delta of the whole Syntalos process over the run
    qint64 peakRssKiB = -1;                     /// peak resident set size of the process (lifetime, not per run)

    QList<ModuleRunStats> modules;
    QList<EventThreadRunStats> eventThreads;
    QList<ConnectionRunStats> connections;

    /// some relevant system info that we want to include in our report
    QMap<QString, QString> system;

    QJsonObject toJson() const;
    QByteArray toJsonData() const;

    /**
     * @brief Render the statistics as an HTML fragment for display in a rich-text widget.
     */
    QString toHtml() const;

    /**
     * @brief Write the statistics as JSON to @p path, replacing any existing file.
     */
    bool saveJson(const QString &path, QString *errorMessage = nullptr) const;
};

#pragma GCC diagnostic pop

} // namespace Syntalos

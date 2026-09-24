/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <atomic>
#include <expected>
#include <functional>

namespace SyBench
{

/**
 * @brief How to run one benchmark step
 */
struct StepRunConfig {
    QString projectFile;
    int durationSec = 20;
    bool ephemeral = true;    /// ephemeral runs store nothing permanently; set false for disk-write tests
    QString exportDir;        /// export directory override for non-ephemeral runs
    QString statsFile;        /// where Syntalos should write its run statistics JSON
    int startupGraceSec = 90; /// extra time allowed for startup and teardown before the run is killed

    /// Kill the run when Syntalos (and its workers) use more resident memory than this.
    /// 0 = automatic: 60% of the memory available when the run starts.
    qint64 memoryLimitKiB = 0;
    /// Kill the run when the whole system has less memory available than this (default 1 GiB).
    qint64 systemMemoryFloorKiB = 1024 * 1024;
};

/**
 * @brief Counts a flow meter module collected
 */
struct MeterStats {
    QString moduleName;
    qint64 items = 0;
    qint64 intervalMeanUs = 0;
    qint64 intervalMaxUs = 0;
    qint64 ageP50Us = 0;
    qint64 ageP95Us = 0;
    qint64 ageP99Us = 0;
    qint64 ageMaxUs = 0;
};

/**
 * @brief Queue statistics of one connection between two modules
 */
struct ConnectionStats {
    QString srcModule;
    QString srcPort;
    QString dstModule;
    QString dstPort;
    qint64 peakPending = 0;
    qint64 pendingAtStop = 0;
    bool directIpc = false;
};

/**
 * @brief Resource usage of one module
 */
struct ModuleUsage {
    QString name;
    QString id;
    QString driver;
    QString eventThread;
    bool outOfProcess = false;
    bool realtimeApplied = false;
    double cpuSec = 0; /// user + system time of the module's thread or worker process
};

/**
 * @brief Everything we learned from one Syntalos run
 */
struct StepResult {
    bool success = false; /// the run completed without error
    bool cancelled = false;
    bool memoryExceeded = false; /// the run was killed for using too much memory (queues overflowing)
    QString failureReason;
    int exitCode = -1;

    double durationSec = 0;
    double usageWindowSec = 0;
    double processCpuSec = 0;
    qint64 peakRssKiB = 0;         /// as reported by Syntalos itself
    qint64 observedPeakRssKiB = 0; /// as sampled by the runner, also available if the run was killed
    qint64 bytesWritten = 0;
    int cpuCores = 0;
    int threadsTotal = 0;
    int threadsElevated = 0;

    QList<MeterStats> meters;
    QList<ConnectionStats> connections;
    QList<ModuleUsage> modules;

    QJsonObject rawStats; /// the complete statistics document, for the report
    QString outputTail;   /// last lines of the Syntalos output, for diagnostics

    [[nodiscard]] const MeterStats *meter(const QString &moduleName) const;
    [[nodiscard]] double processLoad() const; /// CPU seconds per wall-clock second in the usage window
};

/**
 * @brief Runs Syntalos on generated projects and collects the run statistics
 */
class SyntalosRunner
{
public:
    using LogFn = std::function<void(const QString &)>;

    SyntalosRunner();

    /**
     * @brief Find the syntalos executable next to our own binary, in the build tree or in PATH.
     * @return The path, or an empty string if nothing was found.
     */
    static QString findSyntalosBinary();

    void setSyntalosBinary(const QString &path);
    QString syntalosBinary() const;

    void setLogHandler(LogFn fn);

    /**
     * @brief Run one step synchronously. Blocks for the whole run.
     *
     * A failed, killed or cancelled run is still a result; the error branch is only
     * taken when Syntalos could not be launched at all.
     */
    auto run(const StepRunConfig &cfg) -> std::expected<StepResult, QString>;

    /**
     * @brief Abort a running step from another thread.
     */
    void cancel();
    bool isCancelled() const;
    void resetCancel();

private:
    QString m_bin;
    LogFn m_log;
    std::atomic_bool m_cancel{false};

    void log(const QString &msg) const;
};

/**
 * @brief Resident memory of a process and all its descendants, in KiB.
 */
qint64 processTreeRssKiB(qint64 pid);

/**
 * @brief Parse a Syntalos run statistics document into a step result.
 */
void parseRunStatistics(const QJsonObject &stats, StepResult &result);

} // namespace SyBench

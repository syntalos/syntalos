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

#include <QList>
#include <QString>
#include <atomic>
#include <expected>
#include <optional>

#include "fabric/runstatistics.h"
#include "logging.h"

namespace SyBench
{

/**
 * @brief How to run one benchmark step
 */
struct StepRunConfig {
    QString projectFile;
    int durationSec = 20;
    bool ephemeral = true; /// ephemeral runs store nothing permanently; set false for disk-write tests
    QString exportDir;     /// export directory override for non-ephemeral runs
    QString statsFile;     /// where Syntalos should write its run statistics JSON
    /// time allowed until Syntalos reports that all modules are running (project loading, module startup)
    int startupTimeoutSec = 300;
    /// extra time allowed after the run duration for stopping and teardown
    int teardownGraceSec = 60;

    /// Stop the run when Syntalos (and its workers) use more resident memory than this.
    /// 0 = automatic: the memory available when the run starts, minus 2 GiB of headroom.
    qint64 memoryLimitKiB = 0;
    /// Kill the run when the whole system has less memory available than this (default 1 GiB).
    qint64 systemMemoryFloorKiB = 1024 * 1024;
};

/**
 * @brief Counts a flow meter module collected, taken from its module statistics
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

    static std::optional<MeterStats> fromModule(const Syntalos::ModuleRunStats &mod);
};

/**
 * @brief Why the runner stopped Syntalos before it ended the run on its own
 */
enum class StopCause {
    None,           /// Syntalos was not stopped by us
    Cancelled,      /// the benchmark was cancelled
    MemoryLimit,    /// Syntalos used too much memory
    StartupTimeout, /// the modules did not all start in time
    RunTimeout      /// the run did not finish in time
};

QString stopCauseToString(StopCause cause);

/**
 * @brief Everything we learned from one Syntalos run
 */
struct StepResult {
    bool success = false; /// the run completed without error
    StopCause stopCause = StopCause::None;
    bool started = false;  /// Syntalos reported that all modules were running
    double startupSec = 0; /// time from launch until all modules were running
    QString failureReason;
    int exitCode = -1;
    qint64 observedPeakRssKiB = 0; /// sampled by the runner, also available if the run was killed
    QString outputTail;            /// last lines of the Syntalos output, for diagnostics

    std::optional<Syntalos::RunStatistics> stats; /// what Syntalos reported, if it got that far

    [[nodiscard]] double durationSec() const;
    [[nodiscard]] double processCpuSec() const;
    [[nodiscard]] double processLoad() const; /// CPU seconds per wall-clock second in the usage window
    [[nodiscard]] qint64 peakRssKiB() const;  /// the larger of the reported and the observed peak
    [[nodiscard]] int threadsTotal() const;
    [[nodiscard]] int threadsElevated() const;
    [[nodiscard]] QList<MeterStats> meters() const;
    [[nodiscard]] std::optional<MeterStats> meter(const QString &moduleName) const;
};

/**
 * @brief Runs Syntalos on generated projects and collects the run statistics
 */
class SyntalosRunner
{
public:
    SyntalosRunner();

    /**
     * @brief Find the syntalos executable next to our own binary, in the build tree or in PATH.
     * @return The path, or an empty string if nothing was found.
     */
    static QString findSyntalosBinary();

    void setSyntalosBinary(const QString &path);
    QString syntalosBinary() const;

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
    quill::Logger *m_log;
    std::atomic_bool m_cancel{false};
};

} // namespace SyBench

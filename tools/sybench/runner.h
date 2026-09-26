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

#include <QString>
#include <QVariantList>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>

#include "fabric/runstatistics.h"
#include "logging.h"

namespace SyBench
{

/// memory the default system reserve leaves to the rest of the system
constexpr qint64 kMemoryReserveKiB = 2LL * 1024 * 1024;

/**
 * @brief How to run one benchmark step
 */
struct StepRunConfig {
    QString projectFile;
    int durationSec = 20;
    bool ephemeral = true; /// ephemeral runs store nothing permanently; set false for disk-write tests
    QString exportDir;     /// export directory override for non-ephemeral runs
    QString statsFile;     /// where the run statistics JSON of the step is stored
    /// time allowed for loading the project, and again until Syntalos reports that all modules are running
    int startupTimeoutSec = 300;
    /// extra time allowed after the run duration for stopping and teardown
    int teardownGraceSec = 60;
    /// time allowed for a freshly launched Syntalos to answer on D-Bus
    int launchTimeoutSec = 60;
    /// Relaunch Syntalos before the next step when it kept more memory than this after a run
    /// (compared to before the run started), so a leaking instance does not distort later measurements.
    qint64 retainedMemoryRestartKiB = 4LL * 1024 * 1024;

    /// Stop the run when Syntalos (and its workers) use more proportional memory than this, 0 = no limit.
    /// The system reserve below applies in any case.
    qint64 memoryLimitKiB = 0;
    /// Stop the run when the whole system has less memory available than this (default 2 GiB).
    qint64 systemMemoryReserveKiB = kMemoryReserveKiB;
    /// Kill the run when the whole system has less memory available than this (default 1 GiB).
    qint64 systemMemoryFloorKiB = 1024 * 1024;
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
    bool started = false;       /// Syntalos reported that all modules were running
    double loadSec = 0;         /// time Syntalos took to load the project
    double startupSec = 0;      /// time from the start request until all modules were running
    bool freshInstance = false; /// Syntalos was launched for this step, so it ran with cold caches
    QString failureReason;
    qint64 peakPssKiB = 0; /// peak proportional memory of Syntalos and its workers, sampled every second
    QString outputTail;    /// last lines of the Syntalos output, for diagnostics

    std::optional<Syntalos::RunStatistics> stats; /// what Syntalos reported, if it got that far

    [[nodiscard]] double durationSec() const;
    /// CPU time of Syntalos and its worker processes over the usage window
    [[nodiscard]] double processCpuSec() const;
    /// CPU seconds per wall-clock second in the usage window, an average over the run
    [[nodiscard]] double processLoad() const;
    /// the load as a share of the machine's logical CPUs, 0 if unknown
    [[nodiscard]] double loadPercent() const;
    [[nodiscard]] int threadsTotal() const;
    [[nodiscard]] int threadsElevated() const;
};

/**
 * @brief Runs Syntalos on generated projects and collects the run statistics
 *
 * One Syntalos instance is launched and then driven over D-Bus for step after step,
 * so no window pops up for every run and the process stays warm. It is only relaunched
 * when it died, had to be killed, or kept too much memory after a run.
 */
class SyntalosRunner
{
public:
    SyntalosRunner();
    ~SyntalosRunner();

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
     * taken when Syntalos can not run at all (it could not be launched, or another
     * instance is running), so no other step would succeed either.
     * A stop requested through the token (from any thread) cancels the run.
     */
    auto run(const StepRunConfig &cfg, std::stop_token stop = {}) -> std::expected<StepResult, QString>;

    /**
     * @brief Ask the Syntalos instance to quit, and kill it if it does not.
     *
     * Must be called from the thread that ran the steps.
     */
    void shutdown();

private:
    struct Instance;

    /// Launch Syntalos if no usable instance is running, and wait until it answers on D-Bus.
    auto ensureStarted(const StepRunConfig &cfg, std::stop_token stop) -> std::expected<void, QString>;
    bool isAlive() const;
    void markForRelaunch(const QString &reason);

    /// Call a method of the control interface; a failed call marks the instance for relaunch.
    auto callSyntalos(const QString &method, const QVariantList &args, int timeoutMs)
        -> std::expected<QVariantList, QString>;
    /// Call a method that answers with an error message, or an empty string on success.
    auto requestSyntalos(const QString &method, const QVariantList &args, int timeoutMs)
        -> std::expected<void, QString>;
    auto readState() -> std::expected<QString, QString>;

    QString m_bin;
    quill::Logger *m_log;
    std::unique_ptr<Instance> m_inst;
    QString m_relaunchReason; /// why the next step needs a fresh instance, empty if it does not
};

} // namespace SyBench

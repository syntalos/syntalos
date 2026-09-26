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

#include "runner.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

#include "executils.h"
#include "exitcodes.h"
#include "logging.h"
#include "utils/resourceinfo.h"

using namespace Syntalos;

namespace SyBench
{

QString stopCauseToString(StopCause cause)
{
    switch (cause) {
    case StopCause::None:
        return QStringLiteral("none");
    case StopCause::Cancelled:
        return QStringLiteral("cancelled");
    case StopCause::MemoryLimit:
        return QStringLiteral("memory-limit");
    case StopCause::StartupTimeout:
        return QStringLiteral("startup-timeout");
    case StopCause::RunTimeout:
        return QStringLiteral("run-timeout");
    }
    return QStringLiteral("unknown");
}

/**
 * Explain why a run Syntalos ended by itself did not succeed.
 */
static QString failureReasonFromExit(const StepResult &r, bool crashed)
{
    if (r.stats && r.stats->failed && !r.stats->failReason.isEmpty())
        return r.stats->failReason;
    if (crashed)
        return QStringLiteral("Syntalos crashed.");
    switch (r.exitCode) {
    case SY_EXIT_SUCCESS:
        return QStringLiteral("Syntalos wrote no run statistics.");
    case SY_EXIT_RUN_FAILED:
        return QStringLiteral("The run failed (no details recorded).");
    case SY_EXIT_TERMINATED:
        return QStringLiteral("Syntalos was asked to stop by something other than the benchmark.");
    default:
        return QStringLiteral("Syntalos exited with code %1.").arg(r.exitCode);
    }
}

double StepResult::durationSec() const
{
    return stats ? stats->durationSec : 0.0;
}

double StepResult::processCpuSec() const
{
    return (stats && stats->process) ? stats->process->cpuTimeSec() : 0.0;
}

double StepResult::processLoad() const
{
    if (!stats || stats->usageWindowSec <= 0)
        return 0;
    return processCpuSec() / stats->usageWindowSec;
}

int StepResult::threadsTotal() const
{
    return stats ? stats->threadsTotal : 0;
}

int StepResult::threadsElevated() const
{
    return stats ? stats->threadsElevated : 0;
}

SyntalosRunner::SyntalosRunner()
    : m_bin(findSyntalosBinary()),
      m_log(getLogger("bench.runner"))
{
}

QString SyntalosRunner::findSyntalosBinary()
{
    const auto appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("syntalos")),
        // running from the build tree: build/<name>/tools/sybench -> build/<name>/src/syntalos
        QDir(appDir).filePath(QStringLiteral("../../src/syntalos")),
    };
    for (const auto &c : candidates) {
        QFileInfo fi(c);
        if (fi.isFile() && fi.isExecutable())
            return fi.canonicalFilePath();
    }
    return findHostExecutable(QStringLiteral("syntalos"));
}

void SyntalosRunner::setSyntalosBinary(const QString &path)
{
    m_bin = path;
}

QString SyntalosRunner::syntalosBinary() const
{
    return m_bin;
}

auto SyntalosRunner::run(const StepRunConfig &cfg, std::stop_token stop) -> std::expected<StepResult, QString>
{
    if (m_bin.isEmpty())
        return std::unexpected(QStringLiteral("The syntalos executable was not found."));

    StepResult r;
    if (stop.stop_requested()) {
        r.stopCause = StopCause::Cancelled;
        r.failureReason = QStringLiteral("Cancelled.");
        return r;
    }

    // start from a clean slate, so a stale file can never be mistaken for this run's result
    QFile::remove(cfg.statsFile);

    QStringList args;
    args << QStringLiteral("--non-interactive") << QStringLiteral("--run-for") << QString::number(cfg.durationSec)
         << QStringLiteral("--stats-out") << cfg.statsFile;
    if (cfg.ephemeral)
        args << QStringLiteral("--ephemeral");
    else if (!cfg.exportDir.isEmpty())
        args << QStringLiteral("--export-dir") << cfg.exportDir;
    args << cfg.projectFile;

    QProcess proc;
    proc.setProgram(m_bin);
    proc.setArguments(args);
    proc.setProcessChannelMode(QProcess::MergedChannels);
    LOG_INFO(m_log, "Launching: {} {}", m_bin, args.join(QLatin1Char(' ')));
    proc.start();
    if (!proc.waitForStarted(10000))
        return std::unexpected(QStringLiteral("Unable to start Syntalos: %1").arg(proc.errorString()));

    // the engine logs this line once all modules run; everything before is startup
    static const QLatin1String startedMarker("all modules are running");

    QElapsedTimer timer;
    timer.start();
    QStringList outLines;
    const auto collectOutput = [&]() {
        while (proc.canReadLine()) {
            const auto line = QString::fromUtf8(proc.readLine()).trimmed();
            if (line.isEmpty())
                continue;
            if (!r.started && line.contains(startedMarker)) {
                r.started = true;
                r.startupSec = timer.elapsed() / 1000.0;
                LOG_INFO(m_log, "All modules running after {:.1f} s", r.startupSec);
            }
            outLines.append(line);
            if (outLines.size() > 60)
                outLines.removeFirst();
        }
    };

    qint64 memoryLimitKiB = cfg.memoryLimitKiB;
    if (memoryLimitKiB <= 0)
        memoryLimitKiB = std::max<qint64>(readMemInfo().memAvailableKiB - 2LL * 1024 * 1024, 512 * 1024);
    const qint64 pid = proc.processId();

    // Syntalos stops its run and quits cleanly on SIGTERM, so we ask first and only kill it
    // if it does not manage to stop within the grace period
    enum class StopState {
        Running,
        Terminating,
        Killed
    };
    auto stopState = StopState::Running;
    qint64 killDeadlineMs = 0;
    const auto stopProcess = [&](StopCause cause, const QString &reason, int graceMs) {
        if (r.stopCause == StopCause::None) {
            r.stopCause = cause;
            r.failureReason = reason;
        }
        if (stopState != StopState::Running)
            return;
        LOG_WARNING(m_log, "{} Asking Syntalos to stop...", reason);
        proc.terminate();
        stopState = StopState::Terminating;
        killDeadlineMs = timer.elapsed() + graceMs;
    };
    const auto killProcess = [&](const QString &why) {
        LOG_WARNING(m_log, "{}, killing Syntalos", why);
        proc.kill();
        stopState = StopState::Killed;
    };

    qint64 lastRssKiB = 0;
    qint64 lastRssMs = 0;
    double growthMiBPerSec = 0;
    while (!proc.waitForFinished(100)) {
        collectOutput();
        if (proc.state() == QProcess::NotRunning)
            break;
        const auto nowMs = timer.elapsed();

        // An overloaded Syntalos lets its data queues grow without bound and can take the whole
        // machine down with it, so we watch its memory and stop the run before that happens.
        // The guard uses the resident size, which is cheap to read but counts shared memory once
        // per process and so errs on the safe side. The reported peak is the exact proportional
        // size, which is expensive to read, so we only sample it once per second.
        const auto rssKiB = readProcessTreeRssKiB(pid);
        if (nowMs - lastRssMs >= 1000) {
            if (lastRssMs > 0)
                growthMiBPerSec = (rssKiB - lastRssKiB) / 1024.0 / ((nowMs - lastRssMs) / 1000.0);
            lastRssKiB = rssKiB;
            lastRssMs = nowMs;
            r.peakPssKiB = std::max(r.peakPssKiB, readProcessTreePssKiB(pid));
        }
        const auto memAvailableKiB = readMemInfo().memAvailableKiB;
        const bool systemStarved = memAvailableKiB < cfg.systemMemoryFloorKiB;
        if (rssKiB > memoryLimitKiB || systemStarved)
            stopProcess(
                StopCause::MemoryLimit,
                QStringLiteral(
                    "Memory limit exceeded: Syntalos used %1 MiB (limit %2 MiB, %3 MiB left "
                    "on the system), growing by %4 MiB/s. Fast growth means its data queues "
                    "were overflowing.")
                    .arg(rssKiB / 1024)
                    .arg(memoryLimitKiB / 1024)
                    .arg(memAvailableKiB / 1024)
                    .arg(growthMiBPerSec, 0, 'f', 0),
                5000);
        if (stop.stop_requested())
            stopProcess(StopCause::Cancelled, QStringLiteral("Cancelled."), 15000);
        if (!r.started && nowMs > cfg.startupTimeoutSec * 1000LL)
            stopProcess(
                StopCause::StartupTimeout,
                QStringLiteral("Syntalos did not start the run within %1 s.").arg(cfg.startupTimeoutSec),
                15000);
        if (r.started && nowMs > (r.startupSec + cfg.durationSec + cfg.teardownGraceSec) * 1000.0)
            stopProcess(StopCause::RunTimeout, QStringLiteral("Syntalos did not finish the run in time."), 15000);

        if (stopState == StopState::Terminating) {
            // a stopping process that still eats memory is not going to make it, kill it before the machine suffers
            if (systemStarved)
                killProcess(QStringLiteral("System memory is running out"));
            else if (nowMs > killDeadlineMs)
                killProcess(QStringLiteral("Syntalos did not stop in time"));
        }
    }
    collectOutput();
    proc.readAll();
    r.outputTail = outLines.join(QLatin1Char('\n'));
    const bool crashed = proc.exitStatus() == QProcess::CrashExit;
    r.exitCode = crashed ? -1 : proc.exitCode();
    if (r.exitCode == SY_EXIT_ALREADY_RUNNING)
        return std::unexpected(QStringLiteral("Another Syntalos instance is running. Close it before benchmarking."));

    if (r.stopCause == StopCause::Cancelled)
        return r;

    if (QFile::exists(cfg.statsFile)) {
        if (auto stats = RunStatistics::loadJson(cfg.statsFile); stats)
            r.stats = std::move(*stats);
        else
            LOG_WARNING(m_log, "{}", stats.error());
    }

    // a run we had to stop never counts, even if Syntalos managed to write its statistics
    r.success = r.stopCause == StopCause::None && r.exitCode == SY_EXIT_SUCCESS && r.stats && !r.stats->failed;
    if (!r.success && r.failureReason.isEmpty())
        r.failureReason = failureReasonFromExit(r, crashed);

    LOG_INFO(
        m_log,
        "Run finished: exit code {}, {}",
        r.exitCode,
        r.success ? QStringLiteral("success") : r.failureReason);
    return r;
}

} // namespace SyBench

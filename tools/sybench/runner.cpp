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
#include "logging.h"
#include "utils/resourceinfo.h"

using namespace Syntalos;

namespace SyBench
{

// exit codes of the syntalos executable (see src/mainwindow.h)
static constexpr int SY_EXIT_RUN_FAILED = 5;
static constexpr int SY_EXIT_ALREADY_RUNNING = 6;

std::optional<MeterStats> MeterStats::fromModule(const ModuleRunStats &mod)
{
    if (mod.id != QLatin1String("flowmeter") || mod.moduleStats.isEmpty())
        return std::nullopt;
    const auto &ms = mod.moduleStats;
    MeterStats m;
    m.moduleName = mod.name;
    m.items = ms.value(QStringLiteral("items")).toLongLong();
    m.intervalMeanUs = ms.value(QStringLiteral("interval_mean_us")).toLongLong();
    m.intervalMaxUs = ms.value(QStringLiteral("interval_max_us")).toLongLong();
    m.ageP50Us = ms.value(QStringLiteral("age_p50_us")).toLongLong();
    m.ageP95Us = ms.value(QStringLiteral("age_p95_us")).toLongLong();
    m.ageP99Us = ms.value(QStringLiteral("age_p99_us")).toLongLong();
    m.ageMaxUs = ms.value(QStringLiteral("age_max_us")).toLongLong();
    return m;
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

qint64 StepResult::peakRssKiB() const
{
    return std::max(stats ? stats->peakRssKiB : 0, observedPeakRssKiB);
}

int StepResult::threadsTotal() const
{
    return stats ? stats->threadsTotal : 0;
}

int StepResult::threadsElevated() const
{
    return stats ? stats->threadsElevated : 0;
}

QList<MeterStats> StepResult::meters() const
{
    QList<MeterStats> res;
    if (!stats)
        return res;
    for (const auto &mod : stats->modules) {
        if (const auto m = MeterStats::fromModule(mod))
            res.append(*m);
    }
    return res;
}

std::optional<MeterStats> StepResult::meter(const QString &moduleName) const
{
    if (!stats)
        return std::nullopt;
    for (const auto &mod : stats->modules) {
        if (mod.name == moduleName)
            return MeterStats::fromModule(mod);
    }
    return std::nullopt;
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

void SyntalosRunner::cancel()
{
    m_cancel = true;
}

bool SyntalosRunner::isCancelled() const
{
    return m_cancel;
}

void SyntalosRunner::resetCancel()
{
    m_cancel = false;
}

auto SyntalosRunner::run(const StepRunConfig &cfg) -> std::expected<StepResult, QString>
{
    if (m_bin.isEmpty())
        return std::unexpected(QStringLiteral("The syntalos executable was not found."));

    StepResult r;
    if (m_cancel) {
        r.cancelled = true;
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

    // graceful stop first: a healthy but overloaded Syntalos shuts down cleanly on SIGTERM
    qint64 killDeadlineMs = 0;
    const auto stopProcess = [&](int graceMs) {
        if (killDeadlineMs != 0)
            return;
        proc.terminate();
        killDeadlineMs = timer.elapsed() + graceMs;
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
        const auto rssKiB = readProcessTreeRssKiB(pid);
        r.observedPeakRssKiB = std::max(r.observedPeakRssKiB, rssKiB);
        if (nowMs - lastRssMs >= 1000) {
            if (lastRssMs > 0)
                growthMiBPerSec = (rssKiB - lastRssKiB) / 1024.0 / ((nowMs - lastRssMs) / 1000.0);
            lastRssKiB = rssKiB;
            lastRssMs = nowMs;
        }
        const auto memAvailableKiB = readMemInfo().memAvailableKiB;
        const bool systemStarved = memAvailableKiB < cfg.systemMemoryFloorKiB;
        if (!r.memoryExceeded && (rssKiB > memoryLimitKiB || systemStarved)) {
            r.memoryExceeded = true;
            r.failureReason = QStringLiteral(
                                  "Memory limit exceeded: Syntalos used %1 MiB (limit %2 MiB, %3 MiB left "
                                  "on the system), growing by %4 MiB/s. Fast growth means its data queues "
                                  "were overflowing.")
                                  .arg(rssKiB / 1024)
                                  .arg(memoryLimitKiB / 1024)
                                  .arg(memAvailableKiB / 1024)
                                  .arg(growthMiBPerSec, 0, 'f', 0);
            LOG_WARNING(m_log, "Memory limit exceeded ({} MiB used), asking Syntalos to stop...", rssKiB / 1024);
            stopProcess(5000);
        }
        // a stopping process that still eats memory is not going to make it, kill it before the machine suffers
        if (r.memoryExceeded && systemStarved && killDeadlineMs > nowMs) {
            LOG_WARNING(m_log, "System memory is running out, killing Syntalos");
            proc.kill();
            killDeadlineMs = nowMs;
        }

        if (m_cancel) {
            if (!r.cancelled)
                LOG_INFO(m_log, "Cancelling run, terminating Syntalos...");
            r.cancelled = true;
            stopProcess(15000);
        } else if (killDeadlineMs == 0) {
            const bool startupTimedOut = !r.started && nowMs > cfg.startupTimeoutSec * 1000LL;
            const bool runTimedOut = r.started
                                     && nowMs > (r.startupSec + cfg.durationSec + cfg.teardownGraceSec) * 1000.0;
            if (startupTimedOut || runTimedOut) {
                r.failureReason = startupTimedOut ? QStringLiteral("Syntalos did not start the run within %1 s.")
                                                        .arg(cfg.startupTimeoutSec)
                                                  : QStringLiteral("Syntalos did not finish the run in time.");
                LOG_WARNING(m_log, "{} Terminating...", r.failureReason);
                stopProcess(15000);
            }
        }
        if (killDeadlineMs != 0 && nowMs > killDeadlineMs) {
            LOG_WARNING(m_log, "Syntalos did not stop in time, killing it");
            proc.kill();
            killDeadlineMs = nowMs + 3600000; // do not repeat
        }
    }
    collectOutput();
    proc.readAll();
    r.outputTail = outLines.join(QLatin1Char('\n'));
    r.exitCode = (proc.exitStatus() == QProcess::NormalExit) ? proc.exitCode() : -1;

    if (r.cancelled) {
        r.failureReason = QStringLiteral("Cancelled.");
        return r;
    }

    if (QFile::exists(cfg.statsFile)) {
        if (auto stats = RunStatistics::loadJson(cfg.statsFile); stats) {
            r.success = !stats->failed;
            if (stats->failed)
                r.failureReason = stats->failReason;
            r.stats = std::move(*stats);
        } else {
            LOG_WARNING(m_log, "{}", stats.error());
        }
    }

    if (r.memoryExceeded) {
        r.success = false;
    } else if (r.exitCode == SY_EXIT_ALREADY_RUNNING) {
        r.success = false;
        r.failureReason = QStringLiteral("Another Syntalos instance is running. Close it before benchmarking.");
    } else if (r.exitCode != 0 && r.failureReason.isEmpty()) {
        r.success = false;
        r.failureReason = (r.exitCode == SY_EXIT_RUN_FAILED)
                              ? QStringLiteral("The run failed (no details recorded).")
                              : QStringLiteral("Syntalos exited with code %1.").arg(r.exitCode);
    } else if (r.exitCode == 0 && !r.stats) {
        r.success = false;
        r.failureReason = QStringLiteral("Syntalos wrote no run statistics.");
    }

    LOG_INFO(
        m_log,
        "Run finished: exit code {}, {}",
        r.exitCode,
        r.success ? QStringLiteral("success") : r.failureReason);
    return r;
}

} // namespace SyBench

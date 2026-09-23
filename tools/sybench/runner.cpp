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
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <unistd.h>

#include "utils/resourceinfo.h"

namespace SyBench
{

// exit codes of the syntalos executable (see src/mainwindow.h)
static constexpr int SY_EXIT_RUN_FAILED = 5;
static constexpr int SY_EXIT_ALREADY_RUNNING = 6;

const MeterStats *StepResult::meter(const QString &moduleName) const
{
    for (const auto &m : meters) {
        if (m.moduleName == moduleName)
            return &m;
    }
    return nullptr;
}

double StepResult::processLoad() const
{
    if (usageWindowSec <= 0)
        return 0;
    return processCpuSec / usageWindowSec;
}

SyntalosRunner::SyntalosRunner()
    : m_bin(findSyntalosBinary())
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
    return QStandardPaths::findExecutable(QStringLiteral("syntalos"));
}

void SyntalosRunner::setSyntalosBinary(const QString &path)
{
    m_bin = path;
}

QString SyntalosRunner::syntalosBinary() const
{
    return m_bin;
}

void SyntalosRunner::setLogHandler(LogFn fn)
{
    m_log = std::move(fn);
}

void SyntalosRunner::log(const QString &msg) const
{
    if (m_log)
        m_log(msg);
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

static qint64 processRssKiB(qint64 pid)
{
    QFile f(QStringLiteral("/proc/%1/statm").arg(pid));
    if (!f.open(QIODevice::ReadOnly))
        return 0;
    const auto parts = f.readAll().split(' ');
    if (parts.size() < 2)
        return 0;
    static const long pageKiB = sysconf(_SC_PAGESIZE) / 1024;
    return parts[1].toLongLong() * pageKiB;
}

qint64 processTreeRssKiB(qint64 pid)
{
    qint64 total = processRssKiB(pid);

    // find direct children by scanning the parent pid of every process
    QDir procDir(QStringLiteral("/proc"));
    const auto entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &entry : entries) {
        bool isPid = false;
        const qint64 childPid = entry.toLongLong(&isPid);
        if (!isPid || childPid == pid)
            continue;
        QFile f(QStringLiteral("/proc/%1/stat").arg(childPid));
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const auto stat = f.readAll();
        // the parent pid is the first field after the parenthesized command name
        const int nameEnd = stat.lastIndexOf(')');
        if (nameEnd < 0)
            continue;
        const auto fields = stat.mid(nameEnd + 2).split(' ');
        if (fields.size() < 2)
            continue;
        if (fields[1].toLongLong() == pid)
            total += processTreeRssKiB(childPid);
    }
    return total;
}

static double usageCpuSec(const QJsonObject &usage)
{
    return usage.value(QStringLiteral("user_time_sec")).toDouble()
           + usage.value(QStringLiteral("system_time_sec")).toDouble();
}

void parseRunStatistics(const QJsonObject &stats, StepResult &r)
{
    r.rawStats = stats;
    const auto run = stats.value(QStringLiteral("run")).toObject();
    r.success = run.value(QStringLiteral("success")).toBool(false);
    if (!r.success && r.failureReason.isEmpty())
        r.failureReason = run.value(QStringLiteral("failure_reason")).toString();
    r.durationSec = run.value(QStringLiteral("duration_sec")).toDouble();
    r.usageWindowSec = run.value(QStringLiteral("usage_window_sec")).toDouble();
    r.processCpuSec = usageCpuSec(run.value(QStringLiteral("process")).toObject());
    r.peakRssKiB = run.value(QStringLiteral("peak_rss_kib")).toInteger();
    r.bytesWritten = run.value(QStringLiteral("bytes_written")).toInteger();
    r.cpuCores = run.value(QStringLiteral("cpu_cores")).toInt();
    r.threadsTotal = run.value(QStringLiteral("threads_total")).toInt();
    r.threadsElevated = run.value(QStringLiteral("threads_elevated")).toInt();

    r.meters.clear();
    r.modules.clear();
    const auto mods = stats.value(QStringLiteral("modules")).toArray();
    for (const auto &mv : mods) {
        const auto mo = mv.toObject();
        ModuleUsage mu;
        mu.name = mo.value(QStringLiteral("name")).toString();
        mu.id = mo.value(QStringLiteral("id")).toString();
        mu.driver = mo.value(QStringLiteral("driver")).toString();
        mu.eventThread = mo.value(QStringLiteral("event_thread")).toString();
        mu.outOfProcess = mo.value(QStringLiteral("out_of_process")).toBool();
        mu.realtimeApplied = mo.value(QStringLiteral("realtime_applied")).toBool();
        if (mo.contains(QStringLiteral("worker")))
            mu.cpuSec = usageCpuSec(mo.value(QStringLiteral("worker")).toObject());
        else
            mu.cpuSec = usageCpuSec(mo.value(QStringLiteral("thread")).toObject());
        r.modules.append(mu);

        if (mu.id != QLatin1String("flowmeter"))
            continue;
        const auto ms = mo.value(QStringLiteral("module_stats")).toObject();
        if (ms.isEmpty())
            continue;
        MeterStats meter;
        meter.moduleName = mu.name;
        meter.items = ms.value(QStringLiteral("items")).toInteger();
        meter.intervalMeanUs = ms.value(QStringLiteral("interval_mean_us")).toInteger();
        meter.intervalMaxUs = ms.value(QStringLiteral("interval_max_us")).toInteger();
        meter.ageP50Us = ms.value(QStringLiteral("age_p50_us")).toInteger();
        meter.ageP95Us = ms.value(QStringLiteral("age_p95_us")).toInteger();
        meter.ageP99Us = ms.value(QStringLiteral("age_p99_us")).toInteger();
        meter.ageMaxUs = ms.value(QStringLiteral("age_max_us")).toInteger();
        r.meters.append(meter);
    }

    r.connections.clear();
    const auto conns = stats.value(QStringLiteral("connections")).toArray();
    for (const auto &cv : conns) {
        const auto co = cv.toObject();
        ConnectionStats cs;
        cs.srcModule = co.value(QStringLiteral("src_module")).toString();
        cs.srcPort = co.value(QStringLiteral("src_port")).toString();
        cs.dstModule = co.value(QStringLiteral("dst_module")).toString();
        cs.dstPort = co.value(QStringLiteral("dst_port")).toString();
        cs.peakPending = co.value(QStringLiteral("peak_pending")).toInteger();
        cs.pendingAtStop = co.value(QStringLiteral("pending_at_stop")).toInteger();
        cs.directIpc = co.value(QStringLiteral("direct_ipc")).toBool();
        r.connections.append(cs);
    }
}

StepResult SyntalosRunner::run(const StepRunConfig &cfg)
{
    StepResult r;
    if (m_bin.isEmpty()) {
        r.failureReason = QStringLiteral("The syntalos executable was not found.");
        return r;
    }
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
    log(QStringLiteral("Launching: %1 %2").arg(m_bin, args.join(QLatin1Char(' '))));
    proc.start();
    if (!proc.waitForStarted(10000)) {
        r.failureReason = QStringLiteral("Unable to start Syntalos: %1").arg(proc.errorString());
        return r;
    }
    r.launched = true;

    QStringList outLines;
    const auto collectOutput = [&]() {
        while (proc.canReadLine()) {
            const auto line = QString::fromUtf8(proc.readLine()).trimmed();
            if (line.isEmpty())
                continue;
            outLines.append(line);
            if (outLines.size() > 60)
                outLines.removeFirst();
        }
    };

    qint64 memoryLimitKiB = cfg.memoryLimitKiB;
    if (memoryLimitKiB <= 0)
        memoryLimitKiB = Syntalos::readMemInfo().memAvailableKiB * 6 / 10;
    const qint64 pid = proc.processId();

    QElapsedTimer timer;
    timer.start();
    const qint64 hardLimitMs = (cfg.durationSec + cfg.startupGraceSec) * 1000LL;
    qint64 killDeadlineMs = 0;
    const auto stopProcess = [&](const QString &why, int graceMs) {
        if (killDeadlineMs != 0)
            return;
        log(why);
        proc.terminate();
        killDeadlineMs = timer.elapsed() + graceMs;
    };
    while (!proc.waitForFinished(100)) {
        collectOutput();
        if (proc.state() == QProcess::NotRunning)
            break;

        // An overloaded Syntalos lets its data queues grow without bound and can take the whole
        // machine down with it, so we watch its memory and stop the run before that happens.
        const auto rssKiB = processTreeRssKiB(pid);
        r.observedPeakRssKiB = std::max(r.observedPeakRssKiB, rssKiB);
        const auto memAvailableKiB = Syntalos::readMemInfo().memAvailableKiB;
        if (rssKiB > memoryLimitKiB || memAvailableKiB < cfg.systemMemoryFloorKiB) {
            r.memoryExceeded = true;
            r.failureReason = QStringLiteral(
                                  "Memory limit exceeded: Syntalos used %1 MiB (limit %2 MiB, %3 MiB "
                                  "left on the system). Its data queues were most likely overflowing.")
                                  .arg(rssKiB / 1024)
                                  .arg(memoryLimitKiB / 1024)
                                  .arg(memAvailableKiB / 1024);
            // no graceful stop here: the engine would keep filling its queues while draining them
            log(QStringLiteral("Memory limit exceeded, killing Syntalos..."));
            proc.kill();
            killDeadlineMs = timer.elapsed();
        }

        if (m_cancel) {
            r.cancelled = true;
            stopProcess(QStringLiteral("Cancelling run, terminating Syntalos..."), 15000);
        } else if (timer.elapsed() > hardLimitMs && killDeadlineMs == 0) {
            r.failureReason = QStringLiteral("Syntalos did not finish the run in time.");
            stopProcess(QStringLiteral("Run exceeded its time limit, terminating Syntalos..."), 15000);
        }
        if (killDeadlineMs != 0 && timer.elapsed() > killDeadlineMs)
            proc.kill();
    }
    collectOutput();
    proc.readAll();
    r.outputTail = outLines.join(QLatin1Char('\n'));
    r.exitCode = (proc.exitStatus() == QProcess::NormalExit) ? proc.exitCode() : -1;

    if (r.cancelled) {
        r.failureReason = QStringLiteral("Cancelled.");
        return r;
    }

    QFile f(cfg.statsFile);
    if (f.open(QIODevice::ReadOnly)) {
        const auto doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject())
            parseRunStatistics(doc.object(), r);
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
    } else if (r.exitCode == 0 && r.rawStats.isEmpty()) {
        r.success = false;
        r.failureReason = QStringLiteral("Syntalos wrote no run statistics.");
    }

    log(QStringLiteral("Run finished: exit code %1, %2")
            .arg(r.exitCode)
            .arg(r.success ? QStringLiteral("success") : r.failureReason));
    return r;
}

} // namespace SyBench

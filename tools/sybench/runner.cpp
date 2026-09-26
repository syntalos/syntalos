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
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QSaveFile>

#include "dbuscontrol-defs.h"
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
 * Explain why a run that Syntalos ended on its own did not succeed.
 */
static QString failureReasonFromOutcome(const StepResult &r, const QProcess &proc, const QString &runMessage)
{
    if (r.stats && r.stats->failed && !r.stats->failReason.isEmpty())
        return r.stats->failReason;
    if (proc.state() != QProcess::Running) {
        if (proc.exitStatus() == QProcess::CrashExit)
            return QStringLiteral("Syntalos crashed.");
        if (proc.exitCode() == SY_EXIT_TERMINATED)
            return QStringLiteral("Syntalos was asked to stop by something other than the benchmark.");
        return QStringLiteral("Syntalos exited with code %1.").arg(proc.exitCode());
    }
    if (!runMessage.isEmpty())
        return runMessage;
    if (!r.stats)
        return QStringLiteral("Syntalos reported no run statistics.");
    return QStringLiteral("The run failed (no details recorded).");
}

/**
 * Receives the run signals of the Syntalos D-Bus interface in the runner's thread.
 */
class RunSignalSink : public QObject
{
    Q_OBJECT
public:
    bool runStarted = false;
    bool runStopped = false;
    bool runSuccess = false;
    QString runMessage;

    void reset()
    {
        runStarted = false;
        runStopped = false;
        runSuccess = false;
        runMessage.clear();
    }

public slots:
    void onRunStarted()
    {
        runStarted = true;
    }

    void onRunStopped(bool success, const QString &message)
    {
        runStopped = true;
        runSuccess = success;
        runMessage = message;
    }
};

/**
 * The Syntalos process we drive, and what we know about it.
 */
struct SyntalosRunner::Instance {
    QProcess proc;
    qint64 pid = 0;
    QStringList outLines;
    RunSignalSink sink;

    void collectOutput()
    {
        while (proc.canReadLine()) {
            const auto line = QString::fromUtf8(proc.readLine()).trimmed();
            if (line.isEmpty())
                continue;
            outLines.append(line);
            if (outLines.size() > 60)
                outLines.removeFirst();
        }
    }

    bool isRunning() const
    {
        return proc.state() == QProcess::Running;
    }
};

static bool isSyntalosOnBus()
{
    const auto bus = QDBusConnection::sessionBus();
    return bus.isConnected() && bus.interface()->isServiceRegistered(QString::fromLatin1(SY_DBUS_SERVICE_NAME));
}

static QString dbusErrorString(const QDBusMessage &reply)
{
    return QStringLiteral("%1 (%2)").arg(reply.errorMessage(), reply.errorName());
}

/// a call to the Syntalos control interface
static QDBusMessage syntalosCall(const QString &method, const QVariantList &args = {})
{
    auto msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(SY_DBUS_SERVICE_NAME),
        QString::fromLatin1(SY_DBUS_OBJECT_PATH),
        QString::fromLatin1(SY_DBUS_INTERFACE_NAME),
        method);
    msg.setArguments(args);
    return msg;
}

/// memory growth a stopping Syntalos may still show without being considered stuck in overflow
constexpr double kStopGrowthToleranceMiBPerSec = 32;

/// sleep briefly while letting queued D-Bus signals reach us
static void pollWait(QProcess &proc, int ms)
{
    proc.waitForFinished(ms);
    QCoreApplication::processEvents();
}

double StepResult::durationSec() const
{
    return stats ? stats->durationSec : 0.0;
}

double StepResult::processCpuSec() const
{
    if (!stats)
        return 0.0;
    // the process figure covers all threads of Syntalos itself, the workers are separate processes
    double cpuSec = stats->process ? stats->process->cpuTimeSec() : 0.0;
    for (const auto &m : stats->modules) {
        if (m.worker)
            cpuSec += m.worker->cpuTimeSec();
    }
    return cpuSec;
}

double StepResult::processLoad() const
{
    if (!stats || stats->usageWindowSec <= 0)
        return 0;
    return processCpuSec() / stats->usageWindowSec;
}

double StepResult::loadPercent() const
{
    if (!stats || stats->cpuCoreCount <= 0)
        return 0;
    return 100.0 * processLoad() / stats->cpuCoreCount;
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

SyntalosRunner::~SyntalosRunner()
{
    shutdown();
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

bool SyntalosRunner::isAlive() const
{
    return m_inst && m_inst->isRunning() && m_relaunchReason.isEmpty();
}

void SyntalosRunner::markForRelaunch(const QString &reason)
{
    if (m_relaunchReason.isEmpty())
        m_relaunchReason = reason;
}

auto SyntalosRunner::callSyntalos(const QString &method, const QVariantList &args, int timeoutMs)
    -> std::expected<QVariantList, QString>
{
    const auto reply = QDBusConnection::sessionBus().call(syntalosCall(method, args), QDBus::Block, timeoutMs);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        // we can not know what state Syntalos is in now, so we start over for the next step
        markForRelaunch(QStringLiteral("Syntalos did not answer the %1 request.").arg(method));
        return std::unexpected(dbusErrorString(reply));
    }
    return reply.arguments();
}

auto SyntalosRunner::requestSyntalos(const QString &method, const QVariantList &args, int timeoutMs)
    -> std::expected<void, QString>
{
    const auto res = callSyntalos(method, args, timeoutMs);
    if (!res)
        return std::unexpected(res.error());
    if (const auto err = res->value(0).toString(); !err.isEmpty())
        return std::unexpected(err);
    return {};
}

auto SyntalosRunner::readState() -> std::expected<QString, QString>
{
    auto msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(SY_DBUS_SERVICE_NAME),
        QString::fromLatin1(SY_DBUS_OBJECT_PATH),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Get"));
    msg.setArguments({QString::fromLatin1(SY_DBUS_INTERFACE_NAME), QStringLiteral("State")});
    const auto reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 2000);
    if (reply.type() == QDBusMessage::ErrorMessage)
        return std::unexpected(dbusErrorString(reply));
    if (reply.arguments().isEmpty())
        return std::unexpected(QStringLiteral("Empty reply."));
    return qvariant_cast<QDBusVariant>(reply.arguments().first()).variant().toString();
}

auto SyntalosRunner::ensureStarted(const StepRunConfig &cfg, std::stop_token stop) -> std::expected<void, QString>
{
    static const auto otherInstanceError = QStringLiteral(
        "Another Syntalos instance is running. Close it before benchmarking.");

    if (m_bin.isEmpty())
        return std::unexpected(QStringLiteral("The syntalos executable was not found."));

    if (m_inst && !m_inst->isRunning())
        markForRelaunch(QStringLiteral("Syntalos is not running anymore."));
    if (m_inst && !m_relaunchReason.isEmpty()) {
        LOG_INFO(m_log, "Relaunching Syntalos: {}", m_relaunchReason);
        shutdown();
    }
    if (m_inst)
        return {};

    // the D-Bus name is how Syntalos enforces its single instance, so anything owning it now is not ours
    if (isSyntalosOnBus())
        return std::unexpected(otherInstanceError);

    m_relaunchReason.clear();
    m_inst = std::make_unique<Instance>();
    auto &inst = *m_inst;
    const QStringList args{QStringLiteral("--non-interactive")};
    inst.proc.setProgram(m_bin);
    inst.proc.setArguments(args);
    inst.proc.setProcessChannelMode(QProcess::MergedChannels);
    LOG_INFO(m_log, "Launching: {} {}", m_bin, args.join(QLatin1Char(' ')));
    inst.proc.start();
    if (!inst.proc.waitForStarted(10000)) {
        const auto error = inst.proc.errorString();
        shutdown();
        return std::unexpected(QStringLiteral("Unable to start Syntalos: %1").arg(error));
    }
    inst.pid = inst.proc.processId();

    // wait until the instance owns its name and its control interface answers
    QElapsedTimer timer;
    timer.start();
    while (true) {
        pollWait(inst.proc, 100);
        inst.collectOutput();
        if (!inst.isRunning()) {
            const auto exitCode = inst.proc.exitCode();
            const auto tail = inst.outLines.join(QLatin1Char('\n')).section(QLatin1Char('\n'), -8);
            shutdown();
            if (exitCode == SY_EXIT_ALREADY_RUNNING)
                return std::unexpected(otherInstanceError);
            return std::unexpected(
                QStringLiteral("Syntalos exited with code %1 during startup:\n%2").arg(exitCode).arg(tail));
        }
        if (stop.stop_requested()) {
            shutdown();
            return std::unexpected(QStringLiteral("Cancelled."));
        }
        if (timer.elapsed() > cfg.launchTimeoutSec * 1000LL) {
            shutdown();
            return std::unexpected(
                QStringLiteral("Syntalos did not answer on D-Bus within %1 s.").arg(cfg.launchTimeoutSec));
        }
        if (isSyntalosOnBus() && readState())
            break;
    }
    LOG_INFO(m_log, "Syntalos is ready after {:.1f} s", timer.elapsed() / 1000.0);

    auto bus = QDBusConnection::sessionBus();
    const auto service = QString::fromLatin1(SY_DBUS_SERVICE_NAME);
    const auto path = QString::fromLatin1(SY_DBUS_OBJECT_PATH);
    const auto iface = QString::fromLatin1(SY_DBUS_INTERFACE_NAME);
    if (!bus.connect(service, path, iface, QStringLiteral("RunStarted"), &inst.sink, SLOT(onRunStarted()))
        || !bus.connect(
            service,
            path,
            iface,
            QStringLiteral("RunStopped"),
            &inst.sink,
            SLOT(onRunStopped(bool, QString)))) {
        shutdown();
        return std::unexpected(QStringLiteral("Unable to subscribe to Syntalos run signals."));
    }

    return {};
}

void SyntalosRunner::shutdown()
{
    if (!m_inst)
        return;
    auto inst = std::move(m_inst);
    m_relaunchReason.clear();

    auto &proc = inst->proc;
    if (proc.state() == QProcess::Running) {
        LOG_INFO(m_log, "Asking Syntalos to quit...");
        // a stuck instance may not answer, so we do not wait for the reply
        QDBusConnection::sessionBus().asyncCall(syntalosCall(QStringLiteral("Quit")));
        if (!proc.waitForFinished(15000)) {
            LOG_WARNING(m_log, "Syntalos did not quit in time, terminating it");
            proc.terminate();
            if (!proc.waitForFinished(5000)) {
                LOG_WARNING(m_log, "Syntalos did not terminate in time, killing it");
                proc.kill();
                proc.waitForFinished(5000);
            }
        }
    }
    inst->collectOutput();
    proc.readAll();
    // the process is gone, its signal subscriptions are dropped with the sink object
}

auto SyntalosRunner::run(const StepRunConfig &cfg, std::stop_token stop) -> std::expected<StepResult, QString>
{
    StepResult r;
    const auto cancelled = [&r]() {
        r.stopCause = StopCause::Cancelled;
        r.failureReason = QStringLiteral("Cancelled.");
        return r;
    };
    if (stop.stop_requested())
        return cancelled();

    const bool hadInstance = isAlive();
    if (const auto res = ensureStarted(cfg, stop); !res) {
        if (stop.stop_requested())
            return cancelled();
        return std::unexpected(res.error());
    }
    r.freshInstance = !hadInstance;
    auto &inst = *m_inst;
    auto &proc = inst.proc;
    const qint64 pid = inst.pid;
    inst.sink.reset();
    inst.outLines.clear();

    // start from a clean slate, so a stale file can never be mistaken for this run's result
    QFile::remove(cfg.statsFile);

    QElapsedTimer timer;
    timer.start();

    const auto failStep = [&](const QString &reason) {
        r.failureReason = reason;
        LOG_WARNING(m_log, "{}", reason);
        inst.collectOutput();
        r.outputTail = inst.outLines.join(QLatin1Char('\n'));
        return r;
    };

    LOG_INFO(m_log, "Loading project: {}", cfg.projectFile);
    if (const auto res = requestSyntalos(
            QStringLiteral("LoadProject"),
            {cfg.projectFile},
            cfg.startupTimeoutSec * 1000);
        !res)
        return failStep(QStringLiteral("Loading the project failed: %1").arg(res.error()));
    r.loadSec = timer.elapsed() / 1000.0;
    LOG_INFO(m_log, "Project loaded after {:.1f} s", r.loadSec);
    if (!cfg.ephemeral && !cfg.exportDir.isEmpty()) {
        if (const auto res = requestSyntalos(QStringLiteral("SetExportDirectory"), {cfg.exportDir}, 10000); !res)
            return failStep(QStringLiteral("Setting the export directory failed: %1").arg(res.error()));
    }
    // what the instance holds with the project loaded, to see later how much the run left behind
    const auto pssBeforeRunKiB = readProcessTreePssKiB(pid);

    if (stop.stop_requested())
        return cancelled();

    if (const auto res = requestSyntalos(QStringLiteral("StartRun"), {cfg.ephemeral, cfg.durationSec}, 10000); !res)
        return failStep(QStringLiteral("Starting the run failed: %1").arg(res.error()));
    const auto startRequestMs = timer.elapsed();

    // Syntalos stops its run cleanly when asked over D-Bus, so we ask first. Draining overflowing
    // queues can take a long time, so once the grace period is over we only kill it while its
    // memory keeps growing (the stop is not working), or when the hard limit is reached.
    enum class StopState {
        Running,
        Stopping,
        Killed
    };
    auto stopState = StopState::Running;
    qint64 stopRequestMs = 0;
    qint64 graceDeadlineMs = 0;
    qint64 hardDeadlineMs = 0;
    const auto stopProcess = [&](StopCause cause, const QString &reason, int graceMs, int hardLimitMs) {
        if (stopState != StopState::Running)
            return;
        r.stopCause = cause;
        r.failureReason = reason;
        LOG_WARNING(m_log, "{} Asking Syntalos to stop...", reason);
        QDBusConnection::sessionBus().asyncCall(syntalosCall(QStringLiteral("StopRun")));
        stopState = StopState::Stopping;
        stopRequestMs = timer.elapsed();
        graceDeadlineMs = stopRequestMs + graceMs;
        hardDeadlineMs = graceDeadlineMs + hardLimitMs;
    };
    const auto killProcess = [&](const QString &why) {
        LOG_WARNING(m_log, "{}, killing Syntalos", why);
        proc.kill();
        stopState = StopState::Killed;
    };

    qint64 lastAvailableKiB = 0;
    qint64 lastSampleMs = 0;
    qint64 pssKiB = 0;
    double growthMiBPerSec = 0;
    while (true) {
        pollWait(proc, 100);
        inst.collectOutput();
        if (!inst.isRunning())
            break;
        const auto nowMs = timer.elapsed();
        if (inst.sink.runStarted && !r.started) {
            r.started = true;
            r.startupSec = (nowMs - startRequestMs) / 1000.0;
            LOG_INFO(m_log, "All modules running after {:.1f} s", r.startupSec);
        }
        if (inst.sink.runStopped)
            break;

        // An overloaded Syntalos lets its data queues grow without bound and can take the whole
        // machine down with it, so we watch the memory left on the system and stop the run before
        // that happens. What Syntalos and its workers use is the proportional size, which is
        // expensive to read for many processes, so we only sample it once per second.
        const auto memAvailableKiB = readMemInfo().memAvailableKiB;
        if (nowMs - lastSampleMs >= 1000) {
            if (lastSampleMs > 0)
                growthMiBPerSec = (lastAvailableKiB - memAvailableKiB) / 1024.0 / ((nowMs - lastSampleMs) / 1000.0);
            lastAvailableKiB = memAvailableKiB;
            lastSampleMs = nowMs;
            pssKiB = readProcessTreePssKiB(pid);
            r.peakPssKiB = std::max(r.peakPssKiB, pssKiB);
        }
        const bool systemStarved = memAvailableKiB < cfg.systemMemoryFloorKiB;
        const bool memoryShort = memAvailableKiB < cfg.systemMemoryReserveKiB;
        const bool overLimit = cfg.memoryLimitKiB > 0 && pssKiB > cfg.memoryLimitKiB;
        if ((memoryShort || overLimit) && stopState == StopState::Running) {
            // the spike that triggered the stop is what the report should show as peak
            pssKiB = readProcessTreePssKiB(pid);
            r.peakPssKiB = std::max(r.peakPssKiB, pssKiB);
            stopProcess(
                StopCause::MemoryLimit,
                QStringLiteral(
                    "Memory limit exceeded: Syntalos used %1 MiB with %2 MiB left on the system "
                    "(reserve %3 MiB), memory use growing by %4 MiB/s. Fast growth means its data "
                    "queues were overflowing.")
                    .arg(pssKiB / 1024)
                    .arg(memAvailableKiB / 1024)
                    .arg((overLimit ? cfg.memoryLimitKiB : cfg.systemMemoryReserveKiB) / 1024)
                    .arg(growthMiBPerSec, 0, 'f', 0),
                5000,
                cfg.teardownGraceSec * 1000);
        }
        if (stop.stop_requested())
            stopProcess(StopCause::Cancelled, QStringLiteral("Cancelled."), 15000, 15000);
        if (!r.started && nowMs - startRequestMs > cfg.startupTimeoutSec * 1000LL)
            stopProcess(
                StopCause::StartupTimeout,
                QStringLiteral("Syntalos did not start the run within %1 s.").arg(cfg.startupTimeoutSec),
                15000,
                cfg.teardownGraceSec * 1000);
        if (r.started && nowMs - startRequestMs > (r.startupSec + cfg.durationSec + cfg.teardownGraceSec) * 1000.0)
            stopProcess(
                StopCause::RunTimeout,
                QStringLiteral("Syntalos did not finish the run in time."),
                15000,
                cfg.teardownGraceSec * 1000);

        if (stopState == StopState::Stopping) {
            if (systemStarved) {
                // the machine is about to suffer, there is no time left for a clean stop
                killProcess(QStringLiteral("System memory is running out"));
            } else if (nowMs > graceDeadlineMs) {
                if (growthMiBPerSec > kStopGrowthToleranceMiBPerSec)
                    killProcess(QStringLiteral("Syntalos did not stop in time and its memory keeps growing (%1 MiB/s)")
                                    .arg(growthMiBPerSec, 0, 'f', 0));
                else if (nowMs > hardDeadlineMs)
                    killProcess(QStringLiteral("Syntalos did not stop within %1 s")
                                    .arg((hardDeadlineMs - stopRequestMs) / 1000));
            }
        }
    }
    inst.collectOutput();
    r.outputTail = inst.outLines.join(QLatin1Char('\n'));

    // an instance that stopped cleanly when asked is still good, only a killed one needs replacing
    const bool exited = !inst.isRunning();
    if (exited)
        markForRelaunch(QStringLiteral("Syntalos exited during the run."));

    if (r.stopCause == StopCause::Cancelled)
        return r;

    if (!exited) {
        if (const auto res = callSyntalos(QStringLiteral("RunStatisticsJson"), {}, 30000); !res) {
            LOG_WARNING(m_log, "Unable to fetch run statistics: {}", res.error());
        } else if (const auto json = res->value(0).toString(); !json.isEmpty()) {
            const auto data = json.toUtf8();
            QSaveFile f(cfg.statsFile);
            if (f.open(QIODevice::WriteOnly) && f.write(data) == data.size())
                f.commit();
            else
                LOG_WARNING(m_log, "Unable to save run statistics to {}", cfg.statsFile);
            if (auto stats = RunStatistics::fromJson(QJsonDocument::fromJson(data).object()); stats)
                r.stats = std::move(*stats);
            else
                LOG_WARNING(m_log, "{}", stats.error());
        }

        // memory the run left behind adds to everything measured later, so we do not let it pile up
        const auto retainedKiB = readProcessTreePssKiB(pid) - pssBeforeRunKiB;
        if (cfg.retainedMemoryRestartKiB > 0 && retainedKiB > cfg.retainedMemoryRestartKiB)
            markForRelaunch(QStringLiteral("Syntalos kept %1 MiB after the run.").arg(retainedKiB / 1024));
    }

    // a run we had to stop never counts, even if Syntalos managed to report its statistics
    r.success = r.stopCause == StopCause::None && !exited && inst.sink.runSuccess && r.stats && !r.stats->failed;
    if (!r.success && r.failureReason.isEmpty())
        r.failureReason = failureReasonFromOutcome(r, proc, inst.sink.runMessage);

    LOG_INFO(m_log, "Run finished: {}", r.success ? QStringLiteral("success") : r.failureReason);
    if (!m_relaunchReason.isEmpty())
        LOG_INFO(m_log, "Syntalos will be relaunched for the next step: {}", m_relaunchReason);
    return r;
}

} // namespace SyBench

#include "runner.moc"

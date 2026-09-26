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

#include "benchsession.h"

#include <QDir>
#include <QStandardPaths>

#include "logging.h"
#include "projectgen.h"
#include "sysinfo.h"
#include "utils/resourceinfo.h"

namespace SyBench
{

QString defaultDataDir()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)).filePath(QStringLiteral("data"));
}

BenchSession::BenchSession(const SessionConfig &config, QObject *parent)
    : QObject(parent),
      m_config(config),
      m_dimensions(createAllDimensions()),
      m_log(Syntalos::getLogger("bench")),
      m_cpuCores(Syntalos::SysInfo::get()->cpuPhysicalCoreCount()),
      m_sustainableLoad(m_cpuCores + (Syntalos::SysInfo::get()->cpuCount() - m_cpuCores) / 2.0)
{
    qRegisterMetaType<StepRecord>();
    qRegisterMetaType<LadderRecord>();

    if (!m_config.syntalosBinary.isEmpty())
        m_runner.setSyntalosBinary(m_config.syntalosBinary);
}

BenchSession::~BenchSession() = default;

ProfileRef ProfileRef::of(const Dimension &dim, const QString &profileId)
{
    return ProfileRef{
        .dimensionId = dim.id(),
        .dimensionTitle = dim.title(),
        .profileId = profileId,
        .profileTitle = dim.profileTitle(profileId),
        .levelUnit = dim.levelUnit(profileId)};
}

int BenchSession::bisections() const
{
    return m_config.quick ? 1 : 2;
}

int BenchSession::estimatedStepsPerLadder() const
{
    // a few doubling steps plus the refinement steps, a rough guess for the progress display
    return 3 + bisections();
}

int SessionConfig::effectiveStepSeconds() const
{
    if (stepSeconds > 0)
        return stepSeconds;
    return quick ? 10 : 20;
}

void BenchSession::progress(const QString &msg)
{
    LOG_INFO(m_log, "{}", msg);
    emit progressMessage(msg);
}

const Dimension *BenchSession::dimension(const QString &id) const
{
    for (const auto &d : m_dimensions) {
        if (d->id() == id)
            return d.get();
    }
    return nullptr;
}

auto BenchSession::runStep(const Dimension &dim, const QString &profileId, int level, int durationSec)
    -> std::expected<StepRecord, QString>
{
    StepRecord rec{ProfileRef::of(dim, profileId)};
    rec.level = level;

    QDir workDir(m_config.workDir);
    if (!workDir.mkpath(QStringLiteral(".")))
        return std::unexpected(QStringLiteral("Unable to create the work directory %1.").arg(m_config.workDir));
    const auto baseName = QStringLiteral("%1-%2-L%3").arg(dim.id(), profileId).arg(level);
    auto spec = dim.buildProject(profileId, level);
    spec.experimentId = QStringLiteral("bench-%1-%2-%3").arg(dim.id(), profileId).arg(level);
    // Syntalos refuses to start non-interactive runs without a valid export directory,
    // even ephemeral ones which never write there
    spec.exportBaseDir = workDir.absolutePath();

    StepRunConfig cfg;
    cfg.projectFile = workDir.filePath(baseName + QStringLiteral(".syct"));
    cfg.statsFile = workDir.filePath(baseName + QStringLiteral(".stats.json"));
    cfg.durationSec = durationSec;
    cfg.ephemeral = !dim.writesData(profileId);
    if (!cfg.ephemeral) {
        cfg.exportDir = QDir(m_config.dataDir.isEmpty() ? defaultDataDir() : m_config.dataDir)
                            .filePath(QStringLiteral("syntalos-benchmark-run"));
        QDir().mkpath(cfg.exportDir);
    }

    if (const auto res = writeProjectFile(spec, cfg.projectFile); !res)
        return std::unexpected(QStringLiteral("Unable to generate the benchmark project: %1").arg(res.error()));

    auto result = m_runner.run(cfg, m_stop.get_token());
    // recorded data is only there to be measured, do not keep it around
    if (!cfg.ephemeral)
        QDir(cfg.exportDir).removeRecursively();
    if (!result)
        return std::unexpected(result.error());
    rec.result = std::move(*result);
    rec.verdict = dim.evaluate(profileId, level, rec.result);
    return rec;
}

LadderConfig BenchSession::ladderConfig(const Dimension &dim, const QString &profileId) const
{
    LadderConfig lcfg;
    lcfg.maxLevel = m_config.maxLevel > 0 ? m_config.maxLevel : dim.maxLevel(m_cpuCores, profileId);
    lcfg.startLevel = std::min(
        m_config.startLevel > 0 ? m_config.startLevel : dim.startLevel(m_cpuCores, profileId),
        lcfg.maxLevel);
    lcfg.bisections = bisections();
    return lcfg;
}

void BenchSession::cancel()
{
    m_stop.request_stop();
}

void BenchSession::abort(const QString &error)
{
    LOG_ERROR(m_log, "Benchmark aborted: {}", error);
    emit progressMessage(QStringLiteral("Benchmark aborted: %1").arg(error));
    emit phaseChanged(QStringLiteral("Aborted"));
    emit finished(false, error);
}

void BenchSession::run()
{
    if (!QDir().mkpath(m_config.workDir))
        return abort(QStringLiteral("Unable to create the work directory %1.").arg(m_config.workDir));
    if (m_runner.syntalosBinary().isEmpty())
        return abort(QStringLiteral("The syntalos executable was not found."));

    QList<LadderRecord> ladders;
    for (const auto &sel : m_config.selections) {
        const auto *dim = dimension(sel.first);
        if (dim == nullptr) {
            progress(QStringLiteral("Unknown dimension '%1' skipped.").arg(sel.first));
            continue;
        }
        ladders.append(LadderRecord{ProfileRef::of(*dim, sel.second)});
    }
    if (ladders.isEmpty())
        return abort(QStringLiteral("Nothing selected to run."));

    progress(QStringLiteral("Syntalos: %1").arg(m_runner.syntalosBinary()));
    progress(QStringLiteral("Work directory: %1").arg(m_config.workDir));
    progress(
        QStringLiteral("Physical cores: %1, step duration: %2 s").arg(m_cpuCores).arg(m_config.effectiveStepSeconds()));

    // the first launch of Syntalos on a machine is slower (cold caches, Python byte-compilation, ...),
    // so we do one run and throw its result away; it also tells us early if Syntalos can not run at all
    if (m_config.warmup) {
        emit phaseChanged(QStringLiteral("Warm-up run (not counted)"));
        progress(QStringLiteral("Warm-up run (discarded)..."));
        const auto &first = ladders.first();
        const auto *dim = dimension(first.dimensionId);
        const auto warmup = runStep(*dim, first.profileId, ladderConfig(*dim, first.profileId).startLevel, 5);
        if (!warmup)
            return abort(warmup.error());
        if (!warmup->result.success && !m_stop.stop_requested())
            progress(QStringLiteral("Warm-up run failed: %1").arg(warmup->verdict.summary));
    }

    QString sessionError;
    int index = 0;
    for (auto &lr : ladders) {
        if (m_stop.stop_requested() || !sessionError.isEmpty())
            break;
        const auto *dim = dimension(lr.dimensionId);
        emit ladderStarted(index, ladders.size(), lr);
        progress(QStringLiteral("== %1, %2 ==").arg(lr.dimensionTitle, lr.profileTitle));

        lr.outcome = runLadder(ladderConfig(*dim, lr.profileId), [&](int level) -> LevelOutcome {
            if (m_stop.stop_requested())
                return LevelResult::Cancelled;
            emit stepStarted(lr, level);
            emit phaseChanged(QStringLiteral("%1, %2: trying %3 %4")
                                  .arg(lr.dimensionTitle, lr.profileTitle)
                                  .arg(level)
                                  .arg(lr.levelUnit));
            progress(QStringLiteral("Trying %1 %2...").arg(level).arg(lr.levelUnit));

            auto step = runStep(*dim, lr.profileId, level, m_config.effectiveStepSeconds());
            if (!step) {
                // no further run can work, end the ladder like a cancellation
                sessionError = step.error();
                return LevelResult::Cancelled;
            }
            auto &rec = *step;
            if (rec.result.stopCause == StopCause::Cancelled || m_stop.stop_requested())
                return LevelResult::Cancelled;
            if (rec.result.started)
                rec.verdict.summary += QStringLiteral(" [startup %1 s]").arg(rec.result.startupSec, 0, 'f', 1);
            lr.steps.append(rec);
            progress(QStringLiteral("-> %1: %2")
                         .arg(
                             rec.verdict.passed          ? QStringLiteral("PASS")
                             : rec.verdict.sourceLimited ? QStringLiteral("INCONCLUSIVE")
                                                         : QStringLiteral("FAIL"),
                             rec.verdict.summary));
            if (!rec.result.success && !rec.result.outputTail.isEmpty()) {
                // the last lines of what Syntalos printed usually explain a failed run
                LOG_INFO(m_log, "Syntalos output:\n{}", rec.result.outputTail.section(QLatin1Char('\n'), -8));
            }
            emit stepFinished(rec);
            if (rec.verdict.sourceLimited)
                return LevelResult::Inconclusive;
            // how close this step came to the machine's limits decides how far the next one goes
            const auto availableKiB = Syntalos::readMemInfo().memAvailableKiB - kMemoryReserveKiB;
            const double cpuUse = m_sustainableLoad > 0 ? rec.result.processLoad() / m_sustainableLoad : 0.0;
            const double memoryUse = availableKiB > 0 ? static_cast<double>(rec.result.peakPssKiB) / availableKiB : 0.0;
            return {
                rec.verdict.passed ? LevelResult::Passed : LevelResult::Failed,
                rec.verdict.backlogUse,
                cpuUse,
                memoryUse};
        });

        if (lr.outcome.inconclusive) {
            progress(QStringLiteral("Sustained: %1 %2 (source limit reached, the true capacity may be higher)")
                         .arg(lr.outcome.sustained)
                         .arg(lr.levelUnit));
        } else if (lr.outcome.cancelled) {
            progress(QStringLiteral("Stopped, %1 %2 passed so far.").arg(lr.outcome.sustained).arg(lr.levelUnit));
        } else {
            progress(
                QStringLiteral("Sustained: %1 %2%3")
                    .arg(lr.outcome.sustained)
                    .arg(lr.levelUnit, lr.outcome.reachedMax ? QStringLiteral(" (maximum tested level)") : QString()));
        }
        if (!lr.steps.isEmpty())
            emit ladderFinished(lr);
        index++;
    }

    if (!sessionError.isEmpty())
        return abort(sessionError);
    const bool cancelled = m_stop.stop_requested();
    emit phaseChanged(cancelled ? QStringLiteral("Cancelled") : QStringLiteral("Finished"));
    emit finished(cancelled, QString());
}

} // namespace SyBench

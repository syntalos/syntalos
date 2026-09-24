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

#include "projectgen.h"
#include "sysinfo.h"

namespace SyBench
{

BenchSession::BenchSession(const SessionConfig &config, QObject *parent)
    : QObject(parent),
      m_config(config),
      m_dimensions(createAllDimensions()),
      m_cpuCores(Syntalos::SysInfo::get()->cpuPhysicalCoreCount())
{
    qRegisterMetaType<StepRecord>();
    qRegisterMetaType<LadderRecord>();

    if (!m_config.syntalosBinary.isEmpty())
        m_runner.setSyntalosBinary(m_config.syntalosBinary);
    m_runner.setLogHandler([this](const QString &msg) {
        emit logMessage(msg);
    });
}

BenchSession::~BenchSession() = default;

QString BenchSession::syntalosBinary() const
{
    return m_runner.syntalosBinary();
}

int BenchSession::cpuCores() const
{
    return m_cpuCores;
}

int BenchSession::estimatedStepsPerLadder() const
{
    // a few doubling steps plus the refinement steps, a rough guess for the progress display
    return 3 + (m_config.quick ? 1 : 2);
}

int BenchSession::stepSeconds() const
{
    if (m_config.stepSeconds > 0)
        return m_config.stepSeconds;
    return m_config.quick ? 10 : 20;
}

const Dimension *BenchSession::dimension(const QString &id) const
{
    for (const auto &d : m_dimensions) {
        if (d->id() == id)
            return d.get();
    }
    return nullptr;
}

StepRecord BenchSession::runStep(const Dimension &dim, const QString &profileId, int level, int durationSec)
{
    StepRecord rec;
    rec.dimensionId = dim.id();
    rec.profileId = profileId;
    rec.level = level;

    QDir workDir(m_config.workDir);
    const auto baseName = QStringLiteral("%1-%2-L%3").arg(dim.id(), profileId).arg(level);
    auto spec = dim.buildProject(profileId, level);
    // Syntalos refuses to start non-interactive runs without a valid export directory,
    // even ephemeral ones which never write there
    spec.exportBaseDir = workDir.absolutePath();

    StepRunConfig cfg;
    cfg.projectFile = workDir.filePath(baseName + QStringLiteral(".syct"));
    cfg.statsFile = workDir.filePath(baseName + QStringLiteral(".stats.json"));
    cfg.durationSec = durationSec;
    cfg.ephemeral = !dim.writesData();
    rec.statsFile = cfg.statsFile;

    if (const auto res = writeProjectFile(spec, cfg.projectFile); !res) {
        rec.verdict.summary = QStringLiteral("project generation failed: %1").arg(res.error());
        return rec;
    }

    auto result = m_runner.run(cfg);
    if (!result) {
        rec.verdict.summary = QStringLiteral("unable to run Syntalos: %1").arg(result.error());
        return rec;
    }
    rec.result = std::move(*result);
    rec.verdict = dim.evaluate(profileId, level, rec.result);
    return rec;
}

StepRecord BenchSession::runSingleStep(const Dimension &dim, const QString &profileId, int level, int durationSec)
{
    QDir().mkpath(m_config.workDir);
    return runStep(dim, profileId, level, durationSec);
}

void BenchSession::cancel()
{
    m_cancelled = true;
    m_runner.cancel();
}

void BenchSession::run()
{
    m_cancelled = false;
    m_runner.resetCancel();
    if (!QDir().mkpath(m_config.workDir)) {
        emit logMessage(QStringLiteral("Unable to create work directory %1").arg(m_config.workDir));
        emit finished(false);
        return;
    }
    if (m_runner.syntalosBinary().isEmpty()) {
        emit logMessage(QStringLiteral("The syntalos executable was not found."));
        emit finished(false);
        return;
    }

    QList<LadderRecord> ladders;
    for (const auto &sel : m_config.selections) {
        const auto *dim = dimension(sel.first);
        if (dim == nullptr) {
            emit logMessage(QStringLiteral("Unknown dimension '%1' skipped.").arg(sel.first));
            continue;
        }
        LadderRecord lr;
        lr.dimensionId = dim->id();
        lr.dimensionTitle = dim->title();
        lr.profileId = sel.second;
        lr.levelUnit = dim->levelUnit();
        for (const auto &p : dim->profiles()) {
            if (p.id == sel.second)
                lr.profileTitle = p.title;
        }
        ladders.append(lr);
    }
    if (ladders.isEmpty()) {
        emit logMessage(QStringLiteral("Nothing selected to run."));
        emit finished(false);
        return;
    }

    emit logMessage(QStringLiteral("Syntalos: %1").arg(m_runner.syntalosBinary()));
    emit logMessage(QStringLiteral("Work directory: %1").arg(m_config.workDir));
    emit logMessage(QStringLiteral("Physical cores: %1, step duration: %2 s").arg(m_cpuCores).arg(stepSeconds()));

    // the first launch of Syntalos on a machine is slower (cold caches, Python byte-compilation, ...),
    // so we do one run and throw its result away
    if (m_config.warmup) {
        emit phaseChanged(QStringLiteral("Warm-up run (not counted)"));
        emit logMessage(QStringLiteral("Warm-up run (discarded)..."));
        const auto &first = ladders.first();
        const auto *dim = dimension(first.dimensionId);
        runStep(*dim, first.profileId, dim->startLevel(m_cpuCores), 5);
    }

    int index = 0;
    for (auto &lr : ladders) {
        if (m_cancelled)
            break;
        const auto *dim = dimension(lr.dimensionId);
        emit ladderStarted(index, ladders.size(), lr);
        emit logMessage(QStringLiteral("== %1, %2 ==").arg(lr.dimensionTitle, lr.profileTitle));

        LadderConfig lcfg;
        lcfg.startLevel = m_config.startLevel > 0 ? m_config.startLevel : dim->startLevel(m_cpuCores);
        lcfg.maxLevel = dim->maxLevel();
        lcfg.bisections = m_config.quick ? 1 : 2;

        lr.outcome = runLadder(lcfg, [&](int level) -> std::optional<bool> {
            if (m_cancelled)
                return std::nullopt;
            emit stepStarted(lr, level);
            emit phaseChanged(QStringLiteral("%1, %2: trying %3 %4")
                                  .arg(lr.dimensionTitle, lr.profileTitle)
                                  .arg(level)
                                  .arg(lr.levelUnit));
            emit logMessage(QStringLiteral("Trying %1 %2...").arg(level).arg(lr.levelUnit));

            auto rec = runStep(*dim, lr.profileId, level, stepSeconds());
            if (rec.result.cancelled || m_cancelled)
                return std::nullopt;
            lr.steps.append(rec);
            if (!rec.result.success && !rec.result.outputTail.isEmpty()) {
                // the last lines of what Syntalos printed usually explain a failed run
                emit logMessage(rec.result.outputTail.section(QLatin1Char('\n'), -8));
            }
            emit logMessage(
                QStringLiteral("-> %1: %2")
                    .arg(rec.verdict.passed ? QStringLiteral("PASS") : QStringLiteral("FAIL"), rec.verdict.summary));
            emit stepFinished(rec);
            return rec.verdict.passed;
        });

        if (lr.outcome.cancelled) {
            emit logMessage(
                QStringLiteral("Cancelled, %1 %2 passed so far.").arg(lr.outcome.sustained).arg(lr.levelUnit));
        } else {
            emit logMessage(
                QStringLiteral("Sustained: %1 %2%3")
                    .arg(lr.outcome.sustained)
                    .arg(lr.levelUnit, lr.outcome.reachedMax ? QStringLiteral(" (maximum tested level)") : QString()));
        }
        if (!lr.steps.isEmpty())
            emit ladderFinished(lr);
        index++;
    }

    emit phaseChanged(m_cancelled ? QStringLiteral("Cancelled") : QStringLiteral("Finished"));
    emit finished(m_cancelled);
}

} // namespace SyBench

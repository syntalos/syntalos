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

#include "report.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>
#include <cmath>

#include "score.h"
#include "sysinfo.h"

namespace SyBench
{

static const int ReportFormatVersion = 1;

/**
 * A brief digest of the run statistics, enough to see where a step hit its limit.
 */
static QJsonObject diagnosticsToJson(const Syntalos::RunStatistics &stats)
{
    using namespace Syntalos;
    const double window = stats.usageWindowSec;

    struct ThreadLoad {
        QJsonObject info;
        double load;
    };
    QList<ThreadLoad> threads;
    const auto addThread = [&](QJsonObject info, const std::optional<ThreadUsageStats> &usage) {
        if (!usage || window <= 0)
            return;
        const double load = usage->cpuTimeSec() / window;
        if (load < 0.05)
            return;
        info.insert(QStringLiteral("load"), std::round(load * 100.0) / 100.0);
        info.insert(QStringLiteral("involuntary_ctx_switches"), static_cast<qint64>(usage->involuntaryCtxSwitches));
        threads.append({info, load});
    };
    addThread(
        {
            {QStringLiteral("name"), QStringLiteral("main")},
            {QStringLiteral("kind"), QStringLiteral("main")}
    },
        stats.mainThread);
    for (const auto &et : stats.eventThreads)
        addThread(
            {
                {QStringLiteral("name"),    et.key                        },
                {QStringLiteral("kind"),    QStringLiteral("event-thread")},
                {QStringLiteral("modules"), et.moduleNames.size()         }
        },
            et.thread);

    QJsonArray failedModules;
    QJsonObject moduleStats;
    for (const auto &mod : stats.modules) {
        addThread(
            {
                {QStringLiteral("name"), mod.name                },
                {QStringLiteral("kind"), QStringLiteral("module")}
        },
            mod.thread);
        addThread(
            {
                {QStringLiteral("name"), mod.name                },
                {QStringLiteral("kind"), QStringLiteral("worker")}
        },
            mod.worker);
        if (mod.finalState == ModuleState::ERROR || !mod.errorMessage.isEmpty())
            failedModules.append(
                QJsonObject{
                    {QStringLiteral("name"),  mod.name        },
                    {QStringLiteral("error"), mod.errorMessage}
            });
        // flow meter statistics are already part of the step's meters
        if (mod.id != QLatin1String("flowmeter") && !mod.moduleStats.isEmpty())
            moduleStats.insert(mod.name, QJsonObject::fromVariantHash(mod.moduleStats));
    }
    std::ranges::sort(threads, std::ranges::greater{}, &ThreadLoad::load);
    QJsonArray busiest;
    for (const auto &t : threads.first(std::min<qsizetype>(threads.size(), 5)))
        busiest.append(t.info);

    QJsonArray backlogged;
    for (const auto &c : stats.connections) {
        if (c.peakPending <= 2 && c.pendingAtStop <= 2)
            continue;
        backlogged.append(
            QJsonObject{
                {QStringLiteral("from"), QStringLiteral("%1/%2").arg(c.srcModule, c.srcPort)},
                {QStringLiteral("to"), QStringLiteral("%1/%2").arg(c.dstModule, c.dstPort)},
                {QStringLiteral("peak_pending"), static_cast<qint64>(c.peakPending)},
                {QStringLiteral("pending_at_stop"), static_cast<qint64>(c.pendingAtStop)}
        });
    }

    QJsonObject o;
    o.insert(QStringLiteral("busiest_threads"), busiest);
    o.insert(QStringLiteral("backlogged_connections"), backlogged);
    o.insert(QStringLiteral("module_stats"), moduleStats);
    o.insert(QStringLiteral("failed_modules"), failedModules);
    return o;
}

static QJsonObject stepToJson(const StepRecord &s)
{
    QJsonObject o;
    o.insert(QStringLiteral("level"), s.level);
    o.insert(QStringLiteral("passed"), s.verdict.passed);
    o.insert(QStringLiteral("source_limited"), s.verdict.sourceLimited);
    o.insert(QStringLiteral("startup_sec"), s.result.startupSec);
    o.insert(QStringLiteral("summary"), s.verdict.summary);
    o.insert(QStringLiteral("min_rate_fraction"), s.verdict.minRateFraction);
    o.insert(QStringLiteral("max_peak_backlog"), s.verdict.maxPeakBacklog);
    o.insert(QStringLiteral("max_backlog_at_stop"), s.verdict.maxBacklogAtStop);
    o.insert(QStringLiteral("run_success"), s.result.success);
    o.insert(QStringLiteral("stop_cause"), stopCauseToString(s.result.stopCause));
    o.insert(QStringLiteral("duration_sec"), s.result.durationSec());
    o.insert(QStringLiteral("process_load"), s.result.processLoad());
    o.insert(QStringLiteral("process_cpu_sec"), s.result.processCpuSec());
    o.insert(QStringLiteral("threads_total"), s.result.threadsTotal());
    o.insert(QStringLiteral("threads_elevated"), s.result.threadsElevated());
    o.insert(QStringLiteral("peak_pss_kib"), s.result.peakPssKiB);

    // everything the flow meters measured, keyed as they report it
    QJsonArray meters;
    if (s.result.stats) {
        for (const auto &mod : s.result.stats->modules) {
            if (mod.id != QLatin1String("flowmeter") || mod.moduleStats.isEmpty())
                continue;
            auto mo = QJsonObject::fromVariantHash(mod.moduleStats);
            mo.insert(QStringLiteral("module"), mod.name);
            meters.append(mo);
        }
    }
    o.insert(QStringLiteral("meters"), meters);
    if (s.result.stats)
        o.insert(QStringLiteral("diagnostics"), diagnosticsToJson(*s.result.stats));
    return o;
}

QJsonObject buildReport(
    const QList<LadderRecord> &ladders,
    const SessionConfig &config,
    const QList<HealthItem> &health)
{
    auto *sysInfo = Syntalos::SysInfo::get();

    QJsonObject root;
    root.insert(QStringLiteral("format_version"), ReportFormatVersion);
    root.insert(QStringLiteral("created"), QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert(QStringLiteral("benchmark_version"), QCoreApplication::applicationVersion());

    QJsonObject machine;
    machine.insert(QStringLiteral("hostname"), sysInfo->machineHostName());
    machine.insert(QStringLiteral("os"), sysInfo->prettyOSName());
    machine.insert(QStringLiteral("kernel"), sysInfo->kernelInfo());
    machine.insert(QStringLiteral("cpu"), sysInfo->cpu0ModelName());
    machine.insert(QStringLiteral("cpu_physical_cores"), sysInfo->cpuPhysicalCoreCount());
    machine.insert(QStringLiteral("cpu_logical"), sysInfo->cpuCount());
    machine.insert(QStringLiteral("cpu_governor"), sysInfo->cpuGovernor());
    machine.insert(QStringLiteral("syntalos_version"), sysInfo->syntalosVersion());
    root.insert(QStringLiteral("machine"), machine);
    root.insert(QStringLiteral("health"), healthToJson(health));
    root.insert(QStringLiteral("score"), scoreToJson(computeScore(ladders, config)));

    QJsonObject settings;
    settings.insert(QStringLiteral("quick"), config.quick);
    settings.insert(QStringLiteral("warmup"), config.warmup);
    settings.insert(QStringLiteral("step_seconds"), config.effectiveStepSeconds());
    settings.insert(QStringLiteral("data_dir"), config.dataDir);
    root.insert(QStringLiteral("settings"), settings);

    QJsonArray results;
    for (const auto &lr : ladders) {
        QJsonObject o;
        o.insert(QStringLiteral("dimension"), lr.dimensionId);
        o.insert(QStringLiteral("dimension_title"), lr.dimensionTitle);
        o.insert(QStringLiteral("profile"), lr.profileId);
        o.insert(QStringLiteral("profile_title"), lr.profileTitle);
        o.insert(QStringLiteral("level_unit"), lr.levelUnit);
        o.insert(QStringLiteral("sustained"), lr.outcome.sustained);
        o.insert(QStringLiteral("reached_max"), lr.outcome.reachedMax);
        o.insert(QStringLiteral("cancelled"), lr.outcome.cancelled);
        o.insert(QStringLiteral("source_limit_reached"), lr.outcome.inconclusive);
        QJsonArray steps;
        for (const auto &s : lr.steps)
            steps.append(stepToJson(s));
        o.insert(QStringLiteral("steps"), steps);
        results.append(o);
    }
    root.insert(QStringLiteral("results"), results);
    return root;
}

auto saveReport(
    const QString &fileName,
    const QList<LadderRecord> &ladders,
    const SessionConfig &config,
    const QList<HealthItem> &health) -> std::expected<void, QString>
{
    QSaveFile f(fileName);
    if (!f.open(QIODevice::WriteOnly))
        return std::unexpected(QStringLiteral("Unable to write %1: %2").arg(fileName, f.errorString()));
    f.write(QJsonDocument(buildReport(ladders, config, health)).toJson(QJsonDocument::Indented));
    if (!f.commit())
        return std::unexpected(QStringLiteral("Unable to write %1: %2").arg(fileName, f.errorString()));
    return {};
}

} // namespace SyBench

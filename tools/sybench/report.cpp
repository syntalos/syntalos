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
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

#include "health.h"
#include "sysinfo.h"

namespace SyBench
{

static const int ReportFormatVersion = 1;

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
    o.insert(QStringLiteral("memory_exceeded"), s.result.memoryExceeded);
    o.insert(QStringLiteral("duration_sec"), s.result.durationSec());
    o.insert(QStringLiteral("process_load"), s.result.processLoad());
    o.insert(QStringLiteral("process_cpu_sec"), s.result.processCpuSec());
    o.insert(QStringLiteral("threads_total"), s.result.threadsTotal());
    o.insert(QStringLiteral("threads_elevated"), s.result.threadsElevated());
    o.insert(QStringLiteral("peak_rss_kib"), s.result.peakRssKiB());
    o.insert(QStringLiteral("stats_file"), QFileInfo(s.statsFile).fileName());

    QJsonArray meters;
    for (const auto &m : s.result.meters()) {
        QJsonObject mo;
        mo.insert(QStringLiteral("module"), m.moduleName);
        mo.insert(QStringLiteral("items"), m.items);
        mo.insert(QStringLiteral("interval_max_us"), m.intervalMaxUs);
        mo.insert(QStringLiteral("age_p50_us"), m.ageP50Us);
        mo.insert(QStringLiteral("age_p99_us"), m.ageP99Us);
        meters.append(mo);
    }
    o.insert(QStringLiteral("meters"), meters);
    return o;
}

QJsonObject buildReport(const QList<LadderRecord> &ladders, const SessionConfig &config, int cpuCores)
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
    machine.insert(QStringLiteral("cpu_physical_cores"), cpuCores);
    machine.insert(QStringLiteral("cpu_logical"), sysInfo->cpuCount());
    machine.insert(QStringLiteral("cpu_governor"), sysInfo->cpuGovernor());
    machine.insert(QStringLiteral("syntalos_version"), sysInfo->syntalosVersion());
    root.insert(QStringLiteral("machine"), machine);
    root.insert(QStringLiteral("health"), healthToJson(collectHealthItems()));

    QJsonObject settings;
    settings.insert(QStringLiteral("quick"), config.quick);
    settings.insert(QStringLiteral("warmup"), config.warmup);
    settings.insert(QStringLiteral("step_seconds"), config.stepSeconds);
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

auto saveReport(const QString &fileName, const QList<LadderRecord> &ladders, const SessionConfig &config, int cpuCores)
    -> std::expected<void, QString>
{
    QFile f(fileName);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return std::unexpected(QStringLiteral("Unable to write %1: %2").arg(fileName, f.errorString()));
    f.write(QJsonDocument(buildReport(ladders, config, cpuCores)).toJson(QJsonDocument::Indented));
    f.close();

    // keep the raw per-step statistics next to the report, for anyone who wants to dig deeper
    const QFileInfo fi(fileName);
    QDir stepsDir(fi.dir().filePath(fi.completeBaseName() + QStringLiteral("-steps")));
    if (!stepsDir.mkpath(QStringLiteral(".")))
        return std::unexpected(QStringLiteral("Unable to create %1").arg(stepsDir.absolutePath()));
    for (const auto &lr : ladders) {
        for (const auto &s : lr.steps) {
            if (s.statsFile.isEmpty() || !QFile::exists(s.statsFile))
                continue;
            const auto dest = stepsDir.filePath(QFileInfo(s.statsFile).fileName());
            QFile::remove(dest);
            QFile::copy(s.statsFile, dest);
        }
    }
    return {};
}

} // namespace SyBench

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

#include <QCommandLineParser>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include "config.h"
#include "dimension.h"
#include "ladder.h"
#include "projectgen.h"
#include "runner.h"
#include "sysinfo.h"

using namespace SyBench;

namespace
{

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

QTextStream &err()
{
    static QTextStream stream(stderr);
    return stream;
}

struct StepRecord {
    int level;
    StepVerdict verdict;
    StepResult result;
};

/**
 * Run one step: generate the project, run Syntalos on it, judge the outcome.
 */
StepVerdict runStep(
    SyntalosRunner &runner,
    const Dimension &dim,
    const QString &profileId,
    int level,
    int durationSec,
    const QDir &workDir,
    StepResult *resultOut = nullptr)
{
    const auto baseName = QStringLiteral("%1-%2-L%3").arg(dim.id(), profileId).arg(level);
    auto spec = dim.buildProject(profileId, level);
    spec.exportBaseDir = workDir.absolutePath();

    QString error;
    StepRunConfig cfg;
    cfg.projectFile = workDir.filePath(baseName + QStringLiteral(".syct"));
    cfg.statsFile = workDir.filePath(baseName + QStringLiteral(".stats.json"));
    cfg.durationSec = durationSec;
    cfg.ephemeral = !dim.writesData();
    if (!writeProjectFile(spec, cfg.projectFile, &error)) {
        StepVerdict v;
        v.summary = QStringLiteral("project generation failed: %1").arg(error);
        return v;
    }

    const auto result = runner.run(cfg);
    if (resultOut)
        *resultOut = result;
    return dim.evaluate(profileId, level, result);
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("syntalos-benchmark"));
    QCoreApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Syntalos System Benchmark\n\nMeasure what this computer can sustain when running Syntalos."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption optSyBin(
        QStringLiteral("syntalos-bin"),
        QStringLiteral("Path to the syntalos executable to benchmark."),
        QStringLiteral("path"));
    QCommandLineOption optWorkDir(
        QStringLiteral("work-dir"),
        QStringLiteral("Directory for generated projects and statistics."),
        QStringLiteral("dir"));
    QCommandLineOption optDimension(
        QStringLiteral("dimension"),
        QStringLiteral("Dimension to run (default: all)."),
        QStringLiteral("id"));
    QCommandLineOption optProfile(
        QStringLiteral("profile"),
        QStringLiteral("Only run this profile of the dimension."),
        QStringLiteral("id"));
    QCommandLineOption optStepSecs(
        QStringLiteral("step-seconds"),
        QStringLiteral("Duration of each measured run (default: 20)."),
        QStringLiteral("sec"));
    QCommandLineOption optStartLevel(
        QStringLiteral("start-level"),
        QStringLiteral("Level to start the ladder at (default: half the physical cores)."),
        QStringLiteral("n"));
    QCommandLineOption optQuick(QStringLiteral("quick"), QStringLiteral("Quick mode: 10 s steps, one bisection."));
    QCommandLineOption optNoWarmup(QStringLiteral("no-warmup"), QStringLiteral("Skip the discarded warm-up run."));
    QCommandLineOption optSelfTest(
        QStringLiteral("self-test"),
        QStringLiteral("Run a single short step of the camera dimension to verify the setup works."));
    parser.addOption(optSyBin);
    parser.addOption(optWorkDir);
    parser.addOption(optDimension);
    parser.addOption(optProfile);
    parser.addOption(optStepSecs);
    parser.addOption(optStartLevel);
    parser.addOption(optQuick);
    parser.addOption(optNoWarmup);
    parser.addOption(optSelfTest);
    parser.process(app);

    SyntalosRunner runner;
    if (parser.isSet(optSyBin))
        runner.setSyntalosBinary(parser.value(optSyBin));
    if (runner.syntalosBinary().isEmpty()) {
        err() << "Unable to find the syntalos executable. Use --syntalos-bin to set it.\n";
        return 2;
    }
    runner.setLogHandler([](const QString &msg) {
        out() << "    " << msg << "\n";
        out().flush();
    });

    QDir workDir(
        parser.isSet(optWorkDir)
            ? parser.value(optWorkDir)
            : QDir::temp().filePath(QStringLiteral("syntalos-bench-%1").arg(QCoreApplication::applicationPid())));
    if (!workDir.mkpath(QStringLiteral("."))) {
        err() << "Unable to create work directory " << workDir.absolutePath() << "\n";
        return 2;
    }

    const int cpuCores = Syntalos::SysInfo::get()->cpuPhysicalCoreCount();
    const bool quick = parser.isSet(optQuick);
    int stepSecs = quick ? 10 : 20;
    if (parser.isSet(optStepSecs))
        stepSecs = std::max(1, parser.value(optStepSecs).toInt());

    out() << "Syntalos: " << runner.syntalosBinary() << "\n";
    out() << "Work directory: " << workDir.absolutePath() << "\n";
    out() << "Physical cores: " << cpuCores << ", step duration: " << stepSecs << " s\n";
    out().flush();

    if (parser.isSet(optSelfTest)) {
        auto dim = createDimension(QStringLiteral("camera-capacity"));
        StepResult result;
        const auto verdict = runStep(runner, *dim, dim->profiles().first().id, 1, 3, workDir, &result);
        out() << "Self test: " << (verdict.passed ? "PASS" : "FAIL") << " (" << verdict.summary << ")\n";
        if (!verdict.passed && !result.outputTail.isEmpty())
            out() << result.outputTail << "\n";
        return verdict.passed ? 0 : 1;
    }

    std::vector<std::unique_ptr<Dimension>> dims;
    if (parser.isSet(optDimension)) {
        auto d = createDimension(parser.value(optDimension));
        if (!d) {
            err() << "Unknown dimension '" << parser.value(optDimension) << "'. Available:";
            for (const auto &ad : createAllDimensions())
                err() << " " << ad->id();
            err() << "\n";
            return 2;
        }
        dims.push_back(std::move(d));
    } else {
        dims = createAllDimensions();
    }

    // the first launch of Syntalos on a machine is slower, so we do one run and throw its result away
    if (!parser.isSet(optNoWarmup)) {
        out() << "Warm-up run (discarded)...\n";
        const auto &dim = *dims.front();
        runStep(runner, dim, dim.profiles().first().id, dim.startLevel(cpuCores), 5, workDir);
    }

    QJsonArray report;
    for (const auto &dim : dims) {
        for (const auto &profile : dim->profiles()) {
            if (parser.isSet(optProfile) && profile.id != parser.value(optProfile))
                continue;
            out() << "\n== " << dim->title() << ", " << profile.title << " ==\n";
            out().flush();

            LadderConfig lcfg;
            lcfg.startLevel = parser.isSet(optStartLevel) ? parser.value(optStartLevel).toInt()
                                                          : dim->startLevel(cpuCores);
            lcfg.maxLevel = dim->maxLevel();
            lcfg.bisections = quick ? 1 : 2;

            QList<StepRecord> records;
            const auto outcome = runLadder(lcfg, [&](int level) -> std::optional<bool> {
                if (runner.isCancelled())
                    return std::nullopt;
                out() << "  Trying " << level << " " << dim->levelUnit() << "...\n";
                out().flush();
                StepResult result;
                const auto verdict = runStep(runner, *dim, profile.id, level, stepSecs, workDir, &result);
                if (result.cancelled)
                    return std::nullopt;
                records.append(StepRecord{level, verdict, result});
                out() << "  -> " << (verdict.passed ? "PASS" : "FAIL") << ": " << verdict.summary
                      << QStringLiteral(" [load %1, %2/%3 threads elevated, peak RSS %4 MiB]")
                             .arg(result.processLoad(), 0, 'f', 2)
                             .arg(result.threadsElevated)
                             .arg(result.threadsTotal)
                             .arg(std::max(result.peakRssKiB, result.observedPeakRssKiB) / 1024)
                      << "\n";
                if (!verdict.passed && !result.success && !result.outputTail.isEmpty())
                    out() << result.outputTail.section(QLatin1Char('\n'), -5) << "\n";
                out().flush();
                return verdict.passed;
            });

            out() << "  Sustained: " << outcome.sustained << " " << dim->levelUnit()
                  << (outcome.reachedMax ? " (maximum tested level)" : "") << "\n";

            QJsonObject entry;
            entry.insert(QStringLiteral("dimension"), dim->id());
            entry.insert(QStringLiteral("profile"), profile.id);
            entry.insert(QStringLiteral("sustained"), outcome.sustained);
            entry.insert(QStringLiteral("reached_max"), outcome.reachedMax);
            QJsonArray steps;
            for (const auto &rec : records) {
                QJsonObject so;
                so.insert(QStringLiteral("level"), rec.level);
                so.insert(QStringLiteral("passed"), rec.verdict.passed);
                so.insert(QStringLiteral("summary"), rec.verdict.summary);
                so.insert(QStringLiteral("min_rate_fraction"), rec.verdict.minRateFraction);
                so.insert(QStringLiteral("process_load"), rec.result.processLoad());
                so.insert(QStringLiteral("threads_elevated"), rec.result.threadsElevated);
                so.insert(QStringLiteral("peak_rss_kib"), rec.result.peakRssKiB);
                steps.append(so);
            }
            entry.insert(QStringLiteral("steps"), steps);
            report.append(entry);
        }
    }

    out() << "\n" << QJsonDocument(report).toJson(QJsonDocument::Indented) << "\n";
    return 0;
}

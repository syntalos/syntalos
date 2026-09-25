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

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>

#include "appstyle.h"
#include "benchsession.h"
#include "benchwindow.h"
#include "config.h"
#include "logging.h"
#include "report.h"
#include "utils/style.h"

using namespace SyBench;

namespace
{

/**
 * Developer mode: run ladders without a window and write the report into the work directory.
 */
int runHeadless(const SessionConfig &config)
{
    BenchSession session(config);
    QList<LadderRecord> ladders;
    QObject::connect(&session, &BenchSession::ladderFinished, [&ladders](const LadderRecord &lr) {
        ladders.append(lr);
    });
    session.run();

    const auto reportFile = QDir(config.workDir).filePath(QStringLiteral("report.json"));
    if (const auto res = saveReport(reportFile, ladders, config, session.cpuCores()); !res) {
        LOG_ERROR(logRoot, "{}", res.error());
        return 1;
    }
    LOG_INFO(logRoot, "Report written to {}", reportFile);
    return 0;
}

int runSelfTest(const SessionConfig &config)
{
    BenchSession session(config);
    auto dim = createDimension(QStringLiteral("camera-capacity"));
    const auto rec = session.runSingleStep(*dim, dim->profiles().first().id, 1, 3);
    if (rec.verdict.passed) {
        LOG_INFO(logRoot, "Self test: PASS ({})", rec.verdict.summary);
        return 0;
    }
    LOG_ERROR(logRoot, "Self test: FAIL ({})", rec.verdict.summary);
    if (!rec.result.outputTail.isEmpty())
        LOG_INFO(logRoot, "Syntalos output:\n{}", rec.result.outputTail);
    return 1;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("syntalos-benchmark"));
    app.setOrganizationName(QStringLiteral("Syntalos"));
    app.setOrganizationDomain(QStringLiteral("syntalos.org"));
    app.setApplicationVersion(QStringLiteral(PROJECT_VERSION));

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
    QCommandLineOption optDataDir(
        QStringLiteral("data-dir"),
        QStringLiteral("Directory the recording tests write to (default: the user cache directory)."),
        QStringLiteral("dir"));
    QCommandLineOption optSelfTest(
        QStringLiteral("self-test"),
        QStringLiteral("Run a single short step without a window to verify the setup works."));
    // developer options, they run the benchmark without a window
    QCommandLineOption optDimension(
        QStringLiteral("dimension"),
        QStringLiteral("Dimension to run headless."),
        QStringLiteral("id"));
    QCommandLineOption optProfile(
        QStringLiteral("profile"),
        QStringLiteral("Only run this profile."),
        QStringLiteral("id"));
    QCommandLineOption optStepSecs(
        QStringLiteral("step-seconds"),
        QStringLiteral("Duration of each run."),
        QStringLiteral("sec"));
    QCommandLineOption optStartLevel(
        QStringLiteral("start-level"),
        QStringLiteral("Ladder start level."),
        QStringLiteral("n"));
    QCommandLineOption optMaxLevel(
        QStringLiteral("max-level"),
        QStringLiteral("Ladder maximum level."),
        QStringLiteral("n"));
    QCommandLineOption optQuick(QStringLiteral("quick"), QStringLiteral("Quick mode."));
    QCommandLineOption optNoWarmup(QStringLiteral("no-warmup"), QStringLiteral("Skip the warm-up run."));
    for (auto *opt : {&optDimension, &optProfile, &optStepSecs, &optStartLevel, &optMaxLevel, &optQuick, &optNoWarmup})
        opt->setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOptions(
        {optSyBin,
         optWorkDir,
         optDataDir,
         optSelfTest,
         optDimension,
         optProfile,
         optStepSecs,
         optStartLevel,
         optMaxLevel,
         optQuick,
         optNoWarmup});
    parser.process(app);

    // console logging only, a benchmark tool does not need a persistent log
    Syntalos::initializeSyLogSystem();

    SessionConfig config;
    config.syntalosBinary = parser.value(optSyBin);
    config.dataDir = parser.isSet(optDataDir) ? parser.value(optDataDir) : defaultDataDir();
    config.workDir = parser.isSet(optWorkDir)
                         ? parser.value(optWorkDir)
                         : QDir::temp().filePath(
                               QStringLiteral("syntalos-bench-%1").arg(QCoreApplication::applicationPid()));
    config.quick = parser.isSet(optQuick);
    config.warmup = !parser.isSet(optNoWarmup);
    if (parser.isSet(optStepSecs))
        config.stepSeconds = std::max(1, parser.value(optStepSecs).toInt());
    if (parser.isSet(optMaxLevel))
        config.maxLevel = std::max(1, parser.value(optMaxLevel).toInt());
    if (parser.isSet(optStartLevel))
        config.startLevel = std::max(1, parser.value(optStartLevel).toInt());

    int ret = 0;
    if (parser.isSet(optSelfTest)) {
        ret = runSelfTest(config);
    } else if (parser.isSet(optDimension)) {
        auto dim = createDimension(parser.value(optDimension));
        if (!dim) {
            LOG_ERROR(logRoot, "Unknown dimension '{}'.", parser.value(optDimension));
            ret = 2;
        } else {
            for (const auto &p : dim->profiles()) {
                if (!parser.isSet(optProfile) || p.id == parser.value(optProfile))
                    config.selections.append({dim->id(), p.id});
            }
            ret = runHeadless(config);
        }
    } else {
        setDefaultStyle();
        switchIconTheme(QStringLiteral("breeze"));

        BenchWindow w;
        if (!config.syntalosBinary.isEmpty())
            w.setSyntalosBinary(config.syntalosBinary);
        w.setWorkDir(config.workDir);
        w.setDataDir(config.dataDir);
        w.show();
        ret = app.exec();
    }

    Syntalos::shutdownSyLogSystem();
    return ret;
}

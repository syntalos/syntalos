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
#include <QTextStream>

#include "config.h"
#include "benchsession.h"
#include "benchwindow.h"
#include "report.h"
#include "utils/style.h"
#include "appstyle.h"

using namespace SyBench;

namespace
{

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

/**
 * Developer mode: run ladders without a window and print the report as JSON.
 */
int runHeadless(const SessionConfig &config)
{
    BenchSession session(config);
    QList<LadderRecord> ladders;
    QObject::connect(&session, &BenchSession::logMessage, [](const QString &msg) {
        out() << msg << "\n";
        out().flush();
    });
    QObject::connect(&session, &BenchSession::ladderFinished, [&ladders](const LadderRecord &lr) {
        ladders.append(lr);
    });
    session.run();

    out() << QJsonDocument(buildReport(ladders, config, session.cpuCores())).toJson(QJsonDocument::Indented) << "\n";
    return 0;
}

int runSelfTest(const SessionConfig &config)
{
    BenchSession session(config);
    QObject::connect(&session, &BenchSession::logMessage, [](const QString &msg) {
        out() << "    " << msg << "\n";
        out().flush();
    });
    auto dim = createDimension(QStringLiteral("camera-capacity"));
    const auto rec = session.runSingleStep(*dim, dim->profiles().first().id, 1, 3);
    out() << "Self test: " << (rec.verdict.passed ? "PASS" : "FAIL") << " (" << rec.verdict.summary << ")\n";
    if (!rec.verdict.passed && !rec.result.outputTail.isEmpty())
        out() << rec.result.outputTail << "\n";
    return rec.verdict.passed ? 0 : 1;
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
    QCommandLineOption optQuick(QStringLiteral("quick"), QStringLiteral("Quick mode."));
    QCommandLineOption optNoWarmup(QStringLiteral("no-warmup"), QStringLiteral("Skip the warm-up run."));
    for (auto *opt : {&optDimension, &optProfile, &optStepSecs, &optStartLevel, &optQuick, &optNoWarmup})
        opt->setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOptions(
        {optSyBin,
         optWorkDir,
         optSelfTest,
         optDimension,
         optProfile,
         optStepSecs,
         optStartLevel,
         optQuick,
         optNoWarmup});
    parser.process(app);

    SessionConfig config;
    config.syntalosBinary = parser.value(optSyBin);
    config.workDir = parser.isSet(optWorkDir)
                         ? parser.value(optWorkDir)
                         : QDir::temp().filePath(
                               QStringLiteral("syntalos-bench-%1").arg(QCoreApplication::applicationPid()));
    config.quick = parser.isSet(optQuick);
    config.warmup = !parser.isSet(optNoWarmup);
    if (parser.isSet(optStepSecs))
        config.stepSeconds = std::max(1, parser.value(optStepSecs).toInt());
    if (parser.isSet(optStartLevel))
        config.startLevel = std::max(1, parser.value(optStartLevel).toInt());

    if (parser.isSet(optSelfTest))
        return runSelfTest(config);

    if (parser.isSet(optDimension)) {
        auto dim = createDimension(parser.value(optDimension));
        if (!dim) {
            QTextStream(stderr) << "Unknown dimension '" << parser.value(optDimension) << "'.\n";
            return 2;
        }
        for (const auto &p : dim->profiles()) {
            if (!parser.isSet(optProfile) || p.id == parser.value(optProfile))
                config.selections.append({dim->id(), p.id});
        }
        return runHeadless(config);
    }

    setDefaultStyle();
    switchIconTheme(QStringLiteral("breeze"));

    BenchWindow w;
    if (!config.syntalosBinary.isEmpty())
        w.setSyntalosBinary(config.syntalosBinary);
    w.setWorkDir(config.workDir);
    w.show();
    return app.exec();
}

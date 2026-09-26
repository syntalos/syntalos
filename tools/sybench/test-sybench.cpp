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

#include <KArchiveDirectory>
#include <KArchiveFile>
#include <KTar>
#include <QTemporaryDir>
#include <QtTest>

#include "dimension.h"
#include "ladder.h"
#include "projectgen.h"
#include "runner.h"
#include "score.h"
#include "utils/tomlutils.h"

using namespace SyBench;

class TestSyBench : public QObject
{
    Q_OBJECT
private:
    static QVariantHash readToml(const KArchiveDirectory *root, const QString &path)
    {
        const auto *entry = root->entry(path);
        if (entry == nullptr || !entry->isFile())
            return {};
        QString error;
        return parseTomlData(static_cast<const KArchiveFile *>(entry)->data(), error);
    }

private slots:
    void projectGeneration()
    {
        ProjectSpec spec;
        spec.experimentId = QStringLiteral("gen-test");
        spec.addModule(Modules::dataSourceCamera(QStringLiteral("Cam"), 640, 480, 30));
        spec.addModule(
            Modules::flowMeter(
                QStringLiteral("Meter"),
                QStringLiteral("Frame"),
                QStringLiteral("Cam"),
                QStringLiteral("frames-out")));

        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        const auto fname = tmpDir.filePath(QStringLiteral("gen.syct"));
        const auto written = writeProjectFile(spec, fname);
        QVERIFY2(written.has_value(), qPrintable(written.error_or(QString())));

        KTar tar(fname);
        QVERIFY(tar.open(QIODevice::ReadOnly));
        const auto *root = tar.directory();
        const auto entries = root->entries();
        QVERIFY(entries.contains(QStringLiteral("main.toml")));
        QVERIFY(entries.contains(QStringLiteral("graph.toml")));
        QVERIFY(entries.contains(QStringLiteral("subjects.toml")));
        QVERIFY(entries.contains(QStringLiteral("experimenters.toml")));
        QVERIFY(entries.contains(QStringLiteral("000-devel.datasource")));
        QVERIFY(entries.contains(QStringLiteral("001-flowmeter")));

        const auto main = readToml(root, QStringLiteral("main.toml"));
        QCOMPARE(main.value(QStringLiteral("experiment_id")).toString(), QStringLiteral("gen-test"));
        QCOMPARE(main.value(QStringLiteral("version_format")).toString(), QStringLiteral("1"));
        QVERIFY(main.value(QStringLiteral("storage")).toHash().contains(QStringLiteral("order")));

        const auto camSettings = readToml(root, QStringLiteral("000-devel.datasource/devel.datasource.toml"));
        QCOMPARE(camSettings.value(QStringLiteral("fps")).toInt(), 30);
        QCOMPARE(camSettings.value(QStringLiteral("frame_width")).toInt(), 640);
        QCOMPARE(camSettings.value(QStringLiteral("frame_content")).toString(), QStringLiteral("camera"));

        const auto camInfo = readToml(root, QStringLiteral("000-devel.datasource/info.toml"));
        QCOMPARE(camInfo.value(QStringLiteral("id")).toString(), QStringLiteral("devel.datasource"));
        QCOMPARE(camInfo.value(QStringLiteral("name")).toString(), QStringLiteral("Cam"));
        QVERIFY(camInfo.value(QStringLiteral("enabled")).toBool());
        QVERIFY(camInfo.value(QStringLiteral("subscriptions")).toHash().isEmpty());

        const auto meterInfo = readToml(root, QStringLiteral("001-flowmeter/info.toml"));
        QCOMPARE(meterInfo.value(QStringLiteral("name")).toString(), QStringLiteral("Meter"));
        const auto subs = meterInfo.value(QStringLiteral("subscriptions")).toHash();
        QCOMPARE(subs.size(), 1);
        QCOMPARE(
            subs.value(QStringLiteral("data-in")).toStringList(),
            (QStringList{QStringLiteral("Cam"), QStringLiteral("frames-out")}));

        const auto meterSettings = readToml(root, QStringLiteral("001-flowmeter/flowmeter.toml"));
        QCOMPARE(meterSettings.value(QStringLiteral("data_type")).toString(), QStringLiteral("Frame"));

        const auto graph = readToml(root, QStringLiteral("graph.toml"));
        const auto positions = graph.value(QStringLiteral("NodePositions")).toHash();
        QCOMPARE(positions.size(), 2);
        // the meter depends on the camera and must be placed in the next column
        QVERIFY(
            positions.value(QStringLiteral("Meter")).toList().first().toDouble()
            > positions.value(QStringLiteral("Cam")).toList().first().toDouble());
    }

    void projectValidation()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        ProjectSpec dup;
        dup.addModule(Modules::dataSourceCamera(QStringLiteral("Cam"), 640, 480, 30));
        dup.addModule(Modules::dataSourceCamera(QStringLiteral("Cam"), 640, 480, 30));
        const auto dupRes = writeProjectFile(dup, tmpDir.filePath(QStringLiteral("dup.syct")));
        QVERIFY(!dupRes.has_value());
        QVERIFY(dupRes.error().contains(QStringLiteral("Duplicate")));

        ProjectSpec dangling;
        dangling.addModule(
            Modules::flowMeter(
                QStringLiteral("Meter"),
                QStringLiteral("Frame"),
                QStringLiteral("Nobody"),
                QStringLiteral("frames-out")));
        const auto danglingRes = writeProjectFile(dangling, tmpDir.filePath(QStringLiteral("dangling.syct")));
        QVERIFY(!danglingRes.has_value());
        QVERIFY(danglingRes.error().contains(QStringLiteral("unknown module")));
    }

    void cameraDimensionProject()
    {
        auto dim = createDimension(QStringLiteral("camera-capacity"));
        QVERIFY(dim != nullptr);
        QCOMPARE(dim->profiles().size(), 2);
        const auto spec = dim->buildProject(QStringLiteral("720p120"), 3);
        QCOMPARE(spec.modules.size(), 15);
        QVERIFY(spec.hasModule(QStringLiteral("Meter 3")));
        const auto *cam = spec.module(QStringLiteral("Camera 2"));
        QVERIFY(cam != nullptr);
        QCOMPARE(cam->settings.value(QStringLiteral("fps")).toInt(), 120);
        QCOMPARE(cam->settings.value(QStringLiteral("frame_height")).toInt(), 720);
        const auto *scale = spec.module(QStringLiteral("Scale 2"));
        QVERIFY(scale != nullptr);
        QCOMPARE(scale->subscriptions.value(QStringLiteral("frames-in")).srcModuleName, QStringLiteral("Camera 2"));
        QCOMPARE(dim->startLevel(16, QStringLiteral("1080p30")), 8);
        QCOMPARE(dim->startLevel(1, QStringLiteral("1080p30")), 1);
    }

    void allDimensionProjects()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        const auto dims = createAllDimensions();
        QCOMPARE(dims.size(), 5u);
        for (const auto &dim : dims) {
            QVERIFY(!dim->profiles().isEmpty());
            for (const auto &profile : dim->profiles()) {
                const int level = dim->startLevel(8, profile.id);
                const auto spec = dim->buildProject(profile.id, level);
                QVERIFY2(!spec.modules.isEmpty(), qPrintable(dim->id()));
                const auto fname = tmpDir.filePath(QStringLiteral("%1-%2.syct").arg(dim->id(), profile.id));
                const auto res = writeProjectFile(spec, fname);
                QVERIFY2(res.has_value(), qPrintable(dim->id() + QStringLiteral(": ") + res.error_or(QString())));
            }
        }
    }

    void encodingProjects()
    {
        auto enc = createDimension(QStringLiteral("encoding"));
        QVERIFY(enc != nullptr);
        QVERIFY(enc->writesData(QStringLiteral("1080p30-av1")));
        const auto av1 = enc->buildProject(QStringLiteral("1080p30-av1"), 2);
        QCOMPARE(av1.modules.size(), 6);
        const auto *rec = av1.module(QStringLiteral("Recorder 2"));
        QVERIFY(rec != nullptr);
        QCOMPARE(rec->id, QStringLiteral("videorecorder"));
        QCOMPARE(rec->settings.value(QStringLiteral("video_codec")).toInt(), 3);
        QVERIFY(!rec->settings.value(QStringLiteral("lossless")).toBool());
        QVERIFY(!rec->settings.value(QStringLiteral("deferred_encode_enabled")).toBool());
        QCOMPARE(rec->subscriptions.value(QStringLiteral("frames-in")).srcModuleName, QStringLiteral("Camera 2"));

        auto disk = createDimension(QStringLiteral("disk-write"));
        const auto raw = disk->buildProject(QStringLiteral("raw1080p30"), 1);
        const auto *rawRec = raw.module(QStringLiteral("Recorder 1"));
        QVERIFY(rawRec != nullptr);
        QCOMPARE(rawRec->settings.value(QStringLiteral("video_codec")).toInt(), 1);
    }

    void signalAndOopProjects()
    {
        auto sig = createDimension(QStringLiteral("signal-processing"));
        QCOMPARE(sig->startLevel(16, QStringLiteral("30khz-filter")), 64);
        const auto sp = sig->buildProject(QStringLiteral("30khz-filter"), 128);
        const auto *amp = sp.module(QStringLiteral("Amplifier 1"));
        QVERIFY(amp != nullptr);
        QCOMPARE(amp->settings.value(QStringLiteral("signal_channels")).toInt(), 128);
        QCOMPARE(amp->settings.value(QStringLiteral("sample_rate")).toDouble(), 30000.0);
        QVERIFY(!sp.hasModule(QStringLiteral("Zarr Writer 1")));
        QVERIFY(sp.hasModule(QStringLiteral("Meter 1")));
        // more channels are spread over amplifiers of 1024 channels each
        const auto big = sig->buildProject(QStringLiteral("30khz-filter"), 2560);
        QVERIFY(big.hasModule(QStringLiteral("Amplifier 3")));
        QVERIFY(!big.hasModule(QStringLiteral("Amplifier 4")));
        QCOMPARE(
            big.module(QStringLiteral("Amplifier 3"))->settings.value(QStringLiteral("signal_channels")).toInt(),
            512);
        QVERIFY(!sig->writesData(QStringLiteral("30khz-filter")));

        auto disk = createDimension(QStringLiteral("disk-write"));
        QCOMPARE(disk->profiles().size(), 2);
        QCOMPARE(disk->levelUnit(QStringLiteral("zarr-30khz")), QStringLiteral("channels"));
        QCOMPARE(disk->levelUnit(QStringLiteral("raw1080p30")), QStringLiteral("streams"));
        QCOMPARE(disk->startLevel(16, QStringLiteral("zarr-30khz")), 64);
        const auto zarr = disk->buildProject(QStringLiteral("zarr-30khz"), 2560);
        QVERIFY(zarr.hasModule(QStringLiteral("Zarr Writer 3")));
        QVERIFY(!zarr.hasModule(QStringLiteral("Filter 1")));
        QCOMPARE(
            zarr.module(QStringLiteral("Zarr Writer 3"))
                ->subscriptions.value(QStringLiteral("f32sig-in"))
                .srcModuleName,
            QStringLiteral("Amplifier 3"));

        auto oop = createDimension(QStringLiteral("out-of-process"));
        QCOMPARE(oop->profiles().size(), 3);
        QCOMPARE(oop->startLevel(16, QStringLiteral("cpp-frames")), 30);
        QCOMPARE(oop->levelUnit(QStringLiteral("cpp-frames")), QStringLiteral("fps"));
        const auto cpp = oop->buildProject(QStringLiteral("cpp-frames"), 60);
        QVERIFY(cpp.hasModule(QStringLiteral("Reference Source")));
        const auto *worker = cpp.module(QStringLiteral("Worker 1"));
        QVERIFY(worker != nullptr);
        QCOMPARE(worker->id, QStringLiteral("example-mlink"));
        QCOMPARE(worker->subscriptions.value(QStringLiteral("frames-in")).srcModuleName, QStringLiteral("Source 1"));
        const auto *cam = cpp.module(QStringLiteral("Source 1"));
        QCOMPARE(cam->settings.value(QStringLiteral("fps")).toInt(), 60);
        QCOMPARE(cam->settings.value(QStringLiteral("frame_content")).toString(), QStringLiteral("testcard"));
        QVERIFY(!cam->settings.contains(QStringLiteral("rows_per_tick")));
        // high rates are spread over several source/worker pairs
        const auto fast = oop->buildProject(QStringLiteral("cpp-frames"), 960);
        QVERIFY(fast.hasModule(QStringLiteral("Worker 2")));
        QVERIFY(!fast.hasModule(QStringLiteral("Worker 3")));
        QCOMPARE(fast.module(QStringLiteral("Source 2"))->settings.value(QStringLiteral("fps")).toInt(), 480);

        // rows: bursts on a 1 kHz tick, always through a single worker
        QCOMPARE(oop->startLevel(16, QStringLiteral("cpp-rows")), 16000);
        QCOMPARE(oop->levelUnit(QStringLiteral("cpp-rows")), QStringLiteral("rows/s"));
        const auto rows = oop->buildProject(QStringLiteral("cpp-rows"), 6000000);
        QVERIFY(rows.hasModule(QStringLiteral("Worker 1")));
        QVERIFY(!rows.hasModule(QStringLiteral("Worker 2")));
        const auto *rowSrc = rows.module(QStringLiteral("Source 1"));
        QCOMPARE(rowSrc->settings.value(QStringLiteral("fps")).toInt(), 1000);
        QCOMPARE(rowSrc->settings.value(QStringLiteral("rows_per_tick")).toInt(), 6000);
        const auto *rowWorker = rows.module(QStringLiteral("Worker 1"));
        QCOMPARE(rowWorker->id, QStringLiteral("example-mlink"));
        QCOMPARE(rowWorker->subscriptions.value(QStringLiteral("table-in")).srcPortId, QStringLiteral("rows-out"));
        QCOMPARE(
            rows.module(QStringLiteral("Meter 1"))->subscriptions.value(QStringLiteral("data-in")).srcPortId,
            QStringLiteral("table-out"));
        QCOMPARE(
            rows.module(QStringLiteral("Meter 1"))->settings.value(QStringLiteral("data_type")).toString(),
            QStringLiteral("TableRow"));
        const auto pyRows = oop->buildProject(QStringLiteral("python-rows"), 1000);
        QVERIFY(pyRows.module(QStringLiteral("Worker 1"))->extraData.contains("get_input_port('rows-in')"));
    }

    static StepResult makeResult(qint64 items, qint64 peakPending, qint64 pendingAtStop)
    {
        Syntalos::RunStatistics stats;
        stats.durationSec = 10.0;
        Syntalos::ModuleRunStats meter;
        meter.id = QStringLiteral("flowmeter");
        meter.name = QStringLiteral("Meter 1");
        meter.moduleStats.insert(QStringLiteral("items"), items);
        stats.modules.append(meter);
        Syntalos::ConnectionRunStats conn;
        conn.srcModule = QStringLiteral("Cam");
        conn.dstModule = QStringLiteral("Meter 1");
        conn.peakPending = peakPending;
        conn.pendingAtStop = pendingAtStop;
        stats.connections.append(conn);

        StepResult r;
        r.success = true;
        r.stats = stats;
        return r;
    }

    void rateEvaluation()
    {
        const QList<RateCheck> checks = {
            RateCheck{QStringLiteral("Meter 1"), 30.0}
        };

        auto v = evaluateRates(makeResult(298, 3, 0), checks);
        QVERIFY2(v.passed, qPrintable(v.summary));
        QVERIFY(v.minRateFraction > 0.99);

        // too few items
        QVERIFY(!evaluateRates(makeResult(280, 3, 0), checks).passed);
        // items left in the queue at stop
        QVERIFY(!evaluateRates(makeResult(300, 3, 5), checks).passed);
        // more than two seconds of backlog at 30 fps
        QVERIFY(!evaluateRates(makeResult(300, 70, 0), checks).passed);
        QVERIFY(evaluateRates(makeResult(300, 40, 0), checks).passed);

        auto failed = makeResult(300, 0, 0);
        failed.success = false;
        failed.failureReason = QStringLiteral("boom");
        v = evaluateRates(failed, checks);
        QVERIFY(!v.passed);
        QCOMPARE(v.summary, QStringLiteral("boom"));

        // a source starved on a saturated machine is an overload failure, not a source limit
        auto starved = makeResult(250, 0, 0);
        starved.stats->cpuCoreCount = 8;
        starved.stats->usageWindowSec = 10.0;
        Syntalos::ThreadUsageStats busy;
        busy.userTimeSec = 75.0;
        starved.stats->process = busy;
        v = evaluateRates(
            starved,
            {
                RateCheck{.moduleName = QStringLiteral("Meter 1"), .expectedRate = 30.0, .isSource = true}
        });
        QVERIFY(!v.passed);
        QVERIFY(!v.sourceLimited);
        QVERIFY(v.summary.contains(QStringLiteral("overloaded")));

        // a source that misses its own rate makes the step inconclusive rather than a failure
        v = evaluateRates(
            makeResult(250, 0, 0),
            {
                RateCheck{.moduleName = QStringLiteral("Meter 1"), .expectedRate = 30.0, .isSource = true}
        });
        QVERIFY(!v.passed);
        QVERIFY(v.sourceLimited);
        v = evaluateRates(
            makeResult(300, 0, 0),
            {
                RateCheck{.moduleName = QStringLiteral("Meter 1"), .expectedRate = 30.0, .isSource = true}
        });
        QVERIFY(v.passed);

        // a meter that never reported is a failure, not a pass
        QVERIFY(!evaluateRates(
                     makeResult(300, 0, 0),
                     {
                         RateCheck{QStringLiteral("Meter 2"), 30.0}
        })
                     .passed);
    }

    void runStatisticsRoundTrip()
    {
        auto stats = makeResult(123, 4, 1).stats.value();
        stats.runId = QStringLiteral("run-1");
        stats.threadsTotal = 7;
        Syntalos::ThreadUsageStats usage;
        usage.userTimeSec = 1.5;
        usage.systemTimeSec = 0.25;
        stats.process = usage;

        const auto loaded = Syntalos::RunStatistics::fromJson(stats.toJson());
        QVERIFY2(loaded.has_value(), qPrintable(loaded.error_or(QString())));
        QCOMPARE(loaded->runId, QStringLiteral("run-1"));
        QCOMPARE(loaded->threadsTotal, 7);
        QCOMPARE(loaded->durationSec, 10.0);
        QVERIFY(loaded->process.has_value());
        QCOMPARE(loaded->process->cpuTimeSec(), 1.75);
        QCOMPARE(loaded->modules.size(), 1);
        QCOMPARE(loaded->modules.first().moduleStats.value(QStringLiteral("items")).toLongLong(), 123);
        QCOMPARE(loaded->connections.size(), 1);
        QCOMPARE(loaded->connections.first().peakPending, 4u);

        QVERIFY(!Syntalos::RunStatistics::fromJson(QJsonObject()).has_value());
    }

    static LadderRecord ladderResult(const QString &dim, const QString &profile, int sustained)
    {
        LadderRecord lr;
        lr.dimensionId = dim;
        lr.profileId = profile;
        lr.outcome.sustained = sustained;
        return lr;
    }

    void scoreComputation()
    {
        SessionConfig cfg;
        // the reference machine scores 1000 on every profile
        QList<LadderRecord> ref;
        for (const auto &dim : createAllDimensions()) {
            for (const auto &p : dim->profiles()) {
                QVERIFY2(referenceLevel(dim->id(), p.id) > 0, qPrintable(dim->id() + QStringLiteral("/") + p.id));
                ref.append(ladderResult(dim->id(), p.id, referenceLevel(dim->id(), p.id)));
            }
        }
        auto score = computeScore(ref, cfg);
        QVERIFY(score.valid());
        QCOMPARE(score.overall, 1000);
        QVERIFY(!score.partial);
        QVERIFY(!score.quick);
        QCOMPARE(score.profilesScored, score.profilesTotal);
        QCOMPARE(scoreHeadline(score), QStringLiteral("1000 points"));
        for (const auto &d : score.dimensions)
            QCOMPARE(d.score, 1000);

        // twice the capacity everywhere doubles the score
        QList<LadderRecord> twice;
        for (auto lr : ref) {
            lr.outcome.sustained *= 2;
            twice.append(lr);
        }
        QCOMPARE(computeScore(twice, cfg).overall, 2000);

        // half the cameras, everything else at reference: the geometric mean drops accordingly
        QList<LadderRecord> weakCams = ref;
        for (auto &lr : weakCams) {
            if (lr.dimensionId == QLatin1String("camera-capacity"))
                lr.outcome.sustained /= 2;
        }
        score = computeScore(weakCams, cfg);
        QVERIFY(score.overall < 1000 && score.overall > 800);
        for (const auto &d : score.dimensions) {
            if (d.dimensionId == QLatin1String("camera-capacity"))
                QVERIFY(d.score >= 470 && d.score <= 510); // integer halving of 21 streams lands below 500
            else
                QCOMPARE(d.score, 1000);
        }

        // a partial quick run is marked as such, cancelled ladders are ignored
        cfg.quick = true;
        QList<LadderRecord> partial = {ladderResult(QStringLiteral("camera-capacity"), QStringLiteral("1080p30"), 36)};
        auto cancelled = ladderResult(QStringLiteral("encoding"), QStringLiteral("1080p30-av1"), 1);
        cancelled.outcome.cancelled = true;
        partial.append(cancelled);
        score = computeScore(partial, cfg);
        QCOMPARE(score.overall, 1000);
        QVERIFY(score.partial);
        QVERIFY(score.quick);
        QCOMPARE(score.profilesScored, 1);
        QVERIFY(scoreHeadline(score).startsWith(QStringLiteral("~ 1000 points (quick mode, partial run")));

        // nothing scored
        QVERIFY(!computeScore({}, cfg).valid());
        QCOMPARE(scoreHeadline(computeScore({}, cfg)), QStringLiteral("No score"));
    }

    void ladderSearch()
    {
        LadderConfig cfg;
        cfg.startLevel = 4;
        cfg.maxLevel = 1024;
        cfg.bisections = 2;

        // levels the search tried, in order (a cancelled attempt does not count)
        QList<int> tried;
        const auto takeTried = [&tried]() {
            return std::exchange(tried, {});
        };
        const auto recording = [&tried](auto fn) {
            return [&tried, fn](int level) {
                const auto res = fn(level);
                if (res != LevelResult::Cancelled)
                    tried.append(level);
                return res;
            };
        };

        // machine sustains 11 units
        auto o = runLadder(cfg, recording([](int level) {
                               return level <= 11 ? LevelResult::Passed : LevelResult::Failed;
                           }));
        QCOMPARE(takeTried(), (QList<int>{4, 8, 16, 12, 10}));
        QCOMPARE(o.sustained, 10);
        QVERIFY(!o.reachedMax);
        QVERIFY(!o.cancelled);

        // even the start level fails: halve until something passes, then refine
        o = runLadder(cfg, recording([](int level) {
                          return level <= 3 ? LevelResult::Passed : LevelResult::Failed;
                      }));
        QCOMPARE(takeTried(), (QList<int>{4, 2, 3}));
        QCOMPARE(o.sustained, 3);

        // nothing works at all
        o = runLadder(cfg, recording([](int) {
                          return LevelResult::Failed;
                      }));
        QCOMPARE(takeTried(), (QList<int>{4, 2, 1}));
        QCOMPARE(o.sustained, 0);

        // the maximum level passes
        cfg.maxLevel = 16;
        o = runLadder(cfg, recording([](int) {
                          return LevelResult::Passed;
                      }));
        QCOMPARE(takeTried(), (QList<int>{4, 8, 16}));
        QCOMPARE(o.sustained, 16);
        QVERIFY(o.reachedMax);

        // cancellation stops the search
        int calls = 0;
        o = runLadder(cfg, recording([&calls](int) {
                          if (++calls == 2)
                              return LevelResult::Cancelled;
                          return LevelResult::Passed;
                      }));
        QVERIFY(o.cancelled);
        QCOMPARE(takeTried(), (QList<int>{4}));
        QCOMPARE(o.sustained, 4);

        // a source limit bounds the search from above, the refinement still runs below it
        cfg.maxLevel = 1024;
        o = runLadder(cfg, recording([](int level) {
                          return level >= 16 ? LevelResult::Inconclusive : LevelResult::Passed;
                      }));
        QVERIFY(o.inconclusive);
        QVERIFY(!o.cancelled);
        QCOMPARE(takeTried(), (QList<int>{4, 8, 16, 12, 14}));
        QCOMPARE(o.sustained, 14);

        // a real failure below the source-limited levels bounds the result, so it is not source-limited
        o = runLadder(cfg, recording([](int level) {
                          if (level >= 16)
                              return LevelResult::Inconclusive;
                          return level > 10 ? LevelResult::Failed : LevelResult::Passed;
                      }));
        QCOMPARE(takeTried(), (QList<int>{4, 8, 16, 12, 10}));
        QCOMPARE(o.sustained, 10);
        QVERIFY(!o.inconclusive);
    }
};

QTEST_GUILESS_MAIN(TestSyBench)
#include "test-sybench.moc"

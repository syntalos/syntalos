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
        QCOMPARE(oop->startLevel(16, QStringLiteral("python")), 30);
        const auto py = oop->buildProject(QStringLiteral("python"), 60);
        QVERIFY(py.hasModule(QStringLiteral("Reference Camera")));
        const auto *worker = py.module(QStringLiteral("Worker 1"));
        QVERIFY(worker != nullptr);
        QCOMPARE(worker->id, QStringLiteral("pyscript"));
        QVERIFY(worker->extraData.contains("oport.submit(frame)"));
        QCOMPARE(py.module(QStringLiteral("Camera 1"))->settings.value(QStringLiteral("fps")).toInt(), 60);
        // high rates are spread over several camera/worker pairs
        const auto fast = oop->buildProject(QStringLiteral("python"), 960);
        QVERIFY(fast.hasModule(QStringLiteral("Worker 2")));
        QVERIFY(!fast.hasModule(QStringLiteral("Worker 3")));
        QCOMPARE(fast.module(QStringLiteral("Camera 2"))->settings.value(QStringLiteral("fps")).toInt(), 480);
        const auto cpp = oop->buildProject(QStringLiteral("cpp"), 60);
        QCOMPARE(cpp.module(QStringLiteral("Worker 1"))->id, QStringLiteral("example-mlink"));
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

    static QList<int> levelsOf(const LadderOutcome &o)
    {
        QList<int> res;
        for (const auto &s : o.steps)
            res.append(s.level);
        return res;
    }

    void ladderSearch()
    {
        LadderConfig cfg;
        cfg.startLevel = 4;
        cfg.maxLevel = 1024;
        cfg.bisections = 2;

        // machine sustains 11 units
        auto o = runLadder(cfg, [](int level) {
            return level <= 11 ? LevelResult::Passed : LevelResult::Failed;
        });
        QCOMPARE(levelsOf(o), (QList<int>{4, 8, 16, 12, 10}));
        QCOMPARE(o.sustained, 10);
        QVERIFY(!o.reachedMax);
        QVERIFY(!o.cancelled);

        // even the start level fails: halve until something passes, then refine
        o = runLadder(cfg, [](int level) {
            return level <= 3 ? LevelResult::Passed : LevelResult::Failed;
        });
        QCOMPARE(levelsOf(o), (QList<int>{4, 2, 3}));
        QCOMPARE(o.sustained, 3);

        // nothing works at all
        o = runLadder(cfg, [](int) {
            return LevelResult::Failed;
        });
        QCOMPARE(levelsOf(o), (QList<int>{4, 2, 1}));
        QCOMPARE(o.sustained, 0);

        // the maximum level passes
        cfg.maxLevel = 16;
        o = runLadder(cfg, [](int) {
            return LevelResult::Passed;
        });
        QCOMPARE(levelsOf(o), (QList<int>{4, 8, 16}));
        QCOMPARE(o.sustained, 16);
        QVERIFY(o.reachedMax);

        // cancellation stops the search
        int calls = 0;
        o = runLadder(cfg, [&calls](int) {
            if (++calls == 2)
                return LevelResult::Cancelled;
            return LevelResult::Passed;
        });
        QVERIFY(o.cancelled);
        QCOMPARE(o.steps.size(), 1);
        QCOMPARE(o.sustained, 4);

        // a source limit bounds the search from above, the refinement still runs below it
        cfg.maxLevel = 1024;
        o = runLadder(cfg, [](int level) {
            return level >= 16 ? LevelResult::Inconclusive : LevelResult::Passed;
        });
        QVERIFY(o.inconclusive);
        QVERIFY(!o.cancelled);
        QCOMPARE(levelsOf(o), (QList<int>{4, 8, 16, 12, 14}));
        QCOMPARE(o.sustained, 14);

        // a real failure below the source-limited levels bounds the result, so it is not source-limited
        o = runLadder(cfg, [](int level) {
            if (level >= 16)
                return LevelResult::Inconclusive;
            return level > 10 ? LevelResult::Failed : LevelResult::Passed;
        });
        QCOMPARE(levelsOf(o), (QList<int>{4, 8, 16, 12, 10}));
        QCOMPARE(o.sustained, 10);
        QVERIFY(!o.inconclusive);
    }
};

QTEST_GUILESS_MAIN(TestSyBench)
#include "test-sybench.moc"

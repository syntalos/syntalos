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
        const auto data = parseTomlData(static_cast<const KArchiveFile *>(entry)->data(), error);
        if (!error.isEmpty())
            qWarning() << "TOML error in" << path << ":" << error;
        return data;
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
        QCOMPARE(spec.modules.size(), 9);
        QVERIFY(spec.hasModule(QStringLiteral("Meter 3")));
        const auto *cam = spec.module(QStringLiteral("Camera 2"));
        QVERIFY(cam != nullptr);
        QCOMPARE(cam->settings.value(QStringLiteral("fps")).toInt(), 120);
        QCOMPARE(cam->settings.value(QStringLiteral("frame_height")).toInt(), 720);
        const auto *scale = spec.module(QStringLiteral("Scale 2"));
        QVERIFY(scale != nullptr);
        QCOMPARE(scale->subscriptions.value(QStringLiteral("frames-in")).srcModuleName, QStringLiteral("Camera 2"));
        QCOMPARE(dim->startLevel(16), 8);
        QCOMPARE(dim->startLevel(1), 1);
    }

    void rateEvaluation()
    {
        StepResult r;
        r.success = true;
        r.durationSec = 10.0;
        r.meters.append(MeterStats{QStringLiteral("Meter 1"), 298});
        r.connections.append(
            ConnectionStats{
                QStringLiteral("Cam"),
                QStringLiteral("frames-out"),
                QStringLiteral("Meter 1"),
                QStringLiteral("data-in"),
                3,
                0});
        const QList<RateCheck> checks = {
            RateCheck{QStringLiteral("Meter 1"), 30.0}
        };

        auto v = evaluateRates(r, checks);
        QVERIFY2(v.passed, qPrintable(v.summary));
        QVERIFY(v.minRateFraction > 0.99);

        r.meters[0].items = 280;
        v = evaluateRates(r, checks);
        QVERIFY(!v.passed);

        r.meters[0].items = 300;
        r.connections[0].pendingAtStop = 5;
        v = evaluateRates(r, checks);
        QVERIFY(!v.passed);
        r.connections[0].pendingAtStop = 0;

        r.connections[0].peakPending = 40; // more than half a second at 30 fps
        v = evaluateRates(r, checks);
        QVERIFY(!v.passed);
        r.connections[0].peakPending = 10;
        v = evaluateRates(r, checks);
        QVERIFY(v.passed);

        r.success = false;
        r.failureReason = QStringLiteral("boom");
        v = evaluateRates(r, checks);
        QVERIFY(!v.passed);
        QCOMPARE(v.summary, QStringLiteral("boom"));

        // a meter that never reported is a failure, not a pass
        r.success = true;
        v = evaluateRates(
            r,
            {
                RateCheck{QStringLiteral("Meter 2"), 30.0}
        });
        QVERIFY(!v.passed);
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
        auto o = runLadder(cfg, [](int level) -> std::optional<bool> {
            return level <= 11;
        });
        QCOMPARE(levelsOf(o), (QList<int>{4, 8, 16, 12, 10}));
        QCOMPARE(o.sustained, 10);
        QVERIFY(!o.reachedMax);
        QVERIFY(!o.cancelled);

        // even the start level fails: halve until something passes, then refine
        o = runLadder(cfg, [](int level) -> std::optional<bool> {
            return level <= 3;
        });
        QCOMPARE(levelsOf(o), (QList<int>{4, 2, 3}));
        QCOMPARE(o.sustained, 3);

        // nothing works at all
        o = runLadder(cfg, [](int) -> std::optional<bool> {
            return false;
        });
        QCOMPARE(levelsOf(o), (QList<int>{4, 2, 1}));
        QCOMPARE(o.sustained, 0);

        // the maximum level passes
        cfg.maxLevel = 16;
        o = runLadder(cfg, [](int) -> std::optional<bool> {
            return true;
        });
        QCOMPARE(levelsOf(o), (QList<int>{4, 8, 16}));
        QCOMPARE(o.sustained, 16);
        QVERIFY(o.reachedMax);

        // cancellation stops the search
        int calls = 0;
        o = runLadder(cfg, [&calls](int) -> std::optional<bool> {
            if (++calls == 2)
                return std::nullopt;
            return true;
        });
        QVERIFY(o.cancelled);
        QCOMPARE(o.steps.size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestSyBench)
#include "test-sybench.moc"

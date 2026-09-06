#include <QtTest>

#include "utils/resourceinfo.h"

using namespace Syntalos;

class TestResourceInfo : public QObject
{
    Q_OBJECT
private slots:

    void trendProjection()
    {
        ResourceTrend trend;
        QVERIFY(!trend.hasRate());
        QCOMPARE(trend.secondsUntilDepleted(), -1.0);
        QCOMPARE(trend.lastRemaining(), -1);

        // first sample only establishes the baseline
        trend.addSample(1000, 0.0);
        QVERIFY(!trend.hasRate());
        QCOMPARE(trend.lastRemaining(), 1000);

        // steady consumption of 10 units/sec
        trend.addSample(900, 10.0);
        QVERIFY(trend.hasRate());
        QCOMPARE(trend.consumptionRate(), 10.0);
        QCOMPARE(trend.secondsUntilDepleted(), 90.0);

        trend.addSample(800, 10.0);
        QCOMPARE(trend.consumptionRate(), 10.0);
        QCOMPARE(trend.secondsUntilDepleted(), 80.0);

        // speeding up: the moving average follows, but smoothed
        trend.addSample(600, 10.0);
        QCOMPARE(trend.consumptionRate(), 15.0);
        QCOMPARE(trend.secondsUntilDepleted(), 40.0);

        // the resource growing again counts as zero consumption, never negative
        trend.addSample(700, 10.0);
        QCOMPARE(trend.consumptionRate(), 7.5);
        trend.addSample(700, 10.0);
        trend.addSample(700, 10.0);
        trend.addSample(700, 10.0);
        trend.addSample(700, 10.0);
        trend.addSample(700, 10.0);
        QVERIFY(trend.consumptionRate() < 0.5);
        QVERIFY(trend.consumptionRate() >= 0.0);

        // samples that are too close in time are ignored
        const auto rateBefore = trend.consumptionRate();
        trend.addSample(100, 0.001);
        QCOMPARE(trend.consumptionRate(), rateBefore);
        QCOMPARE(trend.lastRemaining(), 700);

        trend.reset();
        QVERIFY(!trend.hasRate());
        QCOMPARE(trend.lastRemaining(), -1);
    }

    void trendMinimumRate()
    {
        ResourceTrend trend(50.0);
        trend.addSample(1000, 0.0);
        trend.addSample(990, 1.0);
        QCOMPARE(trend.consumptionRate(), 10.0);
        QVERIFY(!trend.hasRate());
        QCOMPARE(trend.secondsUntilDepleted(), -1.0);

        trend.addSample(790, 1.0);
        QVERIFY(trend.hasRate());
        QCOMPARE(trend.consumptionRate(), 105.0);
    }

    void trendWallClock()
    {
        ResourceTrend trend;
        trend.addSample(1000);
        QTest::qWait(150);
        trend.addSample(900);
        QVERIFY(trend.hasRate());
        QVERIFY(trend.consumptionRate() > 100.0);
        QVERIFY(trend.consumptionRate() < 1200.0);
    }

    void diskSpace()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        const auto disk = diskSpaceInfo(tmpDir.path());
        QVERIFY(disk.valid);
        QVERIFY(disk.bytesAvailable >= 0);
        QVERIFY(disk.bytesTotal > 0);
        QVERIFY(disk.bytesAvailable <= disk.bytesTotal);
        QVERIFY(!disk.mountPoint.isEmpty());
        QVERIFY(QDir(disk.mountPoint).exists());
        QVERIFY(tmpDir.path().startsWith(disk.mountPoint) || disk.mountPoint == QStringLiteral("/"));

        // a path that does not exist yet reports the filesystem of its closest parent
        const auto future = diskSpaceInfo(tmpDir.filePath("not/yet/created"));
        QVERIFY(future.valid);
        QCOMPARE(future.deviceId, disk.deviceId);
        QCOMPARE(future.mountPoint, disk.mountPoint);

        // the root filesystem is always known
        QVERIFY(diskSpaceInfo(QStringLiteral("/")).valid);
        QCOMPARE(diskSpaceInfo(QStringLiteral("/")).mountPoint, QStringLiteral("/"));
    }

    void memoryInfo()
    {
        const auto mem = readMemInfo();
        QVERIFY(mem.memTotalKiB > 0);
        QVERIFY(mem.memAvailableKiB > 0);
        QVERIFY(mem.memAvailableKiB <= mem.memTotalKiB);
        QCOMPARE(mem.memAvailableMiB, mem.memAvailableKiB / 1024);
        QVERIFY(mem.memAvailablePercent > 0 && mem.memAvailablePercent <= 100);
        QVERIFY(mem.swapFreeKiB <= mem.swapTotalKiB);

        // PSI may be unavailable, but must never yield garbage
        const auto pressure = readMemPressure();
        QVERIFY(pressure.someAvg10 >= 0 && pressure.someAvg10 <= 100);
        QVERIFY(pressure.fullAvg10 >= 0 && pressure.fullAvg10 <= 100);
        if (!pressure.available)
            QCOMPARE(pressure.someAvg60, 0.0);
    }
};

QTEST_MAIN(TestResourceInfo)
#include "test-resourceinfo.moc"

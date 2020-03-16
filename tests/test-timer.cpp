
#include <iostream>
#include <QtTest>
#include <QDebug>

#include "syclock.h"
#include "timesync.h"
#include "utils.h"

using namespace Syntalos;
using namespace Eigen;

int slow_work_with_result(int para)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return 40 + para;
}

class TestTimer : public QObject
{
    Q_OBJECT
private slots:
    void runFuncTimer()
    {
        std::unique_ptr<SyncTimer> timer(new SyncTimer());

        timer->start();

        auto res = TIMER_FUNC_TIMESTAMP(timer, slow_work_with_result(2));
        std::this_thread::sleep_for(std::chrono::milliseconds(12));
        QVERIFY((res.count() < 251) && (res.count() > 249));

        QVERIFY(timer->timeSinceStartMsec().count() >= 512);
    }

    void runTSyncFile()
    {
        auto tsFilename = QStringLiteral("/tmp/tstest-%1").arg(createRandomString(8));

        // write a timesync file
        auto tswriter = new TimeSyncFileWriter;
        tswriter->setFileName(tsFilename);
        auto ret = tswriter->open(microseconds_t(4 * 1000 * 1000), microseconds_t(1500), QStringLiteral("UnittestDummyModule"));
        QVERIFY2(ret, qPrintable(tswriter->lastError()));

        for (int i = 0; i < 100; ++i)
            tswriter->writeTimeOffset(microseconds_t(i * 1000), microseconds_t(i * 50));
        delete tswriter;

        // read the timesync file
        auto tsreader = new TimeSyncFileReader;
        ret = tsreader->open(tsFilename + QStringLiteral(".tsync"));
        QVERIFY2(ret, qPrintable(tsreader->lastError()));

        QCOMPARE(tsreader->checkInterval().count(), 4 * 1000 * 1000);
        QCOMPARE(tsreader->tolerance().count(), 1500);
        QCOMPARE(tsreader->moduleName(), QStringLiteral("UnittestDummyModule"));

        QCOMPARE(tsreader->offsets().count(), 100);
        for (int i = 0; i < tsreader->offsets().count(); ++i) {
            const auto pair = tsreader->offsets()[i];
            QCOMPARE(pair.first, microseconds_t(i * 1000));
            QCOMPARE(pair.second, microseconds_t(i * 50));
        }
        delete tsreader;

        // delete temporary file
        QFile file (tsFilename);
        file.remove();
    }
};

QTEST_MAIN(TestTimer)
#include "test-timer.moc"

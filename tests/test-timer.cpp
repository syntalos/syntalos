
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

        for (int i = 0; i < 100; ++i) {
            const auto tbase = microseconds_t(i * 1000);
            tswriter->writeTimes(tbase, tbase + microseconds_t(i * 50));
        }
        delete tswriter;

        // read the timesync file
        auto tsreader = new TimeSyncFileReader;
        ret = tsreader->open(tsFilename + QStringLiteral(".tsync"));
        QVERIFY2(ret, qPrintable(tsreader->lastError()));

        QCOMPARE(tsreader->checkInterval().count(), 4 * 1000 * 1000);
        QCOMPARE(tsreader->tolerance().count(), 1500);
        QCOMPARE(tsreader->moduleName(), QStringLiteral("UnittestDummyModule"));

        QCOMPARE(tsreader->times().count(), 100);
        for (int i = 0; i < tsreader->times().count(); ++i) {
            const auto pair = tsreader->times()[i];
            const auto tbase = i * 1000;
            QCOMPARE(pair.first, tbase);
            QCOMPARE(pair.second, tbase + i * 50);
        }
        delete tsreader;

        // delete temporary file
        QFile file (tsFilename);
        file.remove();
    }

    void runClockSynchronizer()
    {
        std::shared_ptr<SyncTimer> timer(new SyncTimer());
        std::unique_ptr<SecondaryClockSynchronizer> sync(new SecondaryClockSynchronizer(timer, nullptr));

        const auto toleranceValue = microseconds_t(2000);
        const auto calibrationCount = 20;
        const auto secondClockDefaultOffset = milliseconds_t(-10);
        sync->setStrategies(TimeSyncStrategy::ADJUST_CLOCK |
                            TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD |
                            TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD);
        sync->setCalibrationPointsCount(calibrationCount);
        sync->setTolerance(toleranceValue);

        timer->start();
        sync->start();

        // set the initial, regular timestamps. Our fake external clock has a
        // default offset of -10ms +/- 1ms
        qDebug() << "Calibrating synchronizer";
        for (auto i = 0; i < calibrationCount; ++i) {
            const auto origMasterTS = timer->timeSinceStartMsec();
            auto masterTS = origMasterTS;
            auto secondaryTS = masterTS + secondClockDefaultOffset + ((i % 2)? milliseconds_t(1) : milliseconds_t(0));
            sync->processTimestamp(masterTS, secondaryTS);

            // sanity checks
            QCOMPARE(sync->clockCorrectionOffset().count(), 0);
            QCOMPARE(masterTS.count(), origMasterTS.count());

            // delay
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // run for a short time with zero divergence
        qDebug() << "Testing precise secondary clock";
        for (auto i = 0; i < (calibrationCount * 2 + 5); ++i) {
            const auto origMasterTS = timer->timeSinceStartMsec();
            auto masterTS = origMasterTS;
            auto secondaryTS = masterTS + secondClockDefaultOffset;
            sync->processTimestamp(masterTS, secondaryTS);

            // sanity checks
            QCOMPARE(sync->clockCorrectionOffset().count(), 0);
            QCOMPARE(masterTS.count(), origMasterTS.count());

            // delay
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // run with clock divergence, the secondary clock is "faulty" and
        // runs *faster* than the master clock after a while
        qDebug() << "Testing faster secondary clock";
        auto lastMasterTS = timer->timeSinceStartMsec();
        int currentDivergenceMsec = 0;
        for (auto i = 0; i < 1000; ++i) {
            bool flukeDivergence = i % 50 == 0;
            const auto origMasterTS = timer->timeSinceStartMsec();
            auto masterTS = origMasterTS;
            auto secondaryTS = masterTS + secondClockDefaultOffset + milliseconds_t(currentDivergenceMsec) + milliseconds_t(flukeDivergence? 20 : 0);
            sync->processTimestamp(masterTS, secondaryTS);

            // sanity checks
            if (currentDivergenceMsec < (toleranceValue.count() / 1000)) {
                QCOMPARE(sync->clockCorrectionOffset().count(), 0);
                if (flukeDivergence)
                    QCOMPARE(masterTS.count(), (secondaryTS - secondClockDefaultOffset).count());
                else
                    QCOMPARE(masterTS.count(), (secondaryTS - secondClockDefaultOffset).count());
            } else {
                // clock correction must never "shoot over" the actual divergence
                QVERIFY(sync->clockCorrectionOffset().count() < (currentDivergenceMsec - 1));

                // clock correction offset must be positive
                QVERIFY(sync->clockCorrectionOffset().count() >= 0);

                // timestamps must never go backwards
                QVERIFY(masterTS.count() >= lastMasterTS.count());
            }

            // delay
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            lastMasterTS = masterTS;
            if (i % 10 == 0) {
                currentDivergenceMsec++;
                qDebug().noquote() << "DF Cycle:" << (i + 1) << "Secondary clock divergence is now" << currentDivergenceMsec << "msec";
            }
        }

        // run for a short time with zero divergence again, which should set
        // the clock correction offset back to zero
        qDebug() << "Testing good secondary clock (again)";
        auto lastClockCorrectionOffset = sync->clockCorrectionOffset().count();
        for (auto i = 0; i < (calibrationCount * 2 + 5); ++i) {
            const auto origMasterTS = timer->timeSinceStartMsec();
            auto masterTS = origMasterTS;
            auto secondaryTS = masterTS + secondClockDefaultOffset;
            sync->processTimestamp(masterTS, secondaryTS);

            // sanity checks
            if (i > calibrationCount) {
                // by this point, timestamp adjustments shouldn't happen anymore
                QCOMPARE(masterTS.count(), origMasterTS.count());

                // clock correction offset should be at zero, since everything is in sync again
                QCOMPARE(sync->clockCorrectionOffset().count(), 0);
            } else {
                // we are still resetting back to normal
                QVERIFY(sync->clockCorrectionOffset().count() <= lastClockCorrectionOffset);
                QVERIFY(masterTS.count() >= origMasterTS.count());
            }

            lastClockCorrectionOffset = sync->clockCorrectionOffset().count();

            // delay
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // run with clock divergence, the secondary clock is "faulty" and
        // runs *slower* than the master clock after a while
        qDebug() << "Testing slower secondary clock";
        lastMasterTS = timer->timeSinceStartMsec();
        currentDivergenceMsec = 0;
        for (auto i = 0; i < 1000; ++i) {
            const auto origMasterTS = timer->timeSinceStartMsec();
            auto masterTS = origMasterTS;
            auto secondaryTS = masterTS + secondClockDefaultOffset + milliseconds_t(currentDivergenceMsec);
            sync->processTimestamp(masterTS, secondaryTS);

            // sanity checks
            if (abs(currentDivergenceMsec) < (toleranceValue.count() / 1000)) {
                QCOMPARE(sync->clockCorrectionOffset().count(), 0);
                QCOMPARE(masterTS.count(), origMasterTS.count());
            } else {
                // clock correction must never "shoot over" the actual divergence
                QVERIFY(abs(sync->clockCorrectionOffset().count()) < abs(currentDivergenceMsec - 1));

                // clock correction offset must be negative
                QVERIFY(sync->clockCorrectionOffset().count() <= 0);

                // timestamps must never go backwards
                QVERIFY(masterTS.count() >= lastMasterTS.count());
            }

            // delay
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            lastMasterTS = masterTS;
            if (i % 10 == 0) {
                currentDivergenceMsec--;
                qDebug().noquote() << "DS Cycle:" << (i + 1) << "Secondary clock divergence is now" << currentDivergenceMsec << "msec";
            }
        }

    }
};

QTEST_MAIN(TestTimer)
#include "test-timer.moc"

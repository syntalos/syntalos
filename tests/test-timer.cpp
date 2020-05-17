
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
        QVERIFY((res.count() < (251 * 1000)) && (res.count() > (249 * 1000)));

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

    /**
     * Calculates the expected synchronizer result timestamp in case everything is nominal.
     */
    static long calcExpectedSyncTS(SecondaryClockSynchronizer *sync, const microseconds_t &secondaryAcqTS, const microseconds_t &masterTimestamp)
    {
        return qRound(((secondaryAcqTS.count() - sync->expectedOffsetToMaster().count()) + masterTimestamp.count()) / 2.0);
    }

    void runExClockSynchronizer()
    {
        std::shared_ptr<SyncTimer> syTimer(new SyncTimer());
        std::unique_ptr<SecondaryClockSynchronizer> sync(new SecondaryClockSynchronizer(syTimer, nullptr));

        const auto toleranceValue = microseconds_t(1000);
        const auto calibrationCount = 20;
        sync->setStrategies(TimeSyncStrategy::ADJUST_CLOCK |
                            TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD |
                            TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD);
        sync->setCalibrationPointsCount(calibrationCount);
        sync->setTolerance(toleranceValue);

        syTimer->start();
        sync->start();

        // secondary clock can start at any random value, so
        // we define an offset here for testing
        const auto secondaryClockOffset = microseconds_t(11111);
        auto curSecondaryTS = secondaryClockOffset;

        // master clock starts at 0, but we pretend it was already running for half a second
        auto curMasterTS = microseconds_t(500);

        // set the initial, regular timestamps.
        // Fake external clock has a default offset of -10ms +/- 1ms
        qDebug() << "\n## Calibrating synchronizer";
        for (auto i = 0; !sync->isCalibrated(); ++i) {
            auto syncMasterTS = curMasterTS;
            sync->processTimestamp(syncMasterTS, curSecondaryTS);

            // we must not set a correction offset
            QCOMPARE(sync->clockCorrectionOffset().count(), 0);

            // we must not alter the master timestamp (yet)
            QCOMPARE(syncMasterTS.count(), curMasterTS.count());

            // advance
            curMasterTS = curMasterTS + milliseconds_t(1) + ((i % 2)? microseconds_t(500) : microseconds_t(-500));
            curSecondaryTS = curSecondaryTS + milliseconds_t(1);

            // set some limit on the amount of datapoints used for calibration, so we don't run
            // forever if code is buggy or acting unreasonable
            QVERIFY(i < (calibrationCount * 4));
        }

        // run for a short time with zero divergence
        qDebug() << "\n## Testing precise secondary clock";
        for (auto i = 0; i < (calibrationCount * 2 + 5); ++i) {
            // advance
            curMasterTS = curMasterTS + milliseconds_t(1);
            curSecondaryTS = curSecondaryTS + milliseconds_t(1);

            // adjust
            auto syncMasterTS = curMasterTS;
            sync->processTimestamp(syncMasterTS, curSecondaryTS);

            // sanity checks, we must not make changes yet
            QCOMPARE(sync->clockCorrectionOffset().count(), 0);
            QCOMPARE(syncMasterTS.count(), calcExpectedSyncTS(sync.get(), curSecondaryTS, curMasterTS));
        }

        // run with clock divergence, the secondary clock is "faulty" and
        // runs *faster* than the master clock after a while
        qDebug() << "\n## Testing faster secondary clock";
        QCOMPARE(sync->clockCorrectionOffset().count(), 0);
        int currentDivergenceUsec = 0;
        int adjustmentIterN = 0;
        auto lastMasterTS = curMasterTS;
        for (auto i = 1; i < 3200; ++i) {
            // advance
            curMasterTS = curMasterTS + milliseconds_t(1);
            curSecondaryTS = curSecondaryTS + milliseconds_t(1);

            auto syncMasterTS = curMasterTS;
            sync->processTimestamp(syncMasterTS, curSecondaryTS);

            // timestamps must never go backwards
            QVERIFY2(syncMasterTS.count() >= lastMasterTS.count(), qPrintable(QString::number(syncMasterTS.count())
                                                                             + " >= " + QString::number(lastMasterTS.count())));
            lastMasterTS = syncMasterTS;

            if (adjustmentIterN > 0) {
                adjustmentIterN--;
                continue;
            }

            if (currentDivergenceUsec < (toleranceValue.count() + 251)) {
                QCOMPARE(sync->clockCorrectionOffset().count(), 0);
                QCOMPARE(syncMasterTS.count(), calcExpectedSyncTS(sync.get(), curSecondaryTS, curMasterTS));
            } else {
                // clock correction must never "shoot over" the actual divergence
                QVERIFY2(sync->clockCorrectionOffset().count() < currentDivergenceUsec, qPrintable(QString::number(sync->clockCorrectionOffset().count())
                                                                                                   + " < " + QString::number(currentDivergenceUsec)));

                // clock correction offset must be positive and "reasonably" large
                QVERIFY(sync->clockCorrectionOffset().count() >= (currentDivergenceUsec / 21.0));

                // since the master clock is considered accurate, but the secondary clock is "too fast",
                // we expect timestamps to be shifted backwards a bit in order to match them up again
                QVERIFY(syncMasterTS.count() != curMasterTS.count());
                QVERIFY2(syncMasterTS.count() < curMasterTS.count(), qPrintable(QString::number(syncMasterTS.count())
                                                                                + " < " + QString::number(curMasterTS.count())));
            }

            // adjust divergence
            if (i % 30 == 0) {
                currentDivergenceUsec += 100;
                curSecondaryTS = curSecondaryTS + microseconds_t(100);
                qDebug().noquote() << "DF Cycle:" << (i + 1) << "Secondary clock divergence is now" << currentDivergenceUsec << "µs";

                // give the synchronizer some iterations to adjust
                adjustmentIterN = floor(calibrationCount / 2.0);
            }
        }

        // run for a short time with zero divergence again, which should set
        // the clock correction offset back to zero
        qDebug() << "\n## Testing good secondary clock (again)";
        curSecondaryTS = curSecondaryTS - microseconds_t(currentDivergenceUsec);
        auto lastClockCorrectionOffset = sync->clockCorrectionOffset().count();
        for (auto i = 0; i < (calibrationCount * 2 + 5); ++i) {
            // advance
            curMasterTS = curMasterTS + milliseconds_t(1);
            curSecondaryTS = curSecondaryTS + milliseconds_t(1);

            auto syncMasterTS = curMasterTS;
            sync->processTimestamp(syncMasterTS, curSecondaryTS);

            // timestamps must never go backwards
            QVERIFY2(syncMasterTS.count() >= lastMasterTS.count(), qPrintable(QString::number(syncMasterTS.count())
                                                                             + " >= " + QString::number(lastMasterTS.count())));
            lastMasterTS = syncMasterTS;

            // sanity checks
            if (i > calibrationCount) {
                // correction offset should be gone
                QCOMPARE(sync->clockCorrectionOffset().count(), 0);

                // master timestamp should pass through unaltered
                QCOMPARE(syncMasterTS.count(), calcExpectedSyncTS(sync.get(), curSecondaryTS, curMasterTS));
            } else {
                // we are still resetting back to normal, test if that's happening
                if (i > 1)
                    QVERIFY(sync->clockCorrectionOffset().count() <= lastClockCorrectionOffset);
                QVERIFY2(syncMasterTS.count() <= curMasterTS.count(), qPrintable(QString::number(syncMasterTS.count())
                                                                                + " < " + QString::number(curMasterTS.count())));
            }

            lastClockCorrectionOffset = sync->clockCorrectionOffset().count();
        }

        qDebug() << "\n## Testing fluke divergences";
        currentDivergenceUsec = 0;
        for (auto i = 1; i < 1200; ++i) {
            // advance
            curMasterTS = curMasterTS + milliseconds_t(1);
            curSecondaryTS = curSecondaryTS + milliseconds_t(1);

            auto syncMasterTS = curMasterTS;

            // the master time may fluctuate depending on system load - we are simulating that here
            bool expectFlukeDivergence = false;
            if (i % 10 == 0) {
                qDebug() << "Adding master fluke divergence of 500µs";
                syncMasterTS = syncMasterTS + microseconds_t(500);
                expectFlukeDivergence = true;
            }

            sync->processTimestamp(syncMasterTS, curSecondaryTS);

            // timestamps must never go backwards
            QVERIFY2(syncMasterTS.count() >= lastMasterTS.count(), qPrintable(QString::number(syncMasterTS.count())
                                                                             + " >= " + QString::number(lastMasterTS.count())));
            lastMasterTS = syncMasterTS;

            if (expectFlukeDivergence) {
                // we have a fluke divergence - the algorithm should have adjusted for that!
                const auto secondaryTSOffAdj = (curSecondaryTS - sync->expectedOffsetToMaster()).count();
                QVERIFY2(syncMasterTS.count() < (curMasterTS.count() + 250), qPrintable(QString::number(syncMasterTS.count())
                                                                              + " < " + QString::number(curMasterTS.count() + 250)));
                QVERIFY2(syncMasterTS.count() > secondaryTSOffAdj, qPrintable(QString::number(syncMasterTS.count())
                                                                              + " > " + QString::number(secondaryTSOffAdj)));

            } else {
                // we have no fluke divergence
                QCOMPARE(sync->clockCorrectionOffset().count(), 0);
                QCOMPARE(syncMasterTS.count(), calcExpectedSyncTS(sync.get(), curSecondaryTS, curMasterTS));
            }
        }


        // run with clock divergence, the secondary clock is "faulty" and
        // runs *slower* than the master clock after a while
        qDebug() << "\n## Testing slower secondary clock";
        QCOMPARE(sync->clockCorrectionOffset().count(), 0);
        currentDivergenceUsec = 0;
        adjustmentIterN = 0;
        lastMasterTS = curMasterTS;
        for (auto i = 1; i < 3200; ++i) {
            // advance
            curMasterTS = curMasterTS + milliseconds_t(1);
            curSecondaryTS = curSecondaryTS + milliseconds_t(1);

            auto syncMasterTS = curMasterTS;
            sync->processTimestamp(syncMasterTS, curSecondaryTS);

            // timestamps must never go backwards
            QVERIFY2(syncMasterTS.count() >= lastMasterTS.count(), qPrintable(QString::number(syncMasterTS.count())
                                                                             + " >= " + QString::number(lastMasterTS.count())));
            lastMasterTS = syncMasterTS;

            if (adjustmentIterN > 0) {
                adjustmentIterN--;
                continue;
            }

            if (abs(currentDivergenceUsec) < (toleranceValue.count() - 251)) {
                QCOMPARE(sync->clockCorrectionOffset().count(), 0);
                QCOMPARE(syncMasterTS.count(), calcExpectedSyncTS(sync.get(), curSecondaryTS, curMasterTS));
            } else {
                // clock correction must never "shoot way under" the actual divergence
                QVERIFY2(sync->clockCorrectionOffset().count() > currentDivergenceUsec - 250, qPrintable(QString::number(sync->clockCorrectionOffset().count())
                                                                                                   + " > " + QString::number(currentDivergenceUsec - 250)));

                // clock correction offset must be positive and "reasonably" small
                QVERIFY(sync->clockCorrectionOffset().count() <= (currentDivergenceUsec / 21.0));

                // since the master clock is considered accurate, but the secondary clock is "too slow",
                // we expect timestamps to be shifted forward a bit in order to match them up again
                QVERIFY(syncMasterTS.count() != curMasterTS.count());
                QVERIFY2(syncMasterTS.count() > curMasterTS.count(), qPrintable(QString::number(syncMasterTS.count())
                                                                                + " < " + QString::number(curMasterTS.count())));
            }

            // adjust divergence
            if (i % 30 == 0) {
                currentDivergenceUsec -= 100;
                curSecondaryTS = curSecondaryTS - microseconds_t(100);
                qDebug().noquote() << "DF Cycle:" << (i + 1) << "Secondary clock divergence is now" << currentDivergenceUsec << "µs";

                // give the synchronizer some iterations to adjust
                adjustmentIterN = floor(calibrationCount / 2.0);
            }
        }

    }
};

QTEST_MAIN(TestTimer)
#include "test-timer.moc"

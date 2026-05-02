
#include <QDebug>
#include <QtTest>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>

#include "datactl/syclock.h"
#include "datactl/timesync.h"
#include "utils/misc.h"

using namespace Syntalos;
using namespace Eigen;

static uint vecLast(const VectorXul &vec)
{
    return vec[vec.rows() - 1];
}

/**
 * @brief Helper class to generate time-index blocks
 */
class FakeIndexDevice
{
private:
    VectorXul m_lastIndexBlock;
    int m_freqHz;

public:
    explicit FakeIndexDevice()
    {
        // start at zero for our index counter, 10 elements per block
        const auto elementsPerBlock = 10;
        m_lastIndexBlock = VectorXul::Zero(elementsPerBlock);

        m_freqHz = 20000;
    }

    int freqHz() const
    {
        return m_freqHz;
    };

    VectorXul generateBlock()
    {
        if (vecLast(m_lastIndexBlock) == 0) {
            m_lastIndexBlock += VectorXul::LinSpaced(m_lastIndexBlock.rows(), 0, m_lastIndexBlock.rows());
        } else {
            const auto prevLastVal = m_lastIndexBlock[m_lastIndexBlock.rows() - 1];
            for (uint i = 0; i < m_lastIndexBlock.rows(); ++i)
                m_lastIndexBlock[i] = prevLastVal + i + 1;
        }
        return m_lastIndexBlock;
    }

    long lastIndex() const
    {
        return m_lastIndexBlock[m_lastIndexBlock.rows() - 1];
    }
    uint blockSize() const
    {
        return m_lastIndexBlock.rows();
    };
};

static int slow_work_with_result(int para)
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

        const auto before = timer->timeSinceStartNsec();
        auto res = TIMER_FUNC_TIMESTAMP(timer, slow_work_with_result(2));
        const auto after = timer->timeSinceStartNsec();
        std::this_thread::sleep_for(std::chrono::milliseconds(12));

        const auto expectedMidpoint = std::chrono::round<microseconds_t>((before + after) / 2.0);
        const auto deltaUsec = std::llabs((res - expectedMidpoint).count());
        QVERIFY2(
            deltaUsec <= 2200, qPrintable(QStringLiteral("timestamp midpoint delta too large: %1 us").arg(deltaUsec)));

        QVERIFY(timer->timeSinceStartMsec().count() >= 512);
    }

    void runExClockSynchronizer()
    {
        qDebug() << "\n#\n# External Clock Synchronizer\n#";
        auto syTimer = std::make_shared<SyncTimer>();
        std::unique_ptr<SecondaryClockSynchronizer> sync(new SecondaryClockSynchronizer(syTimer, {}));

        const auto toleranceValue = microseconds_t(1000);
        const auto calibrationCount = 48;
        sync->setStrategies(
            TimeSyncStrategy::ADJUST_CLOCK | TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD
            | TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD);
        sync->setCalibrationPointsCount(calibrationCount);
        sync->setTolerance(toleranceValue);

        syTimer->start();
        sync->start();

        // secondary clock can start at any random value, so
        // we define an offset here for testing
        const auto secondaryClockOffset = microseconds_t(11111);
        auto curSecondaryTS = secondaryClockOffset;

        // master clock starts at 0, but we pretend it was already running for half a second
        auto curMasterTS = microseconds_t(500 * 1000);

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
            curMasterTS = curMasterTS + milliseconds_t(1) + ((i % 2) ? microseconds_t(500) : microseconds_t(-500));
            curSecondaryTS = curSecondaryTS + milliseconds_t(1);

            // set some limit on the amount of datapoints used for calibration, so we don't run
            // forever if code is buggy or acting unreasonable
            QVERIFY(i < (calibrationCount * 4));
        }

        // run for a short time with zero divergence
        qDebug() << "\n## Testing precise secondary clock";
        for (auto i = 0; i < (calibrationCount * 2 + 5); ++i) {
            // advance
            curMasterTS += milliseconds_t(1);
            curSecondaryTS += milliseconds_t(1);

            // adjust
            auto syncMasterTS = curMasterTS;
            sync->processTimestamp(syncMasterTS, curSecondaryTS);

            // sanity checks, we must not make changes yet
            QCOMPARE(sync->clockCorrectionOffset().count(), 0);
            QCOMPARE(syncMasterTS.count(), curMasterTS.count());
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
            curMasterTS += milliseconds_t(1);
            curSecondaryTS += milliseconds_t(1);

            auto syncMasterTS = curMasterTS;
            sync->processTimestamp(syncMasterTS, curSecondaryTS);

            // timestamps must never go backwards
            QVERIFY2(
                syncMasterTS.count() >= lastMasterTS.count(),
                qPrintable(QString::number(syncMasterTS.count()) + " >= " + QString::number(lastMasterTS.count())));
            lastMasterTS = syncMasterTS;

            if (adjustmentIterN > 0) {
                adjustmentIterN--;
                continue;
            }

            if (currentDivergenceUsec < (toleranceValue.count() + 251)) {
                QCOMPARE(sync->clockCorrectionOffset().count(), 0);
                QCOMPARE(syncMasterTS.count(), lastMasterTS.count());
            } else {
                // clock correction must never "shoot over" the actual divergence
                QVERIFY2(
                    sync->clockCorrectionOffset().count() < currentDivergenceUsec,
                    qPrintable(
                        QString::number(sync->clockCorrectionOffset().count()) + " < "
                        + QString::number(currentDivergenceUsec)));

                // clock correction offset must be positive and "reasonably" large
                QVERIFY2(
                    sync->clockCorrectionOffset().count() >= (currentDivergenceUsec / 110.0),
                    qPrintable(QStringLiteral("%1 >= %2")
                                   .arg(sync->clockCorrectionOffset().count())
                                   .arg(currentDivergenceUsec / 110.0)));

                // since the master clock is considered accurate, but the secondary clock is "too fast",
                // we expect timestamps to be shifted forward a bit in order to match them up again
                QVERIFY(syncMasterTS.count() != curMasterTS.count());
                QVERIFY2(
                    syncMasterTS.count() > curMasterTS.count(),
                    qPrintable(QString::number(syncMasterTS.count()) + " > " + QString::number(curMasterTS.count())));
            }

            // adjust divergence
            if (i % 30 == 0) {
                currentDivergenceUsec += 100;
                curSecondaryTS = curSecondaryTS + microseconds_t(100);
                qDebug().noquote() << "DF Cycle:" << (i + 1) << "Secondary clock divergence is now"
                                   << currentDivergenceUsec << "µs";

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
            QVERIFY2(
                syncMasterTS.count() >= lastMasterTS.count(),
                qPrintable(QString::number(syncMasterTS.count()) + " >= " + QString::number(lastMasterTS.count())));
            lastMasterTS = syncMasterTS;

            // sanity checks
            if (i > calibrationCount) {
                // correction offset should be gone
                QCOMPARE(sync->clockCorrectionOffset().count(), 0);

                // master timestamp should pass through unaltered
                QCOMPARE(syncMasterTS.count(), curMasterTS.count());
            } else {
                // we are still resetting back to normal, test if that's happening
                if (i > 1)
                    QVERIFY(sync->clockCorrectionOffset().count() <= lastClockCorrectionOffset);

                if (i > (calibrationCount / 4))
                    QVERIFY2(
                        syncMasterTS.count() <= curMasterTS.count(),
                        qPrintable(
                            QString::number(syncMasterTS.count()) + " < " + QString::number(curMasterTS.count())));
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
            QVERIFY2(
                syncMasterTS.count() >= lastMasterTS.count(),
                qPrintable(QString::number(syncMasterTS.count()) + " >= " + QString::number(lastMasterTS.count())));
            lastMasterTS = syncMasterTS;

            if (expectFlukeDivergence) {
                // we have a fluke divergence - the algorithm should have adjusted for that!
                const auto secondaryTSOffAdj = (curSecondaryTS - sync->expectedOffsetToMaster()).count();
                QVERIFY2(
                    syncMasterTS.count() < (curMasterTS.count() + 250),
                    qPrintable(
                        QString::number(syncMasterTS.count()) + " < " + QString::number(curMasterTS.count() + 250)));
                QVERIFY2(
                    syncMasterTS.count() >= secondaryTSOffAdj,
                    qPrintable(QString::number(syncMasterTS.count()) + " >= " + QString::number(secondaryTSOffAdj)));

            } else {
                // we have no fluke divergence
                QCOMPARE(sync->clockCorrectionOffset().count(), 0);
                QCOMPARE(syncMasterTS.count(), curMasterTS.count());
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
            QVERIFY2(
                syncMasterTS.count() >= lastMasterTS.count(),
                qPrintable(QString::number(syncMasterTS.count()) + " >= " + QString::number(lastMasterTS.count())));
            lastMasterTS = syncMasterTS;

            if (adjustmentIterN > 0) {
                adjustmentIterN--;
                continue;
            }

            if (abs(currentDivergenceUsec) < (toleranceValue.count() - 251)) {
                QCOMPARE(sync->clockCorrectionOffset().count(), 0);
                QCOMPARE(syncMasterTS.count(), curMasterTS.count());
            } else {
                // clock correction must never "shoot way under" the actual divergence
                QVERIFY2(
                    sync->clockCorrectionOffset().count() > currentDivergenceUsec - 250,
                    qPrintable(
                        QString::number(sync->clockCorrectionOffset().count()) + " > "
                        + QString::number(currentDivergenceUsec - 250)));

                // clock correction offset must be positive and "reasonably" small
                QVERIFY2(
                    sync->clockCorrectionOffset().count() <= (currentDivergenceUsec / 68.0),
                    qPrintable(QStringLiteral("%1 <= %2")
                                   .arg(sync->clockCorrectionOffset().count())
                                   .arg(currentDivergenceUsec / 68.0)));

                // since the master clock is considered accurate, but the secondary clock is "too slow",
                // we expect timestamps to be shifted backward a bit in order to match them up again
                QVERIFY(syncMasterTS.count() != curMasterTS.count());
                QVERIFY2(
                    syncMasterTS.count() < curMasterTS.count(),
                    qPrintable(QString::number(syncMasterTS.count()) + " < " + QString::number(curMasterTS.count())));
            }

            // adjust divergence
            if (i % 30 == 0) {
                currentDivergenceUsec -= 100;
                curSecondaryTS = curSecondaryTS - microseconds_t(100);
                qDebug().noquote() << "DF Cycle:" << (i + 1) << "Secondary clock divergence is now"
                                   << currentDivergenceUsec << "µs";

                // give the synchronizer some iterations to adjust
                adjustmentIterN = floor(calibrationCount / 1.20);
            }
        }
    }

    void runExClockSynchronizerMovingAverage()
    {
        qDebug() << "\n#\n# External Clock Synchronizer Moving Average\n#";
        auto syTimer = std::make_shared<SyncTimer>();
        std::unique_ptr<SecondaryClockSynchronizer> sync(new SecondaryClockSynchronizer(syTimer, {}));

        const auto toleranceValue = microseconds_t(4500);
        const auto calibrationCount = 46;
        const auto sampleInterval = milliseconds_t(20);
        const auto steadyOffset = microseconds_t(-500000);
        const auto calibrationJitter = microseconds_t(5000);
        const auto divergenceStep = microseconds_t(4600);

        sync->setStrategies(
            TimeSyncStrategy::ADJUST_CLOCK | TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD
            | TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD);
        sync->setCalibrationPointsCount(calibrationCount);
        sync->setTolerance(toleranceValue);

        syTimer->start();
        QVERIFY(sync->start());

        auto curMasterTS = microseconds_t(500 * 1000);
        int calibrationSamples = 0;
        for (; !sync->isCalibrated(); ++calibrationSamples) {
            const auto curSecondaryTS = curMasterTS + steadyOffset
                                        + ((calibrationSamples % 2 == 0) ? -calibrationJitter : calibrationJitter);

            auto syncMasterTS = curMasterTS;
            sync->processTimestamp(syncMasterTS, curSecondaryTS);

            QCOMPARE(sync->clockCorrectionOffset().count(), 0);
            QCOMPARE(syncMasterTS.count(), curMasterTS.count());

            curMasterTS += sampleInterval;
            QVERIFY(calibrationSamples < (calibrationCount * 4));
        }

        QCOMPARE(sync->expectedOffsetToMaster().count(), steadyOffset.count());

        // Flush the calibration samples out of the moving window while keeping the clock steady.
        for (auto i = 0; i < calibrationCount; ++i) {
            const auto curSecondaryTS = curMasterTS + steadyOffset;

            auto syncMasterTS = curMasterTS;
            sync->processTimestamp(syncMasterTS, curSecondaryTS);

            QCOMPARE(sync->clockCorrectionOffset().count(), 0);
            QCOMPARE(syncMasterTS.count(), curMasterTS.count());

            curMasterTS += sampleInterval;
        }

        // With a 46-point window and 4600us sustained divergence, the historic mean reaches the
        // 4500us tolerance only on the 46th shifted sample. If the implementation incorrectly
        // includes the current sample in that mean, correction starts one sample too early.
        for (auto i = 0; i < (calibrationCount - 1); ++i) {
            const auto curSecondaryTS = curMasterTS + steadyOffset + divergenceStep;

            auto syncMasterTS = curMasterTS;
            sync->processTimestamp(syncMasterTS, curSecondaryTS);

            QVERIFY2(
                sync->clockCorrectionOffset().count() == 0,
                qPrintable(QStringLiteral("shifted sample %1 of %2 changed correction offset to %3")
                               .arg(i + 1)
                               .arg(calibrationCount - 1)
                               .arg(sync->clockCorrectionOffset().count())));
            QVERIFY2(
                syncMasterTS.count() == curMasterTS.count(),
                qPrintable(QStringLiteral("shifted sample %1 of %2 changed master timestamp from %3 to %4")
                               .arg(i + 1)
                               .arg(calibrationCount - 1)
                               .arg(curMasterTS.count())
                               .arg(syncMasterTS.count())));

            curMasterTS += sampleInterval;
        }

        const auto curSecondaryTS = curMasterTS + steadyOffset + divergenceStep;

        auto syncMasterTS = curMasterTS;
        sync->processTimestamp(syncMasterTS, curSecondaryTS);

        QVERIFY2(
            sync->clockCorrectionOffset().count() > 0,
            qPrintable(QString::number(sync->clockCorrectionOffset().count()) + " > 0"));
        QVERIFY2(
            syncMasterTS.count() > curMasterTS.count(),
            qPrintable(QString::number(syncMasterTS.count()) + " > " + QString::number(curMasterTS.count())));
    }

    void runFreqCounterSynchronizer()
    {
        qDebug() << "\n#\n# External FreqCounter Synchronizer\n#";
        std::shared_ptr<SyncTimer> syTimer(new SyncTimer());

        // create our fake device to generate time indices
        std::unique_ptr<FakeIndexDevice> idxDev(new FakeIndexDevice());

        // new synchronizer for 20kHz clock source
        std::unique_ptr<FreqCounterSynchronizer> sync(new FreqCounterSynchronizer(syTimer, {}, idxDev->freqHz()));

        const auto toleranceValue = microseconds_t(1000);
        const int calibrationCount = (idxDev->freqHz() / idxDev->blockSize()) / 2; // half a second of data
        sync->setStrategies(
            TimeSyncStrategy::ADJUST_CLOCK | TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD
            | TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD);
        sync->setCalibrationBlocksCount(calibrationCount);
        sync->setTolerance(toleranceValue);

        syTimer->start();
        sync->start();
        QVERIFY(calibrationCount > 200);

        // master clock starts at 0, but we pretend it was already running for half a second
        auto curMasterTS = microseconds_t(500 * 1000);

        // set the initial, regular timestamps.
        // Fake external clock has a default offset of -10ms +/- 1ms
        qDebug() << "\n## Calibrating index synchronizer";
        for (auto i = 0; !sync->isCalibrated(); ++i) {
            // advance master clock
            curMasterTS = curMasterTS + milliseconds_t(1) + ((i % 2) ? microseconds_t(500) : microseconds_t(-500));
            auto syncMasterTS = curMasterTS;

            auto currentBlock = idxDev->generateBlock();
            sync->processTimestamps(syncMasterTS, 0, 2, currentBlock);
            QCOMPARE(sync->indexOffset(), 0);
            QCOMPARE(vecLast(currentBlock), idxDev->lastIndex());

            currentBlock = idxDev->generateBlock();
            sync->processTimestamps(syncMasterTS, 1, 2, currentBlock);
            QCOMPARE(sync->indexOffset(), 0);
            QCOMPARE(vecLast(currentBlock), idxDev->lastIndex());

            // set some limit on the amount of datapoints used for calibration, so we don't run
            // forever if code is buggy or acting unreasonable
            QVERIFY(i < (calibrationCount * 4));
        }

        // run for a short time with zero divergence
        qDebug() << "\n## Testing precise secondary indices";
        for (auto i = 0; i < (calibrationCount * 2 + calibrationCount * 2); ++i) {
            curMasterTS = curMasterTS + milliseconds_t(1);
            auto syncMasterTS = curMasterTS;

            auto currentBlock = idxDev->generateBlock();
            sync->processTimestamps(syncMasterTS, 0, 2, currentBlock);
            QCOMPARE(sync->indexOffset(), 0);
            QCOMPARE(vecLast(currentBlock), idxDev->lastIndex());

            currentBlock = idxDev->generateBlock();
            sync->processTimestamps(syncMasterTS, 1, 2, currentBlock);
            QCOMPARE(sync->indexOffset(), 0);
            QCOMPARE(vecLast(currentBlock), idxDev->lastIndex());
        }

        // run with clock divergence, the secondary timing device is "faulty" and
        // runs *faster* than the master clock after a while
        qDebug() << "\n## Testing faster secondary index generator";
        QCOMPARE(sync->indexOffset(), 0);
        int currentDivergenceUsec = 0;
        int currentDivergenceIdx = 0;
        int adjustmentIterN = 0;
        for (auto i = 1; i < (calibrationCount * 10 + calibrationCount * 2); ++i) {
            // advance
            curMasterTS = curMasterTS + milliseconds_t(1);

            auto syncMasterTS = curMasterTS;
            auto currentBlock = idxDev->generateBlock();
            sync->processTimestamps(syncMasterTS, 0, 2, currentBlock);
            if (currentDivergenceUsec < toleranceValue.count()) {
                QVERIFY2(
                    vecLast(currentBlock) >= idxDev->lastIndex(),
                    qPrintable(QString::number(vecLast(currentBlock)) + " >= " + QString::number(idxDev->lastIndex())));
            }

            currentBlock = idxDev->generateBlock();
            sync->processTimestamps(syncMasterTS, 1, 2, currentBlock);
            if (currentDivergenceUsec < toleranceValue.count()) {
                QVERIFY2(
                    vecLast(currentBlock) >= idxDev->lastIndex(),
                    qPrintable(QString::number(vecLast(currentBlock)) + " >= " + QString::number(idxDev->lastIndex())));
            }

            if (adjustmentIterN > 0) {
                adjustmentIterN--;
                continue;
            }

            if (currentDivergenceUsec < toleranceValue.count()) {
                QCOMPARE(sync->indexOffset(), 0);
                QCOMPARE(vecLast(currentBlock), idxDev->lastIndex());
            } else {
                // clock correction must never "shoot over" the actual divergence
                QVERIFY2(
                    sync->indexOffset() < currentDivergenceIdx,
                    qPrintable(QString::number(sync->indexOffset()) + " < " + QString::number(currentDivergenceIdx)));

                QVERIFY2(sync->indexOffset() > 0, qPrintable(QString::number(sync->indexOffset()) + " !> 0"));

                // clock correction offset must be positive and "reasonably" large
                QVERIFY2(
                    sync->indexOffset() >= (currentDivergenceIdx / 21.0),
                    qPrintable(
                        QString::number(sync->indexOffset())
                        + " >= " + QString::number((currentDivergenceIdx / 21.0))));

                // since the master clock is considered accurate, but the secondary clock is "too fast",
                // we expect timestamps to be shifted backwards a bit in order to match them up again
                QVERIFY(vecLast(currentBlock) != idxDev->lastIndex());
                QVERIFY2(
                    vecLast(currentBlock) < idxDev->lastIndex(),
                    qPrintable(QString::number(vecLast(currentBlock)) + " < " + QString::number(idxDev->lastIndex())));
            }

            // adjust divergence
            if (i % (calibrationCount * 2) == 0) {
                currentDivergenceUsec += 700;
                currentDivergenceIdx = floor((currentDivergenceUsec / 1000.0 / 1000.0) * idxDev->freqHz());
                curMasterTS = curMasterTS - microseconds_t(700);
                qDebug().noquote() << "DF Cycle:" << (i + 1)
                                   << "Master clock slowed to emulate secondary device speedup by"
                                   << currentDivergenceUsec << "µs "
                                   << "Index Diff: " << currentDivergenceIdx;

                // give the synchronizer some iterations to adjust
                adjustmentIterN = floor(calibrationCount / 2.0);
            }
        }

        qDebug() << "\n## Testing fluke divergences for index device with out-of-sync times";
        auto expectedIdxOffset = sync->indexOffset();
        for (auto i = 1; i < calibrationCount * 6; ++i) {
            curMasterTS = curMasterTS + milliseconds_t(1);
            auto syncMasterTS = curMasterTS;

            if (i == calibrationCount * 2)
                expectedIdxOffset = sync->indexOffset();

            // the master time may fluctuate depending on system load - we are simulating that here
            // the two clocks have to be already divergent (from the previous test), because otherwise
            // any fluke test can easily be skipped.
            if (i % 20 == 0) {
                const auto randomDivergence = microseconds_t(50) + microseconds_t(rand() % 400);
                qDebug().nospace().noquote()
                    << "Adding master fluke divergence of " << randomDivergence.count() << "µs";
                syncMasterTS = syncMasterTS + randomDivergence;
            }

            // check that no matter the timestamp turbulence we will not alter our
            // offset index
            auto currentBlock = idxDev->generateBlock();
            sync->processTimestamps(syncMasterTS, 0, 2, currentBlock);
            if (i > calibrationCount * 2) {
                QCOMPARE(sync->indexOffset(), expectedIdxOffset);
                QCOMPARE(vecLast(currentBlock), idxDev->lastIndex() - expectedIdxOffset);
            }

            currentBlock = idxDev->generateBlock();
            sync->processTimestamps(syncMasterTS, 1, 2, currentBlock);
            if (i > calibrationCount * 2) {
                QCOMPARE(sync->indexOffset(), expectedIdxOffset);
                QCOMPARE(vecLast(currentBlock), idxDev->lastIndex() - expectedIdxOffset);
            }
        }

        // reset master clock to regular, espected value
        curMasterTS = curMasterTS + microseconds_t(currentDivergenceUsec);

        // run for a short time with zero divergence again, which should set
        // the clock correction offset back to zero
        qDebug() << "\n## Testing good secondary indices (again)";
        auto lastIndexOffset = sync->indexOffset();
        for (auto i = 0; i < (calibrationCount * 8 + calibrationCount * 2); ++i) {
            curMasterTS = curMasterTS + milliseconds_t(1);
            auto syncMasterTS = curMasterTS;

            auto currentBlock = idxDev->generateBlock();
            sync->processTimestamps(syncMasterTS, 0, 2, currentBlock);
            if (i > calibrationCount * 2) {
                QCOMPARE(sync->indexOffset(), 0);
                QCOMPARE(vecLast(currentBlock), idxDev->lastIndex());
            }

            currentBlock = idxDev->generateBlock();
            sync->processTimestamps(syncMasterTS, 1, 2, currentBlock);
            if (i > calibrationCount * 2) {
                QCOMPARE(sync->indexOffset(), 0);
                QCOMPARE(vecLast(currentBlock), idxDev->lastIndex());
            } else {
                QVERIFY(sync->indexOffset() <= lastIndexOffset);
            }
        }

        // run with clock divergence, the secondary timing device is "faulty" and
        // runs *slower* than the master clock after a while
        qDebug() << "\n## Testing slower secondary index generator";
        QCOMPARE(sync->indexOffset(), 0);
        currentDivergenceUsec = 0;
        currentDivergenceIdx = 0;
        adjustmentIterN = 0;
        for (auto i = 1; i < (calibrationCount * 10 + calibrationCount / 2); ++i) {
            // advance
            curMasterTS = curMasterTS + milliseconds_t(1);

            auto syncMasterTS = curMasterTS;
            auto currentBlock = idxDev->generateBlock();
            sync->processTimestamps(syncMasterTS, 0, 2, currentBlock);
            if (currentDivergenceUsec < toleranceValue.count()) {
                QVERIFY2(
                    vecLast(currentBlock) >= idxDev->lastIndex(),
                    qPrintable(QString::number(vecLast(currentBlock)) + " >= " + QString::number(idxDev->lastIndex())));
            }

            currentBlock = idxDev->generateBlock();
            sync->processTimestamps(syncMasterTS, 1, 2, currentBlock);
            if (currentDivergenceUsec < toleranceValue.count()) {
                QVERIFY2(
                    vecLast(currentBlock) >= idxDev->lastIndex(),
                    qPrintable(QString::number(vecLast(currentBlock)) + " >= " + QString::number(idxDev->lastIndex())));
            }

            if (adjustmentIterN > 0) {
                adjustmentIterN--;
                continue;
            }

            if (currentDivergenceUsec < toleranceValue.count()) {
                QCOMPARE(sync->indexOffset(), 0);
                QCOMPARE(vecLast(currentBlock), idxDev->lastIndex());
            } else {
                // clock correction must never "underflow" the actual divergence
                QVERIFY2(
                    sync->indexOffset() > currentDivergenceIdx,
                    qPrintable(QString::number(sync->indexOffset()) + " > " + QString::number(currentDivergenceIdx)));

                QVERIFY2(sync->indexOffset() < 0, qPrintable(QString::number(sync->indexOffset()) + " !< 0"));

                // clock correction offset must be negative and "reasonably" large
                QVERIFY2(
                    sync->indexOffset() <= (currentDivergenceIdx / 21.0),
                    qPrintable(
                        QString::number(sync->indexOffset())
                        + " <= " + QString::number((currentDivergenceIdx / 21.0))));

                // since the master clock is considered accurate, but the secondary clock is "too slow",
                // we expect indices to be shifted forward a bit in order to match them up again
                QVERIFY(vecLast(currentBlock) != idxDev->lastIndex());
                QVERIFY2(
                    vecLast(currentBlock) > idxDev->lastIndex(),
                    qPrintable(QString::number(vecLast(currentBlock)) + " > " + QString::number(idxDev->lastIndex())));
            }

            // adjust divergence
            if (i % (calibrationCount * 2) == 0) {
                currentDivergenceUsec += 700;
                currentDivergenceIdx = -1 * floor((currentDivergenceUsec / 1000.0 / 1000.0) * idxDev->freqHz());
                curMasterTS = curMasterTS + microseconds_t(700);
                qDebug().noquote() << "DF Cycle:" << (i + 1)
                                   << "Master clock sped up to emulate secondary device slowdown by"
                                   << currentDivergenceUsec << "µs "
                                   << "Index Diff: " << currentDivergenceIdx;

                // give the synchronizer some iterations to adjust
                adjustmentIterN = floor(calibrationCount / 2.0);
            }
        }
    }
};

QTEST_MAIN(TestTimer)
#include "test-timer.moc"

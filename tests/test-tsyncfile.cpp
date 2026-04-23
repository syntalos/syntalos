
#include <QDebug>
#include <QtTest>
#include <iostream>

#include "datactl/syclock.h"
#include "datactl/timesync.h"
#include "utils/misc.h"

using namespace Syntalos;

class TestTSyncFile : public QObject
{
    Q_OBJECT
private slots:

    void tsyncFileRWForDTypes(TSyncFileDataType dt1, TSyncFileDataType dt2, int values_n = 142000)
    {
        auto tsFilename = QStringLiteral("/tmp/tstest-%1").arg(createRandomString(8)).toStdString();

        // write a timesync file
        auto tswriter = std::make_unique<TimeSyncFileWriter>();
        tswriter->setFileName(tsFilename);
        tswriter->setTimeDataTypes(dt1, dt2);
        auto ret = tswriter->open(
            "UnittestDummyModule", Uuid::fromHex("a12975f1-84b7-4350-8683-7a5fe9ed968f").value(), microseconds_t(1500));
        QVERIFY2(ret, tswriter->lastError().c_str());

        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < values_n; ++i) {
            const auto tbase = microseconds_t(i * 1000);
            tswriter->writeTimes(tbase, tbase + microseconds_t(i * 51));
        }
        tswriter->close();
        qDebug().noquote() << "TSync write operation took" << timer.elapsed() << "milliseconds";

        // read the timesync file
        auto tsreader = std::make_unique<TimeSyncFileReader>();
        timer.start();
        ret = tsreader->open(tsFilename + ".tsync");
        QVERIFY2(ret, tsreader->lastError().c_str());
        qDebug().noquote() << "TSync read operation took" << timer.elapsed() << "milliseconds";

        QCOMPARE(tsreader->moduleName(), QStringLiteral("UnittestDummyModule"));
        QCOMPARE(tsreader->collectionId(), Uuid::fromHex("a12975f1-84b7-4350-8683-7a5fe9ed968f").value());
        QCOMPARE(tsreader->tolerance().count(), 1500);
        QCOMPARE(tsreader->timeDTypes(), qMakePair(dt1, dt2));
        QCOMPARE(tsreader->syncMode(), TSyncFileMode::CONTINUOUS);

        const auto timesRead = tsreader->times();
        QCOMPARE((int)timesRead.size(), values_n);
        for (size_t i = 0; i < timesRead.size(); ++i) {
            const auto pair = timesRead[i];
            const auto tbase = (long)i * 1000;
            QCOMPARE(pair.first, tbase);
            QCOMPARE(pair.second, tbase + (long)i * 51);
        }

        // delete temporary file
        QFile file(QString::fromUtf8(tsFilename + ".tsync"));
        file.remove();
    }

    void runTestTSyncInt32_Int32()
    {
        tsyncFileRWForDTypes(TSyncFileDataType::INT32, TSyncFileDataType::INT32);
    }

    void runTestTSyncInt32_UInt32()
    {
        tsyncFileRWForDTypes(TSyncFileDataType::INT32, TSyncFileDataType::UINT32);
    }

    void runTestTSyncUInt64_UInt64()
    {
        tsyncFileRWForDTypes(TSyncFileDataType::UINT64, TSyncFileDataType::UINT64);
    }

    void runTestTSyncUInt32_UInt64()
    {
        tsyncFileRWForDTypes(TSyncFileDataType::UINT32, TSyncFileDataType::UINT64);
    }

    void runBenchmark()
    {
        QBENCHMARK {
            tsyncFileRWForDTypes(TSyncFileDataType::UINT32, TSyncFileDataType::UINT64, 512000);
        }
    }
};

QTEST_MAIN(TestTSyncFile)
#include "test-tsyncfile.moc"

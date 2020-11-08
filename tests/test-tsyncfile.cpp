
#include <iostream>
#include <QtTest>
#include <QDebug>

#include "syclock.h"
#include "timesync.h"
#include "utils.h"

using namespace Syntalos;

class TestTSyncFile : public QObject
{
    Q_OBJECT
private slots:

    void tsyncFileRWForDTypes(TSyncFileDataType dt1, TSyncFileDataType dt2)
    {
        auto tsFilename = QStringLiteral("/tmp/tstest-%1").arg(createRandomString(8));

        // write a timesync file
        auto tswriter = new TimeSyncFileWriter;
        tswriter->setFileName(tsFilename);
        tswriter->setTimeDataTypes(dt1, dt2);
        auto ret = tswriter->open(QStringLiteral("UnittestDummyModule"), QUuid("a12975f1-84b7-4350-8683-7a5fe9ed968f"), microseconds_t(1500));
        QVERIFY2(ret, qPrintable(tswriter->lastError()));

        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < 142000; ++i) {
            const auto tbase = microseconds_t(i * 1000);
            tswriter->writeTimes(tbase, tbase + microseconds_t(i * 51));
        }
        delete tswriter;
        qDebug().noquote() << "TSync write operation took" << timer.elapsed() << "milliseconds";

        // read the timesync file
        auto tsreader = new TimeSyncFileReader;
        timer.start();
        ret = tsreader->open(tsFilename + QStringLiteral(".tsync"));
        QVERIFY2(ret, qPrintable(tsreader->lastError()));
        qDebug().noquote() << "TSync read operation took" << timer.elapsed() << "milliseconds";

        QCOMPARE(tsreader->moduleName(), QStringLiteral("UnittestDummyModule"));
        QCOMPARE(tsreader->collectionId(), QUuid("a12975f1-84b7-4350-8683-7a5fe9ed968f"));
        QCOMPARE(tsreader->tolerance().count(), 1500);
        QCOMPARE(tsreader->timeDTypes(), qMakePair(dt1, dt2));
        QCOMPARE(tsreader->syncMode(), TSyncFileMode::CONTINUOUS);

        const auto timesRead = tsreader->times();
        QCOMPARE(timesRead.size(), 142000);
        for (size_t i = 0; i < timesRead.size(); ++i) {
            const auto pair = timesRead[i];
            const auto tbase = i * 1000;
            QCOMPARE(pair.first, tbase);
            QCOMPARE(pair.second, tbase + i * 51);
        }
        delete tsreader;

        // delete temporary file
        QFile file (tsFilename);
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

};

QTEST_MAIN(TestTSyncFile)
#include "test-tsyncfile.moc"

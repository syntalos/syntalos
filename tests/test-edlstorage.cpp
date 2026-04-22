
#include <QDebug>
#include <QRegularExpression>
#include <QtTest>
#include <chrono>
#include <cstdint>
#include <iostream>

#include "datactl/edlstorage.h"
#include "utils/tomlutils.h"

using namespace Syntalos;

class TestEDL : public QObject
{
    Q_OBJECT
private:
    const QString expectedToml = QStringLiteral(
        "boolean = true\n"
        "date = 1977-04-23T13:37:12Z\n"
        "list = [ 'spam', 8, 'eggs', true, 12.4, 'spam', false ]\n"
        "string = 'Hello World - öäß-!?'\n"
        "\n"
        "[child]\n"
        "float = 1.248\n"
        "key = 'stringvalue'");

private slots:
    void runTomlSerialize()
    {
        auto dateTime = QDateTime(QDate(1977, 4, 23), QTime(13, 37, 12));
        dateTime.setTimeZone(QTimeZone::utc());

        QVariantHash table;
        table.insert("date", dateTime);
        table.insert("boolean", true);
        table.insert("void", QVariant());
        table.insert("string", "Hello World - öäß-!?");

        QVariantHash subTable;
        subTable.insert("key", "stringvalue");
        subTable.insert("float", 1.248);
        table.insert("child", subTable);

        QVariantList list;
        list.append("spam");
        list.append(8);
        list.append("eggs");
        list.append(true);
        list.append(12.4);
        list.append("spam");
        list.append(false);
        table.insert("list", list);

        auto toml = qVariantHashToTomlTable(table);
        QCOMPARE(serializeTomlTable(toml), expectedToml);
    }

    void runTomlDeserialize()
    {
        auto dateTime = QDateTime(QDate(1977, 4, 23), QTime(13, 37, 12));
        dateTime.setTimeZone(QTimeZone::utc());

        QString errorMessage;
        const auto tab = parseTomlData(expectedToml, errorMessage);
        QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));

        QCOMPARE(tab["date"], dateTime);
        QCOMPARE(tab["boolean"], true);
        QCOMPARE(tab["void"], QVariant());
        QCOMPARE(tab["string"], "Hello World - öäß-!?");

        QCOMPARE(tab["child"].toHash()["key"], "stringvalue");
        QCOMPARE(tab["child"].toHash()["float"], 1.248);

        QVariantList list;
        list.append("spam");
        list.append(8);
        list.append("eggs");
        list.append(true);
        list.append(12.4);
        list.append("spam");
        list.append(false);
        QCOMPARE(tab["list"], list);
    }

    void runEDLWrite()
    {
        std::unique_ptr<EDLCollection> collection(new EDLCollection("test-experiment"));
        collection->addAuthor(EDLAuthor("Rick Sanchez", "rick@c137.local"));
        collection->addAuthor(EDLAuthor("Morty Smith", "morty@c137.local"));

        collection->setGeneratorId(QCoreApplication::applicationName());

        auto dset = collection->datasetByName("mydata", EDLCreateFlag::MUST_CREATE);
        dset->addDataFilePart("/usr/local/share/blah.test");

        QVariantHash attrs;
        attrs.insert(
            "alpha",
            QStringList() << "aaa"
                          << "bbbb"
                          << "cccc");
        QVariantHash subMap;
        subMap.insert("world", 123);
        subMap.insert("nnn", QVariantList() << "spam" << 1.23 << "eggs");
        QVariantHash subSubMap;
        subSubMap.insert("works", true);
        subMap.insert("values", subSubMap);
        attrs.insert("hello", subMap);
        dset->setAttributes(attrs);

        auto vidGroup = collection->groupByName("videos", EDLCreateFlag::CREATE_OR_OPEN);
        auto dsCam = vidGroup->datasetByName("Top Camera", EDLCreateFlag::MUST_CREATE);
        dsCam->addDataFilePart("camera-video.mkv");
        vidGroup->groupByName("cats", EDLCreateFlag::CREATE_OR_OPEN);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        collection->setRootPath(dir.path());
        qDebug() << collection->name();
        qDebug() << collection->path();
        qDebug() << collection->rootPath();

        QCOMPARE(collection->rootPath(), dir.path());
        QCOMPARE(collection->path(), QStringLiteral("%1/%2").arg(dir.path()).arg(collection->name()));

        QVERIFY2(collection->save(), qPrintable(collection->lastError()));
    }

    void runUtilsSortTest()
    {
        QStringList files;
        files << "test_1.mkv"
              << "test_2.mkv"
              << "test_9.mkv"
              << "test_10.mkv"
              << "test_11.mkv"
              << "test_8.mkv";
        stringListNaturalSort(files);
        QCOMPARE(files[0], "test_1.mkv");
        QCOMPARE(files[1], "test_2.mkv");
        QCOMPARE(files[2], "test_8.mkv");
        QCOMPARE(files[3], "test_9.mkv");
        QCOMPARE(files[4], "test_10.mkv");
        QCOMPARE(files[5], "test_11.mkv");
    }

    void runUuidTest()
    {
        const QRegularExpression uuidRe(
            QStringLiteral("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"));

        const auto beforeMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());
        const auto uuid = newUuid7();
        const auto afterMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());

        const auto uuidStr = QString::fromStdString(toHex(uuid));
        qDebug() << "Generated sample UUIDv7:" << uuidStr;
        QVERIFY2(uuidRe.match(uuidStr).hasMatch(), qPrintable(uuidStr));
        QCOMPARE(uuidStr.size(), 36);
        QCOMPARE(uuidStr[8], '-');
        QCOMPARE(uuidStr[13], '-');
        QCOMPARE(uuidStr[18], '-');
        QCOMPARE(uuidStr[23], '-');

        QCOMPARE((uuid[6] >> 4), 0x7);
        QCOMPARE((uuid[8] >> 6), 0x2);

        const auto tsMs = (static_cast<uint64_t>(uuid[0]) << 40) | (static_cast<uint64_t>(uuid[1]) << 32)
                          | (static_cast<uint64_t>(uuid[2]) << 24) | (static_cast<uint64_t>(uuid[3]) << 16)
                          | (static_cast<uint64_t>(uuid[4]) << 8) | static_cast<uint64_t>(uuid[5]);
        QVERIFY(tsMs >= beforeMs);
        QVERIFY(tsMs <= (afterMs + 1));
    }
};

QTEST_MAIN(TestEDL)
#include "test-edlstorage.moc"

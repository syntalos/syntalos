
#include <QDebug>
#include <QtTest>
#include <iostream>

#include "edlstorage.h"
#include "utils/misc.h"
#include "utils/tomlutils.h"

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

        auto dset = collection->datasetByName("mydata", true);
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

        auto vidGroup = collection->groupByName("videos", true);
        auto dsCam = vidGroup->datasetByName("Top Camera", true);
        dsCam->addDataFilePart("camera-video.mkv");
        vidGroup->groupByName("cats", true);

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
};

QTEST_MAIN(TestEDL)
#include "test-edlstorage.moc"

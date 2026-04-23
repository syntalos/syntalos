
#include <QDebug>
#include <QtTest>
#include <chrono>
#include <cstdint>
#include <iostream>

#include "datactl/edlstorage.h"
#include "datactl/edlutils.h"
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
        const QRegularExpression uuidRe(
            QStringLiteral("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"));

        std::unique_ptr<EDLCollection> collection(new EDLCollection("test-experiment"));
        collection->addAuthor(EDLAuthor("Rick Sanchez", "rick@c137.local"));
        collection->addAuthor(EDLAuthor("Morty Smith", "morty@c137.local"));

        collection->setGeneratorId(QCoreApplication::applicationName().toStdString());

        auto dset = collection->datasetByName("mydata", EDLCreateFlag::MUST_CREATE).value_or(nullptr);
        dset->addDataFilePart("/usr/local/share/blah.test");

        MetaStringMap attrs;
        attrs["alpha"] = MetaArray({"aaa", "bbbb", "cccc"});
        MetaStringMap subMap;
        subMap["world"] = 123;
        subMap["nnn"] = MetaArray({"spam", 1.23, "eggs"});
        MetaStringMap subSubMap;
        subSubMap["works"] = true;
        subMap["values"] = subSubMap;
        attrs["hello"] = subMap;
        dset->setAttributes(attrs);

        auto vidGroup = collection->groupByName("videos", EDLCreateFlag::CREATE_OR_OPEN).value_or(nullptr);
        auto dsCam = vidGroup->datasetByName("Top Camera", EDLCreateFlag::MUST_CREATE).value_or(nullptr);
        dsCam->addDataFilePart("camera-video.mkv");
        vidGroup->groupByName("cats", EDLCreateFlag::CREATE_OR_OPEN);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        collection->setRootPath(dir.path().toStdString());
        qDebug() << collection->name();
        qDebug() << collection->path();
        qDebug() << collection->rootPath();

        QCOMPARE(collection->rootPath(), dir.path().toStdString());
        QCOMPARE(collection->path(), std::format("{}/{}", dir.path().toStdString(), collection->name()));

        auto res = collection->save();
        if (!res.has_value())
            QFAIL(res.error().c_str());

        const auto collectionPath = dir.path() + QStringLiteral("/") + QString::fromStdString(collection->name());
        const auto myDataPath = collectionPath + QStringLiteral("/mydata");
        const auto videosPath = collectionPath + QStringLiteral("/videos");
        const auto topCameraPath = videosPath + QStringLiteral("/Top Camera");
        const auto catsPath = videosPath + QStringLiteral("/cats");

        QVERIFY(QFileInfo::exists(collectionPath + QStringLiteral("/manifest.toml")));
        QVERIFY(QFileInfo::exists(collectionPath + QStringLiteral("/attributes.toml")));
        QVERIFY(QFileInfo::exists(myDataPath + QStringLiteral("/manifest.toml")));
        QVERIFY(QFileInfo::exists(myDataPath + QStringLiteral("/attributes.toml")));
        QVERIFY(QFileInfo::exists(videosPath + QStringLiteral("/manifest.toml")));
        QVERIFY(QFileInfo::exists(topCameraPath + QStringLiteral("/manifest.toml")));
        QVERIFY(QFileInfo::exists(catsPath + QStringLiteral("/manifest.toml")));

        // Units without explicit attributes should not emit attributes.toml.
        QVERIFY(!QFileInfo::exists(videosPath + QStringLiteral("/attributes.toml")));
        QVERIFY(!QFileInfo::exists(topCameraPath + QStringLiteral("/attributes.toml")));
        QVERIFY(!QFileInfo::exists(catsPath + QStringLiteral("/attributes.toml")));

        QString err;
        const auto collectionManifest = parseTomlFile(collectionPath + QStringLiteral("/manifest.toml"), err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(collectionManifest.value("type").toString(), QStringLiteral("collection"));
        QCOMPARE(collectionManifest.value("format_version").toString(), QStringLiteral("1"));
        QCOMPARE(
            collectionManifest.value("generator").toString(),
            QString::fromStdString(QCoreApplication::applicationName().toStdString()));
        const auto collectionId = collectionManifest.value("collection_id").toString();
        QVERIFY2(uuidRe.match(collectionId).hasMatch(), qPrintable(collectionId));
        const auto authors = collectionManifest.value("authors").toList();
        QCOMPARE(authors.size(), 2);
        QCOMPARE(authors[0].toHash().value("name").toString(), QStringLiteral("Rick Sanchez"));
        QCOMPARE(authors[1].toHash().value("name").toString(), QStringLiteral("Morty Smith"));

        const auto myDataManifest = parseTomlFile(myDataPath + QStringLiteral("/manifest.toml"), err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(myDataManifest.value("type").toString(), QStringLiteral("dataset"));
        const auto myDataParts = myDataManifest.value("data").toHash().value("parts").toList();
        QCOMPARE(myDataParts.size(), 1);
        QCOMPARE(myDataParts[0].toHash().value("fname").toString(), QStringLiteral("blah.test"));

        const auto myDataAttrs = parseTomlFile(myDataPath + QStringLiteral("/attributes.toml"), err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(myDataAttrs.value("alpha").toList().size(), 3);
        const auto helloTab = myDataAttrs.value("hello").toHash();
        QCOMPARE(helloTab.value("world").toInt(), 123);
        QCOMPARE(helloTab.value("nnn").toList().size(), 3);
        QCOMPARE(helloTab.value("values").toHash().value("works").toBool(), true);

        const auto topCameraManifest = parseTomlFile(topCameraPath + QStringLiteral("/manifest.toml"), err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        const auto topCameraParts = topCameraManifest.value("data").toHash().value("parts").toList();
        QCOMPARE(topCameraParts.size(), 1);
        QCOMPARE(topCameraParts[0].toHash().value("fname").toString(), QStringLiteral("camera-video.mkv"));
    }

    void runUtilsSortTest()
    {
        std::vector<std::string> files{
            "test_1.mkv", "test_2.mkv", "test_9.mkv", "test_07.mkv", "test_10.mkv", "test_11.mkv", "test_8.mkv"};
        edl::naturalNumListSort(files);
        QCOMPARE(files[0], "test_07.mkv");
        QCOMPARE(files[1], "test_1.mkv");
        QCOMPARE(files[2], "test_2.mkv");
        QCOMPARE(files[3], "test_8.mkv");
        QCOMPARE(files[4], "test_9.mkv");
        QCOMPARE(files[5], "test_10.mkv");
        QCOMPARE(files[6], "test_11.mkv");
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

        const auto uuidStr = QString::fromStdString(uuid.toHex());
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

        QCOMPARE(Uuid::fromHex(uuidStr.toStdString()).value(), uuid);
    }
};

QTEST_MAIN(TestEDL)
#include "test-edlstorage.moc"


#include <iostream>
#include <QtTest>
#include <QDebug>

#include "utils.h"
#include "edlstorage.h"

class TestEDL : public QObject
{
    Q_OBJECT
private slots:
    void runEDLWrite()
    {
        std::unique_ptr<EDLCollection> collection(new EDLCollection("test-experiment"));
        collection->addAuthor(EDLAuthor("Max Mustermann", "x@y.de"));
        collection->addAuthor(EDLAuthor("Karl Klammer", "aaaa@bbbb.com"));

        collection->setGeneratorId(QCoreApplication::applicationName());

        auto dset = collection->newDataset("mydata");
        dset->addDataFilePart("/usr/local/share/blah.test");

        QVariantHash attrs;
        attrs.insert("alpha", QStringList() << "aaa" << "bbbb" << "cccc");
        QVariantHash subMap;
        subMap.insert("world", 123);
        subMap.insert("nnn", QVariantList() << "spam" << 1.23 << "eggs");
        QVariantHash subSubMap;
        subSubMap.insert("works", true);
        subMap.insert("values", subSubMap);
        attrs.insert("hello", subMap);
        dset->setAttrs(attrs);

        collection->setRootPath("/tmp/edl-test");
        qDebug() << collection->name();
        qDebug() << collection->path();
        qDebug() << collection->rootPath();

        QVERIFY2(collection->save(), qPrintable(collection->lastError()));
    }
};

QTEST_MAIN(TestEDL)
#include "test-edlstorage.moc"

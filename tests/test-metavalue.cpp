
#include <QtTest>
#include <cstring>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "datactl/binarystream.h"
#include "datactl/streammeta.h"

using namespace Syntalos;

// Helper: serialize v to bytes, deserialize back, return the round-tripped value.
static MetaValue roundtrip(const MetaValue &v)
{
    ByteVector buf;
    BinaryStreamWriter w(buf);
    w.write(v);

    BinaryStreamReader r(buf.data(), buf.size());
    MetaValue out;
    r.read(out);
    return out;
}

static MetaStringMap roundtripMap(const MetaStringMap &m)
{
    ByteVector buf;
    BinaryStreamWriter w(buf);
    w.write(m);

    BinaryStreamReader r(buf.data(), buf.size());
    MetaStringMap out;
    r.read(out);
    return out;
}

class TestMetaSerialize : public QObject
{
    Q_OBJECT

private slots:
    void testNull()
    {
        MetaValue v = nullptr;
        auto out = roundtrip(v);
        QVERIFY(std::holds_alternative<std::nullptr_t>(static_cast<const MetaValue::Base &>(out)));
    }

    void testBool()
    {
        QCOMPARE(roundtrip(MetaValue{true}).get<bool>(), std::optional<bool>(true));
        QCOMPARE(roundtrip(MetaValue{false}).get<bool>(), std::optional<bool>(false));
    }

    void testInt64()
    {
        QCOMPARE(roundtrip(MetaValue{int64_t(0)}).get<int64_t>(), std::optional<int64_t>(0));
        QCOMPARE(roundtrip(MetaValue{int64_t(-1)}).get<int64_t>(), std::optional<int64_t>(-1));
        QCOMPARE(
            roundtrip(MetaValue{int64_t(9223372036854775807LL)}).get<int64_t>(),
            std::optional<int64_t>(9223372036854775807LL));
        // int32_t constructor widens to int64_t
        QCOMPARE(roundtrip(MetaValue{int32_t(42)}).get<int64_t>(), std::optional<int64_t>(42));
    }

    void testDouble()
    {
        QCOMPARE(roundtrip(MetaValue{3.14}).get<double>(), std::optional<double>(3.14));
        QCOMPARE(roundtrip(MetaValue{0.0}).get<double>(), std::optional<double>(0.0));
        QCOMPARE(roundtrip(MetaValue{-1.5e100}).get<double>(), std::optional<double>(-1.5e100));
    }

    void testString()
    {
        QCOMPARE(roundtrip(MetaValue{std::string("hello")}).get<std::string>(), std::optional<std::string>("hello"));
        QCOMPARE(roundtrip(MetaValue{std::string("")}).get<std::string>(), std::optional<std::string>(""));
        // const char* constructor
        MetaValue v("syntalos");
        QCOMPARE(roundtrip(v).get<std::string>(), std::optional<std::string>("syntalos"));
    }

    void testMetaSize()
    {
        MetaValue v = MetaSize(1920, 1080);
        auto out = roundtrip(v).get<MetaSize>();
        QVERIFY(out.has_value());
        QCOMPARE(out->width, 1920);
        QCOMPARE(out->height, 1080);

        // Zero size
        MetaValue z = MetaSize(0, 0);
        auto zout = roundtrip(z).get<MetaSize>();
        QVERIFY(zout.has_value());
        QCOMPARE(zout->width, 0);
        QCOMPARE(zout->height, 0);
    }

    void testMetaArrayFlat()
    {
        MetaArray arr{"alpha", "beta", "gamma"};
        auto out = roundtrip(MetaValue{arr}).get<MetaArray>();
        QVERIFY(out.has_value());
        QCOMPARE(out->size(), size_t(3));
        QCOMPARE((*out)[0].get<std::string>(), std::optional<std::string>("alpha"));
        QCOMPARE((*out)[1].get<std::string>(), std::optional<std::string>("beta"));
        QCOMPARE((*out)[2].get<std::string>(), std::optional<std::string>("gamma"));
    }

    void testMetaArrayMixed()
    {
        MetaArray arr{int64_t(7), 2.5, std::string("x"), true, nullptr};
        auto out = roundtrip(MetaValue{arr}).get<MetaArray>();
        QVERIFY(out.has_value());
        QCOMPARE(out->size(), size_t(5));
        QCOMPARE((*out)[0].get<int64_t>(), std::optional<int64_t>(7));
        QCOMPARE((*out)[1].get<double>(), std::optional<double>(2.5));
        QCOMPARE((*out)[2].get<std::string>(), std::optional<std::string>("x"));
        QCOMPARE((*out)[3].get<bool>(), std::optional<bool>(true));
        QVERIFY(std::holds_alternative<std::nullptr_t>(static_cast<const MetaValue::Base &>((*out)[4])));
    }

    void testMetaArrayEmpty()
    {
        MetaArray arr;
        auto out = roundtrip(MetaValue{arr}).get<MetaArray>();
        QVERIFY(out.has_value());
        QVERIFY(out->empty());
    }

    void testMetaStringMap()
    {
        MetaStringMap m;
        m["sample_rate"] = 30000.0;
        m["time_unit"] = std::string("index");
        m["channel_count"] = int64_t(16);
        m["enabled"] = true;
        m["size"] = MetaSize(640, 480);

        auto out = roundtripMap(m);
        QCOMPARE(out.valueOr<double>("sample_rate", 0.0), 30000.0);
        QCOMPARE(out.valueOr<std::string>("time_unit", {}), std::string("index"));
        QCOMPARE(out.valueOr<int64_t>("channel_count", 0), int64_t(16));
        QCOMPARE(out.valueOr<bool>("enabled", false), true);
        auto sz = out.valueOr<MetaSize>("size", {});
        QCOMPARE(sz.width, 640);
        QCOMPARE(sz.height, 480);
    }

    void testMetaStringMapEmpty()
    {
        MetaStringMap m;
        auto out = roundtripMap(m);
        QVERIFY(out.empty());
    }

    void testNestedArrayInMap()
    {
        MetaStringMap m;
        m["signal_names"] = MetaArray{"ch0", "ch1", "ch2"};

        auto out = roundtripMap(m);
        auto arr = out.valueOr<MetaArray>("signal_names", {});
        QCOMPARE(arr.size(), size_t(3));
        QCOMPARE(arr[0].get<std::string>(), std::optional<std::string>("ch0"));
        QCOMPARE(arr[2].get<std::string>(), std::optional<std::string>("ch2"));
    }

    void testNestedMapInValue()
    {
        MetaStringMap inner;
        inner["x"] = int64_t(1);
        inner["y"] = int64_t(2);

        MetaValue v = inner;
        auto out = roundtrip(v).get<MetaStringMap>();
        QVERIFY(out.has_value());
        QCOMPARE(out->valueOr<int64_t>("x", 0), int64_t(1));
        QCOMPARE(out->valueOr<int64_t>("y", 0), int64_t(2));
    }

    void testValueEquality()
    {
        QCOMPARE(MetaValue{3.14}, MetaValue{3.14});
        QVERIFY(MetaValue{1.0} != MetaValue{2.0});
        QCOMPARE(MetaValue{nullptr}, MetaValue{nullptr});
        QCOMPARE(MetaValue{"hello"}, MetaValue{std::string("hello")});
        QCOMPARE(MetaValue{MetaSize(4, 8)}, MetaValue{MetaSize(4, 8)});
        QVERIFY(MetaValue{MetaSize(1, 2)} != MetaValue{MetaSize(1, 3)});
    }

    void testMetaStringMapValueOrTyped()
    {
        MetaStringMap m;
        m["pi"] = 3.14159;
        m["n"] = int64_t(42);

        QCOMPARE(m.valueOr<double>("pi", 0.0), 3.14159);
        QCOMPARE(m.valueOr<double>("missing", -1.0), -1.0);
        QCOMPARE(m.valueOr<int64_t>("n", 0), int64_t(42));
        QVERIFY(!m.value<double>("missing").has_value());
        QVERIFY(m.value<double>("pi").has_value());
    }

    void testJsonSerializePrimitives()
    {
        QCOMPARE(QString::fromStdString(toJsonString(MetaValue{nullptr})), QStringLiteral("null"));
        QCOMPARE(QString::fromStdString(toJsonString(MetaValue{true})), QStringLiteral("true"));
        QCOMPARE(QString::fromStdString(toJsonString(MetaValue{int64_t(-42)})), QStringLiteral("-42"));
        QCOMPARE(QString::fromStdString(toJsonString(MetaValue{2.5})), QStringLiteral("2.5"));
        QCOMPARE(
            QString::fromStdString(toJsonString(MetaValue{MetaSize(640, 480)})),
            QStringLiteral("{\"width\":640,\"height\":480}"));

        const auto escaped = QString::fromStdString(toJsonString(MetaValue{std::string("line1\n\"q\"\\")}));
        QCOMPARE(escaped, QStringLiteral("\"line1\\n\\\"q\\\"\\\\\""));
    }

    void testJsonSerializeNested()
    {
        MetaStringMap root;
        root["name"] = std::string("cam0");
        root["enabled"] = true;
        root["size"] = MetaSize(320, 240);
        root["samples"] = MetaArray{int64_t(1), std::string("two"), false};

        MetaStringMap nested;
        nested["x"] = int64_t(1);
        nested["y"] = int64_t(2);
        root["coord"] = nested;

        const auto jsonStr = toJsonObjectString(root);
        QJsonParseError err;
        const auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(jsonStr), &err);
        QCOMPARE(err.error, QJsonParseError::NoError);
        QVERIFY(doc.isObject());

        const auto obj = doc.object();
        QCOMPARE(obj.value("name").toString(), QStringLiteral("cam0"));
        QCOMPARE(obj.value("enabled").toBool(), true);
        QCOMPARE(obj.value("size").toObject().value("width").toInt(), 320);
        QCOMPARE(obj.value("size").toObject().value("height").toInt(), 240);

        const auto samples = obj.value("samples").toArray();
        QCOMPARE(samples.size(), 3);
        QCOMPARE(samples.at(0).toInt(), 1);
        QCOMPARE(samples.at(1).toString(), QStringLiteral("two"));
        QCOMPARE(samples.at(2).toBool(), false);

        const auto coord = obj.value("coord").toObject();
        QCOMPARE(coord.value("x").toInt(), 1);
        QCOMPARE(coord.value("y").toInt(), 2);
    }

    void testJsonSerializeMapOrder()
    {
        MetaStringMap m;
        m["b"] = int64_t(2);
        m["a"] = int64_t(1);
        QCOMPARE(QString::fromStdString(toJsonObjectString(m)), QStringLiteral("{\"a\":1,\"b\":2}"));
    }
};

QTEST_MAIN(TestMetaSerialize)
#include "test-metavalue.moc"

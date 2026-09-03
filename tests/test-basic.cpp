
#include <QDebug>
#include <QtTest>
#include <iostream>
#include <limits>
#include "datactl/datatypes.h"
#include "datactl/frametype.h"
#include "streams/stream.h"

using namespace Syntalos;

class TestBasic : public QObject
{
    Q_OBJECT
private:
private slots:
    void testNumToStringIntegers()
    {
        // Signed integers
        QCOMPARE(numToString(int8_t(0)), "0");
        QCOMPARE(numToString(int8_t(42)), "42");
        QCOMPARE(numToString(int8_t(-42)), "-42");
        QCOMPARE(numToString(int8_t(127)), "127");
        QCOMPARE(numToString(int8_t(-128)), "-128");

        QCOMPARE(numToString(int16_t(1234)), "1234");
        QCOMPARE(numToString(int16_t(-1234)), "-1234");
        QCOMPARE(numToString(int16_t(32767)), "32767");
        QCOMPARE(numToString(int16_t(-32768)), "-32768");

        QCOMPARE(numToString(int32_t(123456)), "123456");
        QCOMPARE(numToString(int32_t(-123456)), "-123456");
        QCOMPARE(numToString(int32_t(2147483647)), "2147483647");
        QCOMPARE(numToString(int32_t(-2147483648)), "-2147483648");

        QCOMPARE(numToString(int64_t(9876543210)), "9876543210");
        QCOMPARE(numToString(int64_t(-9876543210)), "-9876543210");
        QCOMPARE(numToString(std::numeric_limits<int64_t>::max()), "9223372036854775807");
        QCOMPARE(numToString(std::numeric_limits<int64_t>::min()), "-9223372036854775808");

        // Unsigned integers
        QCOMPARE(numToString(uint8_t(0)), "0");
        QCOMPARE(numToString(uint8_t(255)), "255");

        QCOMPARE(numToString(uint16_t(0)), "0");
        QCOMPARE(numToString(uint16_t(65535)), "65535");

        QCOMPARE(numToString(uint32_t(0)), "0");
        QCOMPARE(numToString(uint32_t(4294967295)), "4294967295");

        QCOMPARE(numToString(uint64_t(0)), "0");
        QCOMPARE(numToString(uint64_t(18446744073709551615ULL)), "18446744073709551615");
    }

    void testNumToStringFloats()
    {
        // Basic float tests
        QCOMPARE(numToString(0.0f), "0");
        QCOMPARE(numToString(0.0), "0");
        QCOMPARE(numToString(-0.0), "0"); // canonicalized to +0

        // Simple values (should use fixed notation)
        QCOMPARE(numToString(3.14159f), "3.14159");

        QCOMPARE(numToString(2.718281828459045), "2.718281828459045");

        // Negative values
        QCOMPARE(numToString(-123.456), "-123.456");

        // Very small values (uses scientific notation)
        QCOMPARE(numToString(1.23e-10), "1.23e-10");

        // Very large values (uses scientific notation)
        QCOMPARE(numToString(1.23e15), "1.23e+15");

        // Special cases
        QCOMPARE(numToString(999999.0), "999999");
        QCOMPARE(numToString(0.0001), "0.0001");
    }

    void testNumToStringLongDouble()
    {
        // Basic long double tests
        QCOMPARE(numToString(0.0L), "0");
        QCOMPARE(numToString(-0.0L), "0"); // canonicalized to +0
        QCOMPARE(numToString(1.0L), "1");

        // to_chars uses shortest round-trippable representation, not necessarily
        // all digits from the literal. Verify it starts correctly and round-trips.
        auto result = numToString(3.14159265358979323846L);
        QVERIFY2(result.starts_with("3.1415926535897932"), qPrintable(QString::fromStdString(result)));

        // Extreme values
        result = numToString(std::numeric_limits<long double>::min());
        QVERIFY(!result.empty());
        QVERIFY(result.find("e-") != std::string::npos);

        result = numToString(std::numeric_limits<long double>::max());
        QVERIFY(!result.empty());
        QVERIFY(result.find("e+") != std::string::npos);
    }

    void testNumToStringBool()
    {
        QCOMPARE(numToString(true), "true");
        QCOMPARE(numToString(false), "false");
    }

    void testMatEnsureExclusive()
    {
        cv::Mat mat(4, 4, CV_8UC1);
        const auto *buffer = mat.data;

        // sole owner and unchanged shape: the pixel buffer has to be recycled
        matEnsureExclusive(mat, 4, 4, CV_8UC1);
        QCOMPARE(mat.data, buffer);

        // a subscriber still holds a shallow copy of the published frame, so we must
        // not hand the same buffer out again (this is what cv::Mat::create() would do)
        cv::Mat queued = mat;
        matEnsureExclusive(mat, 4, 4, CV_8UC1);
        QVERIFY(mat.data != buffer);
        QCOMPARE(queued.data, buffer);
        QCOMPARE(mat.rows, 4);
        QCOMPARE(mat.cols, 4);
        QCOMPARE(mat.type(), CV_8UC1);

        // a different shape or type always needs a new buffer
        buffer = mat.data;
        matEnsureExclusive(mat, 8, 4, CV_8UC1);
        QVERIFY(mat.data != buffer);
        QCOMPARE(mat.rows, 8);

        buffer = mat.data;
        matEnsureExclusive(mat, 8, 4, CV_16UC1);
        QVERIFY(mat.data != buffer);
        QCOMPARE(mat.type(), CV_16UC1);
    }

    void testStreamTypeCompatibility()
    {
        const auto compat = [](BaseDataType::TypeId from, BaseDataType::TypeId to) {
            return checkStreamTypesCompatible(from, to);
        };
        using T = BaseDataType;

        // identity
        QVERIFY(compat(T::SignalBlockI16, T::SignalBlockI16));
        QVERIFY(compat(T::Frame, T::Frame));

        // widening casts of the 16-bit integer blocks
        QVERIFY(compat(T::SignalBlockI16, T::SignalBlockI32));
        QVERIFY(compat(T::SignalBlockI16, T::SignalBlockF32));
        QVERIFY(compat(T::SignalBlockU16, T::SignalBlockI32));
        QVERIFY(compat(T::SignalBlockU16, T::SignalBlockF32));
        QVERIFY(compat(T::SignalBlockI32, T::SignalBlockF32));
        QVERIFY(compat(T::SignalBlockF32, T::SignalBlockI32));

        // signedness must never be reinterpreted implicitly
        QVERIFY(!compat(T::SignalBlockI16, T::SignalBlockU16));
        QVERIFY(!compat(T::SignalBlockU16, T::SignalBlockI16));

        // no narrowing into the 16-bit types
        QVERIFY(!compat(T::SignalBlockI32, T::SignalBlockI16));
        QVERIFY(!compat(T::SignalBlockF32, T::SignalBlockI16));
        QVERIFY(!compat(T::SignalBlockI32, T::SignalBlockU16));
        QVERIFY(!compat(T::SignalBlockF32, T::SignalBlockU16));

        // unrelated types
        QVERIFY(!compat(T::Frame, T::SignalBlockI16));
        QVERIFY(!compat(T::SignalBlockI16, T::TableRow));

        // name round-trip of the new type
        QCOMPARE(BaseDataType::typeIdFromString("SignalBlockI16"), T::SignalBlockI16);
        QCOMPARE(
            QString::fromStdString(BaseDataType::typeIdToString(T::SignalBlockI16)),
            QStringLiteral("SignalBlockI16"));
    }
};

QTEST_MAIN(TestBasic)
#include "test-basic.moc"

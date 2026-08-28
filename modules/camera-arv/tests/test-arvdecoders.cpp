/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gio/gio.h> // Workaround for gdbusintrospection's use of "signal", must come before Qt.

#include <QtTest>
#include <opencv2/core.hpp>

#include "qarv/qarvdecoder.h"
#include "qarv/decoders/bayer/decoder.h"
#include "qarv/decoders/mono12packed.h"
#include "qarv/decoders/monounpacked.h"
#include "qarv/decoders/swscaledecoder.h"
#include "qarv/decoders/unsupported.h"

using namespace QArv;

static const int TEST_WIDTH = 64;
static const int TEST_HEIGHT = 48;
static const QSize TEST_SIZE = QSize(TEST_WIDTH, TEST_HEIGHT);

/**
 * Build a deterministic, non-uniform payload of the requested size. Two payloads with
 * different seeds differ in every byte, so any decoder must turn them into visibly
 * different images.
 */
static QByteArray makePayload(int nbytes, int seed)
{
    QByteArray payload(nbytes, Qt::Uninitialized);
    for (int i = 0; i < nbytes; ++i)
        payload[i] = static_cast<char>(((i * 7) + (seed * 83) + (i / 13)) & 0xff);
    return payload;
}

class TestArvDecoders : public QObject
{
    Q_OBJECT

private:
    /**
     * Verify that a decoder hands its output to the caller and keeps nothing.
     *
     * Decoding a second frame must not disturb the matrix produced for the first one.
     * The decoders used to keep a single internal buffer and only return a shallow
     * cv::Mat header for it, so every frame that was still queued downstream was
     * silently overwritten by the next one - recordings ended up with runs of
     * duplicated frames (issue #188).
     */
    void checkDecodesIntoCallerStorage(QArvDecoder &decoder, const QByteArray &frameA, const QByteArray &frameB)
    {
        cv::Mat first;
        decoder.decodeInto(frameA, first);
        QVERIFY(!first.empty());

        // remember what the first frame decoded to
        const cv::Mat firstExpected = first.clone();

        cv::Mat second;
        decoder.decodeInto(frameB, second);
        QVERIFY(!second.empty());
        QCOMPARE(second.size(), first.size());
        QCOMPARE(second.type(), first.type());

        // sanity check: if both payloads decoded to the same image, this test proves nothing
        QVERIFY(cv::norm(firstExpected, second, cv::NORM_INF) > 0);

        // the actual contract: the first matrix must still hold the first frame
        QCOMPARE(cv::norm(first, firstExpected, cv::NORM_INF), 0.0);

        // ... which is only possible if the two do not share pixel storage
        QVERIFY(first.data != second.data);
    }

    /**
     * Verify that a caller may recycle its output matrix: handing the same (exclusively
     * owned) matrix back must reuse its pixel buffer instead of allocating a new one.
     * The acquisition path relies on this to avoid a heap allocation per frame.
     */
    void checkReusesCallerBuffer(QArvDecoder &decoder, const QByteArray &frameA, const QByteArray &frameB)
    {
        cv::Mat mat;
        decoder.decodeInto(frameA, mat);
        QVERIFY(!mat.empty());

        const auto *buffer = mat.data;
        decoder.decodeInto(frameB, mat);
        QCOMPARE(mat.data, buffer);
    }

private slots:

    void monoUnpacked8()
    {
        MonoUnpackedDecoder<uint8_t, 8, ARV_PIXEL_FORMAT_MONO_8> decoder(TEST_SIZE);
        const auto a = makePayload(TEST_WIDTH * TEST_HEIGHT, 1);
        const auto b = makePayload(TEST_WIDTH * TEST_HEIGHT, 2);

        checkDecodesIntoCallerStorage(decoder, a, b);
        checkReusesCallerBuffer(decoder, a, b);
    }

    void monoUnpacked12()
    {
        MonoUnpackedDecoder<uint16_t, 12, ARV_PIXEL_FORMAT_MONO_12> decoder(TEST_SIZE);
        const auto a = makePayload(TEST_WIDTH * TEST_HEIGHT * 2, 3);
        const auto b = makePayload(TEST_WIDTH * TEST_HEIGHT * 2, 4);

        checkDecodesIntoCallerStorage(decoder, a, b);
        checkReusesCallerBuffer(decoder, a, b);
    }

    void mono12Packed()
    {
        Mono12PackedDecoder decoder(TEST_SIZE);
        // 12-bit packed stores two pixels in three bytes
        const int payloadSize = (TEST_WIDTH * TEST_HEIGHT * 3) / 2;
        const auto a = makePayload(payloadSize, 5);
        const auto b = makePayload(payloadSize, 6);

        checkDecodesIntoCallerStorage(decoder, a, b);
        checkReusesCallerBuffer(decoder, a, b);
    }

    void swScale()
    {
        SwScaleDecoder decoder(TEST_SIZE, AV_PIX_FMT_RGB24, ARV_PIXEL_FORMAT_RGB_8_PACKED);
        const auto a = makePayload(TEST_WIDTH * TEST_HEIGHT * 3, 7);
        const auto b = makePayload(TEST_WIDTH * TEST_HEIGHT * 3, 8);

        checkDecodesIntoCallerStorage(decoder, a, b);
        checkReusesCallerBuffer(decoder, a, b);
    }

    void bayer8()
    {
        BayerDecoder<ARV_PIXEL_FORMAT_BAYER_RG_8> decoder(TEST_SIZE);
        const auto a = makePayload(TEST_WIDTH * TEST_HEIGHT, 9);
        const auto b = makePayload(TEST_WIDTH * TEST_HEIGHT, 10);

        checkDecodesIntoCallerStorage(decoder, a, b);
        checkReusesCallerBuffer(decoder, a, b);
    }

    void unsupportedFormat()
    {
        // This decoder always emits the same "unsupported" placeholder image, so it can not
        // be checked for content changes - but it still must not hand out its own storage,
        // as the camera module transforms the returned matrix in place.
        Unsupported decoder(ARV_PIXEL_FORMAT_MONO_8, TEST_SIZE);
        const auto frame = makePayload(TEST_WIDTH * TEST_HEIGHT, 11);

        cv::Mat first;
        decoder.decodeInto(frame, first);
        QVERIFY(!first.empty());

        cv::Mat second;
        decoder.decodeInto(frame, second);
        QVERIFY(!second.empty());
        QVERIFY(first.data != second.data);

        checkReusesCallerBuffer(decoder, frame, frame);
    }
};

QTEST_APPLESS_MAIN(TestArvDecoders)
#include "test-arvdecoders.moc"

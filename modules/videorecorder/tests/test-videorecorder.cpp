/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 */

#include <QTemporaryDir>
#include <QtTest>
#include <cstring>
#include <opencv2/core.hpp>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "encodehelper/videoreader.h"
#include "videowriter.h"

using namespace Syntalos;

class TestVideoRecorder : public QObject
{
    Q_OBJECT

public:
    TestVideoRecorder()
    {
        initializeSyLogSystem(quill::LogLevel::Warning);
    }

private slots:
    void rawMatroskaRoundtrip_data();
    void rawMatroskaRoundtrip();
    void ffvhuffMatroskaRoundtrip_data();
    void ffvhuffMatroskaRoundtrip();

private:
    void matroskaRoundtrip(VideoCodec codec, AVCodecID expectedCodecId);
};

/**
 * Create a deterministic test frame. The pattern is asymmetric in both directions,
 * so flipped, transposed or byte-swapped frames do not compare equal.
 */
static cv::Mat makeTestFrame(int width, int height, int depth, bool color)
{
    cv::Mat frame(height, width, CV_MAKETYPE(depth, color ? 3 : 1));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (depth == CV_16U)
                frame.at<uint16_t>(y, x) = static_cast<uint16_t>(x * 997U + y * 7919U);
            else if (color)
                frame.at<cv::Vec3b>(y, x) = cv::Vec3b(
                    static_cast<uint8_t>(x * 3),
                    static_cast<uint8_t>(y * 5),
                    static_cast<uint8_t>(x ^ y));
            else
                frame.at<uint8_t>(y, x) = static_cast<uint8_t>(x * 7 + y * 13);
        }
    }
    return frame;
}

void TestVideoRecorder::rawMatroskaRoundtrip_data()
{
    QTest::addColumn<int>("depth");
    QTest::addColumn<bool>("color");
    QTest::addColumn<bool>("exactColors");
    QTest::addColumn<int>("pixFormat");
    QTest::addColumn<uint>("codecTag");
    QTest::addColumn<bool>("bitExact");
    QTest::addColumn<int>("threads");

    // Native V_UNCOMPRESSED mappings. A regression to the VFW fallback would store 16-bit
    // gray as RGB555, and the old workaround downgraded it to gray8.
    QTest::newRow("gray16") << CV_16U << false << true << static_cast<int>(AV_PIX_FMT_GRAY16LE)
                            << avcodec_pix_fmt_to_codec_tag(AV_PIX_FMT_GRAY16LE) << true << 0;
    QTest::newRow("gray8") << CV_8U << false << true << static_cast<int>(AV_PIX_FMT_GRAY8)
                           << avcodec_pix_fmt_to_codec_tag(AV_PIX_FMT_GRAY8) << true << 0;

    // Raw BGR24 has no native Matroska mapping and is stored via VFW mode, which carries
    // no FourCC. Without exact colors, the frames are converted to YUV 4:2:0 instead.
    QTest::newRow("bgr24-exact") << CV_8U << true << true << static_cast<int>(AV_PIX_FMT_BGR24) << 0U << true << 0;
    QTest::newRow("bgr24-yuv420p") << CV_8U << true << false << static_cast<int>(AV_PIX_FMT_YUV420P)
                                   << avcodec_pix_fmt_to_codec_tag(AV_PIX_FMT_YUV420P) << false << 0;
}

void TestVideoRecorder::rawMatroskaRoundtrip()
{
    matroskaRoundtrip(VideoCodec::Raw, AV_CODEC_ID_RAWVIDEO);
}

void TestVideoRecorder::ffvhuffMatroskaRoundtrip_data()
{
    QTest::addColumn<int>("depth");
    QTest::addColumn<bool>("color");
    QTest::addColumn<bool>("exactColors");
    QTest::addColumn<int>("pixFormat");
    QTest::addColumn<uint>("codecTag");
    QTest::addColumn<bool>("bitExact");
    QTest::addColumn<int>("threads");

    // FFVHuff is the intermediate codec for deferred encoding. It is stored via the VFW
    // mapping in Matroska (tag "FFVH"), and the pixel format has to survive the round-trip.
    // Multi-threaded rows exercise the delayed packet handling of frame threading.
    const uint tag = MKTAG('F', 'F', 'V', 'H');
    QTest::newRow("gray16") << CV_16U << false << true << static_cast<int>(AV_PIX_FMT_GRAY16LE) << tag << true << 4;
    QTest::newRow("gray8") << CV_8U << false << true << static_cast<int>(AV_PIX_FMT_GRAY8) << tag << true << 1;
    QTest::newRow("bgr24-exact") << CV_8U << true << true << static_cast<int>(AV_PIX_FMT_GBRP) << tag << true << 4;
    QTest::newRow("bgr24-yuv420p") << CV_8U << true << false << static_cast<int>(AV_PIX_FMT_YUV420P) << tag << false
                                   << 1;
}

void TestVideoRecorder::ffvhuffMatroskaRoundtrip()
{
    matroskaRoundtrip(VideoCodec::FFVHuff, AV_CODEC_ID_FFVHUFF);
}

void TestVideoRecorder::matroskaRoundtrip(VideoCodec codec, AVCodecID expectedCodecId)
{
    QFETCH(int, depth);
    QFETCH(bool, color);
    QFETCH(bool, exactColors);
    QFETCH(int, pixFormat);
    QFETCH(uint, codecTag);
    QFETCH(bool, bitExact);
    QFETCH(int, threads);

    // non-square, so swapped dimensions are noticed
    constexpr int width = 96;
    constexpr int height = 64;
    // more frames than encoder threads, so frame threading actually delays packets
    constexpr int frameCount = 8;

    const cv::Mat expected = makeTestFrame(width, height, depth, color);

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString fileBase = tempDir.filePath(QStringLiteral("raw"));
    const QString filename = fileBase + QStringLiteral(".mkv");

    VideoWriter writer;
    writer.setContainer(VideoContainer::Matroska);
    CodecProperties codecProps(codec);
    codecProps.setExactColors(exactColors);
    codecProps.setThreadCount(threads);
    writer.setCodecProps(codecProps);
    writer.initialize(fileBase, "test", "source", Uuid{}, "", width, height, 30.0, depth, color, false);
    QCOMPARE(writer.hasExactColors(), bitExact);
    for (int i = 0; i < frameCount; ++i)
        QVERIFY2(writer.encodeFrame(expected, std::chrono::microseconds{i * 33333}), writer.lastError().c_str());
    const auto finalizeResult = writer.finalize();
    QVERIFY2(finalizeResult.has_value(), finalizeResult ? "" : finalizeResult.error().c_str());

    // Verify how the muxer described the stream, independent of how our reader copes with it.
    AVFormatContext *formatCtx = nullptr;
    QVERIFY(avformat_open_input(&formatCtx, qPrintable(filename), nullptr, nullptr) >= 0);
    QVERIFY(avformat_find_stream_info(formatCtx, nullptr) >= 0);
    const AVCodecParameters *videoParams = nullptr;
    for (unsigned int i = 0; i < formatCtx->nb_streams; ++i) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoParams = formatCtx->streams[i]->codecpar;
            break;
        }
    }
    QVERIFY(videoParams != nullptr);
    QCOMPARE(videoParams->codec_id, expectedCodecId);
    QCOMPARE(videoParams->codec_tag, codecTag);
    QCOMPARE(videoParams->format, pixFormat);
    QCOMPARE(videoParams->width, width);
    QCOMPARE(videoParams->height, height);
    avformat_close_input(&formatCtx);

    // Read the frames back the same way the deferred encoder does
    VideoReader reader;
    QVERIFY2(reader.open(filename), qPrintable(reader.lastError()));
    for (int i = 0; i < frameCount; ++i) {
        const auto frame = reader.readFrame();
        QVERIFY2(frame.has_value(), qPrintable(reader.lastError()));
        QCOMPARE(frame->second, i);

        const cv::Mat &actual = frame->first;
        QCOMPARE(actual.type(), expected.type());
        QCOMPARE(actual.rows, expected.rows);
        QCOMPARE(actual.cols, expected.cols);
        QVERIFY(actual.isContinuous());
        if (bitExact) {
            QVERIFY2(
                std::memcmp(actual.data, expected.data, expected.total() * expected.elemSize()) == 0,
                "decoded frame differs from the encoded one");
        } else {
            // YUV 4:2:0 rounds every value and subsamples chroma, but the frame must still
            // be the same image (a flipped frame would produce a much larger error)
            const double meanAbsError = cv::norm(actual, expected, cv::NORM_L1)
                                        / static_cast<double>(expected.total() * expected.channels());
            QVERIFY2(meanAbsError < 8.0, qPrintable(QStringLiteral("mean error %1").arg(meanAbsError)));
        }
    }
    QVERIFY(!reader.readFrame().has_value());
}

QTEST_GUILESS_MAIN(TestVideoRecorder)
#include "test-videorecorder.moc"

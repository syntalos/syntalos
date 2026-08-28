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
    void rawGray16MatroskaRoundtrip();
};

void TestVideoRecorder::rawGray16MatroskaRoundtrip()
{
    constexpr int width = 64;
    constexpr int height = 64;

    cv::Mat expected(height, width, CV_16UC1);
    for (int y = 0; y < height; ++y) {
        auto *row = expected.ptr<uint16_t>(y);
        for (int x = 0; x < width; ++x)
            row[x] = static_cast<uint16_t>(x * 997U + y * 7919U);
    }

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString fileBase = tempDir.filePath(QStringLiteral("gray16"));
    const QString filename = fileBase + QStringLiteral(".mkv");

    VideoWriter writer;
    writer.setContainer(VideoContainer::Matroska);
    writer.setCodecProps(CodecProperties(VideoCodec::Raw));
    writer.initialize(fileBase, "test", "source", Uuid{}, "", width, height, 30.0, CV_16U, false, false);
    QVERIFY(writer.hasExactColors());
    QVERIFY2(writer.encodeFrame(expected, std::chrono::microseconds{0}), writer.lastError().c_str());
    const auto finalizeResult = writer.finalize();
    QVERIFY2(finalizeResult.has_value(), finalizeResult ? "" : finalizeResult.error().c_str());

    // Verify that the muxer wrote native V_UNCOMPRESSED metadata rather than the
    // ambiguous VFW representation, which FFmpeg would decode as RGB555.
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
    QCOMPARE(videoParams->codec_id, AV_CODEC_ID_RAWVIDEO);
    QCOMPARE(videoParams->codec_tag, avcodec_pix_fmt_to_codec_tag(AV_PIX_FMT_GRAY16LE));
    QCOMPARE(videoParams->format, static_cast<int>(AV_PIX_FMT_GRAY16LE));
    avformat_close_input(&formatCtx);

    VideoReader reader;
    QVERIFY2(reader.open(filename), qPrintable(reader.lastError()));
    const auto frame = reader.readFrame();
    QVERIFY2(frame.has_value(), qPrintable(reader.lastError()));
    const cv::Mat &actual = frame->first;
    QCOMPARE(actual.type(), CV_16UC1);
    QCOMPARE(actual.channels(), 1);
    QCOMPARE(actual.rows, expected.rows);
    QCOMPARE(actual.cols, expected.cols);
    QCOMPARE(actual.total() * actual.elemSize(), expected.total() * expected.elemSize());
    QVERIFY(std::memcmp(actual.data, expected.data, expected.total() * expected.elemSize()) == 0);
}

QTEST_GUILESS_MAIN(TestVideoRecorder)
#include "test-videorecorder.moc"

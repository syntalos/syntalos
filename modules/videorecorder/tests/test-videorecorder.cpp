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
#include "videowriter_p.h"

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
    void losslessGrayExactColors_data();
    void losslessGrayExactColors();
    void softwareEncoderSelection_data();
    void softwareEncoderSelection();
    void encoderPresetCompatibility_data();
    void encoderPresetCompatibility();
    void svtAv1PresetConfig_data();
    void svtAv1PresetConfig();
    void svtAv1ParallelismSemantics_data();
    void svtAv1ParallelismSemantics();
    void svtAv1ParallelismLevel_data();
    void svtAv1ParallelismLevel();
    void svtAv1ParameterComposition_data();
    void svtAv1ParameterComposition();
    void codecPresetProperties();
    void presetEncode_data();
    void presetEncode();

private:
    void matroskaRoundtrip(VideoCodec codec, AVCodecID expectedCodecId);
};

namespace
{
AVCodec testPreferredEncoder{};
AVCodec testFallbackEncoder{};
bool testPreferredAvailable = false;
bool testGenericLookupCalled = false;
AVCodecID testGenericLookupId = AV_CODEC_ID_NONE;

const AVCodec *findTestEncoderByName(const char *name)
{
    if (!testPreferredAvailable)
        return nullptr;
    testPreferredEncoder.name = name;
    return &testPreferredEncoder;
}

const AVCodec *findTestEncoderById(AVCodecID id)
{
    testGenericLookupCalled = true;
    testGenericLookupId = id;
    testFallbackEncoder.name = "generic-fallback";
    return &testFallbackEncoder;
}
} // namespace

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
    for (size_t i = 0; i < frameCount; ++i) {
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

void TestVideoRecorder::losslessGrayExactColors_data()
{
    QTest::addColumn<int>("codec");
    QTest::addColumn<int>("depth");
    QTest::addColumn<bool>("exact");

    // Lossless encoders without a native grayscale format store gray frames as YUV 4:2:0.
    // That is still bit-exact for 8-bit luma (the chroma planes are just filled in), but
    // 16-bit data has to be truncated.
    QTest::newRow("h264-gray8") << static_cast<int>(VideoCodec::H264) << CV_8U << true;
    QTest::newRow("h264-gray16") << static_cast<int>(VideoCodec::H264) << CV_16U << false;
    QTest::newRow("ffv1-gray8") << static_cast<int>(VideoCodec::FFV1) << CV_8U << true;
    QTest::newRow("ffv1-gray16") << static_cast<int>(VideoCodec::FFV1) << CV_16U << true;
}

void TestVideoRecorder::losslessGrayExactColors()
{
    QFETCH(int, codec);
    QFETCH(int, depth);
    QFETCH(bool, exact);

    if (codec == static_cast<int>(VideoCodec::H264) && avcodec_find_encoder_by_name("libx264") == nullptr)
        QSKIP("libx264 encoder is not available");

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    VideoWriter writer;
    writer.setContainer(VideoContainer::Matroska);
    CodecProperties codecProps(static_cast<VideoCodec>(codec));
    codecProps.setLossless(true);
    codecProps.setExactColors(true);
    writer.setCodecProps(codecProps);
    writer.initialize(
        tempDir.filePath(QStringLiteral("gray")),
        "test",
        "source",
        Uuid{},
        "",
        96,
        64,
        30.0,
        depth,
        false,
        false);
    QCOMPARE(writer.hasExactColors(), exact);
    QVERIFY(writer.finalize().has_value());
}

void TestVideoRecorder::softwareEncoderSelection_data()
{
    QTest::addColumn<int>("codec");
    QTest::addColumn<QString>("preferredName");
    QTest::addColumn<int>("codecId");
    QTest::addColumn<bool>("allowsGenericFallback");

    QTest::newRow("av1") << static_cast<int>(VideoCodec::AV1) << QStringLiteral("libsvtav1")
                         << static_cast<int>(AV_CODEC_ID_AV1) << false;
    QTest::newRow("vp9") << static_cast<int>(VideoCodec::VP9) << QStringLiteral("libvpx-vp9")
                         << static_cast<int>(AV_CODEC_ID_VP9) << true;
    QTest::newRow("h264") << static_cast<int>(VideoCodec::H264) << QStringLiteral("libx264")
                          << static_cast<int>(AV_CODEC_ID_H264) << true;
    QTest::newRow("hevc") << static_cast<int>(VideoCodec::HEVC) << QStringLiteral("libx265")
                          << static_cast<int>(AV_CODEC_ID_HEVC) << true;
}

void TestVideoRecorder::softwareEncoderSelection()
{
    QFETCH(int, codec);
    QFETCH(QString, preferredName);
    QFETCH(int, codecId);
    QFETCH(bool, allowsGenericFallback);

    testPreferredAvailable = true;
    testGenericLookupCalled = false;
    const auto *selected = VideoWriterPrivate::findSoftwareEncoder(
        static_cast<VideoCodec>(codec),
        findTestEncoderByName,
        findTestEncoderById);
    QCOMPARE(selected, &testPreferredEncoder);
    QCOMPARE(QString::fromLatin1(selected->name), preferredName);
    QVERIFY(!testGenericLookupCalled);
    QVERIFY(VideoWriterPrivate::encoderSupportsPreset(static_cast<VideoCodec>(codec), selected));

    testPreferredAvailable = false;
    testGenericLookupCalled = false;
    selected = VideoWriterPrivate::findSoftwareEncoder(
        static_cast<VideoCodec>(codec),
        findTestEncoderByName,
        findTestEncoderById);
    if (allowsGenericFallback) {
        QCOMPARE(selected, &testFallbackEncoder);
        QVERIFY(testGenericLookupCalled);
        QCOMPARE(testGenericLookupId, static_cast<AVCodecID>(codecId));
        QVERIFY(!VideoWriterPrivate::encoderSupportsPreset(static_cast<VideoCodec>(codec), selected));
    } else {
        QVERIFY(selected == nullptr);
        QVERIFY(!testGenericLookupCalled);
    }
}

void TestVideoRecorder::encoderPresetCompatibility_data()
{
    QTest::addColumn<int>("codec");
    QTest::addColumn<QString>("encoderName");
    QTest::addColumn<bool>("compatible");

    QTest::newRow("svt-av1") << static_cast<int>(VideoCodec::AV1) << QStringLiteral("libsvtav1") << true;
    QTest::newRow("other-av1") << static_cast<int>(VideoCodec::AV1) << QStringLiteral("libaom-av1") << false;
    QTest::newRow("libvpx-vp9") << static_cast<int>(VideoCodec::VP9) << QStringLiteral("libvpx-vp9") << true;
    QTest::newRow("other-vp9") << static_cast<int>(VideoCodec::VP9) << QStringLiteral("vp9_qsv") << false;
    QTest::newRow("x264") << static_cast<int>(VideoCodec::H264) << QStringLiteral("libx264") << true;
    QTest::newRow("other-h264") << static_cast<int>(VideoCodec::H264) << QStringLiteral("h264_v4l2m2m") << false;
    QTest::newRow("x265") << static_cast<int>(VideoCodec::HEVC) << QStringLiteral("libx265") << true;
    QTest::newRow("other-hevc") << static_cast<int>(VideoCodec::HEVC) << QStringLiteral("hevc_vulkan") << false;
}

void TestVideoRecorder::encoderPresetCompatibility()
{
    QFETCH(int, codec);
    QFETCH(QString, encoderName);
    QFETCH(bool, compatible);

    AVCodec encoder{};
    const auto name = encoderName.toLatin1();
    encoder.name = name.constData();
    QCOMPARE(VideoWriterPrivate::encoderSupportsPreset(static_cast<VideoCodec>(codec), &encoder), compatible);
}

void TestVideoRecorder::svtAv1PresetConfig_data()
{
    QTest::addColumn<QString>("version");
    QTest::addColumn<int>("optionMaximum");
    QTest::addColumn<int>("expectedMaximum");
    QTest::addColumn<int>("expectedDefault");

    QTest::newRow("pre-2.3") << QStringLiteral("v2.2.1") << 13 << 13 << 8;
    QTest::newRow("2.3-aliases") << QStringLiteral("v2.3.0") << 13 << 11 << 10;
    QTest::newRow("3.0-range") << QStringLiteral("3.0.0") << 13 << 10 << 8;
    QTest::newRow("newer-range") << QStringLiteral("SVT-AV1 Encoder Lib v4.0.0") << 13 << 10 << 8;
    QTest::newRow("ffmpeg-narrower") << QStringLiteral("v2.3.0") << 9 << 9 << 9;
    QTest::newRow("unknown-version") << QStringLiteral("unknown") << 13 << 13 << 8;
    QTest::newRow("unknown-version-ffmpeg-range") << QStringLiteral("unknown") << 11 << 11 << 8;
}

void TestVideoRecorder::svtAv1PresetConfig()
{
    QFETCH(QString, version);
    QFETCH(int, optionMaximum);
    QFETCH(int, expectedMaximum);
    QFETCH(int, expectedDefault);

    const auto versionBytes = version.toLatin1();
    const auto config = VideoWriterPrivate::svtAv1PresetConfig(versionBytes.constData(), std::pair{0, optionMaximum});
    QCOMPARE(config.minimum, 0);
    QCOMPARE(config.maximum, expectedMaximum);
    QCOMPARE(config.defaultValue, expectedDefault);
    QCOMPARE(config.clamp(99), expectedMaximum);
    QCOMPARE(config.clamp(-1), 0);

    const auto unknownConfig = VideoWriterPrivate::svtAv1PresetConfig(nullptr);
    QCOMPARE(unknownConfig.minimum, 0);
    QCOMPARE(unknownConfig.maximum, 13);
    QCOMPARE(unknownConfig.defaultValue, 8);
}

void TestVideoRecorder::svtAv1ParallelismSemantics_data()
{
    QTest::addColumn<QString>("version");
    QTest::addColumn<bool>("usesLevels");

    QTest::newRow("pre-2.3") << QStringLiteral("SVT-AV1 Encoder Lib v2.2.1") << false;
    QTest::newRow("2.3") << QStringLiteral("SVT-AV1 Encoder Lib v2.3.0") << true;
    QTest::newRow("3.0") << QStringLiteral("3.0.0") << true;
    QTest::newRow("future-major") << QStringLiteral("v4.0.0") << true;
    QTest::newRow("unknown") << QStringLiteral("unknown") << false;
}

void TestVideoRecorder::svtAv1ParallelismSemantics()
{
    QFETCH(QString, version);
    QFETCH(bool, usesLevels);

    const auto versionBytes = version.toLatin1();
    QCOMPARE(VideoWriterPrivate::svtAv1UsesParallelismLevels(versionBytes.constData()), usesLevels);
    QVERIFY(!VideoWriterPrivate::svtAv1UsesParallelismLevels(nullptr));
}

void TestVideoRecorder::svtAv1ParallelismLevel_data()
{
    QTest::addColumn<int>("threadBudget");
    QTest::addColumn<int>("parallelismLevel");

    QTest::newRow("disabled") << 0 << 0;
    QTest::newRow("one") << 1 << 1;
    QTest::newRow("two") << 2 << 2;
    QTest::newRow("three") << 3 << 3;
    QTest::newRow("five") << 5 << 3;
    QTest::newRow("six") << 6 << 4;
    QTest::newRow("eleven") << 11 << 4;
    QTest::newRow("twelve") << 12 << 5;
    QTest::newRow("twenty-three") << 23 << 5;
    QTest::newRow("twenty-four") << 24 << 6;
    QTest::newRow("large") << 128 << 6;
}

void TestVideoRecorder::svtAv1ParallelismLevel()
{
    QFETCH(int, threadBudget);
    QFETCH(int, parallelismLevel);

    QCOMPARE(VideoWriterPrivate::svtAv1ParallelismLevel(threadBudget), parallelismLevel);
}

void TestVideoRecorder::svtAv1ParameterComposition_data()
{
    QTest::addColumn<QString>("parameters");
    QTest::addColumn<QString>("parameter");
    QTest::addColumn<QString>("expected");

    QTest::newRow("first") << QString{} << QStringLiteral("lp=4") << QStringLiteral("lp=4");
    QTest::newRow("after-lossless") << QStringLiteral("lossless=1") << QStringLiteral("lp=4")
                                     << QStringLiteral("lossless=1:lp=4");
    QTest::newRow("preserve-existing") << QStringLiteral("fast-decode=1:lossless=1")
                                        << QStringLiteral("lp=5")
                                        << QStringLiteral("fast-decode=1:lossless=1:lp=5");
}

void TestVideoRecorder::svtAv1ParameterComposition()
{
    QFETCH(QString, parameters);
    QFETCH(QString, parameter);
    QFETCH(QString, expected);

    QCOMPARE(
        VideoWriterPrivate::appendSvtAv1Parameter(parameters.toStdString(), parameter.toStdString()),
        expected.toStdString());
}

void TestVideoRecorder::codecPresetProperties()
{
    for (const auto codec : {VideoCodec::Raw, VideoCodec::FFV1, VideoCodec::FFVHuff, VideoCodec::MPEG4}) {
        const CodecProperties props(codec);
        QVERIFY(!props.hasPreset());
    }

    CodecProperties av1Props(VideoCodec::AV1);
    if (avcodec_find_encoder_by_name("libsvtav1") == nullptr) {
        QVERIFY(!av1Props.hasPreset());
    } else {
        QVERIFY(av1Props.hasPreset());
        QCOMPARE(av1Props.presetMin(), 0);
        QVERIFY(av1Props.presetMax() > av1Props.presetMin());
    }
    if (av1Props.hasPreset()) {
        QVERIFY(av1Props.preset() >= av1Props.presetMin());
        QVERIFY(av1Props.preset() <= av1Props.presetMax());

        av1Props.setPreset(av1Props.presetMax() + 1);
        QCOMPARE(av1Props.preset(), av1Props.presetMax());
        av1Props.setPreset(av1Props.presetMin() - 1);
        QCOMPARE(av1Props.preset(), av1Props.presetMin());

        const CodecProperties restored(av1Props.toVariant());
        QVERIFY(restored.hasPreset());
        QCOMPARE(restored.preset(), av1Props.preset());
    }

    CodecProperties vp9Props(VideoCodec::VP9);
    const auto *vp9Encoder = VideoWriterPrivate::findSoftwareEncoder(
        VideoCodec::VP9, avcodec_find_encoder_by_name, avcodec_find_encoder);
    QCOMPARE(vp9Props.hasPreset(), VideoWriterPrivate::encoderSupportsPreset(VideoCodec::VP9, vp9Encoder));
    if (vp9Props.hasPreset()) {
        QCOMPARE(vp9Props.presetMin(), 0);
        QVERIFY(vp9Props.presetMax() >= 6);
        QCOMPARE(vp9Props.preset(), 6);
        QCOMPARE(vp9Props.presetDisplayName(), QStringLiteral("6"));
    }

    CodecProperties h264Props(VideoCodec::H264);
    const auto *h264Encoder = VideoWriterPrivate::findSoftwareEncoder(
        VideoCodec::H264, avcodec_find_encoder_by_name, avcodec_find_encoder);
    QCOMPARE(h264Props.hasPreset(), VideoWriterPrivate::encoderSupportsPreset(VideoCodec::H264, h264Encoder));
    if (h264Props.hasPreset()) {
        QCOMPARE(h264Props.presetMin(), 0);
        QCOMPARE(h264Props.presetMax(), 9);
        QCOMPARE(h264Props.presetDisplayName(), QStringLiteral("medium"));
        h264Props.setPreset(h264Props.presetMin());
        QCOMPARE(h264Props.presetDisplayName(), QStringLiteral("placebo"));
        h264Props.setPreset(h264Props.presetMax());
        QCOMPARE(h264Props.presetDisplayName(), QStringLiteral("ultrafast"));
    }

    CodecProperties hevcProps(VideoCodec::HEVC);
    const auto *hevcEncoder = VideoWriterPrivate::findSoftwareEncoder(
        VideoCodec::HEVC, avcodec_find_encoder_by_name, avcodec_find_encoder);
    QCOMPARE(hevcProps.hasPreset(), VideoWriterPrivate::encoderSupportsPreset(VideoCodec::HEVC, hevcEncoder));
    if (hevcProps.hasPreset()) {
        QCOMPARE(hevcProps.presetMin(), 0);
        QCOMPARE(hevcProps.presetMax(), 9);
        QCOMPARE(hevcProps.presetDisplayName(), QStringLiteral("veryfast"));
        hevcProps.setLossless(true);
        QCOMPARE(hevcProps.presetDisplayName(), QStringLiteral("medium"));
        hevcProps.setLossless(false);
        QCOMPARE(hevcProps.presetDisplayName(), QStringLiteral("veryfast"));

        auto legacyHevcVariant = hevcProps.toVariant();
        legacyHevcVariant.remove(QStringLiteral("preset"));
        CodecProperties legacyHevcProps(legacyHevcVariant);
        QCOMPARE(legacyHevcProps.presetDisplayName(), QStringLiteral("veryfast"));
        legacyHevcProps.setLossless(true);
        QCOMPARE(legacyHevcProps.presetDisplayName(), QStringLiteral("medium"));

        hevcProps.setPreset(hevcProps.presetMax());
        QCOMPARE(hevcProps.presetDisplayName(), QStringLiteral("ultrafast"));
        hevcProps.setLossless(true);
        QCOMPARE(hevcProps.presetDisplayName(), QStringLiteral("ultrafast"));

        const auto variant = hevcProps.toVariant();
        QCOMPARE(variant.value(QStringLiteral("preset")).toInt(), hevcProps.presetMax());
        const CodecProperties namedRestored(variant);
        QCOMPARE(namedRestored.preset(), hevcProps.presetMax());
        QCOMPARE(namedRestored.presetDisplayName(), QStringLiteral("ultrafast"));
    }
}

void TestVideoRecorder::presetEncode_data()
{
    QTest::addColumn<int>("codec");
    QTest::addColumn<QString>("encoderName");
    QTest::addColumn<bool>("lossless");

    QTest::newRow("av1-lossy") << static_cast<int>(VideoCodec::AV1) << QStringLiteral("libsvtav1") << false;
    QTest::newRow("vp9-lossy") << static_cast<int>(VideoCodec::VP9) << QStringLiteral("libvpx-vp9") << false;
    QTest::newRow("vp9-lossless") << static_cast<int>(VideoCodec::VP9) << QStringLiteral("libvpx-vp9") << true;
    QTest::newRow("h264-lossy") << static_cast<int>(VideoCodec::H264) << QStringLiteral("libx264") << false;
    QTest::newRow("h264-lossless") << static_cast<int>(VideoCodec::H264) << QStringLiteral("libx264") << true;
    QTest::newRow("hevc-lossy") << static_cast<int>(VideoCodec::HEVC) << QStringLiteral("libx265") << false;
    QTest::newRow("hevc-lossless") << static_cast<int>(VideoCodec::HEVC) << QStringLiteral("libx265") << true;
}

void TestVideoRecorder::presetEncode()
{
    QFETCH(int, codec);
    QFETCH(QString, encoderName);
    QFETCH(bool, lossless);

    if (avcodec_find_encoder_by_name(encoderName.toUtf8().constData()) == nullptr)
        QSKIP("requested software encoder is not available");

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    CodecProperties codecProps(static_cast<VideoCodec>(codec));
    codecProps.setLossless(lossless);
    codecProps.setPreset(codecProps.presetMax());
    if (codec == static_cast<int>(VideoCodec::AV1))
        codecProps.setThreadCount(12);

    VideoWriter writer;
    writer.setContainer(VideoContainer::Matroska);
    writer.setCodecProps(codecProps);
    writer.initialize(
        tempDir.filePath(QString::fromUtf8(QTest::currentDataTag())),
        "test",
        "source",
        Uuid{},
        "",
        96,
        64,
        30.0,
        CV_8U,
        false,
        false);
    QCOMPARE(writer.selectedEncoderName(), encoderName);

    const cv::Mat frame = makeTestFrame(96, 64, CV_8U, false);
    QVERIFY2(writer.encodeFrame(frame, std::chrono::microseconds{0}), writer.lastError().c_str());
    const auto finalizeResult = writer.finalize();
    QVERIFY2(finalizeResult.has_value(), finalizeResult ? "" : finalizeResult.error().c_str());
}

QTEST_GUILESS_MAIN(TestVideoRecorder)
#include "test-videorecorder.moc"

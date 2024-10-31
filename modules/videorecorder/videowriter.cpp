/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "videowriter.h"

#include <QDateTime>
#include <QFileInfo>
#include <atomic>
#include <fstream>
#include <iostream>
#include <queue>
#include <string.h>
#include <systemd/sd-device.h>
#include <thread>
#include <opencv2/opencv.hpp>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avconfig.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include "datactl/tsyncfile.h"

namespace Syntalos
{
Q_LOGGING_CATEGORY(logVRecorder, "mod.videorecorder")
}

VideoCodec stringToVideoCodec(const std::string &str)
{
    if (str == "Raw")
        return VideoCodec::Raw;
    if (str == "None")
        return VideoCodec::Raw;
    if (str == "FFV1")
        return VideoCodec::FFV1;
    if (str == "AV1")
        return VideoCodec::AV1;
    if (str == "VP9")
        return VideoCodec::VP9;
    if (str == "HEVC")
        return VideoCodec::HEVC;
    if (str == "H.264")
        return VideoCodec::H264;
    if (str == "MPEG-4")
        return VideoCodec::MPEG4;

    return VideoCodec::Unknown;
}

std::string videoCodecToString(VideoCodec codec)
{
    switch (codec) {
    case VideoCodec::Raw:
        return "None";
    case VideoCodec::FFV1:
        return "FFV1";
    case VideoCodec::AV1:
        return "AV1";
    case VideoCodec::VP9:
        return "VP9";
    case VideoCodec::H264:
        return "H.264";
    case VideoCodec::HEVC:
        return "HEVC";
    case VideoCodec::MPEG4:
        return "MPEG-4";
    default:
        return "Unknown";
    }
}

/**
 * @brief VideoCodec enum hash function
 */
inline uint qHash(const VideoCodec &key)
{
    return qHash((uint)key);
}

std::string videoContainerToString(VideoContainer container)
{
    switch (container) {
    case VideoContainer::Matroska:
        return "Matroska";
    case VideoContainer::AVI:
        return "AVI";
    default:
        return "Unknown";
    }
}

VideoContainer stringToVideoContainer(const std::string &str)
{
    if (str == "Matroska")
        return VideoContainer::Matroska;
    if (str == "AVI")
        return VideoContainer::AVI;

    return VideoContainer::Unknown;
}

static QString averrorToString(int err)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE + 16] = {0};
    av_strerror(AVERROR(err), errbuf, sizeof(errbuf));

    return QString::fromUtf8(errbuf);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
class CodecProperties::Private
{
public:
    VideoCodec codec;
    LosslessMode losslessMode;
    EncoderMode mode;
    bool lossless;

    int threadCount;
    bool canUseVaapi;
    bool useVaapi;
    QString renderNode;

    bool slicingAllowed;
    bool aviAllowed;

    int qualityMin;
    int qualityMax;
    int quality;

    int bitrate;
};
#pragma GCC diagnostic pop

CodecProperties::CodecProperties(VideoCodec codec)
    : d(new CodecProperties::Private())
{
    d->codec = codec;

    d->threadCount = 0;
    d->canUseVaapi = false;
    d->useVaapi = false;
    d->slicingAllowed = true;
    d->aviAllowed = false;
    d->mode = ConstantQuality;
    d->bitrate = 8000;
    d->quality = 0;
    d->qualityMin = 0;
    d->qualityMax = 0;
    d->renderNode = QStringLiteral("/dev/dri/renderD128");

    switch (codec) {
    case VideoCodec::Raw:
        d->losslessMode = Always;
        d->aviAllowed = true;
        d->lossless = true;

        break;

    case VideoCodec::FFV1:
        d->losslessMode = Always;
        d->lossless = true;

        break;

    case VideoCodec::AV1:
        d->losslessMode = Option;
        d->canUseVaapi = true;
        d->slicingAllowed = false; // codec needs init frames

        d->quality = 24;
        d->qualityMax = 0;
        d->qualityMin = 63;

        break;

    case VideoCodec::VP9:
        d->losslessMode = Option;
        d->canUseVaapi = true;
        d->slicingAllowed = false; // codec needs init frames

        d->quality = 24;
        d->qualityMax = 0;
        d->qualityMin = 63;
        d->bitrate = 128 * 1000;

        break;

    case VideoCodec::H264:
        d->losslessMode = Option;
        d->canUseVaapi = true;
        d->slicingAllowed = false; // codec needs init frames

        d->quality = 24;
        d->qualityMax = 0;
        d->qualityMin = 51;

        break;

    case VideoCodec::HEVC:
        d->losslessMode = Option;
        d->canUseVaapi = true;
        d->slicingAllowed = false; // codec needs init frames

        d->quality = 24;
        d->qualityMax = 0;
        d->qualityMin = 51;

        break;

    case VideoCodec::MPEG4:
        d->losslessMode = Never;
        d->aviAllowed = true;

        d->quality = 3;
        d->qualityMax = 0;
        d->qualityMin = 31;

        break;

    default:
        throw std::runtime_error(QStringLiteral("No properties found for codec: %1")
                                     .arg(QString::fromStdString(videoCodecToString(codec)))
                                     .toStdString());
    }
}

CodecProperties::CodecProperties(const QVariantHash &v)
    : CodecProperties(qvariant_cast<VideoCodec>(v["codec"]))
{
    Q_ASSERT(d->codec == static_cast<VideoCodec>(v["codec"].toInt()));

    setBitrateKbps(v["bitrate"].toInt());
    setLossless(v["lossless"].toBool());
    setUseVaapi(v["use-vaapi"].toBool());
    setMode(static_cast<EncoderMode>(v["mode"].toInt()));
    setQuality(v["quality"].toInt());
    d->renderNode = v.value("render-node", QString()).toString();
}

QVariantHash CodecProperties::toVariant() const
{
    QVariantHash v;

    v["bitrate"] = QVariant::fromValue(d->bitrate);
    v["codec"] = QVariant::fromValue(static_cast<int>(d->codec));
    v["lossless"] = QVariant::fromValue(d->lossless);
    v["use-vaapi"] = QVariant::fromValue(d->useVaapi);
    v["mode"] = QVariant::fromValue(static_cast<int>(d->mode));
    v["quality"] = QVariant::fromValue(d->quality);
    if (d->useVaapi)
        v["render-node"] = QVariant::fromValue(d->renderNode);

    return v;
}

CodecProperties::~CodecProperties() {}

CodecProperties::CodecProperties(const CodecProperties &rhs)
    : d(new CodecProperties::Private(*rhs.d))
{
}

CodecProperties &CodecProperties::operator=(const CodecProperties &rhs)
{
    if (this != &rhs)
        d.reset(new CodecProperties::Private(*rhs.d));
    return *this;
}

QString CodecProperties::modeToString(CodecProperties::EncoderMode mode)
{
    switch (mode) {
    case ConstantQuality:
        return QStringLiteral("constant-quality");
    case ConstantBitrate:
        return QStringLiteral("constant-bitrate");
    default:
        return QStringLiteral("unknown");
    }
}

CodecProperties::EncoderMode CodecProperties::stringToMode(const QString &str)
{
    if (str == QStringLiteral("constant-quality"))
        return ConstantQuality;
    if (str == QStringLiteral("constant-bitrate"))
        return ConstantBitrate;
    return None;
}

VideoCodec CodecProperties::codec() const
{
    return d->codec;
}

CodecProperties::LosslessMode CodecProperties::losslessMode() const
{
    return d->losslessMode;
}

bool CodecProperties::isLossless() const
{
    return d->lossless;
}

void CodecProperties::setLossless(bool enabled)
{
    d->lossless = enabled;
}

bool CodecProperties::canUseVaapi() const
{
    return d->canUseVaapi;
}

bool CodecProperties::useVaapi() const
{
    return d->useVaapi;
}

void CodecProperties::setUseVaapi(bool enabled)
{
    if (canUseVaapi())
        d->useVaapi = enabled;
}

void CodecProperties::setRenderNode(const QString &node)
{
    d->renderNode = node;
}

QString CodecProperties::renderNode() const
{
    return d->renderNode;
}

int CodecProperties::threadCount() const
{
    return d->threadCount;
}

void CodecProperties::setThreadCount(int n)
{
    d->threadCount = n;
}

bool CodecProperties::allowsSlicing() const
{
    return d->slicingAllowed;
}

bool CodecProperties::allowsAviContainer() const
{
    return d->aviAllowed;
}

CodecProperties::EncoderMode CodecProperties::mode() const
{
    return d->mode;
}

void CodecProperties::setMode(CodecProperties::EncoderMode mode)
{
    d->mode = mode;
}

int CodecProperties::qualityMin() const
{
    return d->qualityMin;
}

int CodecProperties::qualityMax() const
{
    return d->qualityMax;
}

int CodecProperties::quality() const
{
    return d->quality;
}

void CodecProperties::setQuality(int q)
{
    d->quality = q;
}

int CodecProperties::bitrateKbps() const
{
    return d->bitrate;
}

void CodecProperties::setBitrateKbps(int bitrate)
{
    d->bitrate = bitrate;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
class VideoWriter::Private
{
public:
    Private()
    {
        initialized = false;
        container = VideoContainer::Matroska;
        fileSliceIntervalMin = 0; // never slice our recording by default
        captureStartTimestamp = std::chrono::microseconds(
            0); // by default we assume the first frame was recorded at timepoint 0

        encFrame = nullptr;
        inputFrame = nullptr;
        alignedInput = nullptr;

        octx = nullptr;
        vstrm = nullptr;
        cctx = nullptr;
        swsctx = nullptr;
        encPixFormat = AV_PIX_FMT_YUV420P;

        hwDevCtx = nullptr;
        hwFrameCtx = nullptr;
        hwFrame = nullptr;

        selectedEncoderName = QStringLiteral("No encoder selected yet");
    }

    std::string lastError;

    QString modName;
    QUuid collectionId;
    QString videoTitle;
    QString recordingDate;
    QString fnameBase;
    uint fileSliceIntervalMin;
    uint currentSliceNo;
    CodecProperties codecProps;
    VideoContainer container;
    QString selectedEncoderName;

    bool initialized;
    int width;
    int height;
    AVRational fps;

    bool saveTimestamps;
    TimeSyncFileWriter tsfWriter;
    std::chrono::microseconds captureStartTimestamp;

    AVFrame *encFrame;
    AVFrame *inputFrame;
    int64_t framePts;
    uchar *alignedInput;
    size_t alignedInputSize;

    AVFormatContext *octx;
    AVStream *vstrm;
    AVCodecContext *cctx;
    SwsContext *swsctx;
    AVPixelFormat inputPixFormat;
    AVPixelFormat encPixFormat;

    size_t framesN;

    AVBufferRef *hwDevCtx;
    AVBufferRef *hwFrameCtx;
    AVFrame *hwFrame;
};
#pragma GCC diagnostic pop

VideoWriter::VideoWriter()
    : d(new VideoWriter::Private())
{
    d->initialized = false;

    // initialize codec properties
    CodecProperties cp(VideoCodec::FFV1);
    d->codecProps = cp;
}

VideoWriter::~VideoWriter()
{
    finalize();
}

/**
 * the following function is a modified version of code
 * found in ffmpeg-0.4.9-pre1/output_example.c
 */
static AVFrame *vw_alloc_frame(int pix_fmt, int width, int height, bool allocate)
{
    AVFrame *aframe;
    uint8_t *aframe_buf;

    aframe = av_frame_alloc();
    if (!aframe)
        return nullptr;

    aframe->format = pix_fmt;
    aframe->width = width;
    aframe->height = height;

    auto size = av_image_get_buffer_size(static_cast<AVPixelFormat>(pix_fmt), width, height, 1);
    if (allocate) {
        aframe_buf = static_cast<uint8_t *>(malloc(static_cast<size_t>(size)));
        if (!aframe_buf) {
            av_free(aframe);
            return nullptr;
        }
        av_image_fill_arrays(
            aframe->data, aframe->linesize, aframe_buf, static_cast<AVPixelFormat>(pix_fmt), width, height, 1);
    }

    return aframe;
}

void VideoWriter::initializeHWAccell()
{
    // DRI node for HW acceleration
    const auto hwDevice = d->codecProps.renderNode();

    int ret = av_hwdevice_ctx_create(
        &d->hwDevCtx, av_hwdevice_find_type_by_name("vaapi"), qPrintable(hwDevice), nullptr, 0);

    if (ret != 0)
        throw std::runtime_error(QStringLiteral("Failed to create hardware encoding device for %1: %2")
                                     .arg(hwDevice)
                                     .arg(ret)
                                     .toStdString());

    d->hwFrameCtx = av_hwframe_ctx_alloc(d->hwDevCtx);
    if (!d->hwFrameCtx) {
        av_buffer_unref(&d->hwDevCtx);
        throw std::runtime_error("Failed to initialize hw frame context");
    }

    auto cst = av_hwdevice_get_hwframe_constraints(d->hwDevCtx, nullptr);
    if (!cst) {
        av_buffer_unref(&d->hwDevCtx);
        throw std::runtime_error("Failed to get hwframe constraints");
    }

    auto ctx = (AVHWFramesContext *)d->hwFrameCtx->data;
    ctx->width = d->width;
    ctx->height = d->height;
    ctx->format = cst->valid_hw_formats[0];
    ctx->sw_format = AV_PIX_FMT_NV12;

    if ((ret = av_hwframe_ctx_init(d->hwFrameCtx))) {
        av_buffer_unref(&d->hwDevCtx);
        av_buffer_unref(&d->hwFrameCtx);
        throw std::runtime_error(QStringLiteral("Failed to initialize hwframe context: %1").arg(ret).toStdString());
    }
}

void VideoWriter::initializeInternal()
{
    // if file slicing is used, give our new file the appropriate name
    QString fname;
    if (d->fileSliceIntervalMin > 0)
        fname = QStringLiteral("%1_%2").arg(d->fnameBase).arg(d->currentSliceNo);
    else
        fname = d->fnameBase;

    // prepare timestamp filename
    auto timestampFname = fname + "_timestamps.tsync";

    // set container format
    switch (d->container) {
    case VideoContainer::Matroska:
        if (!fname.endsWith(".mkv"))
            fname = fname + ".mkv";
        break;
    case VideoContainer::AVI:
        if (!fname.endsWith(".avi"))
            fname = fname + ".avi";
        break;
    default:
        if (!fname.endsWith(".mkv"))
            fname = fname + ".mkv";
        break;
    }

    // open output format context
    int ret;
    d->octx = nullptr;
    ret = avformat_alloc_output_context2(&d->octx, nullptr, nullptr, qPrintable(fname));
    if (ret < 0)
        throw std::runtime_error(QStringLiteral("Failed to allocate output context: %1").arg(ret).toStdString());

    // open output IO context
    ret = avio_open2(&d->octx->pb, qPrintable(fname), AVIO_FLAG_WRITE, nullptr, nullptr);
    if (ret < 0) {
        finalizeInternal(false);
        throw std::runtime_error(QStringLiteral("Failed to open output I/O context: %1").arg(ret).toStdString());
    }

    auto codecId = AV_CODEC_ID_AV1;
    switch (d->codecProps.codec()) {
    case VideoCodec::Raw:
        codecId = AV_CODEC_ID_RAWVIDEO;
        break;
    case VideoCodec::FFV1:
        codecId = AV_CODEC_ID_FFV1;
        break;
    case VideoCodec::AV1:
        codecId = AV_CODEC_ID_AV1;
        break;
    case VideoCodec::VP9:
        codecId = AV_CODEC_ID_VP9;
        break;
    case VideoCodec::MPEG4:
        codecId = AV_CODEC_ID_MPEG4;
        break;
    case VideoCodec::H264:
        codecId = AV_CODEC_ID_H264;
        break;
    case VideoCodec::HEVC:
        codecId = AV_CODEC_ID_HEVC;
        break;
    default:
        codecId = AV_CODEC_ID_FFV1;
        break;
    }

    // sanity check to only try VAAPI codecs if we have whitelisted them
    if (d->codecProps.useVaapi()) {
        if (!d->codecProps.canUseVaapi())
            d->codecProps.setUseVaapi(false);
    }

    // initialize codec and context
    const AVCodec *vcodec = nullptr;
    if (d->codecProps.useVaapi()) {
        // we should try to use hardware acceleration
        if (d->codecProps.codec() == VideoCodec::VP9)
            vcodec = avcodec_find_encoder_by_name("vp9_vaapi");
        else if (d->codecProps.codec() == VideoCodec::AV1)
            vcodec = avcodec_find_encoder_by_name("av1_vaapi");
        else if (d->codecProps.codec() == VideoCodec::H264)
            vcodec = avcodec_find_encoder_by_name("h264_vaapi");
        else if (d->codecProps.codec() == VideoCodec::HEVC)
            vcodec = avcodec_find_encoder_by_name("hevc_vaapi");
        else
            throw std::runtime_error("Unable to find hardware-accelerated version of the selected codec.");

        if (vcodec == nullptr)
            throw std::runtime_error(QStringLiteral("Unable to find suitable hardware video encoder for codec %1. Your "
                                                    "accelerator may not support encoding with this codec.")
                                         .arg(videoCodecToString(d->codecProps.codec()).c_str())
                                         .toStdString());
    } else {
        // No hardware acceleration, select software encoder
        // We only use SVT-AV1 for AV1 encoding, because it is much faster and even
        // produced better quality images while encoding live (aom-av1 is not really
        // suitable for live encoding tasks)
        if (codecId == AV_CODEC_ID_AV1)
            vcodec = avcodec_find_encoder_by_name("libsvtav1");
        else
            vcodec = avcodec_find_encoder(codecId);
    }
    if (vcodec == nullptr)
        throw std::runtime_error(
            QStringLiteral("Unable to find suitable video encoder for codec %1. This codec may not have been enabled "
                           "at compile time or the system is missing the required encoder.")
                .arg(videoCodecToString(d->codecProps.codec()).c_str())
                .toStdString());

    if ((d->fps.num / d->fps.den) > 240 && QString::fromUtf8(vcodec->name) == "libsvtav1")
        throw std::runtime_error(
            QStringLiteral("Can not encode videos with a framerate higher than 240 FPS using the %1 encoder.")
                .arg(vcodec->name)
                .toStdString());

    d->cctx = avcodec_alloc_context3(vcodec);
    d->selectedEncoderName = QString::fromUtf8(vcodec->name);

    // create new video stream
    d->vstrm = avformat_new_stream(d->octx, vcodec);
    if (!d->vstrm)
        throw std::runtime_error("Failed to create new video stream.");
    avcodec_parameters_to_context(d->cctx, d->vstrm->codecpar);

    // set codec parameters
    d->cctx->codec_id = codecId;
    d->cctx->codec_type = AVMEDIA_TYPE_VIDEO;
    d->cctx->time_base = av_inv_q(d->fps);
    d->cctx->width = d->width;
    d->cctx->height = d->height;
    d->cctx->framerate = d->fps;
    d->cctx->workaround_bugs = FF_BUG_AUTODETECT;

    // select pixel format
#if LIBAVCODEC_VERSION_MAJOR >= 59
    const enum AVPixelFormat *fmts = nullptr;
    ret = avcodec_get_supported_config(d->cctx, nullptr, AV_CODEC_CONFIG_PIX_FORMAT, 0, (const void **)&fmts, nullptr);
    if (ret < 0 || fmts == nullptr) {
        qCWarning(logVRecorder).noquote().nospace()
            << "Failed to get supported pixel formats for codec " << vcodec->name << ": " << ret;
        d->encPixFormat = AV_PIX_FMT_YUV420P;
    } else {
        d->encPixFormat = fmts[0];
    }
#else
    d->encPixFormat = AV_PIX_FMT_YUV420P;
    if (vcodec->pix_fmts != nullptr)
        d->encPixFormat = vcodec->pix_fmts[0];
#endif

    // We must set time_base on the stream as well, otherwise it will be set to default values for some container
    // formats. See https://projects.blender.org/blender/blender/commit/b2e067d98ccf43657404b917b13ad5275f1c96e2 for
    // details.
    d->vstrm->time_base = d->cctx->time_base;

    if (d->codecProps.threadCount() > 0)
        d->cctx->thread_count = d->codecProps.threadCount() > 16 ? 16 : d->codecProps.threadCount();

    if (d->codecProps.codec() == VideoCodec::Raw) {
        d->encPixFormat = d->inputPixFormat == AV_PIX_FMT_GRAY8 || d->inputPixFormat == AV_PIX_FMT_GRAY16LE
                                  || d->inputPixFormat == AV_PIX_FMT_GRAY16BE
                              ? d->inputPixFormat
                              : AV_PIX_FMT_YUV420P;

        // MKV apparently doesn't handle 16-bit gray
        if (d->container == VideoContainer::Matroska
            && (d->encPixFormat == AV_PIX_FMT_GRAY16LE || d->encPixFormat == AV_PIX_FMT_GRAY16BE))
            d->encPixFormat = AV_PIX_FMT_GRAY8;
    }

    if (d->octx->oformat->flags & AVFMT_GLOBALHEADER)
        d->cctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // setup hardware acceleration, if requested
    if (d->codecProps.useVaapi()) {
        initializeHWAccell();
        d->cctx->hw_frames_ctx = av_buffer_ref(d->hwFrameCtx);
    }

    AVDictionary *codecopts = nullptr;

    // set bitrate/crf
    d->cctx->bit_rate = 0;
    av_dict_set_int(&codecopts, "crf", 0, 0);
    if (d->codecProps.mode() == CodecProperties::ConstantQuality)
        av_dict_set_int(&codecopts, "crf", d->codecProps.quality(), 0);
    else if (d->codecProps.mode() == CodecProperties::ConstantBitrate)
        d->cctx->bit_rate = d->codecProps.bitrateKbps() * 1000;

    if (d->codecProps.useVaapi()) {
        // some hardware-accelerated codecs use different options for some reason
        if (d->codecProps.codec() == VideoCodec::HEVC && d->codecProps.mode() == CodecProperties::ConstantQuality)
            av_dict_set_int(&codecopts, "qp", d->codecProps.quality(), 0);
    }

    d->cctx->gop_size = 100;
    if (d->codecProps.isLossless()) {
        // settings for lossless option

        switch (d->codecProps.codec()) {
        case VideoCodec::Raw:
            // uncompressed frames are always lossless
            break;
        case VideoCodec::AV1:
            av_dict_set_int(&codecopts, "crf", 0, 0);
            av_dict_set_int(&codecopts, "lossless", 1, 0);
            break;
        case VideoCodec::FFV1:
            // This codec is lossless by default
            break;
        case VideoCodec::VP9:
            av_dict_set_int(&codecopts, "lossless", 1, 0);
            break;
        case VideoCodec::H264:
        case VideoCodec::HEVC:
            d->cctx->gop_size = 32;
            av_dict_set_int(&codecopts, "crf", 0, 0);
            av_dict_set_int(&codecopts, "lossless", 1, 0);
            break;
        case VideoCodec::MPEG4:
            // NOTE: MPEG-4 has no lossless option
            std::cerr << "The MPEG-4 codec has no lossless preset, switching to lossy compression." << std::endl;
            d->codecProps.setLossless(false);
            break;
        default:
            break;
        }
    } else {
        // not lossless

        if (d->codecProps.codec() == VideoCodec::HEVC) {
            d->cctx->gop_size = 32;
            av_dict_set(&codecopts, "preset", "veryfast", 0);
        }
    }

    if (d->codecProps.codec() == VideoCodec::VP9) {
        // See https://developers.google.com/media/vp9/live-encoding
        // for more information on the settings.

        d->cctx->gop_size = 90;
        if (d->codecProps.mode() == CodecProperties::ConstantBitrate) {
            d->cctx->qmin = 4;
            d->cctx->qmax = 48;
            av_dict_set_int(&codecopts, "crf", 24, 0);
        }

        av_dict_set(&codecopts, "quality", "realtime", 0);
        av_dict_set(&codecopts, "deadline", "realtime", 0);
        av_dict_set_int(&codecopts, "speed", 6, 0);
        av_dict_set_int(&codecopts, "tile-columns", 3, 0);
        av_dict_set_int(&codecopts, "frame-parallel", 1, 0);
        av_dict_set_int(&codecopts, "static-thresh", 0, 0);
        av_dict_set_int(&codecopts, "max-intra-rate", 300, 0);
        av_dict_set_int(&codecopts, "lag-in-frames", 0, 0);
        av_dict_set_int(&codecopts, "row-mt", 1, 0);
        av_dict_set_int(&codecopts, "error-resilient", 1, 0);
    }

    if (d->codecProps.codec() == VideoCodec::FFV1) {
        d->codecProps.setLossless(true);               // this codec is always lossless
        d->cctx->level = 3;                            // Ensure we use FFV1 v3
        av_dict_set_int(&codecopts, "slicecrc", 1, 0); // Add CRC information to each slice
        av_dict_set_int(&codecopts, "slices", 24, 0);  // Use 24 slices
        av_dict_set_int(&codecopts, "coder", 1, 0);    // Range coder
        av_dict_set_int(&codecopts, "context", 1, 0);  // "large" context

        // NOTE: For archival use, GOP-size should be 1, but that also increases the file size quite a bit.
        // Keeping a good balance between recording space/performance/integrity is difficult sometimes.
        // av_dict_set_int(&codecopts, "g", 1, 0);
    }

    // Adjust pixel color formats for selected video codecs
    switch (d->codecProps.codec()) {
    case VideoCodec::FFV1:
        if (d->inputPixFormat == AV_PIX_FMT_GRAY8)
            d->encPixFormat = AV_PIX_FMT_GRAY8;
        if (d->inputPixFormat == AV_PIX_FMT_GRAY16LE)
            d->encPixFormat = AV_PIX_FMT_GRAY8;
        break;
    default:
        break;
    }

    // set pixel format to encoder pixel format, unless we are in
    // VAAPI mode, in which case VAAPI is the "format" we need
    if (d->hwDevCtx == nullptr) {
        d->cctx->pix_fmt = d->encPixFormat;
    } else {
        // the codec format has to be VAAPI
        d->cctx->pix_fmt = AV_PIX_FMT_VAAPI;
        // only yuv420p seems to reliably work with HW acceleration
        d->encPixFormat = AV_PIX_FMT_YUV420P;
    }

    // open video encoder
    ret = avcodec_open2(d->cctx, vcodec, &codecopts);
    if (ret < 0) {
        finalizeInternal(false);
        av_dict_free(&codecopts);
        throw std::runtime_error(
            QStringLiteral("Failed to open video encoder with the current parameters: %1").arg(ret).toStdString());
    }

    // stream codec parameters must be set after opening the encoder
    avcodec_parameters_from_context(d->vstrm->codecpar, d->cctx);
    d->vstrm->r_frame_rate = d->vstrm->avg_frame_rate = d->fps;

    // initialize sample scaler
    d->swsctx = sws_getCachedContext(
        nullptr,
        d->width,
        d->height,
        d->inputPixFormat,
        d->width,
        d->height,
        d->encPixFormat,
        SWS_BICUBIC,
        nullptr,
        nullptr,
        nullptr);

    if (!d->swsctx) {
        finalizeInternal(false);
        throw std::runtime_error("Failed to initialize sample scaler.");
    }

    // allocate frame buffer for encoding
    d->encFrame = vw_alloc_frame(d->encPixFormat, d->width, d->height, true);

    // allocate input buffer for color conversion
    d->inputFrame = vw_alloc_frame(d->inputPixFormat, d->width, d->height, false);

    if (d->hwDevCtx != nullptr) {
        // setup frame for hardware acceleration

        d->hwFrame = av_frame_alloc();
        auto frctx = (AVHWFramesContext *)d->hwFrameCtx->data;
        d->hwFrame->format = frctx->format;
        d->hwFrame->hw_frames_ctx = av_buffer_ref(d->hwFrameCtx);
        d->hwFrame->width = d->width;
        d->hwFrame->height = d->height;

        if (av_hwframe_get_buffer(d->hwFrameCtx, d->hwFrame, 0)) {
            finalizeInternal(false);
            throw std::runtime_error("Failed to retrieve HW frame buffer.");
        }
    }

    // set file metadata
    AVDictionary *metadataDict = nullptr;
    av_dict_set(&metadataDict, "title", qPrintable(d->videoTitle), 0);
    av_dict_set(&metadataDict, "collection_id", qPrintable(d->collectionId.toString(QUuid::WithoutBraces)), 0);
    av_dict_set(&metadataDict, "date_recorded", qPrintable(d->recordingDate), 0);
    d->octx->metadata = metadataDict;

    // write format header, after this we are ready to encode frames
    ret = avformat_write_header(d->octx, nullptr);
    if (ret < 0) {
        finalizeInternal(false);
        throw std::runtime_error(
            QStringLiteral("Failed to write format header: %1").arg(averrorToString(ret)).toStdString());
    }
    d->framePts = 0;

    if (d->saveTimestamps) {
        d->tsfWriter.close(); // ensure file is closed
        d->tsfWriter.setSyncMode(TSyncFileMode::CONTINUOUS);
        d->tsfWriter.setTimeNames(QStringLiteral("frame-no"), QStringLiteral("master-time"));
        d->tsfWriter.setTimeUnits(TSyncFileTimeUnit::INDEX, TSyncFileTimeUnit::MICROSECONDS);
        d->tsfWriter.setTimeDataTypes(TSyncFileDataType::UINT32, TSyncFileDataType::UINT64);
        d->tsfWriter.setChunkSize((d->fps.num / d->fps.den) * 60 * 1); // new chunk about every minute
        d->tsfWriter.setFileName(timestampFname);
        if (!d->tsfWriter.open(d->modName, d->collectionId)) {
            finalizeInternal(false);
            throw std::runtime_error(
                QStringLiteral("Unable to initialize timesync file: %1").arg(d->tsfWriter.lastError()).toStdString());
        }
    }

    d->initialized = true;
}

void VideoWriter::finalizeInternal(bool writeTrailer)
{
    if (d->initialized) {
        if (d->vstrm != nullptr) {
            avcodec_send_frame(d->cctx, nullptr);

            AVPacket *pkt = av_packet_alloc();
            if (!pkt)
                qCCritical(logVRecorder).noquote() << "Unable to allocate packet for flushing.";

            while (true) {
                auto ret = avcodec_receive_packet(d->cctx, pkt);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    qCCritical(logVRecorder).noquote()
                        << "Unable to receive packet during flush:" << averrorToString(ret);
                    break;
                }

                // rescale packet timestamp
                pkt->duration = 1;
                av_packet_rescale_ts(pkt, d->cctx->time_base, d->vstrm->time_base);

                // write packet
                ret = av_write_frame(d->octx, pkt);
                if (ret < 0) {
                    qCCritical(logVRecorder).noquote() << "Unable to write frame during flush:" << averrorToString(ret);
                    break;
                }

                av_packet_unref(pkt);
            }
        }

        // write trailer
        if (writeTrailer && (d->octx != nullptr))
            av_write_trailer(d->octx);
    }

    // ensure timestamps file is closed
    if (d->saveTimestamps)
        d->tsfWriter.close();

    // free all FFmpeg resources
    if (d->encFrame != nullptr) {
        av_frame_free(&d->encFrame);
        d->encFrame = nullptr;
    }
    if (d->inputFrame != nullptr) {
        av_frame_free(&d->inputFrame);
        d->inputFrame = nullptr;
    }
    if (d->hwFrame != nullptr) {
        av_frame_free(&d->hwFrame);
        d->hwFrame = nullptr;
    }

    if (d->hwDevCtx != nullptr)
        av_buffer_unref(&d->hwDevCtx);
    if (d->hwFrameCtx != nullptr)
        av_buffer_unref(&d->hwDevCtx);

    if (d->cctx != nullptr) {
        avcodec_free_context(&d->cctx);
        d->cctx = nullptr;
    }
    if (d->octx != nullptr) {
        if (d->octx->pb != nullptr)
            avio_close(d->octx->pb);
        avformat_free_context(d->octx);
        d->octx = nullptr;
    }

    if (d->alignedInput != nullptr) {
        av_freep(&d->alignedInput);
        d->alignedInputSize = 0;
    }

    d->initialized = false;
}

void VideoWriter::initialize(
    const QString &fname,
    const QString &modName,
    const QString &sourceModName,
    const QUuid &collectionId,
    const QString &subjectName,
    int width,
    int height,
    int fps,
    int imgDepth,
    bool hasColor,
    bool saveTimestamps)
{
    if (d->initialized)
        throw std::runtime_error("Tried to initialize an already initialized video writer.");

    d->width = width;
    d->height = height;
    d->fps = {fps, 1};
    d->alignedInputSize = 0;
    d->framesN = 0;
    d->saveTimestamps = saveTimestamps;
    d->currentSliceNo = 1;
    if (fname.midRef(fname.lastIndexOf('.') + 1).length() == 3)
        d->fnameBase = fname.left(fname.length() - 4); // remove 3-char suffix from filename
    else
        d->fnameBase = fname;

    // select FFmpeg pixel format of OpenCV matrices
    if (hasColor) {
        d->inputPixFormat = AV_PIX_FMT_BGR24;
    } else {
        if (imgDepth == CV_16U || imgDepth == CV_16S)
            d->inputPixFormat = AV_PIX_FMT_GRAY16LE;
        else
            d->inputPixFormat = AV_PIX_FMT_GRAY8;
    }

    d->modName = modName;
    d->collectionId = collectionId;

    const auto time = QDateTime::currentDateTime();
    d->recordingDate = time.date().toString("yyyy-MM-dd");

    auto subjectInfo = subjectName;
    if (subjectInfo.isEmpty()) {
        QFileInfo tmpFi(d->fnameBase);
        subjectInfo = QStringLiteral("Video ") + tmpFi.fileName();
    }
    if (sourceModName.isEmpty())
        d->videoTitle = QStringLiteral("%1 (%2 on %3)").arg(subjectInfo, modName, d->recordingDate);
    else
        d->videoTitle = QStringLiteral("%1 via %2 on %3").arg(subjectInfo, sourceModName, d->recordingDate);

    // initialize encoder
    initializeInternal();
}

void VideoWriter::finalize()
{
    finalizeInternal(true);
}

bool VideoWriter::initialized() const
{
    return d->initialized;
}

bool VideoWriter::startNewSection(const QString &fname)
{
    if (!d->initialized) {
        d->lastError = "Can not start a new slice if we are not initialized.";
        return false;
    }

    try {
        // finalize the current file
        finalizeInternal(true);

        // set new filrname for this section
        if (fname.midRef(fname.lastIndexOf('.') + 1).length() == 3)
            d->fnameBase = fname.left(fname.length() - 4); // remove 3-char suffix from filename
        else
            d->fnameBase = fname;

        // set slice number to one, since we are starting fresh
        d->currentSliceNo = 1;
        initializeInternal();
    } catch (const std::exception &e) {
        // propagate error and stop, we can not really recover from this
        d->lastError = e.what();
        return false;
    }

    return true;
}

std::chrono::microseconds VideoWriter::captureStartTimestamp() const
{
    return d->captureStartTimestamp;
}

void VideoWriter::setCaptureStartTimestamp(const std::chrono::microseconds &startTimestamp)
{
    d->captureStartTimestamp = startTimestamp;
}

void VideoWriter::setTsyncFileCreationTimeOverride(const QDateTime &dt)
{
    d->tsfWriter.setCreationTimeOverride(dt);
}

inline bool VideoWriter::prepareFrame(const cv::Mat &inImage)
{
    cv::Mat image;
    auto channels = inImage.channels();

    // Convert color formats around to match what was actually selected as
    // input pixel format
    if (d->inputPixFormat == AV_PIX_FMT_GRAY8) {
        if (channels != 1)
            cv::cvtColor(inImage, image, cv::COLOR_BGR2GRAY);
        else
            image = inImage;
    } else if (d->inputPixFormat == AV_PIX_FMT_BGR24) {
        if (channels == 4)
            cv::cvtColor(inImage, image, cv::COLOR_BGRA2BGR);
        else if (channels == 1)
            cv::cvtColor(inImage, image, cv::COLOR_GRAY2BGR);
        else
            image = inImage;
    } else {
        image = inImage;
    }

    auto step = image.step[0];
    auto data = image.ptr();
    channels = image.channels();

    const auto height = image.rows;
    const auto width = image.cols;

    // sanity checks
    if ((static_cast<int>(height) > d->height) || (static_cast<int>(width) > d->width))
        throw std::runtime_error(
            QStringLiteral("Received bigger frame than we expected for %1 (%2x%3 instead of %4x%5)")
                .arg(d->modName)
                .arg(width)
                .arg(height)
                .arg(d->width)
                .arg(d->height)
                .toStdString());
    if ((d->inputPixFormat == AV_PIX_FMT_RGB24) && (channels != 3)) {
        d->lastError = QStringLiteral("Expected RGB colored image, but received image has %1 channels")
                           .arg(channels)
                           .toStdString();
        return false;
    } else if ((d->inputPixFormat == AV_PIX_FMT_GRAY8) && (channels != 1)) {
        d->lastError =
            QStringLiteral("Expected grayscale image, but received image has %1 channels").arg(channels).toStdString();
        return false;
    }

    // FFmpeg contains SIMD optimizations which can sometimes read data past
    // the supplied input buffer. To ensure that doesn't happen, we pad the
    // step to a multiple of 32 (that's the minimal alignment for which Valgrind
    // doesn't raise any warnings).
    const size_t CV_STEP_ALIGNMENT = 32;
    const size_t CV_SIMD_SIZE = 32;
    const size_t CV_PAGE_MASK = ~(size_t)(4096 - 1);
    const unsigned char *dataend = data + ((size_t)height * step);
    if (step % CV_STEP_ALIGNMENT != 0
        || (((size_t)dataend - CV_SIMD_SIZE) & CV_PAGE_MASK) != (((size_t)dataend + CV_SIMD_SIZE) & CV_PAGE_MASK)) {
        auto alignedStep = (step + CV_STEP_ALIGNMENT - 1) & ~(CV_STEP_ALIGNMENT - 1);

        // reallocate alignment buffer if needed
        size_t newSize = (alignedStep * height + CV_SIMD_SIZE);
        if (d->alignedInput == nullptr || d->alignedInputSize < newSize) {
            if (d->alignedInput != nullptr)
                av_freep(&d->alignedInput);
            d->alignedInputSize = newSize;
            d->alignedInput = (unsigned char *)av_mallocz(d->alignedInputSize);
        }

        for (size_t y = 0; y < static_cast<size_t>(height); y++)
            memcpy(d->alignedInput + y * alignedStep, data + y * step, step);

        data = d->alignedInput;
        step = alignedStep;
    }

    // let input_picture point to the raw data buffer of 'image'
    av_image_fill_arrays(
        d->inputFrame->data,
        d->inputFrame->linesize,
        static_cast<const uint8_t *>(data),
        d->inputPixFormat,
        width,
        height,
        1);
    d->inputFrame->linesize[0] = static_cast<int>(step);

    // perform scaling and pixel format conversion
    // FIXME: If encPixFormat == inputPixFormat we should be able to skip this step,
    // but newer FFmpeg versions seem to crash in this case within avcodec_send_frame(),
    // so as a workaround we will always run sws_scale
    if (sws_scale(
            d->swsctx,
            d->inputFrame->data,
            d->inputFrame->linesize,
            0,
            height,
            d->encFrame->data,
            d->encFrame->linesize)
        < 0) {
        d->lastError = "Unable to scale image in pixel format conversion.";
        return false;
    }

    d->encFrame->pts = d->framePts++;
    return true;
}

bool VideoWriter::encodeFrame(const cv::Mat &frame, const std::chrono::microseconds &timestamp)
{
    int ret;
    bool success = false;
    bool havePacket = false;

    if (!prepareFrame(frame)) {
        std::cerr << "Unable to prepare frame. N: " << d->framesN + 1 << "(" << d->lastError << ")" << std::endl;
        return false;
    }

    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        d->lastError = QStringLiteral("Unable to allocate packet.").toStdString();
        return false;
    }

    AVBufferRef *savedBuf0 = nullptr;
    auto outputFrame = d->encFrame;

    const auto tsMsec = timestamp.count();

    if (d->hwDevCtx == nullptr) {
        // force FFmpeg to create a copy of the frame, if the codec needs it
        savedBuf0 = d->encFrame->buf[0];
        d->encFrame->buf[0] = nullptr;
    } else {
        // we are GPU accelerated! Copy frame to the GPU.
        if (av_hwframe_transfer_data(d->hwFrame, d->encFrame, 0)) {
            d->lastError = QStringLiteral("Failed to upload data to the GPU").toStdString();
            std::cerr << d->lastError << std::endl;
            goto out;
        }
        d->hwFrame->pts = d->encFrame->pts;
        outputFrame = d->hwFrame;
    }

    // encode video frame
    ret = avcodec_send_frame(d->cctx, outputFrame);
    if (ret < 0) {
        d->lastError = QStringLiteral("Unable to send frame to encoder. N: %1").arg(d->framesN + 1).toStdString();
        std::cerr << d->lastError << std::endl;
        goto out;
    }

    ret = avcodec_receive_packet(d->cctx, pkt);
    if (ret != 0) {
        if (ret == AVERROR(EAGAIN)) {
            // Some encoders need to be fed a few frames before they produce a packet, but the
            // frames are still saved. So we encounter for that fact.
            havePacket = false;
        } else {
            // we have a real error and can not continue
            d->lastError = QStringLiteral("Unable to send packet to codec: %1").arg(averrorToString(ret)).toStdString();
            std::cerr << d->lastError << std::endl;
            goto out;
        }
    } else {
        havePacket = true;
    }

    if (havePacket) {
        // rescale packet timestamp
        pkt->duration = 1;
        av_packet_rescale_ts(pkt, d->cctx->time_base, d->vstrm->time_base);

        // write packet
        av_write_frame(d->octx, pkt);
    }

    // store timestamp (if necessary)
    if (d->saveTimestamps)
        d->tsfWriter.writeTimes(d->framePts, tsMsec);

    if (d->fileSliceIntervalMin != 0) {
        const auto tsMin = static_cast<double>(tsMsec - d->captureStartTimestamp.count()) / 1000.0 / 60.0;
        if (tsMin >= (d->fileSliceIntervalMin * d->currentSliceNo)) {
            try {
                // we need to start a new file now since the maximum time for this file has elapsed,
                // so finalize this one
                finalizeInternal(true);

                // increment current slice number and attempt to reinitialize recording.
                d->currentSliceNo += 1;
                initializeInternal();
            } catch (const std::exception &e) {
                // propagate error and stop encoding thread, as we can not really recover from this
                d->lastError = e.what();
                goto out;
            }
        }
    }

    success = true;
out:
    av_packet_free(&pkt);

    // restore frame buffer, so that it can be properly freed in the end
    if (savedBuf0)
        d->encFrame->buf[0] = savedBuf0;

    return success;
}

CodecProperties VideoWriter::codecProps() const
{
    return d->codecProps;
}

void VideoWriter::setCodec(VideoCodec codec)
{
    if ((codec == VideoCodec::Unknown) || (codec == VideoCodec::Last))
        return;
    CodecProperties cp(codec);
    d->codecProps = cp;
}

void VideoWriter::setCodecProps(CodecProperties props)
{
    d->codecProps = props;
}

QString VideoWriter::selectedEncoderName() const
{
    return d->selectedEncoderName;
}

VideoContainer VideoWriter::container() const
{
    return d->container;
}

int VideoWriter::width() const
{
    return d->width;
}

int VideoWriter::height() const
{
    return d->height;
}

int VideoWriter::fps() const
{
    return d->fps.num;
}

uint VideoWriter::fileSliceInterval() const
{
    return d->fileSliceIntervalMin;
}

void VideoWriter::setFileSliceInterval(uint minutes)
{
    d->fileSliceIntervalMin = minutes;
}

std::string VideoWriter::lastError() const
{
    return d->lastError;
}

void VideoWriter::setContainer(VideoContainer container)
{
    d->container = container;
}

QMap<QString, QString> findVideoRenderNodes()
{
    __attribute__((cleanup(sd_device_enumerator_unrefp))) sd_device_enumerator *e = NULL;
    int r;

    QMap<QString, QString> renderNodes;
    r = sd_device_enumerator_new(&e);
    if (r < 0) {
        qCWarning(logVRecorder, "Unable to enumerate render devices: %s", strerror(r));
        return renderNodes;
    }

    r = sd_device_enumerator_allow_uninitialized(e);
    if (r < 0) {
        qCWarning(logVRecorder, "Failed to allow search for uninitialized devices: %s", strerror(r));
        return renderNodes;
    }

    r = sd_device_enumerator_add_match_subsystem(e, "drm", true);
    if (r < 0) {
        qCWarning(logVRecorder, "Failed to add DRM subsystem match: %s", strerror(r));
        return renderNodes;
    }
    r = sd_device_enumerator_add_match_property(e, "DEVTYPE", "drm_minor");
    if (r < 0) {
        qCWarning(logVRecorder, "Failed to add property match to find render nodes: %s", strerror(r));
        return renderNodes;
    }

    for (sd_device *dev = sd_device_enumerator_get_device_first(e); dev;
         dev = sd_device_enumerator_get_device_next(e)) {

        sd_device *p;
        const char *devnode = nullptr;
        const char *vendor_id = nullptr;
        const char *model_id = nullptr;

        r = sd_device_get_devname(dev, &devnode);
        if (r < 0) {
            qCWarning(logVRecorder, "Failed to read DRM device node: %s", strerror(r));
            continue;
        }

        if (strstr(devnode, "/dev/dri/render") == nullptr)
            continue;
        if (sd_device_get_parent(dev, &p) < 0)
            continue;

        sd_device_get_property_value(p, "ID_VENDOR_ID", &vendor_id);
        if (vendor_id == nullptr) {
            sd_device_get_property_value(p, "ID_VENDOR_FROM_DATABASE", &vendor_id);
            if (vendor_id == nullptr)
                sd_device_get_property_value(p, "DRIVER", &vendor_id);
        }

        sd_device_get_property_value(p, "ID_MODEL_ID", &model_id);
        if (model_id == nullptr) {
            sd_device_get_property_value(p, "ID_MODEL_FROM_DATABASE", &model_id);
            if (model_id == nullptr)
                model_id = devnode;
        }

        auto fullName = QStringLiteral("%1 - %2").arg(model_id, vendor_id);
        if (fullName.length() > 40)
            fullName = QString::fromUtf8(model_id);
        renderNodes.insert(QString::fromUtf8(devnode), fullName);
    }

    return renderNodes;
}

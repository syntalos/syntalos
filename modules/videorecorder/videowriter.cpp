/*
 * Copyright (C) 2019-2026 Matthias Klumpp <matthias@tenstral.net>
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
#include <algorithm>
#include <cmath>
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
#include <libavutil/cpu.h>
#include <libavutil/avconfig.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include "datactl/tsyncfile.h"
#include "ffmpeg-utils.h"

using namespace Syntalos;

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
    if (str == "FFVHuff")
        return VideoCodec::FFVHuff;

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
    case VideoCodec::FFVHuff:
        return "FFVHuff";
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

static AVCodecID vw_codec_id_for(VideoCodec codec)
{
    switch (codec) {
    case VideoCodec::Raw:
        return AV_CODEC_ID_RAWVIDEO;
    case VideoCodec::FFV1:
        return AV_CODEC_ID_FFV1;
    case VideoCodec::AV1:
        return AV_CODEC_ID_AV1;
    case VideoCodec::VP9:
        return AV_CODEC_ID_VP9;
    case VideoCodec::MPEG4:
        return AV_CODEC_ID_MPEG4;
    case VideoCodec::H264:
        return AV_CODEC_ID_H264;
    case VideoCodec::HEVC:
        return AV_CODEC_ID_HEVC;
    case VideoCodec::FFVHuff:
        return AV_CODEC_ID_FFVHUFF;
    default:
        return AV_CODEC_ID_FFV1;
    }
}

/**
 * Find the software encoder that we use for the given codec.
 *
 * We only use SVT-AV1 for AV1 encoding, because it is much faster and even
 * produced better quality images while encoding live (aom-av1 is not really
 * suitable for live encoding tasks).
 */
static const AVCodec *vw_find_sw_encoder(VideoCodec codec)
{
    const auto codecId = vw_codec_id_for(codec);
    if (codecId == AV_CODEC_ID_AV1)
        return avcodec_find_encoder_by_name("libsvtav1");
    return avcodec_find_encoder(codecId);
}

/**
 * Retrieve the list of pixel formats an encoder accepts, or NULL if it takes anything.
 */
static const enum AVPixelFormat *vw_encoder_pixfmts(const AVCodec *vcodec, const AVCodecContext *cctx)
{
    if (vcodec == nullptr)
        return nullptr;

    const enum AVPixelFormat *fmts = nullptr;
#if LIBAVCODEC_VERSION_INT > AV_VERSION_INT(61, 13, 100)
    if (avcodec_get_supported_config(cctx, vcodec, AV_CODEC_CONFIG_PIX_FORMAT, 0, (const void **)&fmts, nullptr) < 0)
        return nullptr;
#else
    fmts = vcodec->pix_fmts;
#endif
    return fmts;
}

/**
 * Check whether every value of @p from survives a conversion to @p to.
 */
static bool vw_pixfmt_conversion_is_lossless(AVPixelFormat from, AVPixelFormat to)
{
    if (from == to)
        return true;

    const auto *fromDesc = av_pix_fmt_desc_get(from);
    const auto *toDesc = av_pix_fmt_desc_get(to);
    if (fromDesc == nullptr || toDesc == nullptr)
        return false;

    // converting between the RGB and YUV color models always rounds
    const bool fromRgb = (fromDesc->flags & AV_PIX_FMT_FLAG_RGB) != 0;
    const bool toRgb = (toDesc->flags & AV_PIX_FMT_FLAG_RGB) != 0;
    if (fromRgb != toRgb)
        return false;

    // we must neither subsample any plane nor lose bit depth or components
    return toDesc->log2_chroma_w == fromDesc->log2_chroma_w && toDesc->log2_chroma_h == fromDesc->log2_chroma_h
           && toDesc->comp[0].depth >= fromDesc->comp[0].depth && toDesc->nb_components >= fromDesc->nb_components;
}

/**
 * Check whether an encoder accepts the given pixel format.
 * A NULL format list means the encoder takes anything (e.g. rawvideo).
 */
static bool vw_encoder_supports_pixfmt(const enum AVPixelFormat *fmts, AVPixelFormat fmt)
{
    if (fmts == nullptr)
        return true;
    for (int i = 0; fmts[i] != AV_PIX_FMT_NONE; i++) {
        if (fmts[i] == fmt)
            return true;
    }
    return false;
}

/**
 * Pick the best RGB pixel format an encoder accepts, so color frames can be stored
 * without a lossy conversion. Returns AV_PIX_FMT_NONE if the encoder has none.
 */
static AVPixelFormat vw_select_rgb_pixfmt(const enum AVPixelFormat *fmts)
{
    // ordered by preference: packed without an alpha plane first, then planar RGB
    static const AVPixelFormat candidates[] = {
        AV_PIX_FMT_BGR0,
        AV_PIX_FMT_GBRP,
        AV_PIX_FMT_BGR24,
        AV_PIX_FMT_BGRA,
    };

    for (const auto fmt : candidates) {
        if (vw_encoder_supports_pixfmt(fmts, fmt))
            return fmt;
    }
    return AV_PIX_FMT_NONE;
}

static std::string averrorToString(int err)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE + 16] = {0};
    av_strerror(err, errbuf, sizeof(errbuf));

    return std::string(errbuf);
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
    bool exactColors;

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
    d->lossless = false;
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

    case VideoCodec::FFVHuff:
        // very fast lossless codec, used as intermediate format for deferred encoding
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

    // by default we only pay the size and CPU cost of exact colors when the user asked
    // for a lossless recording anyway
    d->exactColors = d->lossless;
}

CodecProperties::CodecProperties(const QVariantHash &v)
    : CodecProperties(qvariant_cast<VideoCodec>(v["codec"]))
{
    Q_ASSERT(d->codec == static_cast<VideoCodec>(v["codec"].toInt()));

    setBitrateKbps(v["bitrate"].toInt());
    setLossless(v["lossless"].toBool());
    setExactColors(v.value("exact-colors", isLossless()).toBool());
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
    v["exact-colors"] = QVariant::fromValue(d->exactColors);
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
    // Whether a codec can be lossless is a property of the codec itself, so codecs that are
    // always (or never) lossless keep their fixed value no matter what we are asked for here.
    switch (d->losslessMode) {
    case Always:
        d->lossless = true;
        break;
    case Never:
        d->lossless = false;
        break;
    case Option:
        d->lossless = enabled;
        break;
    }
}

bool CodecProperties::exactColors() const
{
    return d->exactColors;
}

void CodecProperties::setExactColors(bool enabled)
{
    d->exactColors = enabled;
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

        selectedEncoderName = QStringLiteral("No encoder selected yet");
    }

    QuillLogger *log;
    std::string lastError;

    QString modName;
    Uuid collectionId;
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
};
#pragma GCC diagnostic pop

VideoWriter::VideoWriter()
    : d(new VideoWriter::Private())
{
    d->log = getLogger("videowriter");
    d->initialized = false;

    // initialize codec properties
    CodecProperties cp(VideoCodec::FFV1);
    d->codecProps = cp;
}

VideoWriter::~VideoWriter()
{
    // we can not report failures from here, but a botched finalization may leave an
    // unplayable video behind, so at least make some noise about it in the log
    const auto res = finalize();
    if (!res)
        LOG_WARNING(d->log, "Failed to finalize video on destruction: {}", res.error());
}

void VideoWriter::setLogger(QuillLogger *logger)
{
    d->log = logger;
}

/**
 * Allocate an AVFrame with the given properties.
 *
 * If allocate is true, also allocates the frame buffer and fill the data pointers.
 * If false, just allocates the AVFrame struct and set the properties.
 */
static AVFrame *vw_alloc_frame(AVPixelFormat pix_fmt, int width, int height, bool allocate)
{
    AVFrame *aframe;

    aframe = av_frame_alloc();
    if (aframe == nullptr)
        return nullptr;

    aframe->format = pix_fmt;
    aframe->width = width;
    aframe->height = height;

    if (allocate) {
        // allocate the image buffer
        const size_t align = ffmpeg_get_buffer_alignment();
        int size = av_image_get_buffer_size(pix_fmt, width, height, align);

        AVBufferRef *buf = av_buffer_alloc(size);
        if (buf == nullptr) {
            av_frame_free(&aframe);
            return nullptr;
        }

        auto ret = av_image_fill_arrays(aframe->data, aframe->linesize, buf->data, pix_fmt, width, height, align);
        if (ret < 0) {
            av_buffer_unref(&buf);
            av_frame_free(&aframe);
            return nullptr;
        }

        aframe->buf[0] = buf;
    }

    return aframe;
}

void VideoWriter::initializeHWAccell()
{
    // DRI node for HW acceleration
    const auto hwDevice = d->codecProps.renderNode();

    int ret = av_hwdevice_ctx_create(
        &d->hwDevCtx,
        av_hwdevice_find_type_by_name("vaapi"),
        qPrintable(hwDevice),
        nullptr,
        0);

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
    // Delayed hardware encoders may retain frame references after avcodec_send_frame().
    ctx->initial_pool_size = 16;

    if ((ret = av_hwframe_ctx_init(d->hwFrameCtx))) {
        av_hwframe_constraints_free(&cst);
        av_buffer_unref(&d->hwDevCtx);
        av_buffer_unref(&d->hwFrameCtx);
        throw std::runtime_error(QStringLiteral("Failed to initialize hwframe context: %1").arg(ret).toStdString());
    }

    av_hwframe_constraints_free(&cst);
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
        (void)finalizeInternal(false);
        throw std::runtime_error(QStringLiteral("Failed to open output I/O context: %1").arg(ret).toStdString());
    }

    auto codecId = AV_CODEC_ID_AV1;
    codecId = vw_codec_id_for(d->codecProps.codec());

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
            throw std::runtime_error(QStringLiteral(
                                         "Unable to find suitable hardware video encoder for codec %1. Your "
                                         "accelerator may not support encoding with this codec.")
                                         .arg(videoCodecToString(d->codecProps.codec()).c_str())
                                         .toStdString());
    } else {
        // no hardware acceleration, select software encoder
        vcodec = vw_find_sw_encoder(d->codecProps.codec());
    }
    if (vcodec == nullptr)
        throw std::runtime_error(
            QStringLiteral(
                "Unable to find suitable video encoder for codec %1. This codec may not have been enabled "
                "at compile time or the system is missing the required encoder.")
                .arg(videoCodecToString(d->codecProps.codec()).c_str())
                .toStdString());

    if (av_q2d(d->fps) > 240.0 && QString::fromUtf8(vcodec->name) == "libsvtav1")
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

    // select pixel format. A NULL list means the codec supports every pixel format
    // (e.g. rawvideo); use our default then - for Raw this is overridden further below
    // based on the input format.
    const enum AVPixelFormat *fmts = vw_encoder_pixfmts(vcodec, d->cctx);
    d->encPixFormat = (fmts == nullptr) ? AV_PIX_FMT_YUV420P : fmts[0];

    // We must set time_base on the stream as well, otherwise it will be set to default values for some container
    // formats. See https://projects.blender.org/blender/blender/commit/b2e067d98ccf43657404b917b13ad5275f1c96e2 for
    // details.
    d->vstrm->time_base = d->cctx->time_base;

    if (d->codecProps.threadCount() > 0)
        d->cctx->thread_count = d->codecProps.threadCount() > 16 ? 16 : d->codecProps.threadCount();

    if (d->codecProps.codec() == VideoCodec::Raw) {
        // this codec stores the frames as they came in, so keep their format
        d->encPixFormat = d->inputPixFormat;

        // halving the size of color frames is the whole point of not preserving them exactly
        if (d->encPixFormat == AV_PIX_FMT_BGR24 && !d->codecProps.exactColors())
            d->encPixFormat = AV_PIX_FMT_YUV420P;

        // Raw BGR24 and GRAY16 need explicit Matroska mappings; these are selected when
        // writing the container header below.
    }

    if (d->octx->oformat->flags & AVFMT_GLOBALHEADER)
        d->cctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // setup hardware acceleration, if requested
    if (d->codecProps.useVaapi()) {
        initializeHWAccell();
        d->cctx->hw_frames_ctx = av_buffer_ref(d->hwFrameCtx);
    }

    AVDictionary *codecopts = nullptr;
    const bool useVaapi = d->codecProps.useVaapi();
    bool wantSvtAv1Lossless = false;

    // normalize the lossless setting first, so all rate-control decisions below
    // act on what we are actually going to do
    if (d->codecProps.codec() == VideoCodec::FFV1 || d->codecProps.codec() == VideoCodec::FFVHuff
        || d->codecProps.codec() == VideoCodec::Raw) {
        // these codecs are always lossless
        d->codecProps.setLossless(true);
    } else if (d->codecProps.isLossless() && d->codecProps.losslessMode() == CodecProperties::Never) {
        LOG_WARNING(
            d->log,
            "The {} codec has no lossless preset, switching to lossy compression.",
            videoCodecToString(d->codecProps.codec()));
        d->codecProps.setLossless(false);
    }

    // None of the VA-API encoders we support is able to encode losslessly (their lowest
    // quantizer setting is still lossy). Refuse that combination instead of silently
    // recording lossy data for an experiment that asked for lossless.
    if (useVaapi && d->codecProps.isLossless()) {
        (void)finalizeInternal(false);
        throw std::runtime_error(
            std::format(
                "Lossless encoding is not available for the {} codec when VA-API hardware acceleration is used. "
                "Please disable either the lossless option or hardware acceleration, or select the FFV1 codec "
                "for lossless recordings.",
                videoCodecToString(d->codecProps.codec())));
    }

    // Set the bitrate/quality target. For lossless recordings this is skipped entirely,
    // the codec-specific settings further below pick the quantizer in that case.
    d->cctx->bit_rate = 0;
    if (!d->codecProps.isLossless()) {
        if (d->codecProps.mode() == CodecProperties::ConstantBitrate) {
            d->cctx->bit_rate = d->codecProps.bitrateKbps() * 1000;
        } else if (d->codecProps.mode() == CodecProperties::ConstantQuality) {
            // Some encoder wrappers interpret a quantizer of zero as "no quality requested"
            // and quietly substitute their own default, which is far worse than the best
            // quality the user just asked for. None of them can encode at a zero quantizer
            // anyway (that would be lossless, which has its own setting), so ask for the
            // next-best value instead of being ignored.
            const int quality = std::max(1, d->codecProps.quality());

            if (useVaapi) {
                // VA-API encoders have no CRF setting, they are set to a fixed quantizer via
                // their rate-control mode and the generic "global quality" value instead
                av_dict_set(&codecopts, "rc_mode", "CQP", 0);
                d->cctx->global_quality = quality;
            } else if (d->codecProps.codec() == VideoCodec::MPEG4) {
                // the MPEG-4 encoder is one of the old-style ones which only knows qscale
                d->cctx->flags |= AV_CODEC_FLAG_QSCALE;
                d->cctx->global_quality = quality * FF_QP2LAMBDA;
            } else {
                // all other software encoders that we use understand CRF
                av_dict_set_int(&codecopts, "crf", quality, 0);
            }
        }
    }

    d->cctx->gop_size = 100;
    if (d->codecProps.isLossless()) {
        // settings for lossless option

        switch (d->codecProps.codec()) {
        case VideoCodec::Raw:
            // uncompressed frames are always lossless
            break;
        case VideoCodec::FFV1:
        case VideoCodec::FFVHuff:
            // These codecs are lossless by default
            break;
        case VideoCodec::AV1:
            // SVT-AV1 has no "lossless" AVOption, and its lowest CRF value is *not* lossless.
            // Truly lossless coding has to be requested through the encoder's own parameter
            // list and requires SVT-AV1 >= 3.0
            av_dict_set(&codecopts, "svtav1-params", "lossless=1", 0);
            wantSvtAv1Lossless = true;
            break;
        case VideoCodec::VP9:
            av_dict_set_int(&codecopts, "lossless", 1, 0);
            break;
        case VideoCodec::H264:
            // x264 encodes losslessly at CRF 0
            d->cctx->gop_size = 32;
            av_dict_set_int(&codecopts, "crf", 0, 0);
            break;
        case VideoCodec::HEVC:
            // unlike x264, x265 is not lossless at CRF 0 and has to be told explicitly
            // via its own parameter list
            d->cctx->gop_size = 32;
            av_dict_set(&codecopts, "x265-params", "lossless=1", 0);
            break;
        default:
            break;
        }
    } else {
        // not lossless

        if (d->codecProps.codec() == VideoCodec::HEVC) {
            d->cctx->gop_size = 32;
            if (!useVaapi)
                av_dict_set(&codecopts, "preset", "veryfast", 0);
        }
    }

    if (d->codecProps.codec() == VideoCodec::VP9 && !useVaapi) {
        // See https://developers.google.com/media/vp9/live-encoding
        // for more information on the settings.

        d->cctx->gop_size = 90;
        if (!d->codecProps.isLossless() && d->codecProps.mode() == CodecProperties::ConstantBitrate) {
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
        d->cctx->level = 3;                            // Ensure we use FFV1 v3
        av_dict_set_int(&codecopts, "slicecrc", 1, 0); // Add CRC information to each slice
        av_dict_set_int(&codecopts, "slices", 24, 0);  // Use 24 slices
        av_dict_set_int(&codecopts, "coder", 1, 0);    // Range coder
        av_dict_set_int(&codecopts, "context", 1, 0);  // "large" context

        // NOTE: For archival use, GOP-size should be 1, but that also increases the file size quite a bit.
        // Keeping a good balance between recording space/performance/integrity is difficult sometimes.
        // av_dict_set_int(&codecopts, "g", 1, 0);
    }

    // NOTE: FFVHuff is intentionally left at its defaults. Its "context=1" option shrinks files
    // by roughly another 10%, but makes the Huffman tables adaptive and thereby disables frame
    // threading, which matters much more while we are recording.

    // Adjust pixel color formats for selected video codecs
    switch (d->codecProps.codec()) {
    case VideoCodec::FFV1:
    case VideoCodec::FFVHuff:
        if (d->inputPixFormat == AV_PIX_FMT_GRAY8)
            d->encPixFormat = AV_PIX_FMT_GRAY8;
        if (d->inputPixFormat == AV_PIX_FMT_GRAY16LE)
            d->encPixFormat = AV_PIX_FMT_GRAY16LE;
        break;
    default:
        break;
    }

    // If exact colors were requested, store color frames in an RGB format instead of letting
    // them be converted to YUV, which rounds every value and throws away half of the color
    // resolution in the subsampled chroma planes. The "Raw" codec is handled above, as it
    // simply keeps the input format and is restricted by the container instead.
    if (d->codecProps.exactColors() && d->codecProps.isLossless() && d->inputPixFormat == AV_PIX_FMT_BGR24
        && d->hwDevCtx == nullptr && d->codecProps.codec() != VideoCodec::Raw) {
        const auto rgbFmt = vw_select_rgb_pixfmt(fmts);
        if (rgbFmt != AV_PIX_FMT_NONE)
            d->encPixFormat = rgbFmt;
    }

    // If exact colors were requested, warn when the encoder still can not store the frames
    // in their original form. Converting color frames to YUV rounds, subsampled chroma planes
    // drop color detail, and a lower bit depth truncates.
    if (d->codecProps.exactColors() && d->codecProps.isLossless()
        && !vw_pixfmt_conversion_is_lossless(d->inputPixFormat, d->encPixFormat)) {
        const auto *inDesc = av_pix_fmt_desc_get(d->inputPixFormat);
        const auto *encDesc = av_pix_fmt_desc_get(d->encPixFormat);
        LOG_WARNING(
            d->log,
            "Lossless encoding with {} into {} has to convert the frames from {} to {}, which can not represent "
            "them exactly. Use the FFV1 codec in a Matroska container for bit-exact recordings.",
            d->selectedEncoderName.toStdString(),
            videoContainerToString(d->container),
            inDesc == nullptr ? "an unknown format" : inDesc->name,
            encDesc == nullptr ? "an unknown format" : encDesc->name);
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

    // Lossless recordings keep the full 0-255 value range of the source data instead of
    // the limited "TV" range that video codecs use by default, so flag that in the stream.
    if (d->codecProps.isLossless() && d->encPixFormat != d->inputPixFormat)
        d->cctx->color_range = AVCOL_RANGE_JPEG;

    // open video encoder
    ret = avcodec_open2(d->cctx, vcodec, &codecopts);
    if (ret < 0) {
        (void)finalizeInternal(false);
        av_dict_free(&codecopts);
        if (wantSvtAv1Lossless)
            throw std::runtime_error(
                std::format(
                    "Failed to open the AV1 video encoder for lossless recording: {}. Lossless AV1 encoding "
                    "requires SVT-AV1 3.0 or newer - if this system provides an older version, please disable "
                    "the lossless option or select the FFV1 codec for lossless recordings.",
                    averrorToString(ret)));
        throw std::runtime_error(
            std::format("Failed to open video encoder with the current parameters: {}", averrorToString(ret)));
    }

    // Any option left in the dictionary was not understood by the encoder and was silently
    // dropped. That is always a bug on our side, so make it visible instead of letting the
    // user believe a setting was applied.
    if (codecopts != nullptr) {
        std::string unusedOpts;
        const AVDictionaryEntry *entry = nullptr;
        while ((entry = av_dict_get(codecopts, "", entry, AV_DICT_IGNORE_SUFFIX)) != nullptr) {
            if (!unusedOpts.empty())
                unusedOpts += ", ";
            unusedOpts += std::format("{}={}", entry->key, entry->value);
        }
        LOG_WARNING(d->log, "Video encoder {} ignored codec option(s): {}", vcodec->name, unusedOpts);
    }
    av_dict_free(&codecopts);

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
        (void)finalizeInternal(false);
        throw std::runtime_error("Failed to initialize sample scaler.");
    }

    // If we convert between color spaces for a lossless recording, we must not let the
    // scaler compress our full-range input data into the limited "TV" range - that would
    // silently throw away information before the encoder ever gets to see it.
    // The encoder was already told to flag its output as full-range above.
    if (d->codecProps.isLossless() && d->encPixFormat != d->inputPixFormat) {
        int *invTable, *table;
        int srcRange, dstRange, brightness, contrast, saturation;

        if (sws_getColorspaceDetails(
                d->swsctx,
                &invTable,
                &srcRange,
                &table,
                &dstRange,
                &brightness,
                &contrast,
                &saturation)
            >= 0) {
            if (sws_setColorspaceDetails(d->swsctx, invTable, 1, table, 1, brightness, contrast, saturation) < 0)
                LOG_WARNING(
                    d->log,
                    "Unable to select full color range for lossless encoding: The recorded data may not be "
                    "bit-exact.");
        }
    }

    // allocate frame buffer for encoding
    d->encFrame = vw_alloc_frame(d->encPixFormat, d->width, d->height, true);

    // Old-style encoders like MPEG-4 take their fixed quantizer from the frame rather than
    // from the codec context. We reuse the same frame for every picture, so setting this once
    // is enough.
    if (d->cctx->flags & AV_CODEC_FLAG_QSCALE)
        d->encFrame->quality = d->cctx->global_quality;

    // allocate input buffer for color conversion
    d->inputFrame = vw_alloc_frame(d->inputPixFormat, d->width, d->height, false);

    // set file metadata
    AVDictionary *metadataDict = nullptr;
    av_dict_set(&metadataDict, "title", qPrintable(d->videoTitle), 0);
    av_dict_set(&metadataDict, "collection_id", d->collectionId.toHex().c_str(), 0);
    av_dict_set(&metadataDict, "date_recorded", qPrintable(d->recordingDate), 0);
    d->octx->metadata = metadataDict;

    // write format header, after this we are ready to encode frames
    const bool rawMatroska = d->container == VideoContainer::Matroska && d->codecProps.codec() == VideoCodec::Raw;
    const bool rawGray16 = rawMatroska
                           && (d->encPixFormat == AV_PIX_FMT_GRAY16LE || d->encPixFormat == AV_PIX_FMT_GRAY16BE);

    // Matroska has no native mapping for raw BGR24, so it has to be stored through the
    // VFW compatibility mode. FFmpeg logs a spurious "codec rawvideo is not supported by
    // this format" warning when it takes this path, but the frames are stored and read
    // back bit-exactly.
    AVDictionary *formatOpts = nullptr;
    if (rawMatroska && d->encPixFormat == AV_PIX_FMT_BGR24)
        av_dict_set(&formatOpts, "allow_raw_vfw", "1", 0);

    if (rawGray16) {
        // Matroska represents native raw video as V_UNCOMPRESSED and requires an
        // UncompressedFourCC to identify its pixel packing. FFmpeg knows the GRAY16
        // FourCC, but generic mux initialization clears rawvideo tags before the
        // Matroska muxer sees them. Initialize first and restore the tag afterwards;
        // the VFW fallback is not suitable because it reads 16-bit gray as RGB555.
        ret = avformat_init_output(d->octx, nullptr);
        if (ret >= 0)
            d->vstrm->codecpar->codec_tag = avcodec_pix_fmt_to_codec_tag(d->encPixFormat);
        if (ret >= 0 && d->vstrm->codecpar->codec_tag == 0)
            ret = AVERROR(EINVAL);
        if (ret >= 0)
            ret = avformat_write_header(d->octx, nullptr);
    } else {
        ret = avformat_write_header(d->octx, &formatOpts);
    }
    av_dict_free(&formatOpts);
    if (ret < 0) {
        (void)finalizeInternal(false);
        throw std::runtime_error(std::format("Failed to write format header: {}", averrorToString(ret)));
    }
    d->framePts = 0;

    if (d->saveTimestamps) {
        d->tsfWriter.close(); // ensure file is closed
        d->tsfWriter.setSyncMode(TSyncFileMode::CONTINUOUS);
        d->tsfWriter.setTimeNames("frame-no", "master-time");
        d->tsfWriter.setTimeUnits(TSyncFileTimeUnit::INDEX, TSyncFileTimeUnit::MICROSECONDS);
        d->tsfWriter.setTimeDataTypes(TSyncFileDataType::UINT32, TSyncFileDataType::UINT64);
        d->tsfWriter.setChunkSize(std::lround(av_q2d(d->fps) * 60.0)); // new chunk about every minute
        d->tsfWriter.setFileName(timestampFname.toStdString());
        if (!d->tsfWriter.open(d->modName.toStdString(), d->collectionId)) {
            (void)finalizeInternal(false);
            throw std::runtime_error(std::format("Unable to initialize timesync file: {}", d->tsfWriter.lastError()));
        }
    }

    d->initialized = true;
}

std::expected<void, std::string> VideoWriter::finalizeInternal(bool writeTrailer)
{
    // Records the first error that makes the on-disk file untrustworthy (truncated/
    // corrupt). We keep tearing down all resources regardless, but report this error
    // so callers are aware of the broken file.
    std::optional<std::string> finalizeError;

    if (d->initialized) {
        AVPacket *pkt = av_packet_alloc();
        if (pkt == nullptr) {
            LOG_CRITICAL(d->log, "Unable to allocate packet for flushing.");
            if (!finalizeError)
                finalizeError = "Unable to allocate packet for flushing the encoder.";
        }

        if (d->vstrm != nullptr && pkt != nullptr) {
            avcodec_send_frame(d->cctx, nullptr);

            while (true) {
                auto ret = avcodec_receive_packet(d->cctx, pkt);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    LOG_CRITICAL(d->log, "Unable to receive packet during flush: {}", averrorToString(ret));
                    if (!finalizeError)
                        finalizeError = std::format("Unable to receive packet during flush: {}", averrorToString(ret));
                    break;
                }

                // rescale packet timestamp
                pkt->duration = 1;
                av_packet_rescale_ts(pkt, d->cctx->time_base, d->vstrm->time_base);

                // write packet
                ret = av_write_frame(d->octx, pkt);
                if (ret < 0) {
                    LOG_CRITICAL(d->log, "Unable to write frame during flush: {}", averrorToString(ret));
                    if (!finalizeError)
                        finalizeError = std::format("Unable to write frame during flush: {}", averrorToString(ret));
                    break;
                }

                av_packet_unref(pkt);
            }
        }
        av_packet_free(&pkt);

        // write trailer
        if (writeTrailer && (d->octx != nullptr)) {
            const auto ret = av_write_trailer(d->octx);
            if (ret < 0) {
                LOG_CRITICAL(d->log, "Unable to write trailer while finalizing video: {}", averrorToString(ret));
                if (!finalizeError)
                    finalizeError = std::format(
                        "Unable to write trailer while finalizing video: {}",
                        averrorToString(ret));
            }
        }
    }

    // ensure timestamps file is closed and all its data has been written
    if (d->saveTimestamps) {
        if (!d->tsfWriter.close()) {
            LOG_CRITICAL(d->log, "Unable to finalize timesync file: {}", d->tsfWriter.lastError());
            if (!finalizeError)
                finalizeError = std::format("Unable to finalize timesync file: {}", d->tsfWriter.lastError());
        }
    }

    // free all FFmpeg resources
    if (d->encFrame != nullptr) {
        av_frame_free(&d->encFrame);
        d->encFrame = nullptr;
    }
    if (d->inputFrame != nullptr) {
        av_frame_free(&d->inputFrame);
        d->inputFrame = nullptr;
    }

    if (d->hwDevCtx != nullptr)
        av_buffer_unref(&d->hwDevCtx);
    if (d->hwFrameCtx != nullptr)
        av_buffer_unref(&d->hwFrameCtx);

    if (d->cctx != nullptr) {
        avcodec_free_context(&d->cctx);
        d->cctx = nullptr;
    }
    if (d->swsctx != nullptr) {
        sws_freeContext(d->swsctx);
        d->swsctx = nullptr;
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

    if (finalizeError)
        return std::unexpected(*finalizeError);
    return {};
}

void VideoWriter::initialize(
    const QString &fname,
    const QString &modName,
    const QString &sourceModName,
    const Uuid &collectionId,
    const QString &subjectName,
    int width,
    int height,
    double fps,
    int imgDepth,
    bool hasColor,
    bool saveTimestamps)
{
    if (d->initialized)
        throw std::runtime_error("Tried to initialize an already initialized video writer.");
    if (!std::isfinite(fps) || fps <= 0.0)
        throw std::runtime_error(QStringLiteral("Received invalid framerate: %1").arg(fps).toStdString());
    if (width < 64 || height < 64)
        throw std::runtime_error("Frame dimensions are to small: Need to be at least 64x64px for most decoders.");
    if ((width % 2) != 0 || (height % 2) != 0)
        throw std::runtime_error(QStringLiteral(
                                     "Received odd frame dimensions: %1x%2px. Most video codecs subsample the color "
                                     "planes by two and can not encode frames with an odd width or height, so please "
                                     "adjust the frame size of the data source to be even in both dimensions.")
                                     .arg(width)
                                     .arg(height)
                                     .toStdString());

    d->width = width;
    d->height = height;
    d->fps = av_d2q(fps, INT_MAX);
    if (d->fps.num <= 0 || d->fps.den <= 0)
        throw std::runtime_error(
            QStringLiteral("Unable to convert framerate %1 to rational value.").arg(fps).toStdString());
    d->alignedInputSize = 0;
    d->framesN = 0;
    d->saveTimestamps = saveTimestamps;
    d->currentSliceNo = 1;
    if (QStringView{fname}.mid(fname.lastIndexOf('.') + 1).length() == 3)
        d->fnameBase = fname.left(fname.length() - 4); // remove 3-char suffix from filename
    else
        d->fnameBase = fname;

    // select FFmpeg pixel format of OpenCV matrices
    if (hasColor) {
        d->inputPixFormat = AV_PIX_FMT_BGR24;
    } else {
        if (imgDepth == CV_16U || imgDepth == CV_16S) {
            d->inputPixFormat = AV_PIX_FMT_GRAY16LE;
            if (imgDepth == CV_16S)
                LOG_WARNING(
                    d->log,
                    "Signed 16-bit grayscale input is stored as unsigned GRAY16. Negative samples will be "
                    "interpreted as large positive intensities when the video is decoded; use CV_16U input "
                    "when numeric values must be preserved.");
        } else {
            d->inputPixFormat = AV_PIX_FMT_GRAY8;
        }
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

std::expected<void, std::string> VideoWriter::finalize()
{
    return finalizeInternal(true);
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
        const auto res = finalizeInternal(true);
        if (!res) {
            d->lastError = res.error();
            return false;
        }

        // set new filrname for this section
        if (QStringView{fname}.mid(fname.lastIndexOf('.') + 1).length() == 3)
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

void VideoWriter::setTsyncFileCreationTimeOverride(const EdlDateTime &dt)
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
        d->lastError = QStringLiteral("Expected grayscale image, but received image has %1 channels")
                           .arg(channels)
                           .toStdString();
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

    if (!prepareFrame(frame)) {
        LOG_ERROR(d->log, "Unable to prepare frame. N: {} ({})", d->framesN + 1, d->lastError);
        return false;
    }

    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        d->lastError = QStringLiteral("Unable to allocate packet.").toStdString();
        return false;
    }

    AVBufferRef *savedBuf0 = nullptr;
    AVFrame *hwFrame = nullptr;
    auto outputFrame = d->encFrame;

    const auto tsUsec = timestamp.count();

    int drainedPackets = 0;
    auto receivePackets = [&]() -> bool {
        while (true) {
            ret = avcodec_receive_packet(d->cctx, pkt);
            if (ret == AVERROR(EAGAIN))
                return true;
            if (ret < 0) {
                d->lastError = std::format("Unable to receive packet from encoder: {}", averrorToString(ret));
                LOG_ERROR(d->log, "{}", d->lastError);
                return false;
            }

            // rescale packet timestamp
            pkt->duration = 1;
            av_packet_rescale_ts(pkt, d->cctx->time_base, d->vstrm->time_base);

            // write packet
            ret = av_write_frame(d->octx, pkt);
            if (ret < 0) {
                d->lastError = std::format("Unable to write frame packet to output: {}", averrorToString(ret));
                LOG_ERROR(d->log, "{}", d->lastError);
                return false;
            }

            drainedPackets++;
            av_packet_unref(pkt);
        }
    };

    if (d->hwDevCtx == nullptr) {
        // force FFmpeg to create a copy of the frame, if the codec needs it
        savedBuf0 = d->encFrame->buf[0];
        d->encFrame->buf[0] = nullptr;
    } else {
        // we are GPU accelerated! Copy frame to the GPU.
        hwFrame = av_frame_alloc();
        if (hwFrame == nullptr) {
            d->lastError = QStringLiteral("Unable to allocate VAAPI video frame.").toStdString();
            LOG_ERROR(d->log, "{}", d->lastError);
            goto out;
        }

        ret = av_hwframe_get_buffer(d->hwFrameCtx, hwFrame, 0);
        if (ret < 0) {
            d->lastError = std::format("Failed to retrieve VAAPI frame buffer: {}", averrorToString(ret));
            LOG_ERROR(d->log, "{}", d->lastError);
            goto out;
        }

        ret = av_hwframe_transfer_data(hwFrame, d->encFrame, 0);
        if (ret < 0) {
            d->lastError = std::format("Failed to upload data to the GPU: {}", averrorToString(ret));
            LOG_ERROR(d->log, "{}", d->lastError);
            goto out;
        }
        hwFrame->pts = d->encFrame->pts;
        outputFrame = hwFrame;
    }

    // encode video frame
    while (true) {
        ret = avcodec_send_frame(d->cctx, outputFrame);
        if (ret == AVERROR(EAGAIN)) {
            // the encoder wants us to make room first, so drain its output queue and retry
            drainedPackets = 0;
            if (!receivePackets())
                goto out;
            if (drainedPackets == 0) {
                // this must not happen per the FFmpeg send/receive contract - bail out
                // instead of spinning here forever
                d->lastError = std::format(
                    "Encoder refused frame {}, but did not emit a packet to make room for it.",
                    d->framesN + 1);
                LOG_ERROR(d->log, "{}", d->lastError);
                goto out;
            }
            continue;
        }
        if (ret < 0) {
            d->lastError = std::format("Unable to send frame {} to encoder: {}.", d->framesN + 1, averrorToString(ret));
            LOG_ERROR(d->log, "{}", d->lastError);
            goto out;
        }
        break;
    }
    if (!receivePackets())
        goto out;

    // Give the frame its buffer back now that the encoder is done with it: a slice rollover
    // below frees d->encFrame and allocates a new one, and restoring the old buffer onto that
    // new frame at "out" would leak the new frame's buffer.
    if (savedBuf0 != nullptr) {
        d->encFrame->buf[0] = savedBuf0;
        savedBuf0 = nullptr;
    }

    // store timestamp (if necessary)
    if (d->saveTimestamps) {
        // framePts - 1 is used because the counter has already advanced to the next index
        // at this point, so we need to go back by one
        d->tsfWriter.writeTimes(d->framePts - 1, tsUsec);
        if (d->tsfWriter.hasError()) {
            d->lastError = std::format("Unable to write timestamp: {}", d->tsfWriter.lastError());
            LOG_ERROR(d->log, "{}", d->lastError);
            goto out;
        }
    }

    if (d->fileSliceIntervalMin != 0) {
        const auto tsMin = static_cast<double>(tsUsec - d->captureStartTimestamp.count()) / US_PER_MIN;
        if (tsMin >= (d->fileSliceIntervalMin * d->currentSliceNo)) {
            try {
                // we need to start a new file now since the maximum time for this file has elapsed,
                // so finalize this one
                const auto res = finalizeInternal(true);
                if (!res) {
                    d->lastError = res.error();
                    goto out;
                }

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
    av_frame_free(&hwFrame);

    // restore frame buffer, so that it can be properly freed in the end
    if (savedBuf0) {
        if (d->encFrame != nullptr) {
            d->encFrame->buf[0] = savedBuf0;
        } else {
            // the frame was already freed (e.g. a failed slice-boundary finalize jumped here);
            // release the buffer ourselves to avoid leaking it, since av_frame_free() skipped it
            // (buf[0] had been nulled out above).
            av_buffer_unref(&savedBuf0);
        }
    }

    return success;
}

bool VideoWriter::hasExactColors() const
{
    // a lossy encoder quantizes the frames no matter which pixel format they are stored in
    return d->codecProps.isLossless() && vw_pixfmt_conversion_is_lossless(d->inputPixFormat, d->encPixFormat);
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

double VideoWriter::fps() const
{
    return av_q2d(d->fps);
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

bool videoCodecCanStoreExactColors(VideoCodec codec, VideoContainer container)
{
    // "Raw" simply stores the frames as they arrive. Matroska uses VFW compatibility mode.
    if (codec == VideoCodec::Raw)
        return container == VideoContainer::Matroska || container == VideoContainer::AVI;

    const auto *vcodec = vw_find_sw_encoder(codec);
    if (vcodec == nullptr)
        return false;

    return vw_select_rgb_pixfmt(vw_encoder_pixfmts(vcodec, nullptr)) != AV_PIX_FMT_NONE;
}

QMap<QString, QString> findVideoRenderNodes()
{
    __attribute__((cleanup(sd_device_enumerator_unrefp))) sd_device_enumerator *e = NULL;
    int r;

    auto log = getLogger("videowriter");

    QMap<QString, QString> renderNodes;
    r = sd_device_enumerator_new(&e);
    if (r < 0) {
        LOG_WARNING(log, "Unable to enumerate render devices: {}", strerror(r));
        return renderNodes;
    }

    r = sd_device_enumerator_allow_uninitialized(e);
    if (r < 0) {
        LOG_WARNING(log, "Failed to allow search for uninitialized devices: {}", strerror(r));
        return renderNodes;
    }

    r = sd_device_enumerator_add_match_subsystem(e, "drm", true);
    if (r < 0) {
        LOG_WARNING(log, "Failed to add DRM subsystem match: {}", strerror(r));
        return renderNodes;
    }
    r = sd_device_enumerator_add_match_property(e, "DEVTYPE", "drm_minor");
    if (r < 0) {
        LOG_WARNING(log, "Failed to add property match to find render nodes: {}", strerror(r));
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
            LOG_WARNING(log, "Failed to read DRM device node: {}", strerror(r));
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

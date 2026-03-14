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

#include "videoreader.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include "../ffmpeg-utils.h"

static double r2d(AVRational r)
{
    return r.num == 0 || r.den == 0 ? 0. : (double)r.num / (double)r.den;
}

class VideoReader::Private
{
public:
    Private() {}

    QString lastError;

    AVFormatContext *formatCtx = nullptr;
    AVCodecContext *codecCtx = nullptr;
    int videoStreamIndex = -1;
    size_t frameIndex = 0;

    // cached swscale state
    SwsContext *swsCtx = nullptr;
    AVPixelFormat swsSrcFmt = AV_PIX_FMT_NONE;
    AVPixelFormat swsDstFmt = AV_PIX_FMT_NONE;
    int swsWidth = -1;
    int swsHeight = -1;
    uint8_t *swsDstData[4] = {nullptr};
    int swsDstLinesize[4] = {0};
};

VideoReader::VideoReader()
    : d(new VideoReader::Private())
{
}

VideoReader::~VideoReader()
{
    if (d->swsCtx)
        sws_freeContext(d->swsCtx);
    if (d->swsDstData[0])
        av_freep(&d->swsDstData[0]);
    if (d->codecCtx)
        avcodec_free_context(&d->codecCtx);
    if (d->formatCtx)
        avformat_close_input(&d->formatCtx);
}

QString VideoReader::lastError() const
{
    return d->lastError;
}

bool VideoReader::open(const QString &filename)
{
    d->frameIndex = 0;
    if (avformat_open_input(&d->formatCtx, qPrintable(filename), nullptr, nullptr) != 0) {
        d->lastError = "Could not open video file.";
        return false;
    }

    if (avformat_find_stream_info(d->formatCtx, nullptr) < 0) {
        d->lastError = "Couldn't find stream information.";
        avformat_close_input(&d->formatCtx);
        return false;
    }

    for (unsigned int i = 0; i < d->formatCtx->nb_streams; i++) {
        if (d->formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            d->videoStreamIndex = i;
            break;
        }
    }

    if (d->videoStreamIndex == -1) {
        d->lastError = "Didn't find a video stream.";
        avformat_close_input(&d->formatCtx);
        return false;
    }

    AVCodecParameters *codecParameters = d->formatCtx->streams[d->videoStreamIndex]->codecpar;
    auto codec = avcodec_find_decoder(codecParameters->codec_id);
    if (!codec) {
        d->lastError = "Unsupported codec.";
        goto failure;
    }

    d->codecCtx = avcodec_alloc_context3(codec);
    if (!d->codecCtx) {
        d->lastError = "Failed to allocate codec context.";
        goto failure;
    }

    if (avcodec_parameters_to_context(d->codecCtx, codecParameters) < 0) {
        d->lastError = "Failed to copy codec parameters to decoder context.";
        goto failure;
    }

    if (avcodec_open2(d->codecCtx, codec, nullptr) < 0) {
        d->lastError = "Failed to open codec.";
        goto failure;
    }

    return true;

failure:
    if (d->codecCtx != nullptr)
        avcodec_free_context(&d->codecCtx);
    d->codecCtx = nullptr;
    if (d->formatCtx != nullptr)
        avformat_close_input(&d->formatCtx);
    d->formatCtx = nullptr;

    return false;
}

double VideoReader::durationSec() const
{
    double sec = (double)d->formatCtx->duration / (double)AV_TIME_BASE;

    if (sec < 0.000025) {
        sec = (double)d->formatCtx->streams[d->videoStreamIndex]->duration
              * r2d(d->formatCtx->streams[d->videoStreamIndex]->time_base);
    }

    return sec;
}

ssize_t VideoReader::totalFrames() const
{
    if (d->videoStreamIndex == -1 || d->formatCtx == nullptr)
        return -1;

    ssize_t frameCount = -1;
    AVStream *videoStream = d->formatCtx->streams[d->videoStreamIndex];
    if (videoStream->nb_frames != AV_NOPTS_VALUE)
        frameCount = videoStream->nb_frames;

    if (frameCount <= 0) {
        // Estimate the number of frames based on the stream's duration and frame rate
        frameCount = static_cast<ssize_t>(floor(durationSec() * framerate() + 0.5));
        if (frameCount < 0)
            return -1;
    }

    return frameCount;
}

double VideoReader::framerate() const
{
    if (d->videoStreamIndex == -1 || d->formatCtx == nullptr)
        return -1;

    AVStream *videoStream = d->formatCtx->streams[d->videoStreamIndex];
    if (videoStream->avg_frame_rate.den != 0) {
        return av_q2d(videoStream->avg_frame_rate);
    } else {
        // The FPS is not known
        return -1;
    }
}

std::optional<std::pair<cv::Mat, size_t>> VideoReader::readFrame()
{
    AVFrame *frame = av_frame_alloc();
    AVPacket *packet = av_packet_alloc();
    while (av_read_frame(d->formatCtx, packet) >= 0) {
        if (packet->stream_index == d->videoStreamIndex) {
            if (avcodec_send_packet(d->codecCtx, packet) == 0) {
                if (avcodec_receive_frame(d->codecCtx, frame) == 0) {
                    auto img = frameToCVImage(frame);
                    av_packet_unref(packet);
                    av_frame_free(&frame);
                    av_packet_free(&packet);
                    if (!img.has_value()) {
                        d->lastError = "Failed to convert frame to OpenCV image.";
                        return std::nullopt;
                    }
                    return std::make_pair(img.value(), d->frameIndex++);
                }
            }
        }
        av_packet_unref(packet);
    }
    av_frame_free(&frame);
    av_packet_free(&packet);

    d->lastError = "Could not read frame.";
    return std::nullopt;
}

std::optional<cv::Mat> VideoReader::frameToCVImage(AVFrame *frame)
{
    if (frame == nullptr || frame->width <= 0 || frame->height <= 0) {
        d->lastError = "Invalid frame received from decoder.";
        return std::nullopt;
    }

    AVPixelFormat srcFormat = static_cast<AVPixelFormat>(frame->format);
    AVPixelFormat dstFormat;
    int cvMatType;

    // Determine if the source format is grayscale and set the destination format accordingly
    if (srcFormat == AV_PIX_FMT_GRAY8) {
        dstFormat = srcFormat;
        cvMatType = CV_8UC1;
    } else if (srcFormat == AV_PIX_FMT_GRAY16LE || srcFormat == AV_PIX_FMT_GRAY16BE) {
        dstFormat = srcFormat;
        cvMatType = CV_16UC1;
    } else {
        dstFormat = AV_PIX_FMT_BGR24;
        cvMatType = CV_8UC3;
    }

    if (sws_isSupportedInput(srcFormat) <= 0 || sws_isSupportedOutput(dstFormat) <= 0) {
        d->lastError = "Unsupported pixel format conversion requested.";
        return std::nullopt;
    }

    const int width = frame->width;
    const int height = frame->height;

    // Recreate the swscale context and destination buffer only when format or dimensions change.
    // For a normal video file this happens at most once.
    if (d->swsCtx == nullptr || d->swsSrcFmt != srcFormat || d->swsDstFmt != dstFormat || d->swsWidth != width
        || d->swsHeight != height) {

        if (d->swsCtx) {
            sws_freeContext(d->swsCtx);
            d->swsCtx = nullptr;
        }
        if (d->swsDstData[0])
            av_freep(&d->swsDstData[0]);

        d->swsCtx = sws_getContext(
            width, height, srcFormat, width, height, dstFormat, SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (d->swsCtx == nullptr) {
            d->lastError = "Could not initialize conversion context.";
            return std::nullopt;
        }

        const auto align = ffmpeg_get_buffer_alignment();
        if (av_image_alloc(d->swsDstData, d->swsDstLinesize, width, height, dstFormat, align) < 0) {
            sws_freeContext(d->swsCtx);
            d->swsCtx = nullptr;
            d->lastError = "Could not allocate conversion buffer.";
            return std::nullopt;
        }

        d->swsSrcFmt = srcFormat;
        d->swsDstFmt = dstFormat;
        d->swsWidth = width;
        d->swsHeight = height;
    }

    const auto scaledHeight = sws_scale(
        d->swsCtx, frame->data, frame->linesize, 0, height, d->swsDstData, d->swsDstLinesize);
    if (scaledHeight != height) {
        d->lastError = "Failed to scale the frame into the destination pixel format.";
        return std::nullopt;
    }

    // clone immediately so swsDstData is free for the next frame
    cv::Mat mat(height, width, cvMatType, d->swsDstData[0], d->swsDstLinesize[0]);
    return mat.clone();
}

ssize_t VideoReader::lastFrameIndex() const
{
    return d->frameIndex;
}

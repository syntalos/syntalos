/*
 * Copyright (C) 2019 Matthias Klumpp <matthias@tenstral.net>
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

#include <string.h>
#include <iostream>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <fstream>
#include <boost/format.hpp>
#include <boost/algorithm/string/predicate.hpp>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libavutil/avconfig.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

/**
 * @brief FRAME_QUEUE_MAX_COUNT
 * The maximum number of frames we want to hold in the queue in memory
 * before dropping frames.
 */
static const uint FRAME_QUEUE_MAX_COUNT = 512;

#pragma GCC diagnostic ignored "-Wpadded"
class VideoWriter::VideoWriterData
{
public:
    VideoWriterData()
        : thread(nullptr)
    {
        initialized = false;
        codec = VideoCodec::VP9;
        container = VideoContainer::Matroska;
        fileSliceIntervalMin = 0;  // never slice our recording by default

        frame = nullptr;
        inputFrame = nullptr;
        alignedInput = nullptr;

        octx = nullptr;
        vstrm = nullptr;
        cctx = nullptr;
        swsctx = nullptr;
        lossless = false;
    }

    std::string lastError;
    std::thread *thread;
    std::mutex mutex;
    std::queue<std::pair<cv::Mat, std::chrono::milliseconds>> frameQueue;

    std::string fnameBase;
    uint fileSliceIntervalMin;
    uint currentSliceNo;
    VideoCodec codec;
    VideoContainer container;

    bool initialized;
    std::atomic_bool acceptFrames;
    int width;
    int height;
    AVRational fps;
    bool lossless;

    bool saveTimestamps;
    std::ofstream timestampFile;

    AVFrame *frame;
    AVFrame *inputFrame;
    int64_t framePts;
    uchar *alignedInput;

    AVFormatContext *octx;
    AVStream *vstrm;
    AVCodecContext *cctx;
    SwsContext *swsctx;
    AVPixelFormat inputPixFormat;

    size_t frames_n;
};
#pragma GCC diagnostic pop

VideoWriter::VideoWriter()
    : d(new VideoWriterData())
{
    d->initialized = false;
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
        aframe_buf = static_cast<uint8_t*>(malloc(static_cast<size_t>(size)));
        if (!aframe_buf) {
            av_free(aframe);
            return nullptr;
        }
        av_image_fill_arrays(aframe->data, aframe->linesize, aframe_buf, static_cast<AVPixelFormat>(pix_fmt), width, height, 1);
    }

    return aframe;
}

void VideoWriter::initializeInternal()
{
    // sanity check. 'Raw' is the only "codec" that we allow to only actually work with one
    // container, all other codecs have to work with all containers.
    if ((d->codec == VideoCodec::Raw) && (d->container != VideoContainer::AVI)) {
        std::cerr << "Video codec was set to 'Raw', but container was not 'AVI'. Assuming 'AVI' as desired container format." << std::endl;
        d->container = VideoContainer::AVI;

    }

    // if file slicing is used, give our new file the appropriate name
    std::string fname;
    if (d->fileSliceIntervalMin > 0)
        fname = boost::str(boost::format("%1%_%2%") % d->fnameBase % d->currentSliceNo);
    else
        fname = d->fnameBase;

    // prepare timestamp filename
    auto timestampFname = fname + "_timestamps.csv";

    // set container format
    switch (d->container) {
    case VideoContainer::Matroska:
        if (!boost::algorithm::ends_with(fname, ".mkv"))
            fname = fname + ".mkv";
        break;
    case VideoContainer::AVI:
        if (!boost::algorithm::ends_with(fname, ".avi"))
            fname = fname + ".avi";
        break;
    }

    // open output format context
    int ret;
    d->octx = nullptr;
    ret = avformat_alloc_output_context2(&d->octx, nullptr, nullptr, fname.c_str());
    if (ret < 0)
        throw std::runtime_error(boost::str(boost::format("Failed to allocate output context: %1%") % ret));

    // open output IO context
    ret = avio_open2(&d->octx->pb, fname.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr);
    if (ret < 0) {
        finalizeInternal(false);
        throw std::runtime_error(boost::str(boost::format("Failed to open output I/O context: %1%") % ret));
    }

    auto codecId = AV_CODEC_ID_AV1;
    switch (d->codec) {
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
    case VideoCodec::H265:
        codecId = AV_CODEC_ID_H265;
        break;
    }

    // initialize codec and context
    auto vcodec = avcodec_find_encoder(codecId);
    d->cctx = avcodec_alloc_context3(vcodec);

    // create new video stream
    d->vstrm = avformat_new_stream(d->octx, vcodec);
    if (!d->vstrm)
        throw std::runtime_error("Failed to create new video stream.");
    avcodec_parameters_to_context(d->cctx, d->vstrm->codecpar);

    // set codec parameters
    d->cctx->codec_id = codecId;
    d->cctx->codec_type = AVMEDIA_TYPE_VIDEO;
    if (vcodec->pix_fmts != nullptr)
        d->cctx->pix_fmt = vcodec->pix_fmts[0];
    d->cctx->time_base = av_inv_q(d->fps);
    d->cctx->width = d->width;
    d->cctx->height = d->height;
    d->cctx->framerate = d->fps;
    d->cctx->workaround_bugs = FF_BUG_AUTODETECT;

    if (d->codec == VideoCodec::Raw)
        d->cctx->pix_fmt = d->inputPixFormat == AV_PIX_FMT_GRAY8 ||
                           d->inputPixFormat == AV_PIX_FMT_GRAY16LE ||
                           d->inputPixFormat == AV_PIX_FMT_GRAY16BE ? d->inputPixFormat : AV_PIX_FMT_YUV420P;

    // enable experimental mode to encode AV1
    if (d->codec == VideoCodec::AV1)
        d->cctx->strict_std_compliance = -2;

    if (d->octx->oformat->flags & AVFMT_GLOBALHEADER)
        d->cctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    AVDictionary *codecopts = nullptr;
    if (d->lossless) {
        switch (d->codec) {
        case VideoCodec::Raw:
            // uncompressed frames are always lossless
            break;
        case VideoCodec::AV1:
            av_dict_set_int(&codecopts, "lossless", 1, 0);
            break;
        case VideoCodec::FFV1:
            // This codec is lossless by default
            break;
        case VideoCodec::VP9:
            av_dict_set_int(&codecopts, "lossless", 1, 0);
            break;
        case VideoCodec::H265:
            av_dict_set_int(&codecopts, "crf", 0, 0);
            av_dict_set(&codecopts, "preset", "veryfast", 0);
            break;
        case VideoCodec::MPEG4:
            // NOTE: MPEG-4 has no lossless option
            std::cerr << "The MPEG-4 codec has no lossless preset, switching to lossy compression." << std::endl;
            d->lossless = false;
            break;
        }
    }

    if (d->codec == VideoCodec::VP9)
        av_dict_set(&codecopts, "deadline", "realtime", 0);

    if (d->codec == VideoCodec::FFV1) {
        d->lossless = true; // this codec is always lossless
        d->cctx->level = 3; // Ensure we use FFV1 v3
        av_dict_set_int(&codecopts, "slicecrc", 1, 0); // Add CRC information to each slice
        // NOTE: For archival use, GOP-size should be 1, but that also increases the file size quite a bit.
        // Keeping a good balance between recording space/performance/integrity is difficult sometimes.
    }

    // open video encoder
    ret = avcodec_open2(d->cctx, vcodec, &codecopts);
    if (ret < 0) {
        finalizeInternal(false);
        av_dict_free(&codecopts);
        throw std::runtime_error(boost::str(boost::format("Failed to open video encoder: %1%") % ret));
    }

    // stream codec parameters must be set after opening the encoder
    avcodec_parameters_from_context(d->vstrm->codecpar, d->cctx);
    d->vstrm->r_frame_rate = d->vstrm->avg_frame_rate = d->fps;

    // initialize sample scaler
    d->swsctx = sws_getCachedContext(nullptr,
                                     d->width,
                                     d->height,
                                     d->inputPixFormat,
                                     d->width,
                                     d->height,
                                     d->cctx->pix_fmt,
                                     SWS_BICUBIC,
                                     nullptr,
                                     nullptr,
                                     nullptr);

    if (!d->swsctx) {
        finalizeInternal(false);
        throw std::runtime_error("Failed to initialize sample scaler.");
    }

    // allocate frame buffer for encoding
    d->frame = vw_alloc_frame(d->cctx->pix_fmt, d->width, d->height, true);

    // allocate input buffer for color conversion
    d->inputFrame = vw_alloc_frame(d->cctx->pix_fmt, d->width, d->height, false);

    // write format header, after this we are ready to encode frames
    ret = avformat_write_header(d->octx, nullptr);
    if (ret < 0) {
        finalizeInternal(false);
        throw std::runtime_error(boost::str(boost::format("Failed to write format header: %1%") % ret));
    }
    d->framePts = 0;

    if (d->saveTimestamps) {
        d->timestampFile.close(); // ensure file is closed
        d->timestampFile.clear();
        d->timestampFile.open(timestampFname);
        d->timestampFile << "frame; timestamp" << "\n";
        d->timestampFile.flush();
    }

    d->initialized = true;
}

void VideoWriter::finalizeInternal(bool writeTrailer, bool stopRecThread)
{
    // stop encoding frames and write the last bits to disk.
    // wait for the encoding thread to join.
    // if no thread was running, do nothing
    // (unless of course we are in the recording thread and just want to start
    // a new file, in this case `stopRecThread` will be set to false)
    if (stopRecThread)
        stopEncodeThread();

    if (d->initialized) {
        if (d->vstrm != nullptr)
            avcodec_send_frame(d->cctx, nullptr);

        // write trailer
        if (writeTrailer && (d->octx != nullptr))
            av_write_trailer(d->octx);
    }

    // ensure timestamps file is closed
    if (d->saveTimestamps)
        d->timestampFile.close();

    // free all FFmpeg resources
    if (d->frame != nullptr) {
        av_frame_free(&d->frame);
        d->frame = nullptr;
    }
    if (d->inputFrame != nullptr) {
        av_frame_free(&d->inputFrame);
        d->inputFrame = nullptr;
    }

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

    if (d->alignedInput != nullptr)
        av_freep(&d->alignedInput);

    d->initialized = false;
}

void VideoWriter::initialize(std::string fname, int width, int height, int fps, bool hasColor, bool saveTimestamps)
{
    if (d->initialized)
        throw std::runtime_error("Tried to initialize an already initialized video writer.");

    d->width = width;
    d->height = height;
    d->fps = {fps, 1};
    d->frames_n = 0;
    d->saveTimestamps = saveTimestamps;
    d->currentSliceNo = 1;
    if (fname.substr(fname.find_last_of(".") + 1).length() == 3)
        d->fnameBase = fname.substr(0, fname.length() - 4); // remove 3-char suffix from filename
    else
        d->fnameBase = fname;

    // select FFMpeg pixel format of OpenCV matrixes
    d->inputPixFormat = hasColor? AV_PIX_FMT_BGR24 : AV_PIX_FMT_GRAY8;

    // initialize encoder
    initializeInternal();

    // start encoding data
    startEncodeThread();
}

void VideoWriter::finalize()
{
    finalizeInternal(true, true);
}

bool VideoWriter::initialized() const
{
    return d->initialized;
}

bool VideoWriter::prepareFrame(const cv::Mat &image)
{
    const auto channels = image.channels();

    auto step = image.step[0];
    auto data = image.ptr();

    const auto height = image.rows;
    const auto width = image.cols;

    // sanity checks
    if ((static_cast<int>(height) > d->height) || (static_cast<int>(width) > d->width))
        throw std::runtime_error(boost::str(boost::format("Received bigger frame than we expected (%1%x%2% instead %3%x%4%)") % width % height % d->width % d->height));
    if ((d->inputPixFormat == AV_PIX_FMT_BGR24) && (channels != 3))
        return false;
    else if ((d->inputPixFormat == AV_PIX_FMT_GRAY8) && (channels != 1))
        return false;

    // FFmpeg contains SIMD optimizations which can sometimes read data past
    // the supplied input buffer. To ensure that doesn't happen, we pad the
    // step to a multiple of 32 (that's the minimal alignment for which Valgrind
    // doesn't raise any warnings).
    const size_t STEP_ALIGNMENT = 32;
    if (step % STEP_ALIGNMENT != 0) {
        auto aligned_step = (step + STEP_ALIGNMENT - 1) & -STEP_ALIGNMENT;

        if (d->alignedInput == nullptr)
            d->alignedInput = static_cast<uchar*>(av_mallocz(aligned_step * static_cast<size_t>(height)));

        for (size_t y = 0; y < static_cast<size_t>(height); y++)
            memcpy(d->alignedInput + y*aligned_step, image.ptr() + y*step, step);

        data = d->alignedInput;
        step = aligned_step;
    }

    if (d->cctx->pix_fmt != d->inputPixFormat) {
        // let input_picture point to the raw data buffer of 'image'
        av_image_fill_arrays(d->inputFrame->data, d->inputFrame->linesize, static_cast<const uint8_t*>(data), d->inputPixFormat, width, height, 1);
        d->inputFrame->linesize[0] = static_cast<int>(step);

        if (sws_scale(d->swsctx, d->inputFrame->data,
                               d->inputFrame->linesize, 0,
                               d->height,
                               d->frame->data, d->frame->linesize) < 0)
                    return false;


    } else {
        av_image_fill_arrays(d->frame->data, d->frame->linesize, static_cast<const uint8_t*>(data), d->inputPixFormat, width, height, 1);
        d->frame->linesize[0] = static_cast<int>(step);
    }

    d->frame->pts = d->framePts++;
    return true;
}

bool VideoWriter::encodeFrame(const cv::Mat &frame, const std::chrono::milliseconds &timestamp)
{
    int ret;

    if (!prepareFrame(frame)) {
        std::cerr << "Unable to prepare frame. N: " << d->frames_n + 1 << std::endl;
        return false;
    }

    // encode video frame
    ret = avcodec_send_frame(d->cctx, d->frame);
    if (ret < 0) {
        std::cerr << "Unable to send frame to encoder. N:" << d->frames_n + 1 << std::endl;
        return false;
    }

    AVPacket pkt;
    pkt.data = nullptr;
    pkt.size = 0;
    av_init_packet(&pkt);
    ret = avcodec_receive_packet(d->cctx, &pkt);
    if (ret != 0)
        return false;

    // rescale packet timestamp
    pkt.duration = 1;
    av_packet_rescale_ts(&pkt, d->cctx->time_base, d->vstrm->time_base);

    // write packet
    av_write_frame(d->octx, &pkt);
    d->frames_n++;
    av_packet_unref(&pkt);

    // store timestamp (if necessary)
    const auto tsMsec = timestamp.count();
    if (d->saveTimestamps)
        d->timestampFile << d->framePts << "; " << tsMsec << "\n";

    if (d->fileSliceIntervalMin != 0) {
        const auto tsMin = static_cast<double>(tsMsec) / 1000.0 / 60.0;
        if (tsMin > (d->fileSliceIntervalMin * d->currentSliceNo)) {
            try {
                // we need to start a new file now since the maximum time for this file has elapsed,
                // so finalize this one without suspending the thread we are currently in
                finalizeInternal(true, false);

                // increment current slice number and attempt to reinitialize recording.
                d->currentSliceNo += 1;
                initializeInternal();
            } catch (const std::exception& e) {
                // propagate error and stop encoding thread, as we can not really recover from this
                d->lastError = e.what();
                d->acceptFrames = false;
            }
        }
    }

    return true;
}

void VideoWriter::startEncodeThread()
{
    assert(d->initialized);

    // clear last error message
    d->lastError.clear();
    stopEncodeThread();
    while (!d->frameQueue.empty())
        d->frameQueue.pop();
    d->acceptFrames = true;
    d->thread = new std::thread(encodeThread, this);
}

void VideoWriter::stopEncodeThread()
{
    if (d->thread == nullptr)
        return;
    assert(d->initialized);

    d->acceptFrames = false;
    d->thread->join();
    delete d->thread;
    d->thread = nullptr;
}

bool VideoWriter::pushFrame(const cv::Mat &frame, const std::chrono::milliseconds &time)
{
    std::lock_guard<std::mutex> lock(d->mutex);
    if (!d->acceptFrames)
        return false;
    if (d->frameQueue.size() > FRAME_QUEUE_MAX_COUNT) {
        d->lastError = "Frame encoding buffer was full and new frame could not be added. Maybe encoding or storage is too slow.";
        return false;
    }

    d->frameQueue.push(std::make_pair(frame, time));
    return true;
}

VideoCodec VideoWriter::codec() const
{
    return d->codec;
}

void VideoWriter::setCodec(VideoCodec codec)
{
    d->codec = codec;
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

bool VideoWriter::lossless() const
{
    return d->lossless;
}

void VideoWriter::setLossless(bool enabled)
{
    d->lossless = enabled;
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

void VideoWriter::encodeThread(void *vwPtr)
{
    VideoWriter *self = static_cast<VideoWriter*> (vwPtr);

    while (self->d->acceptFrames) {
        cv::Mat frame;
        std::chrono::milliseconds timestamp;
        while (self->getNextFrameFromQueue(&frame, &timestamp)) {
            self->encodeFrame(frame, timestamp);
        }
    }
}

bool VideoWriter::getNextFrameFromQueue(cv::Mat *frame, std::chrono::milliseconds *timestamp)
{
    std::lock_guard<std::mutex> lock(d->mutex);
    if (d->frameQueue.empty())
        return false;

    auto pair = d->frameQueue.front();
    *frame = pair.first;
    *timestamp = pair.second;
    d->frameQueue.pop();
    return true;
}

/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef VIDEOWRITER_H
#define VIDEOWRITER_H

#include <memory>
#include <chrono>
#include <opencv2/core.hpp>
#include <QMetaType>

#include "streams/frametype.h"

/**
 * @brief The VideoContainer enum
 *
 * Video container formats that we support in VideoWriter.
 * Each container must be compatible with every codec type
 * that we also support.
 */
enum class VideoContainer {
    Unknown,
    Matroska,
    AVI
};
Q_DECLARE_METATYPE(VideoContainer);

std::string videoContainerToString(VideoContainer container);
VideoContainer stringToVideoContainer(const std::string& str);

/**
 * @brief The VideoCodec enum
 *
 * Video codecs that we support in VideoWriter.
 * Each codec must be compatible with every container type
 * that we also support, to avoid unnecessary user confusion and
 * API errors.
 * Currently, the only permanent exception to this rule is the "Raw" encoder,
 * which only supports the AVI container.
 */
enum class VideoCodec {
    Unknown,
    Raw,
    FFV1,
    AV1,
    VP9,
    H265,
    MPEG4
};
Q_DECLARE_METATYPE(VideoCodec);

std::string videoCodecToString(VideoCodec codec);
VideoCodec stringToVideoCodec(const std::string& str);

/**
 * @brief The VideoWriter class
 *
 * A simple class to write OpenCV matrices to disk as quickly as possible,
 * with a pleasant but very simplified API and all the nasty video encoding
 * issues hidden away.
 * This class intentionally supports only few container/codec formats and options.
 */
class VideoWriter
{
public:
    VideoWriter();
    ~VideoWriter();

    void initialize(std::string fname, int width, int height, int fps, bool hasColor, bool saveTimestamps = true);
    void finalize();
    bool initialized() const;

    std::chrono::milliseconds captureStartTimestamp() const;
    void setCaptureStartTimestamp(const std::chrono::milliseconds& startTimestamp);

    bool pushFrame(const Frame &frame);

    VideoCodec codec() const;
    void setCodec(VideoCodec codec);

    VideoContainer container() const;
    void setContainer(VideoContainer container);

    int width() const;
    int height() const;
    int fps() const;

    bool lossless() const;
    void setLossless(bool enabled);

    uint fileSliceInterval() const;
    void setFileSliceInterval(uint minutes);

    std::string lastError() const;

private:
    class VideoWriterData;
    std::unique_ptr<VideoWriterData> d;

    void initializeInternal();
    void finalizeInternal(bool writeTrailer, bool stopRecThread = true);
    static void encodeThread(void* vwPtr);
    bool getNextFrameFromQueue(cv::Mat *data, milliseconds_t *timestamp);
    bool prepareFrame(const cv::Mat &inImage);
    bool encodeFrame(const cv::Mat& frame, const std::chrono::milliseconds& timestamp);
    void startEncodeThread();
    void stopEncodeThread();
};

#endif // VIDEOWRITER_H

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

#pragma once
#include "datatypes.h"
#include <opencv2/core.hpp>

/**
 * @brief A single frame of a video stream
 *
 * Describes a single frame in a stream of frames that make up
 * a complete video.
 * Each frame is timestamped with the exact time of its acquisition.
 */
struct Frame : BaseDataType {
    SY_DEFINE_DATA_TYPE(Frame)

    explicit Frame() {}
    explicit Frame(const cv::Mat imgMat, const microseconds_t &t)
        : index(0),
          time(t),
          mat(imgMat)
    {
    }

    explicit Frame(const cv::Mat &imgMat, const uint64_t &idx, const microseconds_t &t)
        : index(idx),
          time(t),
          mat(imgMat)
    {
    }

    explicit Frame(const uint64_t &idx)
        : index(idx),
          time(microseconds_t(0))
    {
    }

    uint64_t index;
    microseconds_t time;
    cv::Mat mat;

    inline cv::Mat copyMat() const
    {
        return mat.clone();
    }

    ssize_t memorySize() const override
    {
        // Calculate data size based on the format
        size_t dataSize = mat.elemSize() * mat.cols * mat.rows;

        // Calculate total buffer size:
        // 1x uint64 + 1x int64 for index and timestamp
        // 4x int for image metadata
        // + size of image data itself
        return static_cast<ssize_t>(sizeof(uint64_t) + sizeof(int64_t) + (sizeof(int) * 4) + dataSize);
    }

    bool writeToMemory(void *buffer, ssize_t size = -1) const override
    {
        // fetch metadata
        int width = mat.cols;
        int height = mat.rows;
        int channels = mat.channels();
        int type = mat.type();

        // Calculate data size based on the format
        size_t dataSize = mat.elemSize() * width * height;

        // calculate our memory segment size, if it wasn't passed
        if (size < 0)
            size = memorySize();

        // copy metadata to buffer
        size_t offset = 0;

        // copy index and timestamp
        std::memcpy(static_cast<unsigned char *>(buffer) + offset, &index, sizeof(index));
        offset += sizeof(index);

        const int64_t timeC = time.count();
        std::memcpy(static_cast<unsigned char *>(buffer) + offset, &timeC, sizeof(timeC));
        offset += sizeof(timeC);

        // copy image metadata
        std::memcpy(static_cast<unsigned char *>(buffer) + offset, &width, sizeof(width));
        offset += sizeof(width);
        std::memcpy(static_cast<unsigned char *>(buffer) + offset, &height, sizeof(height));
        offset += sizeof(height);
        std::memcpy(static_cast<unsigned char *>(buffer) + offset, &channels, sizeof(channels));
        offset += sizeof(channels);
        std::memcpy(static_cast<unsigned char *>(buffer) + offset, &type, sizeof(type));
        offset += sizeof(type);

        // copy image data
        std::memcpy(static_cast<unsigned char *>(buffer) + offset, mat.data, dataSize);

        return true;
    };

    QByteArray toBytes() const override
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);

        // TODO

        return bytes;
    }

    static Frame fromMemory(const void *buffer, size_t size)
    {
        Frame frame;

        int width, height, channels, type;
        int64_t timeC;
        size_t offset = 0;

        // unpack index and timestamp
        std::memcpy(&frame.index, static_cast<const unsigned char *>(buffer) + offset, sizeof(frame.index));
        offset += sizeof(frame.index);
        std::memcpy(&timeC, static_cast<const unsigned char *>(buffer) + offset, sizeof(timeC));
        offset += sizeof(timeC);
        frame.time = microseconds_t(timeC);

        // unpack image metadata
        std::memcpy(&width, static_cast<const unsigned char *>(buffer) + offset, sizeof(width));
        offset += sizeof(width);
        std::memcpy(&height, static_cast<const unsigned char *>(buffer) + offset, sizeof(height));
        offset += sizeof(height);
        std::memcpy(&channels, static_cast<const unsigned char *>(buffer) + offset, sizeof(channels));
        offset += sizeof(channels);
        std::memcpy(&type, static_cast<const unsigned char *>(buffer) + offset, sizeof(type));
        offset += sizeof(type);

        // Create cv::Mat from the memory buffer
        frame.mat = cv::Mat(height, width, type, (void *)(static_cast<const unsigned char *>(buffer) + offset)).clone();

        return frame;
    }
};

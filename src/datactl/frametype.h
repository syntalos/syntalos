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

namespace Syntalos
{
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
    explicit Frame(const cv::Mat &imgMat, const microseconds_t &t)
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

    bool toBytes(ByteVector &output) const override
    {
        // TODO: Implement serialization if needed
        assert(0 && "toBytes() is not implemented for Frame. Use writeToMemory() instead.");

        return false;
    }

    static Frame fromMemory(const void *buffer, size_t size)
    {
        Frame frame;
        fromMemoryInto(buffer, size, frame);
        return frame;
    }

    /**
     * @brief Deserialize a Frame from @p buffer into @p frame, reusing its
     *        cv::Mat pixel buffer when possible.
     *
     * Unlike fromMemory(), this variant avoids a malloc/free pair on every
     * call when the frame dimensions and type are unchanged between calls AND
     * @p frame is the exclusive owner of its mat data (cv::Mat refcount == 1).
     *
     * If the buffer is shared with subscriber queues (refcount > 1) the old
     * data is released safely - each subscriber still holds its own reference
     * - and a fresh allocation is made for this frame.
     */
    static void fromMemoryInto(const void *buffer, size_t size, Frame &frame)
    {
        const auto *ptr = static_cast<const unsigned char *>(buffer);
        int width, height, channels, type;
        int64_t timeC;
        size_t offset = 0;

        // unpack index and timestamp
        std::memcpy(&frame.index, ptr + offset, sizeof(frame.index));
        offset += sizeof(frame.index);
        std::memcpy(&timeC, ptr + offset, sizeof(timeC));
        offset += sizeof(timeC);
        frame.time = microseconds_t(timeC);

        // unpack image metadata
        std::memcpy(&width, ptr + offset, sizeof(width));
        offset += sizeof(width);
        std::memcpy(&height, ptr + offset, sizeof(height));
        offset += sizeof(height);
        std::memcpy(&channels, ptr + offset, sizeof(channels));
        offset += sizeof(channels);
        std::memcpy(&type, ptr + offset, sizeof(type));
        offset += sizeof(type);

        // Reuse the existing cv::Mat pixel buffer when:
        // - same dimensions and type (no reallocation needed), AND
        // - refcount == 1, i.e. we are the sole owner.
        // If refcount > 1 the subscriber queues still hold references to the
        // previous frame's pixels.  Assigning a new cv::Mat decrements the old
        // refcount (keeping their copies valid) and allocates a fresh buffer
        // so we never overwrite data that is still in use downstream.
        const bool canReuse = (frame.mat.data != nullptr) && (frame.mat.u != nullptr && frame.mat.u->refcount == 1)
                              && (frame.mat.rows == height) && (frame.mat.cols == width) && (frame.mat.type() == type);
        if (!canReuse)
            frame.mat = cv::Mat(height, width, type);

        const size_t dataSize = frame.mat.elemSize() * static_cast<size_t>(width * height);
        std::memcpy(frame.mat.data, ptr + offset, dataSize);
    }
};

} // namespace Syntalos

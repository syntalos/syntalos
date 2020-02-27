/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ipcmarshal.h"

#include <chrono>

/**
 * @brief Write OpenCV Matrix to shared memory region.
 */
bool cvMatToShm(std::unique_ptr<SharedMemory> &shm, const cv::Mat &frame)
{
    int mat_type = frame.type();
    int mat_channels = frame.channels();
    size_t memsize = sizeof(int) * 6;
    if (frame.isContinuous())
        memsize += frame.dataend - frame.datastart;
    else
        memsize += CV_ELEM_SIZE(mat_type) * frame.cols * frame.rows;

    if (shm->size() == 0) {
        // this is a fresh shared-memory object, so create it
        if (!shm->create(memsize))
            return false;
    } else {
        if (shm->size() != memsize) {
            // the memory segment doesn't have the right size, let's create a new one!
            shm.reset(new SharedMemory);
            if (!shm->create(memsize))
                return false;
        }
    }

    shm->lock();
    auto shm_data = static_cast<char*>(shm->data());

    // write header
    size_t pos = 0;
    int header[4] = {mat_type, mat_channels, frame.rows, frame.cols};

    std::memcpy(shm_data + pos, &header, sizeof(header));
    pos += sizeof(header);

    // write image data
    if (frame.isContinuous()) {
        std::memcpy(shm_data + pos, frame.ptr<char>(0), (frame.dataend - frame.datastart));
    }
    else {
        size_t rowsz = static_cast<size_t>(CV_ELEM_SIZE(mat_type) * frame.cols);
        for (int r = 0; r < frame.rows; ++r) {
            std::memcpy(shm_data + pos, frame.ptr<char>(r), rowsz);
        }
    }

    shm->unlock();
    return true;
}

/**
 * @brief Retrieve OpenCV Matrix from shared memory segment.
 */
cv::Mat cvMatFromShm(std::unique_ptr<SharedMemory> &shm, bool copy)
{
    if (!shm->isAttached() && !shm->attach())
        return cv::Mat();

    shm->lock();
    auto shm_data = static_cast<const char*>(shm->data());
    size_t pos = 0;

    // read header
    int header[4]; // contains type, channels, rows, cols in that order
    std::memcpy(&header, shm_data + pos, sizeof(header));
    pos += sizeof(header);

    // read data
    if (copy) {
        cv::Mat mat(header[2], header[3], header[0]);
        std::memcpy(mat.data, shm_data + pos, shm->size() - pos);
        shm->unlock();
        return mat;
    } else {
        cv::Mat mat(header[2], header[3], header[0], (uchar*) (shm_data + pos));
        shm->unlock();
        return mat;
    }
}

bool marshalDataElement(int typeId, const QVariant &data,
                        QVariantList &params, std::unique_ptr<SharedMemory> &shm)
{
    if (typeId == qMetaTypeId<Frame>()) {
        auto frame = data.value<Frame>();
        if (!cvMatToShm(shm, frame.mat))
            return false;

        params.append(QVariant::fromValue(frame.time.count()));
        return true;
    }

    if (typeId == qMetaTypeId<ControlCommand>()) {
        auto command = data.value<ControlCommand>();
        params.append(QVariant::fromValue(command.kind));
        params.append(QVariant::fromValue(command.command));
        return true;
    }

    // for any other type, we just have it serialize itself
    // and append it as first parameter
    params.append(data);

    return true;
}

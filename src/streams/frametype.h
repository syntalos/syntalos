/*
 * Copyright (C) 2019-2022 Matthias Klumpp <matthias@tenstral.net>
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
 * Each frame is timestamped for accuracy.
 */
class Frame
{
public:
    explicit Frame() {}
    explicit Frame(const cv::Mat &m, const milliseconds_t &t)
        : index(0),
          time(t),
          mat(m)
    {}

    explicit Frame(const size_t &i, const cv::Mat &m, const milliseconds_t &t)
        : index(i),
          time(t),
          mat(m)
    {}

    size_t index;
    milliseconds_t time;
    cv::Mat mat;
};
Q_DECLARE_METATYPE(Frame)

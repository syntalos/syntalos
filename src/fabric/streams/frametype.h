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
#include "vips8-q.h"

/**
 * @brief A single frame of a video stream
 *
 * Describes a single frame in a stream of frames that make up
 * a complete video.
 * Each frame is timestamped with the exact time of its acquisition.
 */
class Frame
{
public:
    explicit Frame() {}
    explicit Frame(const vips::VImage &img, const milliseconds_t &t)
        : index(0),
          time(t),
          mat(img)
    {
    }

    explicit Frame(const vips::VImage &img, const size_t &idx, const milliseconds_t &t)
        : index(idx),
          time(t),
          mat(img)
    {
    }

    explicit Frame(const size_t &idx)
        : index(idx)
    {
    }

    size_t index;
    milliseconds_t time;
    vips::VImage mat;
};
Q_DECLARE_METATYPE(Frame)

/*
 * Copyright (C) 2023-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "vips8-q.h"
#include "opencv2/core.hpp"

/**
 * @brief Transform a cv::Mat to a vips::VImage
 * @param mat The image matrix to transform
 * @return A copy of the image as VIPS image.
 */
vips::VImage cvMatToVips(const cv::Mat &mat);

/**
 * @brief Transform a VipsImage into a cv::Mat
 * @param vimg The image to transform
 * @return A copy of the image as cv::Mat
 */
cv::Mat vipsToCvMat(vips::VImage vimg);

/**
 * @brief Get the size of the VIPS band format at compile-time
 * @param format The VIPS band format
 * @return The size of the format in bytes
 */
template<VipsBandFormat format>
constexpr size_t vipsBandFormatSize()
{
    switch (format) {
    case VIPS_FORMAT_UCHAR:
        return sizeof(uint8_t);
    case VIPS_FORMAT_CHAR:
        return sizeof(int8_t);
    case VIPS_FORMAT_USHORT:
        return sizeof(uint16_t);
    case VIPS_FORMAT_SHORT:
        return sizeof(int16_t);
    default:
        static_assert(false, "Selected VIPS band format is currently unsupported by Syntalos");
        return 0;
    }
}

/**
 * @brief Create a new VIPS image with the given dimensions and format
 * @tparam format The VIPS format to use
 * @param width The width of the image
 * @param height The height of the image
 * @param bands The number of bands in the image
 * @return The new VIPS image
 */
template<VipsBandFormat format>
vips::VImage newVipsImage(int width, int height, int bands = 1)
{
    size_t buffer_size = width * height * vipsBandFormatSize<format>();

    auto buffer = vips_malloc(nullptr, buffer_size);
    return vips::VImage::new_from_memory_steal(buffer, buffer_size, width, height, bands, format);
}

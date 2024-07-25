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

#include "vipsutils.h"

#include <opencv2/opencv.hpp>

vips::VImage cvMatToVips(const cv::Mat &mat)
{
    const auto channels = mat.channels();

    VipsBandFormat format;
    switch (mat.depth()) {
    case CV_8U:
        format = VIPS_FORMAT_UCHAR;
        break;
    case CV_16U:
        format = VIPS_FORMAT_USHORT;
        break;
    case CV_32F:
        format = VIPS_FORMAT_FLOAT;
        break;
    default:
        throw vips::VError("Unsupported cv::Mat depth for VIPS conversion");
    }

    // make continuous Mat, if we don't have one already
    auto contMat = mat.isContinuous() ? mat : mat.clone();

    // perform color conversion
    if (channels == 3)
        cv::cvtColor(contMat, contMat, cv::COLOR_BGR2RGB);
    else if (channels == 4)
        cv::cvtColor(contMat, contMat, cv::COLOR_BGRA2RGBA);

    // unfortunately we need to copy here, as OpenCV does not relinquish control over its memory
    // easily and cleanly
    const size_t dataSize = contMat.total() * contMat.elemSize();
    vips::VImage vimg = vips::VImage::new_from_memory_copy(
        contMat.data, dataSize, mat.cols, mat.rows, channels, format);

    // update interpretation
    if (channels == 3 || channels == 4)
        vimg.set("interpretation", VIPS_INTERPRETATION_RGB);

    return vimg;
}

cv::Mat vipsToCvMat(vips::VImage vimg)
{
    const auto channels = vimg.bands();

    // convert some common formats
    int cvType = -1;
    switch (vimg.format()) {
    case VIPS_FORMAT_UCHAR:
        switch (channels) {
        case 1:
            cvType = CV_8UC1;
            break;
        case 3:
            cvType = CV_8UC3;
            break;
        case 4:
            cvType = CV_8UC4;
            break;
        }
        break;
    case VIPS_FORMAT_CHAR:
        switch (channels) {
        case 1:
            cvType = CV_8SC1;
            break;
        case 3:
            cvType = CV_8SC3;
            break;
        case 4:
            cvType = CV_8SC4;
            break;
        }
        break;
    case VIPS_FORMAT_USHORT:
        switch (channels) {
        case 1:
            cvType = CV_16UC1;
            break;
        case 3:
            cvType = CV_16UC3;
            break;
        case 4:
            cvType = CV_16UC4;
            break;
        }
        break;
    case VIPS_FORMAT_SHORT:
        switch (channels) {
        case 1:
            cvType = CV_16SC1;
            break;
        case 3:
            cvType = CV_16SC3;
            break;
        case 4:
            cvType = CV_16SC4;
            break;
        }
        break;
    case VIPS_FORMAT_FLOAT:
        switch (channels) {
        case 1:
            cvType = CV_32FC1;
            break;
        case 3:
            cvType = CV_32FC3;
            break;
        case 4:
            cvType = CV_32FC4;
            break;
        }
        break;
    default:
        throw vips::VError("Unsupported number of channels or pixel format for cv::Mat conversion");
    }

    // wrap the data in a cv::Mat
    cv::Mat mat(cv::Size(vimg.width(), vimg.height()), cvType, (void *)vimg.data());

    // create a copy so the resulting cv::Mat takes ownership of the data
    auto resMat = mat.clone();

    // perform color conversion
    if (channels == 3)
        cv::cvtColor(resMat, resMat, cv::COLOR_RGB2BGR);
    else if (channels == 4)
        cv::cvtColor(resMat, resMat, cv::COLOR_RGBA2BGRA);

    return resMat;
}

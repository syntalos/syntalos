/*
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "cvutils.h"

#include <QDebug>
#include <QPixmap>
#include <opencv2/opencv.hpp>

namespace Syntalos
{

std::optional<QImage> cvMatToQImage(const cv::Mat &mat)
{
    QImage qimg;
    if (mat.channels() == 1) {
        return QImage(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_Grayscale8);
    }

    if (mat.channels() == 3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage(
                   rgb.data,
                   rgb.cols,
                   rgb.rows,
                   static_cast<int>(rgb.step),
                   QImage::Format_RGB888)
            .copy(); // copy so it owns its data
    }

    if (mat.channels() == 4) {
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        return QImage(rgba.data, rgba.cols, rgba.rows, static_cast<int>(rgba.step), QImage::Format_RGBA8888).copy();
    }

    return std::nullopt;
}

std::optional<QPixmap> cvMatToQPixmap(const cv::Mat &mat)
{
    auto qimg = cvMatToQImage(mat);
    if (!qimg.has_value())
        return std::nullopt;
    return QPixmap::fromImage(qimg.value());
}

} // namespace Syntalos

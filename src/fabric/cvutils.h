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

#pragma once

#include <QObject>
#include <QRect>
#include <opencv2/core.hpp>

namespace Syntalos
{
/**
 * @brief Converts an OpenCV Mat to a QImage.
 *
 * This function attempts to convert the given OpenCV Mat to a QImage.
 * It supports grayscale (1 channel), RGB (3 channels), and RGBA (4 channels) images.
 * If the conversion is successful, it returns a QImage wrapped in std::optional.
 * If the conversion fails (e.g., unsupported number of channels), it returns std::nullopt.
 *
 * @param mat The OpenCV Mat to convert.
 * @return An optional containing the converted QImage, or std::nullopt if conversion fails.
 */
std::optional<QImage> cvMatToQImage(const cv::Mat &mat);
std::optional<QPixmap> cvMatToQPixmap(const cv::Mat &mat);

/**
 * Helper to convert a QRect into a cv::Rect
 *
 * For a cv::Rect, right and bottom edges are exclusive,
 * while they are inclusive for a QRect. This helper just
 * ensures we don't accidentally make any conversion mistakes.
 */
inline cv::Rect qRectToCvRect(const QRect &r)
{
    return cv::Rect(r.x(), r.y(), r.width(), r.height());
}

} // namespace Syntalos

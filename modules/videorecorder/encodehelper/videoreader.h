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

#include <QMetaType>
#include <chrono>
#include <memory>

#include "datactl/frametype.h"

struct AVFrame;

/**
 * @brief The VideoReader class
 *
 * A simple class to write OpenCV matrices to disk as quickly as possible,
 * with a pleasant but very simplified API and all the nasty video encoding
 * issues hidden away.
 * This class intentionally supports only few container/codec formats and options.
 */
class VideoReader
{
public:
    explicit VideoReader();
    ~VideoReader();

    QString lastError() const;
    bool open(const QString &filename);

    double durationSec() const;
    ssize_t totalFrames() const;
    ssize_t lastFrameIndex() const;
    double framerate() const;

    std::optional<std::pair<cv::Mat, int64_t>> readFrame();

private:
    class Private;
    std::unique_ptr<Private> d;
    Q_DISABLE_COPY(VideoReader)

    std::optional<cv::Mat> frameToCVImage(AVFrame *frame);
};

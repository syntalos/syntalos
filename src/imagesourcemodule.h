/**
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef IMAGESOURCEMODULE_H
#define IMAGESOURCEMODULE_H

#include <chrono>
#include <opencv2/core.hpp>

#include "moduleapi.h"
#include "streams/frametype.h"

class VideoWriter;

class ImageSourceModule : public AbstractModule
{
    Q_OBJECT
public:
    explicit ImageSourceModule(QObject *parent = nullptr);

    virtual bool prepare();
    virtual bool prepare(const QString &storageRootDir, const TestSubject &testSubject) override;
    virtual int selectedFramerate() const = 0;
    virtual cv::Size selectedResolution() const = 0;

    /**
     * @brief Attach a VideoWriter class to this image source, for fast video capture.
     * This is done for efficiency reasons, as the newFrame() signal may not be fast
     * enough to record with higher framerates.
     */
    virtual void attachVideoWriter(VideoWriter *vwriter);

signals:
    void newFrame(const Frame& frame);

};

#endif // IMAGESOURCEMODULE_H

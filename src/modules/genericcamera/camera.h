/*
 * Copyright (C) 2019 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef GENERIC_CAMERA_H
#define GENERIC_CAMERA_H

#include <QObject>
#include <QScopedPointer>
#include <QSize>
#include <opencv2/core.hpp>

#include "syclock.h"
#include "timesync.h"
#include "streams/frametype.h"

class CameraData;
class Camera
{
public:
    Camera();
    ~Camera();

    void setCamId(int id);
    int camId() const;
    void setStartTime(const symaster_timepoint &time);

    void setResolution(const cv::Size &size);
    cv::Size resolution() const;

    void setExposure(double value);
    double exposure() const;

    void setGain(double value);
    double gain() const;

    bool connect();
    void disconnect();

    bool recordFrame(Frame &frame, SecondaryClockSynchronizer *clockSync);

    QString lastError() const;

    static QList<QPair<QString, int>> availableCameras();

private:
    QScopedPointer<CameraData> d;

    void fail(const QString &msg);
};

#endif // GENERIC_CAMERA_H

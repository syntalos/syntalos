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

#ifndef GENERIC_CAMERA_H
#define GENERIC_CAMERA_H

#include <QObject>
#include <QScopedPointer>
#include <QSize>
#include <opencv2/core.hpp>

#include "datactl/frametype.h"
#include "datactl/syclock.h"
#include "datactl/timesync.h"

namespace Syntalos
{
Q_DECLARE_LOGGING_CATEGORY(logGenCamera)
}

struct CameraPixelFormat {
    QString name;
    unsigned int fourcc;

    friend QDataStream &operator<<(QDataStream &out, const CameraPixelFormat &obj)
    {
        out << (quint32)obj.fourcc << obj.name;
        return out;
    }

    friend QDataStream &operator>>(QDataStream &in, CameraPixelFormat &obj)
    {
        in >> obj.fourcc >> obj.name;
        return in;
    }
};
Q_DECLARE_METATYPE(CameraPixelFormat)

class CameraData;
class Camera
{
public:
    Camera();
    ~Camera();

    void setCamId(int id);
    int camId() const;
    void setStartTime(const symaster_timepoint &time);

    cv::Size resolution() const;
    void setResolution(const cv::Size &size);

    int framerate() const;
    void setFramerate(int fps);

    double exposure() const;
    void setExposure(double value);

    double brightness() const;
    void setBrightness(double value);

    double contrast() const;
    void setContrast(double value);

    double saturation() const;
    void setSaturation(double value);

    double hue() const;
    void setHue(double value);

    double gain() const;
    void setGain(double value);

    int autoExposureRaw() const;
    void setAutoExposureRaw(int value);

    bool connect();
    void disconnect();

    QList<CameraPixelFormat> readPixelFormats();
    void setPixelFormat(const CameraPixelFormat &pixFmt);

    bool recordFrame(Frame &frame, SecondaryClockSynchronizer *clockSync);

    QString lastError() const;

    static QList<QPair<QString, int>> availableCameras();

private:
    QScopedPointer<CameraData> d;

    void fail(const QString &msg);
};

#endif // GENERIC_CAMERA_H

/*
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef V4LCAMERA_H
#define V4LCAMERA_H

#include <QObject>
#include <QPair>
#include <QList>
#include <QSize>
#include <opencv2/core/core.hpp>

#include "videotracker.h"

class V4LCamera : public MACamera
{
    Q_OBJECT
public:
    explicit V4LCamera(QObject *parent = 0);
    ~V4LCamera();
    QString lastError() const;

    QList<QPair<QString, int>> getCameraList() const;

    bool open(int cameraId, const QSize& size);
    bool close();
    bool setFramerate(double fps);

    QPair<time_t, cv::Mat> getFrame();
    void getFrame(time_t *time, cv::Mat& buffer);

    QList<QSize> getResolutionList(int cameraId);

    bool setAutoWhiteBalance(bool enabled) { Q_UNUSED(enabled); return true; };
    bool setAutoGain(bool enabled) { Q_UNUSED(enabled); return true; };
    bool setExposureTime(double val) { Q_UNUSED(val); return true; };

public slots:

private:
    typedef enum {
        PIXELFORMAT_NONE,
        PIXELFORMAT_YUYV,
        PIXELFORMAT_UYVY,
        PIXELFORMAT_MJPEG
    } PixFormat;
    void setError(const QString& message, int code = 0);
    int testCameraPixFormat(uint v4lFmt);

    QString m_lastError;
    int m_cameraFD;
    void *m_camBuf;
    size_t m_camBufLen;
    
    PixFormat m_pixFmt;
    QSize m_frameSize;
};

#endif // V4LCAMERA_H

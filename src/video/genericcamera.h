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

#ifndef GENERICCAMERA_H
#define GENERICCAMERA_H

#include <QObject>
#include <QPair>
#include <QList>
#include <QSize>
#include <QCamera>
#include <QMutex>
#include <opencv2/core/core.hpp>

#include "mazevideo.h"

class QCameraViewfinder;

class GenericCamera : public MACamera
{
    Q_OBJECT
public:
    explicit GenericCamera(QObject *parent = 0);
    ~GenericCamera();
    QString lastError() const;

    QList<QPair<QString, QVariant>> getCameraList() const;

    bool open(QVariant cameraId, const QSize& size);
    bool close();
    bool setFramerate(double fps);

    QPair<time_t, cv::Mat> getFrame();
    bool getFrame(time_t *time, cv::Mat& buffer);

    QList<QSize> getResolutionList(QVariant cameraId);

    bool setAutoWhiteBalance(bool enabled) { Q_UNUSED(enabled); return true; };
    bool setAutoGain(bool enabled) { Q_UNUSED(enabled); return true; };
    bool setExposureTime(double val) { Q_UNUSED(val); return true; };

public slots:

private slots:
    void videoFrameReceived(const QVideoFrame &frame);
    void recvCameraError();

private:
    void setError(const QString& message, int code = 0);

    QString m_lastError;

    QCamera *m_camera;
    QVideoFrame m_lastFrame;
    QMutex m_frameMutex;

    time_t m_lastTimestamp;

    QSize m_frameSize;
};

#endif // GENERICCAMERA_H

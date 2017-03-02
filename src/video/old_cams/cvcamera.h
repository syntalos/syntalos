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

#ifndef CVCAMERA_H
#define CVCAMERA_H

#include <QObject>
#include <QPair>
#include <QList>
#include <QSize>
#include <opencv2/core/core.hpp>

#include "videotracker.h"

namespace cv {
class VideoCapture;
}

class CvCamera : public MACamera
{
    Q_OBJECT
public:
    explicit CvCamera(QObject *parent = 0);
    ~CvCamera();
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
    void setError(const QString& message, int code = 0);

    QString m_lastError;
    cv::VideoCapture *m_camera;

    QSize m_frameSize;
    cv::Mat m_mat;
};

#endif // CVCAMERA_H

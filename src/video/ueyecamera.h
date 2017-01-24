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

#ifndef UEYECAMERA_H
#define UEYECAMERA_H

#include <QObject>
#include <QPair>
#include <QList>
#include <QSize>
#include <opencv2/core/core.hpp>

#include "videotracker.h"

class UEyeCamera : public MACamera
{
    Q_OBJECT
public:
    explicit UEyeCamera(QObject *parent = 0);
    ~UEyeCamera();
    QString lastError() const;

    QList<QPair<QString, int>> getCameraList() const;

    bool open(int cameraId, const QSize& size);
    bool close();
    bool setFramerate(double fps);

    QPair<time_t, cv::Mat> getFrame();
    void getFrame(time_t *time, cv::Mat& buffer);

    bool setAutoWhiteBalance(bool enabled);
    bool setAutoGain(bool enabled);
    bool setExposureTime(double val);

    void setConfFile(const QString& fileName);
    QString confFile() const;

    bool setGPIOFlash(bool enabled);

    QList<QSize> getResolutionList(int cameraId);

public slots:

private:
    bool checkInit();
    void setError(const QString& message, int code = 0);

    QString m_lastError;
    uint32_t m_hCam;

    QSize m_frameSize;
    cv::Mat m_mat;

    QString m_confFile;
};

#endif // UEYECAMERA_H

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

#include "camera.h"

#include <QFileInfo>
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <QDebug>

#pragma GCC diagnostic ignored "-Wpadded"
class CameraData
{
public:
    CameraData()
        : camId(0),
          connected(false),
          failed(false),
          droppedFramesCount(0)
    {
    }

    std::chrono::time_point<steady_hr_clock> startTime;
    cv::VideoCapture cam;
    int camId;

    cv::Size frameSize;

    bool connected;
    bool failed;

    double exposure;
    double gain;

    uint droppedFramesCount;

    QString lastError;
};
#pragma GCC diagnostic pop


Camera::Camera()
    : d(new CameraData())
{
    d->exposure = 1;
    d->gain = 0;
    d->frameSize = cv::Size(640, 480);
}

Camera::~Camera()
{
    disconnect();
}

void Camera::fail(const QString& msg)
{
    d->failed = true;
    d->lastError = msg;
}

void Camera::setCamId(int id)
{
    d->camId = id;
}

int Camera::camId() const
{
    return d->camId;
}

void Camera::setStartTime(std::chrono::time_point<steady_hr_clock> time)
{
    d->startTime = time;
}

void Camera::setResolution(const cv::Size& size)
{
    d->frameSize = size;
}

cv::Size Camera::resolution() const
{
    return d->frameSize;
}

void Camera::setExposure(double value)
{
    if (floor(value) == 0)
        value = 1;
    if (value > 100)
        value = 100;

    // NOTE: With V4L as backend, 255 seems to be the max value here

    d->exposure = value;
    d->cam.set(cv::CAP_PROP_BRIGHTNESS, value * 2.55);
}

double Camera::exposure() const
{
    return d->exposure;
}

void Camera::setGain(double value)
{
    // NOTE: With V4L as backend, 100 seems to be the max value here

    d->gain = value;
    d->cam.set(cv::CAP_PROP_GAIN, value);
}

double Camera::gain() const
{
    return d->gain;
}

bool Camera::connect()
{
    if (d->connected) {
        if (d->failed) {
            qDebug() << "Reconnecting camera" << d->camId << "to recover from previous failure.";
            disconnect();
        } else {
            qWarning() << "Tried to reconnect already connected camera.";
            return false;
        }
    }

    d->cam.open(d->camId, cv::CAP_V4L);
    d->cam.set(cv::CAP_PROP_FRAME_WIDTH, d->frameSize.width);
    d->cam.set(cv::CAP_PROP_FRAME_HEIGHT, d->frameSize.height);

    // Apparently, setting this to 1 *disables* auto exposure for most cameras when V4L
    // is used and gives us manual control. This is a bit insane, and maybe we need to expose
    // this as a setting in case we find cameras that behave differently.
    // The values for this setting, according to some docs, are:
    // 0: Auto Mode 1: Manual Mode 2: Shutter Priority Mode 3: Aperture Priority Mode
    d->cam.set(cv::CAP_PROP_AUTO_EXPOSURE, 1);

    // set default values
    setExposure(d->exposure);
    setGain(d->gain);

    d->failed = false;
    d->connected = true;

    d->startTime = currentTimePoint();

    qDebug() << "Initialized camera" << d->camId;
    return true;
}

void Camera::disconnect()
{
    d->cam.release();
    if (d->connected)
        qDebug() << "Disconnected camera" << d->camId;
    d->connected = false;
}

bool Camera::recordFrame(cv::Mat *frame, std::chrono::milliseconds *timestamp)
{
    auto status = d->cam.grab();
    (*timestamp) = timeDiffToNowMsec(d->startTime);
    if (!status) {
        fail("Failed to grab frame.");
        return false;
    }

    try {
        status = d->cam.retrieve(*frame);
    } catch (const cv::Exception& e) {
        status = false;
        std::cerr << "Caught OpenCV exception:" << e.what() << std::endl;
    }

    if (!status) {
        d->droppedFramesCount++;
        if (d->droppedFramesCount > 10) {
            qWarning() << "Too many dropped frames on camera" << d->camId << " - Reconnecting Camera...";
            d->cam.release();
            d->cam.open(d->camId);
            qInfo() << "Camera reconnected.";
        }

        if (d->droppedFramesCount > 80)
            fail("Too many dropped frames. Giving up.");
        return false;
    }

    // adjust to selected resolution
    cv::resize((*frame), (*frame), d->frameSize);

    return true;
}

QString Camera::lastError() const
{
    return d->lastError;
}

QList<QPair<QString, int> > Camera::availableCameras()
{
    QList<QPair<QString, int>> res;

    // we just iterate over all IDs, dirty but effective
    int deviceId = 0;
    int notfoundCount = 0;
    while (true) {
        const auto devicePath = QStringLiteral("/dev/video%1").arg(deviceId);
        QFileInfo cf(devicePath);

        if (cf.exists()) {
            const auto nameInfoPath = QStringLiteral("/sys/class/video4linux/video%1/name").arg(deviceId);

            QString deviceName;
            QFile f(nameInfoPath);
            if (f.open(QFile::ReadOnly | QFile::Text)) {
                QTextStream ts(&f);
                deviceName = ts.readAll();
                deviceName = deviceName.simplified();
                if (deviceName.isEmpty())
                    deviceName = QStringLiteral("Camera %1").arg(deviceId);
            } else {
                deviceName = QStringLiteral("Camera %1").arg(deviceId);
            }

            res.append(qMakePair(deviceName, deviceId));
            deviceId++;
        } else {
            // sometimes, a few indices may be missing, so add yet another hack to
            // work around that (usually video0 disappears on some machines)
            notfoundCount++;
            deviceId++;
            if (notfoundCount >= 4)
                break;
        }
    }

    return res;
}

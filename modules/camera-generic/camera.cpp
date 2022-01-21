/*
 * Copyright (C) 2019-2022 Matthias Klumpp <matthias@tenstral.net>
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
class CameraData
{
public:
    CameraData()
        : camId(0),
          connected(false),
          failed(false),
          droppedFrameCount(0)
    {}

    std::chrono::time_point<symaster_clock> startTime;
    cv::VideoCapture *cam;
    int camId;

    int fps;
    cv::Size frameSize;

    bool connected;
    bool failed;

    double exposure;
    double exposureScaleFactor;

    double brightness;
    double brightnessScaleFactor;

    double contrast;
    double contrastScaleFactor;

    double saturation;
    double hue;

    double gain;

    uint droppedFrameCount;
    QString lastError;
};
#pragma GCC diagnostic pop

Camera::Camera()
    : d(new CameraData())
{
    // NOTE: We scale the values from 100 to the actual device value that V4L
    // apparently accepts. Hopefully this isn't device-specific, and ideally
    // this isn't even backend-specific.
    // We have to test this (otherwise we need a way to autodetect max values,
    // or have the user enter manual values which would suck)
    // OpenCV VideoCapture API really isn't ideal sometimes...
    d->exposureScaleFactor = 100;
    d->brightnessScaleFactor = 0.64;
    d->contrastScaleFactor = 0.5;

    // set some default values
    d->frameSize = cv::Size(960, 720);
    d->fps = 30;
    d->exposure = 10;
    d->brightness = 0;
    d->contrast = 0;
    d->saturation = 55;
    d->hue = 0;
    d->gain = 0;

    d->cam = new cv::VideoCapture();
}

Camera::~Camera()
{
    disconnect();
    delete d->cam;
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

void Camera::setStartTime(const symaster_timepoint &time)
{
    d->startTime = time;
}

void Camera::setResolution(const cv::Size& size)
{
    d->frameSize = size;
}

int Camera::framerate() const
{
    const int capFps = d->cam->get(cv::CAP_PROP_FPS);
    if (capFps <= 0)
        return d->fps;
    return capFps;
}

void Camera::setFramerate(int fps)
{
    d->fps = fps;
    d->cam->set(cv::CAP_PROP_FPS, d->fps);
}

cv::Size Camera::resolution() const
{
    return d->frameSize;
}

double Camera::exposure() const
{
    return d->exposure;
}

void Camera::setExposure(double value)
{
    if (floor(value) == 0)
        value = 1;
    if (value > 100)
        value = 100;

    d->exposure = value;
    d->cam->set(cv::CAP_PROP_EXPOSURE, value * d->exposureScaleFactor);
}

double Camera::brightness() const
{
    return d->brightness;
}

void Camera::setBrightness(double value)
{
    if (value > 100)
        value = 100;
    if (value < -100)
        value = -100;

    d->brightness = value;
    d->cam->set(cv::CAP_PROP_BRIGHTNESS, value * d->brightnessScaleFactor);
}

double Camera::contrast() const
{
    return d->contrast;
}

void Camera::setContrast(double value)
{
    if (floor(value) == 0)
        value = 1;
    if (value > 100)
        value = 100;

    d->contrast = value;
    d->cam->set(cv::CAP_PROP_CONTRAST, value * d->contrastScaleFactor);
}

double Camera::saturation() const
{
    return d->saturation;
}

void Camera::setSaturation(double value)
{
    if (value > 100)
        value = 100;

    d->saturation = value;
    d->cam->set(cv::CAP_PROP_SATURATION, value);
}

double Camera::hue() const
{
    return d->hue;
}

void Camera::setHue(double value)
{
    if (value > 100)
        value = 100;
    if (value < -100)
        value = -100;

    d->hue = value;
    d->cam->set(cv::CAP_PROP_HUE, value);
}

double Camera::gain() const
{
    return d->gain;
}

void Camera::setGain(double value)
{
    if (value > 100)
        value = 100;

    d->gain = value;
    d->cam->set(cv::CAP_PROP_GAIN, value);
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

    delete d->cam;
    d->cam = new cv::VideoCapture;

    auto apiPreference = cv::CAP_ANY;
#ifdef Q_OS_LINUX
    apiPreference = cv::CAP_V4L2;
#elif defined(Q_OS_WINDOWS)
    apiPreference = cv::CAP_DSHOW;
#endif
    auto ret = d->cam->open(d->camId, apiPreference);
    if (!ret) {
        // we failed opening the camera - try again using OpenCV's backend autodetection
        qDebug() << "Unable to use preferred camera backend for" << d->camId << "falling back to autodetection.";
        ret = d->cam->open(d->camId);
    }
    d->cam->set(cv::CAP_PROP_FRAME_WIDTH, d->frameSize.width);
    d->cam->set(cv::CAP_PROP_FRAME_HEIGHT, d->frameSize.height);
    d->cam->set(cv::CAP_PROP_FPS, d->fps);

    // Apparently, setting this to 1 *disables* auto exposure for most cameras when V4L
    // is used and gives us manual control. This is a bit insane, and maybe we need to expose
    // this as a setting in case we find cameras that behave differently.
    // The values for this setting, according to some docs, are:
    // 0: Auto Mode 1: Manual Mode 2: Shutter Priority Mode 3: Aperture Priority Mode
    d->cam->set(cv::CAP_PROP_AUTO_EXPOSURE, 1);

    // set default values
    setExposure(d->exposure);
    setBrightness(d->brightness);
    setContrast(d->contrast);
    setSaturation(d->saturation);
    setHue(d->hue);

    d->failed = false;
    d->connected = true;

    // temporary dummy timepoint, until the actual reference starting
    // time is set from an external source
    d->startTime = currentTimePoint();

    qDebug() << "Initialized camera" << d->camId;
    return true;
}

void Camera::disconnect()
{
    d->cam->release();
    if (d->connected)
        qDebug() << "Disconnected camera" << d->camId;
    d->connected = false;
}

bool Camera::recordFrame(Frame &frame, SecondaryClockSynchronizer *clockSync)
{
    bool status = false;
    auto frameRecvTime = FUNC_DONE_TIMESTAMP(d->startTime, status = d->cam->grab());

    // timestamp in "driver time", which usually seems to be a UNIX timestamp, but
    // we can't be sure of that
    const auto driverFrameTimestamp = microseconds_t(static_cast<time_t> (d->cam->get(cv::CAP_PROP_POS_MSEC) * 1000.0));

    // adjust the received time if necessary, gather clock sync information
    clockSync->processTimestamp(frameRecvTime, driverFrameTimestamp);

    // set the adjusted timestamp as frame time
    frame.time = usecToMsec(frameRecvTime);
    if (!status) {
        fail("Failed to grab frame.");
        return false;
    }

    try {
        status = d->cam->retrieve(frame.mat);
    } catch (const cv::Exception& e) {
        status = false;
        std::cerr << "Caught OpenCV exception:" << e.what() << std::endl;
    }

    if (!status) {
        d->droppedFrameCount++;
        if (d->droppedFrameCount > 80)
            fail("Too many dropped frames. Giving up.");
        return false;
    }

    // adjust to selected resolution
    cv::resize(frame.mat, frame.mat, d->frameSize);

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

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

#include "camera.h"

#include <array>
#include <QFileInfo>
#include <cmath>
#include <fcntl.h>
#include <libv4l2.h>
#include <linux/videodev2.h>
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <sys/ioctl.h>
#include "datactl/frametype.h"

struct CameraPropertyInfo
{
    int id;
    const char *name;
};

static QString cameraPropertyName(int propertyId)
{
    static const std::array<CameraPropertyInfo, 11> propertyNames = {{
        {cv::CAP_PROP_FRAME_WIDTH, "frame width"},
        {cv::CAP_PROP_FRAME_HEIGHT, "frame height"},
        {cv::CAP_PROP_FPS, "framerate"},
        {cv::CAP_PROP_EXPOSURE, "exposure"},
        {cv::CAP_PROP_BRIGHTNESS, "brightness"},
        {cv::CAP_PROP_CONTRAST, "contrast"},
        {cv::CAP_PROP_SATURATION, "saturation"},
        {cv::CAP_PROP_HUE, "hue"},
        {cv::CAP_PROP_GAIN, "gain"},
        {cv::CAP_PROP_AUTO_EXPOSURE, "auto exposure"},
        {cv::CAP_PROP_FOURCC, "pixel format"},
    }};

    for (const auto &property : propertyNames) {
        if (property.id == propertyId)
            return QString::fromUtf8(property.name);
    }

    return QStringLiteral("OpenCV property %1").arg(propertyId);
}

static bool isPositiveFinite(double value)
{
    return std::isfinite(value) && value > 0.0;
}

static bool differsFromRequested(double reported, double requested, double tolerance = 0.5)
{
    return !std::isfinite(reported) || std::abs(reported - requested) >= tolerance;
}

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
    {
    }

    QuillLogger *log;
    std::chrono::time_point<symaster_clock> startTime;
    cv::VideoCapture *cam{};
    int camId;

    // Fractional rates exist, e.g. V4L2: `Interval: Discrete 0.133s (7.500 fps)`
    double fps{};
    double activeFps{};
    cv::Size frameSize;
    CameraPixelFormat captureFormat;

    bool connected;
    bool failed;

    double exposure{};
    double brightness{};
    double contrast{};

    double saturation{};
    double hue{};

    double gain{};

    int autoExposureRaw{};

    uint droppedFrameCount;
    QString lastError;
};
#pragma GCC diagnostic pop

Camera::Camera(QuillLogger *logger)
    : d(new CameraData())
{
    d->log = logger;

    // set some default values
    d->frameSize = cv::Size(960, 720);
    d->fps = 30;
    d->activeFps = d->fps;
    d->exposure = 10;
    d->brightness = 0;
    d->contrast = 0;
    d->saturation = 55;
    d->hue = 0;
    d->gain = 0;

    // Apparently, setting this to 1 *disables* auto exposure for most cameras when V4L
    // is used and gives us manual control. This is a bit insane, so we expose this as a
    // quirk setting for cameras that behave differently.
    // The values for this setting, according to some docs, are:
    // 0: Auto Mode 1: Manual Mode 2: Shutter Priority Mode 3: Aperture Priority Mode
    d->autoExposureRaw = 1;

    d->cam = new cv::VideoCapture();
}

Camera::~Camera()
{
    disconnect();
    delete d->cam;
}

void Camera::setLogger(QuillLogger *logger)
{
    d->log = logger;
}

void Camera::fail(const QString &msg)
{
    d->failed = true;
    d->lastError = msg;
}

std::expected<double, QString> Camera::setCameraProperty(int propertyId, double value, bool check, double tolerance)
{
    if (!d->cam || !d->cam->isOpened())
        return std::unexpected(QStringLiteral("Camera is not open."));

    const auto name = cameraPropertyName(propertyId);
    if (!d->cam->set(propertyId, value)) {
        const auto cameraValue = d->cam->get(propertyId);
        const auto msg = QStringLiteral(
                             "Failed to set camera property '%1' to %2. Camera reports %3. "
                             "For more OpenCV video I/O details, run Syntalos with: "
                             "OPENCV_LOG_LEVEL=DEBUG OPENCV_VIDEOIO_DEBUG=1")
                             .arg(name)
                             .arg(value)
                             .arg(cameraValue);
        LOG_WARNING(
            d->log,
            "{}",
            msg);
        return std::unexpected(msg);
    }

    const auto reportedValue = d->cam->get(propertyId);
    if (check)
        warnIfCameraReportsDifferent(propertyId, value, reportedValue, tolerance);

    return reportedValue;
}

void Camera::warnIfCameraReportsDifferent(
    int propertyId,
    double requestedValue,
    double reportedValue,
    double tolerance) const
{
    if (!d->cam || !d->cam->isOpened())
        return;
    if (!differsFromRequested(reportedValue, requestedValue, tolerance))
        return;

    const auto name = cameraPropertyName(propertyId);
    LOG_WARNING(
        d->log,
        "Requested camera property '{}' value {}, but the backend reports {}. "
        "Keeping the requested value for future connection attempts.",
        name,
        requestedValue,
        reportedValue);
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

void Camera::setResolution(const cv::Size &size)
{
    d->frameSize = size;

    const double requestedWidth = d->frameSize.width;
    const double requestedHeight = d->frameSize.height;
    const auto reportedWidth = setCameraProperty(cv::CAP_PROP_FRAME_WIDTH, requestedWidth, false);
    const auto reportedHeight = setCameraProperty(cv::CAP_PROP_FRAME_HEIGHT, requestedHeight, false);
    const auto backendReportsDifferentResolution =
        (reportedWidth && differsFromRequested(*reportedWidth, requestedWidth)) ||
        (reportedHeight && differsFromRequested(*reportedHeight, requestedHeight));

    if (d->cam && d->cam->isOpened() && backendReportsDifferentResolution) {
        const auto reportedWidthValue = reportedWidth ? *reportedWidth : requestedWidth;
        const auto reportedHeightValue = reportedHeight ? *reportedHeight : requestedHeight;
        LOG_WARNING(
            d->log,
            "Requested camera output resolution {}x{}, but the backend reports capture resolution {}x{}. "
            "Captured frames will be scaled to the requested output resolution.",
            requestedWidth,
            requestedHeight,
            reportedWidthValue,
            reportedHeightValue);
    }
}

double Camera::framerate() const
{
    return isPositiveFinite(d->activeFps) ? d->activeFps : d->fps;
}

void Camera::setFramerate(double fps)
{
    d->fps = fps;
    d->activeFps = d->fps;

    const auto reportedFps = setCameraProperty(cv::CAP_PROP_FPS, d->fps, true, 0.5);
    if (reportedFps && isPositiveFinite(*reportedFps)) {
        d->activeFps = *reportedFps;
    } else if (d->cam && d->cam->isOpened()) {
        const auto currentFps = d->cam->get(cv::CAP_PROP_FPS);
        if (isPositiveFinite(currentFps))
            d->activeFps = currentFps;
    }
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
    if (value > 2047)
        value = 2047;

    d->exposure = value;
    setCameraProperty(cv::CAP_PROP_EXPOSURE, value);
}

double Camera::brightness() const
{
    return d->brightness;
}

void Camera::setBrightness(double value)
{
    if (value > 255)
        value = 255;
    if (value < -100)
        value = -100;

    d->brightness = value;
    setCameraProperty(cv::CAP_PROP_BRIGHTNESS, value);
}

double Camera::contrast() const
{
    return d->contrast;
}

void Camera::setContrast(double value)
{
    if (floor(value) == 0)
        value = 1;
    if (value > 255)
        value = 255;

    d->contrast = value;
    setCameraProperty(cv::CAP_PROP_CONTRAST, value);
}

double Camera::saturation() const
{
    return d->saturation;
}

void Camera::setSaturation(double value)
{
    if (value > 255)
        value = 255;

    d->saturation = value;
    setCameraProperty(cv::CAP_PROP_SATURATION, value);
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
    setCameraProperty(cv::CAP_PROP_HUE, value);
}

double Camera::gain() const
{
    return d->gain;
}

void Camera::setGain(double value)
{
    if (value > 255)
        value = 255;

    d->gain = value;
    setCameraProperty(cv::CAP_PROP_GAIN, value);
}

int Camera::autoExposureRaw() const
{
    return d->autoExposureRaw;
}

void Camera::setAutoExposureRaw(int value)
{
    d->autoExposureRaw = value;
    setCameraProperty(cv::CAP_PROP_AUTO_EXPOSURE, d->autoExposureRaw, true, 0.5);
}

bool Camera::connect()
{
    if (d->connected) {
        if (d->failed) {
            LOG_INFO(d->log, "Reconnecting camera {} to recover from previous failure.", d->camId);
            disconnect();
        } else {
            LOG_WARNING(d->log, "Tried to reconnect already connected camera.");
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
        LOG_INFO(d->log, "Unable to use preferred camera backend for {} falling back to autodetection.", d->camId);
        ret = d->cam->open(d->camId);
    }
    if (!ret) {
        const auto msg = QStringLiteral("Unable to open camera %1.").arg(d->camId);
        LOG_ERROR(d->log, "{}", msg);
        fail(msg);
        return false;
    }

    d->failed = false;
    d->lastError.clear();

    // set initial defaults
    setPixelFormat(d->captureFormat);
    setFramerate(d->fps);
    setAutoExposureRaw(d->autoExposureRaw);
    setExposure(d->exposure);
    setBrightness(d->brightness);
    setContrast(d->contrast);
    setSaturation(d->saturation);
    setHue(d->hue);
    setGain(d->gain);

    setResolution(d->frameSize);

    // we are connected now
    d->connected = true;

    // temporary dummy timepoint, until the actual reference starting
    // time is set from an external source
    d->startTime = currentTimePoint();

    LOG_INFO(d->log, "Initialized camera {}", d->camId);
    return true;
}

void Camera::disconnect()
{
    d->cam->release();
    if (d->connected)
        LOG_INFO(d->log, "Disconnected camera {}", d->camId);
    d->connected = false;
    d->activeFps = d->fps;
}

QList<CameraPixelFormat> Camera::readPixelFormats()
{
    QList<CameraPixelFormat> result;
    if (d->camId < 0)
        return result;

    int fd = v4l2_open(qPrintable(QStringLiteral("/dev/video%1").arg(d->camId)), O_RDWR);
    if (fd == -1)
        return result;

    struct v4l2_fmtdesc fmtdesc;
    memset(&fmtdesc, 0, sizeof(fmtdesc));
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        CameraPixelFormat cpf;
        cpf.name = QString::fromUtf8((const char *)fmtdesc.description);
        cpf.fourcc = fmtdesc.pixelformat;
        result.append(cpf);

        fmtdesc.index++;
    }
    v4l2_close(fd);

    return result;
}

void Camera::setPixelFormat(const CameraPixelFormat &pixFmt)
{
    if (pixFmt.fourcc == 0 || pixFmt.name.isEmpty())
        return;

    LOG_INFO(d->log, "Setting pixel format to: {}", pixFmt.fourcc);
    d->captureFormat = pixFmt;

    const auto actualFourcc = setCameraProperty(cv::CAP_PROP_FOURCC, pixFmt.fourcc, false);
    const auto backendReportsDifferentFormat =
        actualFourcc && (!isPositiveFinite(*actualFourcc) || std::round(*actualFourcc) != pixFmt.fourcc);

    if (d->cam && d->cam->isOpened() && backendReportsDifferentFormat) {
        LOG_WARNING(
            d->log,
            "Requested camera pixel format '{}' ({}), but the backend reports {}. "
            "Keeping the selected pixel format for future connection attempts.",
            pixFmt.name,
            pixFmt.fourcc,
            *actualFourcc);
    }
}

bool Camera::recordFrame(Frame &frame, SecondaryClockSynchronizer *clockSync)
{
    bool status = false;
    auto frameRecvTime = FUNC_DONE_TIMESTAMP(d->startTime, status = d->cam->grab());

    // timestamp in "driver time", which usually seems to be a UNIX timestamp, but
    // we can't be sure of that
    const auto driverFrameTimestamp = microseconds_t(static_cast<time_t>(d->cam->get(cv::CAP_PROP_POS_MSEC) * 1000.0));

    // adjust the received time if necessary, gather clock sync information
    clockSync->processTimestamp(frameRecvTime, driverFrameTimestamp);

    // set the adjusted timestamp as frame time
    frame.time = frameRecvTime;
    if (!status) {
        fail("Failed to grab frame.");
        return false;
    }

    try {
        cv::Mat mat;
        status = d->cam->retrieve(mat);
        frame.mat = mat;
    } catch (const cv::Exception &e) {
        status = false;
        LOG_ERROR(d->log, "Caught OpenCV exception: {}", e.what());
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

QList<QPair<QString, int>> Camera::availableCameras()
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

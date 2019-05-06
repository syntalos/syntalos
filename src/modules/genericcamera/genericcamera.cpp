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

#include "genericcamera.h"

#include <QDebug>
#include <stdio.h>
#include <QFileInfo>
#include <QVideoProbe>
#include <QCameraInfo>
#include <QCameraImageCapture>

#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui_c.h>

#include "simplevprobe.h"

GenericCamera::GenericCamera(QObject *parent)
    : QObject(parent),
      m_camera(nullptr)
{
}

GenericCamera::~GenericCamera()
{
    close();
}

QList<QPair<QString, QVariant> > GenericCamera::getCameraList() const
{
    QList<QPair<QString, QVariant>> res;

    auto cameras = QCameraInfo::availableCameras();
    foreach (const QCameraInfo &cameraInfo, cameras) {
        res.append(qMakePair(cameraInfo.description(), QVariant(cameraInfo.deviceName())));
    }

    return res;
}

void GenericCamera::setError(const QString& message, int code)
{
    if (code == 0)
        m_lastError = message;
    else
        m_lastError = QString("%1 (%2)").arg(message).arg(code);
}

void GenericCamera::recvCameraError()
{
    setError(m_camera->errorString(), 0);
    qCritical() << "Camera error" << m_camera->errorString();
}

QString GenericCamera::lastError() const
{
    return m_lastError;
}

bool GenericCamera::open(QVariant cameraId, const QSize& size)
{
    m_lastTimestamp = -1;
    m_frameSize = size;
    auto camDevName = cameraId.toString();

    auto cameras = QCameraInfo::availableCameras();
    foreach (const QCameraInfo &cameraInfo, cameras) {
        if (cameraInfo.deviceName() == camDevName)
            m_camera = new QCamera(cameraInfo);
    }

    if (m_camera == nullptr) {
        // we couldn't find the camera
        setError(QStringLiteral("Unable to find the camera '%1'").arg(camDevName));
        return false;
    }

    connect(m_camera, SIGNAL(error(QCamera::Error)), this, SLOT(recvCameraError()));

    m_camera->setCaptureMode(QCamera::CaptureVideo);
    auto videoProbe = new SimpleVProbe(m_camera);
    if (videoProbe->setSource(m_camera)) {
        connect(videoProbe, &SimpleVProbe::videoFrameProbed, this, &GenericCamera::videoFrameReceived);
    } else {
        setError("Unable to attach video probe to camera.");
        return false;
    }

    // start reading images
    m_camera->start();

    auto settings = m_camera->viewfinderSettings();
    settings.setResolution(640, 480); // FIXME: Don't hardcode this!
    m_camera->setViewfinderSettings(settings);

    if (!m_lastError.isEmpty()) {
        close ();
        return false;
    }

    return true;
}

bool GenericCamera::close()
{
    if (m_camera != nullptr)
        delete m_camera;

    qDebug() << "GenericCamera closed.";
    return true;
}

bool GenericCamera::setFramerate(double fps)
{
    if (m_camera == nullptr)
        return false;
    auto settings = m_camera->viewfinderSettings();
    settings.setMaximumFrameRate(0);
    settings.setMinimumFrameRate(fps);
    m_camera->setViewfinderSettings(settings);
    return true;
}

double GenericCamera::framerate() const
{
    return m_camera->viewfinderSettings().minimumFrameRate();
}

QPair<time_t, cv::Mat> GenericCamera::getFrame()
{
    QPair<time_t, cv::Mat> frame;
    cv::Mat mat;

    bool ret = getFrame(&frame.first, mat);
    frame.second = mat;
    if (!ret)
        frame.first = -1;

    return frame;
}

bool GenericCamera::getFrame(time_t *time, cv::Mat& buffer)
{
    m_frameMutex.lock();

    QImage tmpImg;
    QImage rgb;
    cv::Mat tmpMat;

    auto frame = m_lastFrame;
    if (!frame.isValid())
        goto fail;

    // get timestamp, convert usec to msec
    (*time) = frame.startTime() / 1000;
    if ((*time) == m_lastTimestamp)
        goto fail; // frame is not new
    m_lastTimestamp = (*time);

    if (frame.map(QAbstractVideoBuffer::ReadOnly)) {
        auto format = QVideoFrame::imageFormatFromPixelFormat(frame.pixelFormat());
        tmpImg = QImage(frame.bits(), frame.width(), frame.height(), frame.bytesPerLine(), format);
    } else {
        qCritical() << "Unable to map video frame!";
        goto fail;
    }

    rgb = tmpImg.convertToFormat(QImage::Format_RGB888);
    tmpMat = cv::Mat(rgb.height(),
                     rgb.width(),
                     CV_8UC3,
                     static_cast<void*>(frame.bits()),
                     static_cast<size_t>(rgb.bytesPerLine()));
    cv::cvtColor(tmpMat, buffer, CV_BGR2RGB);

    frame.unmap();
    m_frameMutex.unlock();

    return true;
fail:
    m_frameMutex.unlock();
    return false;
}

QList<QSize> GenericCamera::getResolutionList(QVariant cameraId)
{
    QList<QSize> res;

    auto camDevName = cameraId.toString();

    QCamera *camera = nullptr;
    auto cameras = QCameraInfo::availableCameras();
    foreach (const QCameraInfo &cameraInfo, cameras) {
        if (cameraInfo.deviceName() == camDevName)
            camera = new QCamera(cameraInfo);
    }

    if (camera == nullptr) {
        // we couldn't find the camera
        setError(QStringLiteral("Unable to find the camera '%1'").arg(camDevName));
        qWarning() << "Unable to read resolutions: Camera was not found!";
        return res;
    }

    QCameraImageCapture *imageCapture = new QCameraImageCapture(camera);
    camera->start();

    res = imageCapture->supportedResolutions();
    delete camera;

    return res;
}

void GenericCamera::videoFrameReceived(const QVideoFrame &frame)
{
    if (!m_frameMutex.tryLock())
        return;
    m_lastFrame = QVideoFrame(frame);
    m_frameMutex.unlock();
}

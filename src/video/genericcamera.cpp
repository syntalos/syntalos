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
#include <QCameraViewfinder>

#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui_c.h>

#include "simplevprobe.h"

GenericCamera::GenericCamera(QObject *parent)
    : MACamera(parent),
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

    connect(m_camera, SIGNAL(error(QCamera::Error)), this, SLOT(displayCameraError()));

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
    if (!m_lastFrame.isValid())
        return false;
    if (m_lastFrame.startTime() == m_lastTimestamp)
        return false; // frame is not new

    // get timestamp
    (*time) = m_lastFrame.startTime();
    m_lastTimestamp = m_lastFrame.startTime();

    QImage tmpImg;
    if (m_lastFrame.map(QAbstractVideoBuffer::ReadOnly)) {
        auto format = QVideoFrame::imageFormatFromPixelFormat(m_lastFrame.pixelFormat());
        tmpImg = QImage(m_lastFrame.bits(), m_lastFrame.width(), m_lastFrame.height(), m_lastFrame.bytesPerLine(), format);
    } else {
        qCritical() << "Unable to map video frame!";
        return false;
    }

    auto rgb = tmpImg.convertToFormat(QImage::Format_RGB888);
    auto tmpMat = cv::Mat(rgb.height(), rgb.width(), CV_8UC3, (void*) m_lastFrame.bits(), rgb.bytesPerLine());
    tmpMat.copyTo(buffer);

    m_lastFrame.unmap();

    return true;
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

    if (m_camera == nullptr) {
        // we couldn't find the camera
        setError(QStringLiteral("Unable to find the camera '%1'").arg(camDevName));
        return res;
    }

    res = camera->supportedViewfinderResolutions();
    delete camera;

    return res;
}

void GenericCamera::videoFrameReceived(const QVideoFrame &frame)
{
    m_lastFrame = QVideoFrame(frame);
}

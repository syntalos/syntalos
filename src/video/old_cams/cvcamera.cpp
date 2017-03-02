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

#include "cvcamera.h"

#include <QDebug>
#include <ueye.h>
#include <chrono>

#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui_c.h>

using namespace std::chrono;

CvCamera::CvCamera(QObject *parent)
    : MACamera(parent)
{
    m_camera = new cv::VideoCapture();
}

CvCamera::~CvCamera()
{
    m_camera->release();
    delete m_camera;
}

QList<QPair<QString, int>> CvCamera::getCameraList() const
{
    QList<QPair<QString, int>> res;

    cv::VideoCapture camera;
    int deviceId = 0;
    while (true) {
        if (!camera.open(deviceId))
            break;

        res.append(qMakePair(QStringLiteral("Camera %1").arg(deviceId), deviceId));
        deviceId++;
    }
    camera.release();

    return res;
}

void CvCamera::setError(const QString& message, int code)
{
    if (code == 0)
        m_lastError = message;
    else
        m_lastError = QString("%1 (%2)").arg(message).arg(code);
}

QString CvCamera::lastError() const
{
    return m_lastError;
}

bool CvCamera::open(int cameraId, const QSize& size)
{
    if (cameraId < 0) {
        setError("Not initialized.");
        return false;
    }

    m_mat = cv::Mat(size.height(), size.width(), CV_8UC3);
    m_frameSize = size;
    qDebug() << "Opening camera with resolution:" << size;

    if (!m_camera->open(cameraId)) {
        setError("Unable to open device");
        return false;
    }

    return true;
}

bool CvCamera::close()
{
    return true;
}

bool CvCamera::setFramerate(double fps)
{
    m_camera->set(CV_CAP_PROP_FPS, fps);
    return true;
}

QPair<time_t, cv::Mat> CvCamera::getFrame()
{
    QPair<time_t, cv::Mat> frame;

    getFrame(&frame.first, m_mat);
    frame.second = m_mat;

    return frame;
}

void CvCamera::getFrame(time_t *time, cv::Mat& buffer)
{
    auto res = m_camera->grab();
    if (!res) {
        setError("Unable to grab frame");
        return;
    }

    // get millisecond-resolution timestamp
    auto ms = duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch());
    *time = ms.count();

    res = m_camera->retrieve(buffer);
    if (!res) {
        setError("Unable to retrieve frame");
        return;
    }

    // TODO: Check whether we can use CV_CAP_PROP_POS_MSEC here with our camera
}

QList<QSize> CvCamera::getResolutionList(int cameraId)
{
    QList<QSize> res;

    cv::VideoCapture camera;
    if (!camera.open(cameraId))
        return res;

    res.append(QSize(camera.get(CV_CAP_PROP_FRAME_WIDTH), camera.get(CV_CAP_PROP_FRAME_HEIGHT)));

    camera.release();
    return res;
}

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

#include "videotracker.h"

#include <QDebug>
#include <chrono>
#include <QThread>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "ueyecamera.h"

using namespace std::chrono;

VideoTracker::VideoTracker(QObject *parent)
    : QObject(parent),
      m_resolution(QSize(1280, 1024)),
      m_cameraId(-1)
{
    m_framerate = 20;
    m_exportResolution = QSize(1024, 768);
    m_camera = nullptr;
    m_autoGain = true;
    m_exposureTime = 8;
}

void VideoTracker::setResolution(const QSize &size)
{
    m_resolution = size;
    qDebug() << "Camera resolution selected:" << size;
}

void VideoTracker::setCameraId(int cameraId)
{
    m_cameraId = cameraId;
    qDebug() << "Selected camera:" << m_cameraId;
}

int VideoTracker::cameraId() const
{
    return m_cameraId;
}

QList<QSize> VideoTracker::resolutionList(int cameraId)
{
    auto camera = new UEyeCamera();
    auto ret = camera->getResolutionList(cameraId);
    delete camera;

    return ret;
}

void VideoTracker::setFramerate(int fps)
{
    m_framerate = fps;
    qDebug() << "Camera framerate set to" << fps << "FPS";
}

int VideoTracker::framerate() const
{
    return m_framerate;
}

void VideoTracker::emitErrorFinished(const QString &message)
{
    emit error(message);
    moveToThread(QApplication::instance()->thread()); // give back our object to the main thread
    emit finished();
}

static time_t getMsecEpoch()
{
    auto ms = duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch());
    return ms.count();
}

bool VideoTracker::openCamera()
{
    m_camera = new UEyeCamera;

    if (!m_camera->open(m_cameraId, m_resolution)) {
        emitErrorFinished(m_camera->lastError());
        delete m_camera;
        m_camera = nullptr;
        return false;
    }

    m_camera->setFramerate(m_framerate);
    m_camera->setAutoGain(m_autoGain);
    m_camera->setExposureTime(m_exposureTime);

    return true;
}

bool VideoTracker::closeCamera()
{
    if (m_running)
        return false;
    if (m_camera == nullptr)
        return false;

    delete m_camera;
    m_camera = nullptr;

    return true;
}

void VideoTracker::setUEyeConfigFile(const QString &fileName)
{
    m_uEyeConfigFile = fileName;
}

QString VideoTracker::uEyeConfigFile() const
{
    return m_uEyeConfigFile;
}

void VideoTracker::run()
{
    if (m_exportDir.isEmpty()) {
        emitErrorFinished("No visual analysis export location is set.");
        return;
    }

    if (m_mouseId.isEmpty()) {
        emitErrorFinished("No mouse ID is set.");
        return;
    }

    if (m_camera == nullptr) {
        emitErrorFinished("Camera was not opened.");
        return;
    }

    // create storage location for frames
    auto frameBaseDir = QStringLiteral("%1/frames").arg(m_exportDir);
    QDir().mkpath(frameBaseDir);
    auto frameBasePath = QStringLiteral("%1/%2_").arg(frameBaseDir).arg(m_mouseId);

    // get a first frame and store details
    auto initialFrame = m_camera->getFrame();

    auto infoPath = QStringLiteral("%1/%2_videoinfo.json").arg(m_exportDir).arg(m_mouseId);
    QJsonObject vInfo;
    vInfo.insert("frameRows", initialFrame.second.rows);
    vInfo.insert("frameCols", initialFrame.second.cols);

    vInfo.insert("exportWidth", m_exportResolution.width());
    vInfo.insert("exportHeight", m_exportResolution.height());

    QFile vInfoFile(infoPath);
    if (!vInfoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emitErrorFinished("Unable to open video info file for writing.");
        return;
    }

    QTextStream vInfoFileOut(&vInfoFile);
    vInfoFileOut << QJsonDocument(vInfo).toJson() << "\n";

    // prepare position output CSV file
    auto posInfoPath = QStringLiteral("%1/%2_positions.csv").arg(m_exportDir).arg(m_mouseId);
    QFile posInfoFile(posInfoPath);
    if (!posInfoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emitErrorFinished("Unable to open position CSV file for writing.");
        return;
    }
    QTextStream posInfoOut(&posInfoFile);
    posInfoOut << "Time (msec)" << ";" <<
                  "Red X" << ";" << "Red Y" << ";"
                  "Green X" << ";" << "Green Y" << ";"
                  "Yellow X" << ";" << "Yellow Y" << "\n";

    auto frameInterval = 1000 / m_framerate; // framerate in FPS, interval msec delay
    m_startTime = getMsecEpoch();

    m_running = true;
    while (m_running) {
        auto frame = m_camera->getFrame();
        auto timeSinceStart = frame.first - m_startTime;
        emit newFrame(timeSinceStart, frame.second);

        // store raw frame on disk
        cv::Mat emat;

        cv::resize(frame.second,
                   emat,
                   cv::Size(m_exportResolution.width(), m_exportResolution.height()));
        cv::imwrite(QString("%1%2.jpg").arg(frameBasePath).arg(timeSinceStart).toStdString(), emat);

        // do the tracking on the downscaled frame
        auto pointList = trackPoints(timeSinceStart, frame.second);
        Q_ASSERT(pointList.size() == 3); // we must always receive the position of 3 points

        // the layout of the CSV file is:
        //  time;Red X;Red Y; Green X; Green Y; Yellow X; Yellow Y

        // store time value
        posInfoOut << timeSinceStart << ";";

        // red
        posInfoOut << pointList[0].x() << ";" << pointList[0].y() << ";";
        // green
        posInfoOut << pointList[1].x() << ";" << pointList[1].y() << ";";
        // yellow
        posInfoOut << pointList[2].x() << ";" << pointList[2].y() << ";";

        posInfoOut << "\n";

        // wait the remaining time for the next frame
        auto remainingTime = frameInterval - (getMsecEpoch() - frame.first);
        if (remainingTime > 0)
            QThread::usleep(remainingTime * 1000); // sleep remainingTime msecs
    }

    m_startTime = 0;
    moveToThread(QApplication::instance()->thread()); // give back our object to the main thread
    emit finished();
    qDebug() << "Finished video.";
}

QList<QPoint> VideoTracker::trackPoints(time_t time, const cv::Mat &image)
{
    double maxVal;
    cv::Point maxLoc;
    QList<QPoint> res;

    cv::Mat trackMat(image.size(), image.type());
    cv::Mat grayMat(image.size(), image.type());
    
    cv::cvtColor(image, grayMat, cv::COLOR_RGB2GRAY);
    cv::cvtColor(grayMat, trackMat, cv::COLOR_GRAY2RGBA);
    
    cv::Mat redMaskMat(image.size(), image.type());
    cv::Mat redMat(image.size(), image.type());
    
    cv::Mat greenMaskMat(image.size(), image.type());
    cv::Mat greenMat(image.size(), image.type());
    
    cv::Mat yellowMaskMat(image.size(), image.type());
    cv::Mat yellowMat(image.size(), image.type());
    
    // colors are in BGR
    // find red colored parts and erode noise
    cv::inRange(image, cv::Scalar(0, 0, 248), cv::Scalar(160, 160, 255), redMaskMat);
    cv::morphologyEx(redMaskMat, redMaskMat, cv::MORPH_CLOSE, cv::Mat::ones(5, 5, redMaskMat.type()));

    // red maximum
    grayMat.copyTo(redMat, redMaskMat);
    cv::minMaxLoc(redMat,
                  nullptr, // minimum value
                  &maxVal,
                  nullptr, // minimum location (Point),
                  &maxLoc,
                  cv::Mat());
    cv::circle(trackMat, maxLoc, 6, cv::Scalar(255, 0, 0), -1);
    res.append(QPoint(maxLoc.x, maxLoc.y));

    // green maximum
    cv::inRange(image, cv::Scalar(0, 100, 0), cv::Scalar(140, 255, 110), greenMaskMat);
    cv::morphologyEx(greenMaskMat, greenMaskMat, cv::MORPH_CLOSE, cv::Mat::ones(5, 5, greenMaskMat.type()));
    grayMat.copyTo(greenMat, greenMaskMat);
    
    cv::minMaxLoc(greenMat,
                  nullptr, // minimum value
                  &maxVal,
                  nullptr, // minimum location (Point),
                  &maxLoc,
                  cv::Mat());
    cv::circle(trackMat, maxLoc, 6, cv::Scalar(0, 255, 0), -1);
    res.append(QPoint(maxLoc.x, maxLoc.y));

    // yellow maximum
    cv::inRange(image, cv::Scalar(0, 210, 210), cv::Scalar(50, 255, 255), yellowMaskMat);
    cv::morphologyEx(yellowMaskMat, yellowMaskMat, cv::MORPH_CLOSE, cv::Mat::ones(5, 5, yellowMaskMat.type()));
    grayMat.copyTo(yellowMat, yellowMaskMat);
    
    cv::minMaxLoc(yellowMat,
                  nullptr, // minimum value
                  &maxVal,
                  nullptr, // minimum location (Point),
                  &maxLoc,
                  cv::Mat());
    cv::circle(trackMat, maxLoc, 6, cv::Scalar(255, 255, 0), -1);
    res.append(QPoint(maxLoc.x, maxLoc.y));

    // finalize
    emit newTrackingFrame(time, trackMat);

    return res;
}

void VideoTracker::stop()
{
    m_running = false;
}

void VideoTracker::setDataLocation(const QString& dir)
{
    m_exportDir = dir;
}

void VideoTracker::setMouseId(const QString& mid)
{
    m_mouseId = mid;
}

void VideoTracker::setAutoGain(bool enabled)
{
    m_autoGain = enabled;
}

QList<QPair<QString, int>> VideoTracker::getCameraList() const
{
    auto camera = new UEyeCamera();
    auto ret = camera->getCameraList();
    delete camera;

    return ret;
}

void VideoTracker::setExportResolution(const QSize& size)
{
    m_exportResolution = size;
}

QSize VideoTracker::exportResolution() const
{
    return m_exportResolution;
}

void VideoTracker::setExposureTime(double value)
{
    m_exposureTime = value;
    qDebug() << "Exposure time set to" << value;
}

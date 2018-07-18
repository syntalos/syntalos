/*
 * Copyright (C) 2016-2017 Matthias Klumpp <matthias@tenstral.net>
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

#include "config.h"
#include "mazevideo.h"

#include <QDebug>
#include <QThread>
#include <QApplication>
#include <QFile>
#include <QDir>

#include <ktar.h>

#ifdef USE_UEYE_CAMERA
#include "ueyecamera.h"
#endif
#include "genericcamera.h"
#include "utils.h"
#include "tracker.h"


MazeVideo::MazeVideo(QObject *parent)
    : QObject(parent),
      m_resolution(QSize(1280, 1024)),
      m_cameraId(-1),
      m_tracker(nullptr)
{
    m_framerate = 20;
    m_exportResolution = QSize(1024, 768);
    m_camera = nullptr;
    m_autoGain = false;
    m_exposureTime = 8;
}

QString MazeVideo::lastError() const
{
    return m_lastError;
}

void MazeVideo::setResolution(const QSize &size)
{
    m_resolution = size;
    qDebug() << "Camera resolution selected:" << size;
}

void MazeVideo::setCameraId(QVariant cameraId)
{
    m_cameraId = cameraId;
    qDebug() << "Selected camera:" << m_cameraId;
}

QVariant MazeVideo::cameraId() const
{
    return m_cameraId;
}

static bool qsizeBiggerThan(const QSize &s1, const QSize &s2)
{
    auto s1v = s1.width() + s1.height();
    auto s2v = s2.width() + s2.height();

    return s1v > s2v;
}

QList<QSize> MazeVideo::resolutionList(QVariant cameraId)
{
#ifdef USE_UEYE_CAMERA
    auto camera = new UEyeCamera();
#else
    auto camera = new GenericCamera();
#endif
    auto ret = camera->getResolutionList(cameraId);
    delete camera;

    qSort(ret.begin(), ret.end(), qsizeBiggerThan);
    return ret;
}

void MazeVideo::setFramerate(int fps)
{
    m_framerate = fps;
    qDebug() << "Camera framerate set to" << fps << "FPS";
}

int MazeVideo::framerate() const
{
    return m_framerate;
}

void MazeVideo::emitErrorFinished(const QString &message)
{
    emit error(message);
    m_lastError = message;

    // we no longer need the camera, it's safe to close it
    closeCamera();
    emit finished();
}

bool MazeVideo::openCamera()
{
#ifdef USE_UEYE_CAMERA
    m_camera = new UEyeCamera;
#else
    m_camera = new GenericCamera;
#endif

    if (!m_camera->open(m_cameraId, m_resolution)) {
        emitErrorFinished(m_camera->lastError());
        delete m_camera;
        m_camera = nullptr;
        return false;
    }

    m_camera->setConfFile(m_uEyeConfigFile);
    m_camera->setAutoGain(m_autoGain);
    m_camera->setExposureTime(m_exposureTime);
    m_camera->setFramerate(m_framerate);
    m_camera->setGPIOFlash(m_gpioFlash);

    return true;
}

bool MazeVideo::closeCamera()
{
    if (m_tracker != nullptr)
        return false;
    if (m_camera == nullptr)
        return false;

    delete m_camera;
    m_camera = nullptr;

    return true;
}

void MazeVideo::setUEyeConfigFile(const QString &fileName)
{
    m_uEyeConfigFile = fileName;
}

QString MazeVideo::uEyeConfigFile() const
{
    return m_uEyeConfigFile;
}

void MazeVideo::setGPIOFlash(bool enabled)
{
    m_gpioFlash = enabled;
}

bool MazeVideo::gpioFlash() const
{
    return m_gpioFlash;
}

void MazeVideo::setTrackingEnabled(bool enabled)
{
    m_trackingEnabled = enabled;
}

void MazeVideo::stop()
{
    if (m_tracker == nullptr)
        return;
    m_tracker->stop();

    // QObject::deleteLater will take care of actually removing the instance
    // once it thread has terminated.
    m_tracker = nullptr;
}

void MazeVideo::setDataLocation(const QString& dir)
{
    m_exportDir = dir;
}

void MazeVideo::setSubjectId(const QString& mid)
{
    m_subjectId = mid;
}

void MazeVideo::setAutoGain(bool enabled)
{
    m_autoGain = enabled;
}

QList<QPair<QString, QVariant>> MazeVideo::getCameraList() const
{
#ifdef USE_UEYE_CAMERA
    auto camera = new UEyeCamera;
#else
    auto camera = new GenericCamera;
#endif
    auto ret = camera->getCameraList();
    delete camera;

    return ret;
}

void MazeVideo::setExportResolution(const QSize& size)
{
    m_exportResolution = size;
}

QSize MazeVideo::exportResolution() const
{
    return m_exportResolution;
}

void MazeVideo::setExposureTime(double value)
{
    m_exposureTime = value;
    qDebug() << "Exposure time set to" << value;
}

void MazeVideo::run(Barrier barrier)
{
    if (m_exportDir.isEmpty()) {
        emitErrorFinished("No visual analysis export location is set.");
        return;
    }

    if (m_subjectId.isEmpty()) {
        emitErrorFinished("No subject ID is set.");
        return;
    }

    if (m_camera == nullptr) {
        emitErrorFinished("Camera was not opened.");
        return;
    }

    if (m_tracker != nullptr && m_tracker->running()) {
        emitErrorFinished("Can not start an already running recording.");
        return;
    }

    // create storage location for frames
    auto frameBaseDir = QStringLiteral("%1/frames").arg(m_exportDir);
    QDir().mkpath(frameBaseDir);
    auto frameBasePath = QStringLiteral("%1/%2_").arg(frameBaseDir).arg(m_subjectId);

    auto vThread = new QThread;
    m_tracker = new Tracker(barrier, m_camera, m_framerate, m_exportDir, frameBasePath,
                            m_subjectId, m_exportResolution);
    m_tracker->moveToThread(vThread);

    if (m_trackingEnabled) {
        connect(vThread, &QThread::started, [&]() {
            m_tracker->runTracking();
        });
    } else {
        connect(vThread, &QThread::started, [&]() {
            m_tracker->runRecordingOnly();
        });
    }

    // connect the actions that happen on thread and tracker termination
    connect(m_tracker, &Tracker::finished, [&](bool success, const QString& errorMessage) {
        if (success) {
            emit finished();
        } else {
            emitErrorFinished(errorMessage);
        }

        // we no longer need the camera, it's safe to close it
        closeCamera();

        qDebug() << "Finished video.";
    });

    connect(vThread, &QThread::finished, vThread, &QObject::deleteLater);
    connect(vThread, &QThread::finished, m_tracker, &QObject::deleteLater);
    connect(vThread, &QThread::finished, [&]() { m_tracker = nullptr; });

    // connect the image events
    connect(m_tracker, &Tracker::newFrame, [&](time_t time, const cv::Mat image) { emit newFrame(time, image); });
    connect(m_tracker, &Tracker::newTrackingFrame, [&](time_t time, const cv::Mat image) { emit newTrackingFrame(time, image); });
    connect(m_tracker, &Tracker::newInfoGraphic, [&](const cv::Mat image) { emit newInfoGraphic(image); });

    // actually start the new thread
    vThread->start();
}

bool MazeVideo::makeFrameTarball()
{
    auto frameDirPath = QStringLiteral("%1/frames").arg(m_exportDir);
    QDir frameDir(frameDirPath);

    // check if we have work to do
    if (!frameDir.exists())
        return true;

    auto frameTarFname = QStringLiteral("%1/%2_frames.tar.gz").arg(m_exportDir).arg(m_subjectId);
    KTar tar(frameTarFname);
    if (!tar.open(QIODevice::WriteOnly)) {
        m_lastError = "Unable to open tarball for writing.";
        return false;
    }

    auto files = frameDir.entryList(QDir::Files);
    auto max = files.count();

    auto i = 0;
    foreach (auto fname, files) {
        if (!tar.addLocalFile(frameDir.absoluteFilePath(fname), fname)) {
            m_lastError = QStringLiteral("Could not add frame '%1' image to tarball.").arg(fname);
            return false;
        }
        emit progress(max, i);
        i++;
    }

    tar.close();
    frameDir.removeRecursively();

    return true;
}

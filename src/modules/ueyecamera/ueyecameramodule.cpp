/*
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
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

#include "ueyecameramodule.h"

#include <QDir>
#include <QMutexLocker>
#include <QMessageBox>
#include <opencv2/opencv.hpp>

#include "ueyecamera.h"
#include "videoviewwidget.h"
#include "ueyecamerasettingsdialog.h"

#include "modules/videorecorder/videowriter.h"

QString UEyeCameraModuleInfo::id() const
{
    return QStringLiteral("ueye-camera");
}

QString UEyeCameraModuleInfo::name() const
{
    return QStringLiteral("uEye Camera");
}

QString UEyeCameraModuleInfo::description() const
{
    return QStringLiteral("Capture video with an IDS camera that is compatible with the uEye API.");
}

QPixmap UEyeCameraModuleInfo::pixmap() const
{
    return QPixmap(":/module/ueye-camera");
}

AbstractModule *UEyeCameraModuleInfo::createModule(QObject *parent)
{
    return new UEyeCameraModule(parent);
}

UEyeCameraModule::UEyeCameraModule(QObject *parent)
    : ImageSourceModule(parent),
      m_camera(nullptr),
      m_videoView(nullptr),
      m_camSettingsWindow(nullptr),
      m_thread(nullptr)
{
    m_camera = new UEyeCamera;

    m_frameRing = boost::circular_buffer<Frame>(64);

    m_videoView = new VideoViewWidget;
    m_camSettingsWindow = new UEyeCameraSettingsDialog(m_camera);
    addDisplayWindow(m_videoView);
    addSettingsWindow(m_camSettingsWindow);

    // set all window titles
    setName(name());
}

UEyeCameraModule::~UEyeCameraModule()
{
    finishCaptureThread();
}

void UEyeCameraModule::setName(const QString &name)
{
    ImageSourceModule::setName(name);
    m_videoView->setWindowTitle(name);
    m_camSettingsWindow->setWindowTitle(QStringLiteral("Settings for %1").arg(name));
}

void UEyeCameraModule::attachVideoWriter(VideoWriter *vwriter)
{
    m_vwriters.append(vwriter);
}

int UEyeCameraModule::selectedFramerate() const
{
    return m_camSettingsWindow->framerate();
}

cv::Size UEyeCameraModule::selectedResolution() const
{
    return m_camSettingsWindow->resolution();
}

bool UEyeCameraModule::prepare()
{
    m_started = false;
    if (!startCaptureThread())
        return false;

    return true;
}

void UEyeCameraModule::start()
{
    m_started = true;
    statusMessage("Acquiring frames...");
}

bool UEyeCameraModule::runEvent()
{
    m_mutex.lock();

    if (m_frameRing.size() == 0) {
        m_mutex.unlock();
        return true;
    }

    if (!m_frameRing.empty()) {
        auto frame = m_frameRing.front();
        m_videoView->showImage(frame.mat);
        m_frameRing.pop_front();

        // send frame away to connected image sinks, and hope they are
        // handling this efficiently and don't block the loop
        emit newFrame(frame);

        auto statusText = QStringLiteral("<html>Display buffer: %1/%2").arg(m_frameRing.size()).arg(m_frameRing.capacity());

        // END OF SAFE ZONE
        m_mutex.unlock();

        // warn if there is a bigger framerate drop
        if (m_currentFps < (m_fps - 2))
            statusText = QStringLiteral("%1 - <font color=\"red\"><b>Framerate is too low!</b></font>").arg(statusText);
        statusMessage(statusText);

        // show framerate directly in the window title, to make reduced framerate very visible
        m_videoView->setWindowTitle(QStringLiteral("%1 (%2 fps)").arg(m_name).arg(m_currentFps));
    } else {
        m_mutex.unlock();
    }

    return true;
}

void UEyeCameraModule::stop()
{
    finishCaptureThread();
}

QByteArray UEyeCameraModule::serializeSettings(const QString &confBaseDir)
{
    QDir cdir(confBaseDir);
    QJsonObject videoSettings;
    videoSettings.insert("camera", m_camera->camId());
    videoSettings.insert("width", m_camSettingsWindow->resolution().width);
    videoSettings.insert("height", m_camSettingsWindow->resolution().height);
    videoSettings.insert("fps", m_camSettingsWindow->framerate());
    videoSettings.insert("autoGain", m_camSettingsWindow->automaticGain());
    videoSettings.insert("exposureTime", m_camSettingsWindow->exposure());
    videoSettings.insert("uEyeConfig", cdir.relativeFilePath(m_camSettingsWindow->uEyeConfigFile()));
    videoSettings.insert("gpioFlash", m_camSettingsWindow->gpioFlash());

    return jsonObjectToBytes(videoSettings);
}

bool UEyeCameraModule::loadSettings(const QString&, const QByteArray &data)
{
    auto jsettings = jsonObjectFromBytes(data);

    m_camSettingsWindow->setCameraId(jsettings.value("camera").toInt());
    m_camSettingsWindow->setResolution(cv::Size(jsettings.value("width").toInt(), jsettings.value("height").toInt()));
    m_camSettingsWindow->setFramerate(jsettings.value("fps").toInt());
    m_camSettingsWindow->setGpioFlash(jsettings.value("gpioFlash").toBool());
    m_camSettingsWindow->setAutomaticGain(jsettings.value("autoGain").toBool());
    m_camSettingsWindow->setExposure(jsettings.value("exposureTime").toDouble());
    m_camSettingsWindow->setUEyeConfigFile(jsettings.value("uEyeConfig").toString());

    return true;
}

void UEyeCameraModule::captureThread(void *gcamPtr)
{
    UEyeCameraModule *self = static_cast<UEyeCameraModule*> (gcamPtr);

    self->m_currentFps = self->m_fps;
    auto firstFrame = true;
    time_t startTime = 0;
    auto frameRecordFailedCount = 0;

    while (self->m_running) {
        const auto cycleStartTime = currentTimePoint();

        // wait until we actually start
        while (!self->m_started) { }

        time_t time;
        auto frame = self->m_camera->getFrame(&time);
        if (frame.empty()) {
            frameRecordFailedCount++;
            if (frameRecordFailedCount > 32) {
                self->m_running = false;
                self->raiseError(QStringLiteral("Too many attempts to fetch frames from this camera have failed. Is the camera connected properly?"));
            }
            continue;
        }

        // assume first frame is starting point
        if (firstFrame) {
            firstFrame = false;
            startTime = time;
        }
        auto timestampMsec = std::chrono::milliseconds(time - startTime);

        // record this frame, if we have any video writers registered
        for (auto &vwriter : self->m_vwriters)
            vwriter->pushFrame(frame, timestampMsec);

        self->m_mutex.lock();
        self->m_frameRing.push_back(Frame(frame, timestampMsec));
        self->m_mutex.unlock();

        // wait a bit if necessary, to keep the right framerate
        const auto cycleTime = timeDiffToNowMsec(cycleStartTime);
        const auto extraWaitTime = std::chrono::milliseconds((1000 / self->m_fps) - cycleTime.count());
        if (extraWaitTime.count() > 0)
            std::this_thread::sleep_for(extraWaitTime);

        const auto totalTime = timeDiffToNowMsec(cycleStartTime);
        self->m_currentFps = static_cast<int>(1 / (totalTime.count() / static_cast<double>(1000)));
    }
}

bool UEyeCameraModule::startCaptureThread()
{
    finishCaptureThread();

    statusMessage("Connecting camera...");
    if (!m_camera->open(m_camSettingsWindow->resolution())) {
        raiseError(QStringLiteral("Unable to connect camera: %1").arg(m_camera->lastError()));
        return false;
    }
    statusMessage("Launching DAQ thread...");

    m_camSettingsWindow->setRunning(true);
    m_fps = m_camSettingsWindow->framerate();
    m_running = true;
    m_thread = new std::thread(captureThread, this);
    statusMessage("Waiting.");
    return true;
}

void UEyeCameraModule::finishCaptureThread()
{
    if (!initialized())
        return;

    // ensure we unregister all video writers before starting another run,
    // and after finishing the current one, as the modules they belong to
    // may meanwhile have been removed
    m_vwriters.clear();

    statusMessage("Cleaning up...");
    if (m_thread != nullptr) {
        m_running = false;
        m_started = true;
        m_thread->join();
        delete m_thread;
        m_thread = nullptr;
        m_started = false;
    }
    m_camera->disconnect();
    m_camSettingsWindow->setRunning(false);
    statusMessage("Camera disconnected.");
}

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

#include "genericcameramodule.h"

#include <QMutexLocker>
#include <QMessageBox>
#include <opencv2/opencv.hpp>

#include "camera.h"
#include "videoviewwidget.h"
#include "genericcamerasettingsdialog.h"

#include "modules/videorecorder/videowriter.h"

GenericCameraModule::GenericCameraModule(QObject *parent)
    : ImageSourceModule(parent),
      m_camera(nullptr),
      m_videoView(nullptr),
      m_camSettingsWindow(nullptr),
      m_thread(nullptr)
{
    m_name = QStringLiteral("Generic Camera");
    m_camera = new Camera;

    m_frameRing = boost::circular_buffer<FrameData>(32);
}

GenericCameraModule::~GenericCameraModule()
{
    finishCaptureThread();
    if (m_videoView != nullptr)
        delete m_videoView;
    if (m_camSettingsWindow != nullptr)
        delete m_camSettingsWindow;
}

QString GenericCameraModule::id() const
{
    return QStringLiteral("generic-camera");
}

QString GenericCameraModule::description() const
{
    return QStringLiteral("Capture a video with a regular, Linux-compatible camera.");
}

QPixmap GenericCameraModule::pixmap() const
{
    return QPixmap(":/module/generic-camera");
}

void GenericCameraModule::setName(const QString &name)
{
    ImageSourceModule::setName(name);
    if (initialized()) {
        m_videoView->setWindowTitle(name);
        m_camSettingsWindow->setWindowTitle(QStringLiteral("Settings for %1").arg(name));
    }
}

void GenericCameraModule::attachVideoWriter(VideoWriter *vwriter)
{
    m_vwriters.append(vwriter);
}

double GenericCameraModule::selectedFramerate() const
{
    assert(initialized());
    return static_cast<double>(m_camSettingsWindow->selectedFps());
}

cv::Size GenericCameraModule::selectedResolution() const
{
    assert(initialized());
    return m_camSettingsWindow->selectedSize();
}

bool GenericCameraModule::initialize(ModuleManager *manager)
{
    assert(!initialized());
    Q_UNUSED(manager);

    m_videoView = new VideoViewWidget;
    m_camSettingsWindow = new GenericCameraSettingsDialog(m_camera);

    setState(ModuleState::READY);
    setInitialized();

    // set all window titles
    setName(name());

    return true;
}

bool GenericCameraModule::prepare(HRTimer *timer)
{
    m_started = false;
    m_timer = timer;

    setState(ModuleState::PREPARING);
    if (!startCaptureThread())
        return false;
    setState(ModuleState::WAITING);
    return true;
}

void GenericCameraModule::start()
{
    m_started = true;
    m_camera->setStartTime(m_timer->startTime());
    statusMessage("Acquiring frames...");
    setState(ModuleState::RUNNING);
}

bool GenericCameraModule::runCycle()
{
    QMutexLocker locker(&m_mutex);

    if (m_frameRing.size() == 0)
        return true;

    if (!m_frameRing.empty()) {
        auto frameInfo = m_frameRing.front();
        m_videoView->showImage(frameInfo.first);
        m_frameRing.pop_front();

        // send frame away to connected image sinks, and hope they are
        // handling this efficiently and don't block the loop
        emit newFrame(frameInfo);

        // show framerate directly in the window title, to make reduced framerate very visible
        m_videoView->setWindowTitle(QStringLiteral("%1 (%2 fps)").arg(m_name).arg(m_currentFps));
    }

    return true;
}

void GenericCameraModule::stop()
{
    finishCaptureThread();
}

void GenericCameraModule::showDisplayUi()
{
    assert(initialized());
    m_videoView->show();
}

void GenericCameraModule::hideDisplayUi()
{
    assert(initialized());
    m_videoView->hide();
}

void GenericCameraModule::showSettingsUi()
{
    assert(initialized());
    m_camSettingsWindow->show();
}

void GenericCameraModule::hideSettingsUi()
{
    assert(initialized());
    m_camSettingsWindow->hide();
}

void GenericCameraModule::captureThread(void *gcamPtr)
{
    GenericCameraModule *self = static_cast<GenericCameraModule*> (gcamPtr);

    self->m_currentFps = self->m_fps;

    while (self->m_running) {
        const auto cycleStartTime = currentTimePoint();

        // wait until we actually start
        while (!self->m_started) { }

        cv::Mat frame;
        std::chrono::milliseconds time;
        if (!self->m_camera->recordFrame(&frame, &time)) {
            continue;
        }

        // record this frame, if we have any video writers registered
        Q_FOREACH(auto vwriter, self->m_vwriters)
            vwriter->pushFrame(frame, time);

        self->m_mutex.lock();
        self->m_frameRing.push_back(std::pair<cv::Mat, std::chrono::milliseconds>(frame, time));
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

bool GenericCameraModule::startCaptureThread()
{
    finishCaptureThread();

    statusMessage("Connecting camera...");
    if (!m_camera->connect()) {
        raiseError(QStringLiteral("Unable to connect camera: %1").arg(m_camera->lastError()));
        return false;
    }
    m_camera->setResolution(m_camSettingsWindow->selectedSize());
    statusMessage("Launching DAQ thread...");

    m_camSettingsWindow->setRunning(true);
    m_fps = m_camSettingsWindow->selectedFps();
    m_running = true;
    m_thread = new std::thread(captureThread, this);
    statusMessage("Waiting.");
    return true;
}

void GenericCameraModule::finishCaptureThread()
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
        m_thread->join();
        delete m_thread;
        m_thread = nullptr;
    }
    m_camera->disconnect();
    m_camSettingsWindow->setRunning(false);
    statusMessage("Camera disconnected.");
}

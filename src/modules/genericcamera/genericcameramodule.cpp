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

#include <QMessageBox>

#include "videoviewwidget.h"
#include "genericcamera.h"
#include "genericcamerasettingsdialog.h"

GenericCameraModule::GenericCameraModule(QObject *parent)
    : ImageSourceModule(parent),
      m_videoView(nullptr),
      m_camSettingsWindow(nullptr),
      m_camera(nullptr),
      m_thread(nullptr)
{
    m_name = QStringLiteral("Generic Camera");
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

double GenericCameraModule::selectedFramerate() const
{
    assert(initialized());
    return m_camera->framerate();
}

bool GenericCameraModule::initialize(ModuleManager *manager)
{
    assert(!initialized());
    Q_UNUSED(manager);

    m_camera = new GenericCamera(this);
    m_videoView = new VideoViewWidget;
    m_camSettingsWindow = new GenericCameraSettingsDialog(m_camera);


    setState(ModuleState::READY);
    setInitialized();
    return true;
}

bool GenericCameraModule::prepareThreads()
{
    startCaptureThread();
}

bool GenericCameraModule::runCycle()
{
    //
}

void GenericCameraModule::stop()
{

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
    m_camSettingsWindow->show();
}

void GenericCameraModule::captureThread(void *gcamPtr)
{
    GenericCameraModule *self = static_cast<GenericCameraModule*> (gcamPtr);

    while (self->m_running) {

        // wait until we actually start
        while (!self->m_started) { }
    }
}

void GenericCameraModule::startCaptureThread()
{
    finishCaptureThread();
    m_running = true;
    m_thread = new std::thread(captureThread, this);
}

void GenericCameraModule::finishCaptureThread()
{
    if (m_thread != nullptr) {
        m_running = false;
        m_thread->join();
        delete m_thread;
        m_thread = nullptr;
    }
}

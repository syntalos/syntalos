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

#ifndef UEYECAMERAMODULE_H
#define UEYECAMERAMODULE_H

#include <QObject>
#include <thread>
#include <atomic>
#include <QQueue>
#include <QMutex>
#include <boost/circular_buffer.hpp>

#include "moduleapi.h"
#include "streams/frametype.h"

class UEyeCameraModuleInfo : public ModuleInfo
{
    Q_OBJECT
public:
    QString id() const override;
    QString name() const override;
    QString description() const override;
    QPixmap pixmap() const override;
    AbstractModule *createModule(QObject *parent = nullptr) override;
};

class UEyeCamera;
class VideoViewWidget;
class UEyeCameraSettingsDialog;

class UEyeCameraModule : public AbstractModule
{
    Q_OBJECT
public:
    explicit UEyeCameraModule(QObject *parent = nullptr);
    ~UEyeCameraModule() override;

    void setName(const QString& name) override;

    bool prepare(const QString& storageRootDir, const TestSubject& testSubject) override;
    void start() override;
    bool runUIEvent() override;

    void stop() override;

    QByteArray serializeSettings(const QString& confBaseDir) override;
    bool loadSettings(const QString& confBaseDir, const QByteArray& data) override;

private:
    UEyeCamera *m_camera;
    VideoViewWidget *m_videoView;
    UEyeCameraSettingsDialog *m_camSettingsWindow;

    std::thread *m_thread;
    QMutex m_mutex;
    std::atomic_bool m_running;
    std::atomic_bool m_started;
    std::atomic_int m_currentFps;
    int m_fps;
    boost::circular_buffer<Frame> m_frameRing;

    static void captureThread(void *gcamPtr);
    bool startCaptureThread();
    void finishCaptureThread();
};

#endif // UEYECAMERAMODULE_H

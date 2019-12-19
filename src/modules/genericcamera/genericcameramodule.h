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

#ifndef GENERICCAMERAMODULE_H
#define GENERICCAMERAMODULE_H

#include <QObject>
#include <thread>
#include <atomic>
#include <boost/circular_buffer.hpp>
#include <QQueue>
#include <QMutex>

#include "streams/frametype.h"
#include "imagesourcemodule.h"
#include "moduleapi.h"

class Camera;
class VideoViewWidget;
class GenericCameraSettingsDialog;

class GenericCameraModuleInfo : public ModuleInfo
{
    Q_OBJECT
public:
    QString id() const override;
    QString name() const override;
    QString description() const override;
    QPixmap pixmap() const override;
    AbstractModule *createModule(QObject *parent = nullptr) override;
};

class GenericCameraModule : public ImageSourceModule
{
    Q_OBJECT
public:
    explicit GenericCameraModule(QObject *parent = nullptr);
    ~GenericCameraModule() override;

    void setName(const QString& name) override;

    void attachVideoWriter(VideoWriter *vwriter) override;

    int selectedFramerate() const override;
    cv::Size selectedResolution() const override;

    bool initialize(ModuleManager *manager) override;

    bool prepare() override;
    void start() override;
    bool runEvent() override;

    void stop() override;

    QByteArray serializeSettings(const QString& confBaseDir) override;
    bool loadSettings(const QString& confBaseDir, const QByteArray& data) override;

private:
    Camera *m_camera;
    VideoViewWidget *m_videoView;
    GenericCameraSettingsDialog *m_camSettingsWindow;

    QList<VideoWriter*> m_vwriters;
    std::thread *m_thread;
    QMutex m_mutex;
    std::atomic_bool m_running;
    std::atomic_bool m_started;
    std::atomic_int m_currentFps;
    int m_fps;
    boost::circular_buffer<Frame> m_frameRing;
    std::shared_ptr<DataStream<Frame>> m_outStream;

    static void captureThread(void *gcamPtr);
    bool startCaptureThread();
    void finishCaptureThread();
};

#endif // GENERICCAMERAMODULE_H

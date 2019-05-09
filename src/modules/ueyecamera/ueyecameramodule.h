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
#include <boost/circular_buffer.hpp>
#include <QQueue>
#include <QMutex>

#include "imagesourcemodule.h"
#include "abstractmodule.h"

class UEyeCamera;
class VideoViewWidget;
class UEyeCameraSettingsDialog;

class UEyeCameraModule : public ImageSourceModule
{
    Q_OBJECT
public:
    explicit UEyeCameraModule(QObject *parent = nullptr);
    ~UEyeCameraModule() override;

    QString id() const override;
    QString description() const override;
    QPixmap pixmap() const override;
    void setName(const QString& name) override;

    void attachVideoWriter(VideoWriter *vwriter) override;

    int selectedFramerate() const override;
    cv::Size selectedResolution() const override;

    bool initialize(ModuleManager *manager) override;

    bool prepare(HRTimer *timer) override;
    void start() override;
    bool runCycle() override;

    void stop() override;

    void showDisplayUi() override;
    void hideDisplayUi() override;
    void showSettingsUi() override;
    void hideSettingsUi() override;

private:
    UEyeCamera *m_camera;
    VideoViewWidget *m_videoView;
    UEyeCameraSettingsDialog *m_camSettingsWindow;
    HRTimer *m_timer;

    QList<VideoWriter*> m_vwriters;
    std::thread *m_thread;
    QMutex m_mutex;
    std::atomic_bool m_running;
    std::atomic_bool m_started;
    std::atomic_int m_currentFps;
    int m_fps;
    boost::circular_buffer<FrameData> m_frameRing;

    static void captureThread(void *gcamPtr);
    bool startCaptureThread();
    void finishCaptureThread();
};

#endif // UEYECAMERAMODULE_H

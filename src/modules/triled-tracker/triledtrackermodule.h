/**
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

#ifndef TRILEDTRACKERMODULE_H
#define TRILEDTRACKERMODULE_H

#include <QObject>
#include <thread>
#include <queue>
#include <mutex>
#include <boost/circular_buffer.hpp>

#include "imagesinkmodule.h"
#include "abstractmodule.h"

class ImageSourceModule;
class LedTrackerSettingsDialog;
class VideoViewWidget;

class TriLedTrackerModuleInfo : public ModuleInfo
{
    Q_OBJECT
public:
    QString id() const override;
    QString name() const override;
    QString description() const override;
    QPixmap pixmap() const override;
    AbstractModule *createModule(QObject *parent = nullptr) override;
};

class TriLedTrackerModule : public ImageSinkModule
{
    Q_OBJECT
public:
    explicit TriLedTrackerModule(QObject *parent = nullptr);
    ~TriLedTrackerModule() override;

    void setName(const QString& name) override;
    ModuleFeatures features() const override;

    bool initialize(ModuleManager *manager) override;
    bool prepare(const QString& storageRootDir, const TestSubject& testSubject, HRTimer *timer) override;
    void start() override;
    bool runCycle() override;
    void stop() override;

    bool canRemove(AbstractModule *mod) override;

    void showSettingsUi() override;

    QByteArray serializeSettings(const QString& confBaseDir) override;
    bool loadSettings(const QString& confBaseDir, const QByteArray& data) override;

public slots:
    void receiveFrame(const FrameData& frameData) override;

private slots:
    void recvModuleCreated(AbstractModule *mod);
    void recvModulePreRemove(AbstractModule *mod);

private:
    QList<ImageSourceModule*> m_frameSourceModules;
    QString m_dataStorageDir;
    QString m_subjectId;

    std::atomic_bool m_running;
    std::atomic_bool m_started;
    std::thread *m_thread;
    std::mutex m_mutex;
    std::mutex m_dispmutex;
    std::queue<FrameData> m_frameQueue;

    LedTrackerSettingsDialog *m_settingsDialog;
    VideoViewWidget *m_trackInfoDisplay;
    VideoViewWidget *m_trackingDisplay;

    boost::circular_buffer<cv::Mat> m_trackDispRing;
    boost::circular_buffer<cv::Mat> m_trackInfoDispRing;

    static void trackingThread(void *tmPtr);
    bool startTrackingThread();
    void finishTrackingThread();
};

#endif // TRILEDTRACKERMODULE_H

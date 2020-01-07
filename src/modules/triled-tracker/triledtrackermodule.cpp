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

#include "triledtrackermodule.h"

#include <QMessageBox>
#include <QDebug>

#include "imagesourcemodule.h"
#include "ledtrackersettingsdialog.h"
#include "videoviewwidget.h"
#include "tracker.h"

QString TriLedTrackerModuleInfo::id() const
{
    return QStringLiteral("triled-tracker");
}

QString TriLedTrackerModuleInfo::name() const
{
    return QStringLiteral("TriLED Tracker");
}

QString TriLedTrackerModuleInfo::description() const
{
    return QStringLiteral("Track subject behavior via three LEDs mounted on its head.");
}

QPixmap TriLedTrackerModuleInfo::pixmap() const
{
    return QPixmap(":/module/triled-tracker");
}

AbstractModule *TriLedTrackerModuleInfo::createModule(QObject *parent)
{
    return new TriLedTrackerModule(parent);
}

/**
 * @brief FRAME_QUEUE_MAX_COUNT
 * The maximum number of frames we want to hold in the queue
 * before dropping data.
 */
static const uint FRAME_QUEUE_MAX_COUNT = 512;

TriLedTrackerModule::TriLedTrackerModule(QObject *parent)
    : ImageSinkModule(parent),
      m_thread(nullptr),
      m_settingsDialog(nullptr),
      m_trackInfoDisplay(nullptr),
      m_trackingDisplay(nullptr)
{
    m_trackDispRing = boost::circular_buffer<cv::Mat>(16);
    m_trackInfoDispRing = boost::circular_buffer<cv::Mat>(16);
}

TriLedTrackerModule::~TriLedTrackerModule()
{
    finishTrackingThread();
    if (m_settingsDialog != nullptr)
        delete m_settingsDialog;
    if (m_trackInfoDisplay != nullptr)
        delete m_trackInfoDisplay;
    if (m_trackingDisplay != nullptr)
        delete m_trackingDisplay;
}

void TriLedTrackerModule::setName(const QString &name)
{
    ImageSinkModule::setName(name);
}

ModuleFeatures TriLedTrackerModule::features() const
{
    return ModuleFeature::RUN_EVENTS | ModuleFeature::SHOW_SETTINGS | ModuleFeature::SHOW_DISPLAY;
}

bool TriLedTrackerModule::initialize(ModuleManager *manager)
{
    assert(!initialized());
    setState(ModuleState::INITIALIZING);

    m_settingsDialog = new LedTrackerSettingsDialog;
    m_settingsDialog->setResultsName("tracking");
    m_settingsWindows.append(m_settingsDialog);

    // find all modules suitable as frame sources
    Q_FOREACH(auto mod, manager->activeModules()) {
        auto imgSrcMod = qobject_cast<ImageSourceModule*>(mod);
        if (imgSrcMod == nullptr)
            continue;
        m_frameSourceModules.append(imgSrcMod);
    }
    if (m_frameSourceModules.isEmpty())
        m_settingsDialog->setSelectedImageSourceMod(nullptr);
    else
        m_settingsDialog->setSelectedImageSourceMod(m_frameSourceModules.first()); // set first module as default

    m_trackInfoDisplay = new VideoViewWidget;
    m_trackingDisplay = new VideoViewWidget;
    m_displayWindows.append(m_trackInfoDisplay);
    m_displayWindows.append(m_trackingDisplay);

    m_trackInfoDisplay->setWindowTitle(QStringLiteral("Animal Tracking Info"));
    m_trackingDisplay->setWindowTitle(QStringLiteral("Tracking Display"));

    setState(ModuleState::IDLE);
    setInitialized();
    setName(name());
    return true;
}

bool TriLedTrackerModule::prepare(const QString &storageRootDir, const TestSubject &testSubject)
{
    setState(ModuleState::PREPARING);

    m_dataStorageDir = QStringLiteral("%1/tracking").arg(storageRootDir);
    m_subjectId = testSubject.id;

    if (!makeDirectory(m_dataStorageDir))
        return false;

    const auto resultsName = m_settingsDialog->resultsName();
    if (m_subjectId.isEmpty())
        m_subjectId = resultsName;

    if (resultsName.isEmpty()) {
        raiseError("Tracking result name is not set. Please set it in the module settings to continue.");
        return false;
    }

    auto imgSrcMod = m_settingsDialog->selectedImageSourceMod();
    if (imgSrcMod == nullptr) {
        raiseError("No frame source is set for subject tracking. Please set it in the module settings to continue.");
        return false;
    }
    connect(imgSrcMod, &ImageSourceModule::newFrame, this, &TriLedTrackerModule::receiveFrame);

    m_trackingDisplay->setWindowTitle(QStringLiteral("Tracking: %1").arg(resultsName));
    m_trackInfoDisplay->setWindowTitle(QStringLiteral("Tracking Info: %1").arg(resultsName));

    m_settingsDialog->setRunning(true);
    m_started = false;
    startTrackingThread();

    statusMessage(QStringLiteral("Tracking via %1").arg(imgSrcMod->name()));
    setState(ModuleState::READY);
    return true;
}

void TriLedTrackerModule::start()
{
    m_started = true;
    setState(ModuleState::RUNNING);
}

bool TriLedTrackerModule::runEvent()
{
    std::lock_guard<std::mutex> lock(m_dispmutex);

    if (!m_trackDispRing.empty()) {
        m_trackingDisplay->showImage(m_trackDispRing.front());
        m_trackDispRing.pop_front();
    }
    if (!m_trackInfoDispRing.empty()) {
        m_trackInfoDisplay->showImage(m_trackInfoDispRing.front());
        m_trackInfoDispRing.pop_front();
    }

    return true;
}

void TriLedTrackerModule::stop()
{
    finishTrackingThread();

    m_settingsDialog->setRunning(false);

    auto imgSrcMod = m_settingsDialog->selectedImageSourceMod();
    disconnect(imgSrcMod, &ImageSourceModule::newFrame, this, &TriLedTrackerModule::receiveFrame);

    statusMessage(QStringLiteral("Tracker stopped."));
}

bool TriLedTrackerModule::canRemove(AbstractModule *mod)
{
    return mod != m_settingsDialog->selectedImageSourceMod();
}

void TriLedTrackerModule::showSettingsUi()
{
    assert(initialized());
    m_settingsDialog->setImageSourceModules(m_frameSourceModules);
    m_settingsDialog->show();
}

QByteArray TriLedTrackerModule::serializeSettings(const QString &confBaseDir)
{
    Q_UNUSED(confBaseDir);
    QJsonObject jsettings;
    auto mod = m_settingsDialog->selectedImageSourceMod();
    if (mod != nullptr)
        jsettings.insert("imageSourceModule", mod->name());
    jsettings.insert("resultsName", m_settingsDialog->resultsName());

    return jsonObjectToBytes(jsettings);
}

bool TriLedTrackerModule::loadSettings(const QString &confBaseDir, const QByteArray &data)
{
    Q_UNUSED(confBaseDir)
    auto jsettings = jsonObjectFromBytes(data);

    auto modName = jsettings.value("imageSourceModule").toString();
    Q_FOREACH(auto mod, m_frameSourceModules) {
        if (mod->name() == modName) {
            m_settingsDialog->setSelectedImageSourceMod(mod);
            break;
        }
    }
    m_settingsDialog->setResultsName(jsettings.value("resultsName").toString());

    return true;
}

void TriLedTrackerModule::recvModuleCreated(AbstractModule *mod)
{
    auto imgSrcMod = qobject_cast<ImageSourceModule*>(mod);
    if (imgSrcMod != nullptr)
        m_frameSourceModules.append(imgSrcMod);
}

void TriLedTrackerModule::recvModulePreRemove(AbstractModule *mod)
{
    auto imgSrcMod = qobject_cast<ImageSourceModule*>(mod);
    if (imgSrcMod == nullptr)
        return;
    for (int i = 0; i < m_frameSourceModules.size(); i++) {
        auto fsmod = m_frameSourceModules.at(i);
        if (fsmod == imgSrcMod) {
            m_frameSourceModules.removeAt(i);
            break;
        }
    }
}

void TriLedTrackerModule::receiveFrame(const Frame &frame)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frameQueue.push(frame);
}

void TriLedTrackerModule::trackingThread(void *tmPtr)
{
    auto self = static_cast<TriLedTrackerModule*> (tmPtr);

    auto tracker = new Tracker(self->m_dataStorageDir, self->m_subjectId);
    if (!tracker->initialize()) {
        self->raiseError(tracker->lastError());
        delete tracker;
        return;
    }

    while (self->m_running) {
        while (!self->m_started) { }

        self->m_mutex.lock();
        if (self->m_frameQueue.empty()) {
            self->m_mutex.unlock();
            continue;
        }
        if (self->m_frameQueue.size() > FRAME_QUEUE_MAX_COUNT) {
            self->raiseError("Tracking frame queue was full: Could not analyze frames fast enough.");
            break;
        }

        auto frame = self->m_frameQueue.front();
        self->m_frameQueue.pop();
        self->m_mutex.unlock();

        cv::Mat infoMat;
        cv::Mat trackMat;
        tracker->analyzeFrame(frame.mat, frame.time, &trackMat, &infoMat);

        self->m_dispmutex.lock();
        self->m_trackInfoDispRing.push_back(infoMat);
        self->m_trackDispRing.push_back(trackMat);
        self->m_dispmutex.unlock();
    }

    delete tracker;
}

bool TriLedTrackerModule::startTrackingThread()
{
    finishTrackingThread();

    statusMessage("Launching thread...");
    m_running = true;
    m_thread = new std::thread(trackingThread, this);
    statusMessage("Waiting.");
    return true;
}

void TriLedTrackerModule::finishTrackingThread()
{
    if (!initialized())
        return;

    statusMessage("Cleaning up...");
    if (m_thread != nullptr) {
        m_running = false;
        m_started = true;
        m_thread->join();
        delete m_thread;
        m_thread = nullptr;
        m_started = false;
    }
}

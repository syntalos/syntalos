/*
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include <QDebug>

#include "camera.h"
#include "genericcamerasettingsdialog.h"

class GenericCameraModule : public AbstractModule
{
    Q_OBJECT
private:
    Camera *m_camera;
    GenericCameraSettingsDialog *m_camSettingsWindow;

    std::atomic_bool m_stopped;
    int m_fps;
    std::shared_ptr<DataStream<Frame>> m_outStream;

    std::unique_ptr<SecondaryClockSynchronizer> m_clockSync;

public:
    explicit GenericCameraModule(QObject *parent = nullptr)
        : AbstractModule(parent),
          m_camera(new Camera),
          m_camSettingsWindow(nullptr),
          m_stopped(true)
    {
        m_outStream = registerOutputPort<Frame>(QStringLiteral("video"), QStringLiteral("Video"));

        m_camSettingsWindow = new GenericCameraSettingsDialog(m_camera);
        addSettingsWindow(m_camSettingsWindow);

        // set initial window titles
        setName(name());
    }

    void setName(const QString &name) override
    {
        AbstractModule::setName(name);
        m_camSettingsWindow->setWindowTitle(QStringLiteral("Settings for %1").arg(name));
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::RUN_THREADED |
               ModuleFeature::SHOW_SETTINGS;
    }

    bool prepare(const TestSubject &) override
    {
        if (m_camera->camId() < 0) {
            raiseError("Unable to continue: No valid camera was selected!");
            return false;
        }

        statusMessage("Connecting camera...");
        if (!m_camera->connect()) {
            raiseError(QStringLiteral("Unable to connect camera: %1").arg(m_camera->lastError()));
            return false;
        }
        m_camera->setResolution(m_camSettingsWindow->resolution());

        m_camSettingsWindow->setRunning(true);
        m_fps = m_camSettingsWindow->framerate();

        // set the required stream metadata for video capture
        m_outStream->setMetadataValue("size", QSize(m_camera->resolution().width,
                                                    m_camera->resolution().height));
        m_outStream->setMetadataValue("framerate", m_fps);

        // start the stream
        m_outStream->start();

        // set up clock synchronizer
        m_clockSync = initClockSynchronizer(m_fps);
        m_clockSync->setStrategies(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD | TimeSyncStrategy::ADJUST_CLOCK);

        // permit tolerance of about a third of a frame - since we can easily adjust the DAQ speed,
        // we want to know early about whether we should do that
        m_clockSync->setTolerance(microseconds_t(static_cast<long>((1000.0 / m_fps) * 250)));

        // check accuracy every 500msec
        m_clockSync->setCheckInterval(milliseconds_t(500));

        // start the synchronizer
        if (!m_clockSync->start()) {
            raiseError(QStringLiteral("Unable to set up clock synchronizer!"));
            return false;
        }

        statusMessage("Waiting.");

        return true;
    }

    void start() override
    {
        m_camera->setStartTime(m_syTimer->startTime());
        statusMessage("Acquiring frames...");

        AbstractModule::start();
    }

    void runThread(OptionalWaitCondition *waitCondition) override
    {
        auto fpsLow = false;
        auto currentFps = m_fps;
        auto frameRecordFailedCount = 0;
        m_stopped = false;

        // wait until we actually start acquiring data
        waitCondition->wait(this);

        while (m_running) {
            const auto cycleStartTime = currentTimePoint();

            Frame frame;
            if (!m_camera->recordFrame(frame, m_clockSync.get())) {
                frameRecordFailedCount++;
                if (frameRecordFailedCount > 32) {
                    m_running = false;
                    raiseError(QStringLiteral("Too many attempts to record frames from this camera have failed. Is the camera connected properly?"));
                }
                continue;
            }

            // emit this frame on our output port
            m_outStream->push(frame);

            // wait a bit if necessary, to keep the right framerate
            const auto cycleTime = timeDiffToNowMsec(cycleStartTime);
            const auto extraWaitTime = std::chrono::milliseconds((1000 / m_fps) - cycleTime.count() + (m_clockSync->clockCorrectionOffset().count() / 2));
            if (extraWaitTime.count() > 0)
                std::this_thread::sleep_for(extraWaitTime);

            const auto totalTime = timeDiffToNowMsec(cycleStartTime);
            currentFps = static_cast<int>(1 / (totalTime.count() / static_cast<double>(1000)));

            // warn if there is a bigger framerate drop
            if (currentFps < (m_fps - 2)) {
                fpsLow = true;
                setStatusMessage(QStringLiteral("<font color=\"red\"><b>Framerate (%1fps) is too low!</b></font>").arg(currentFps));
            } else if (fpsLow) {
                fpsLow = false;
                statusMessage("Acquiring frames...");
            }
        }

        m_stopped = true;
    }

    void stop() override
    {
        statusMessage("Cleaning up...");
        AbstractModule::stop();

        while (!m_stopped) {}

        m_camera->disconnect();
        m_camSettingsWindow->setRunning(false);
        m_clockSync->stop();
        statusMessage("Camera disconnected.");
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert("camera", m_camera->camId());
        settings.insert("width", m_camSettingsWindow->resolution().width);
        settings.insert("height", m_camSettingsWindow->resolution().height);
        settings.insert("fps", m_camSettingsWindow->framerate());
        settings.insert("exposure", m_camera->exposure());
        settings.insert("brightness", m_camera->brightness());
        settings.insert("contrast", m_camera->contrast());
        settings.insert("saturation", m_camera->saturation());
        settings.insert("hue", m_camera->hue());
        settings.insert("gain", m_camera->gain());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_camera->setCamId(settings.value("camera").toInt());
        m_camera->setResolution(cv::Size(settings.value("width").toInt(), settings.value("height").toInt()));
        m_camera->setExposure(settings.value("exposure").toDouble());
        m_camera->setBrightness(settings.value("brightness").toDouble());
        m_camera->setContrast(settings.value("contrast").toDouble());
        m_camera->setSaturation(settings.value("saturation").toDouble());
        m_camera->setHue(settings.value("hue").toDouble());
        m_camera->setGain(settings.value("gain").toDouble());
        m_camSettingsWindow->setFramerate(settings.value("fps").toInt());

        m_camSettingsWindow->updateValues();
        return true;
    }
};

QString GenericCameraModuleInfo::id() const
{
    return QStringLiteral("generic-camera");
}

QString GenericCameraModuleInfo::name() const
{
    return QStringLiteral("Generic Camera");
}

QString GenericCameraModuleInfo::description() const
{
    return QStringLiteral("Capture a video with a regular camera compatible with Linux' V4L API.");
}

QPixmap GenericCameraModuleInfo::pixmap() const
{
    return QPixmap(":/module/generic-camera");
}

QColor GenericCameraModuleInfo::color() const
{
    return QColor::fromRgba(qRgba(29, 158, 246, 180)).darker();
}

AbstractModule *GenericCameraModuleInfo::createModule(QObject *parent)
{
    return new GenericCameraModule(parent);
}

#include "genericcameramodule.moc"

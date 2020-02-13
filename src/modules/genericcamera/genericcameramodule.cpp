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

    int m_fps;
    std::shared_ptr<DataStream<Frame>> m_outStream;

public:
    explicit GenericCameraModule(QObject *parent = nullptr)
        : AbstractModule(parent),
          m_camera(new Camera),
          m_camSettingsWindow(nullptr)
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

    bool prepare(const QString &, const TestSubject &) override
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
        m_outStream->setMetadataVal("size", QSize(m_camera->resolution().width,
                                                  m_camera->resolution().height));
        m_outStream->setMetadataVal("framerate", m_fps);

        // start the stream
        m_outStream->start();

        statusMessage("Waiting.");

        return true;
    }

    void start() override
    {
        m_camera->setStartTime(m_timer->startTime());
        statusMessage("Acquiring frames...");

        AbstractModule::start();
    }

    void runThread(OptionalWaitCondition *waitCondition) override
    {
        auto fpsLow = false;
        auto currentFps = m_fps;
        auto frameRecordFailedCount = 0;

        // wait until we actually start acquiring data
        waitCondition->wait(this);

        while (m_running) {
            const auto cycleStartTime = currentTimePoint();

            cv::Mat mat;
            std::chrono::milliseconds time;
            if (!m_camera->recordFrame(&mat, &time)) {
                frameRecordFailedCount++;
                if (frameRecordFailedCount > 32) {
                    m_running = false;
                    raiseError(QStringLiteral("Too many attempts to record frames from this camera have failed. Is the camera connected properly?"));
                }
                continue;
            }
            // construct frame container
            Frame frame(mat, time);

            // emit this frame on our output port
            m_outStream->push(frame);

            // wait a bit if necessary, to keep the right framerate
            const auto cycleTime = timeDiffToNowMsec(cycleStartTime);
            const auto extraWaitTime = std::chrono::milliseconds((1000 / m_fps) - cycleTime.count());
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
    }

    void stop() override
    {
        statusMessage("Cleaning up...");

        m_camera->disconnect();
        m_camSettingsWindow->setRunning(false);
        statusMessage("Camera disconnected.");

        AbstractModule::stop();
    }

    QByteArray serializeSettings(const QString &) override
    {
        QJsonObject jsettings;
        jsettings.insert("camera", m_camera->camId());
        jsettings.insert("width", m_camSettingsWindow->resolution().width);
        jsettings.insert("height", m_camSettingsWindow->resolution().height);
        jsettings.insert("fps", m_camSettingsWindow->framerate());
        jsettings.insert("gain", m_camera->gain());
        jsettings.insert("exposure", m_camera->exposure());

        return jsonObjectToBytes(jsettings);
    }

    bool loadSettings(const QString &, const QByteArray &data) override
    {
        auto jsettings = jsonObjectFromBytes(data);
        m_camera->setCamId(jsettings.value("camera").toInt());
        m_camera->setResolution(cv::Size(jsettings.value("width").toInt(), jsettings.value("height").toInt()));
        m_camera->setExposure(jsettings.value("exposure").toDouble());
        m_camera->setGain(jsettings.value("gain").toDouble());
        m_camSettingsWindow->setFramerate(jsettings.value("fps").toInt());

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

/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libcameramodule.h"

#include "datactl/frametype.h"

#include "lccamera.h"
#include "lcsettingsdialog.h"

#include <cmath>
#include <thread>

SYNTALOS_MODULE(LibcameraModule)

class LibcameraModule : public AbstractModule
{
    Q_OBJECT
private:
    LcCamera *m_camera;
    LcSettingsDialog *m_camSettingsWindow;

    std::atomic_bool m_stopped;
    double m_fps;
    std::shared_ptr<DataStream<Frame>> m_outStream;

    std::unique_ptr<SecondaryClockSynchronizer> m_clockSync;

public:
    explicit LibcameraModule(const ModuleInfo *info = nullptr, QObject *parent = nullptr)
        : AbstractModule(info, parent),
          m_camera(new LcCamera(m_log)),
          m_camSettingsWindow(nullptr),
          m_stopped(true)
    {
        m_outStream = registerOutputPort<Frame>(QStringLiteral("video"), QStringLiteral("Video"));

        m_camSettingsWindow = new LcSettingsDialog(m_camera);
        m_camSettingsWindow->setWindowIcon(info->icon());
        addSettingsWindow(m_camSettingsWindow);

        // set initial window titles
        setName(name());
    }

    ~LibcameraModule() override
    {
        delete m_camera;
    }

    bool initialize() override
    {
        m_camera->setLogger(m_log);
        return AbstractModule::initialize();
    }

    void setName(const QString &name) override
    {
        AbstractModule::setName(name);
        m_camSettingsWindow->setWindowTitle(QStringLiteral("Settings for %1").arg(name));
    }

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::THREAD_DEDICATED;
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::REALTIME | ModuleFeature::SHOW_SETTINGS;
    }

    bool prepare(const TestSubject &) override
    {
        if (m_camera->cameraId().isEmpty()) {
            raiseError("Unable to continue: No valid camera was selected!");
            return false;
        }

        const auto requestedFps = m_camSettingsWindow->framerate();
        if (!std::isfinite(requestedFps) || requestedFps <= 0) {
            raiseError(
                QStringLiteral("Unable to continue: Invalid camera framerate %1fps requested.").arg(requestedFps));
            return false;
        }

        statusMessage("Connecting camera...");
        m_camera->setResolution(m_camSettingsWindow->resolution());
        m_camera->setFramerate(requestedFps);
        m_camera->setPixelFormat(m_camSettingsWindow->pixelFormatName());
        if (auto res = m_camera->connect(); !res) {
            raiseError(QStringLiteral("Unable to connect camera: %1").arg(res.error()));
            return false;
        }

        m_camSettingsWindow->setRunning(true);
        m_fps = requestedFps;

        // set the required stream metadata for video capture
        m_outStream->setMetadataValue("size", MetaSize(m_camera->resolution().width, m_camera->resolution().height));
        m_outStream->setMetadataValue("framerate", m_fps);

        // start the stream
        m_outStream->start();

        // set up clock synchronizer
        m_clockSync = initClockSynchronizer(m_fps);
        m_clockSync->setStrategies(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD | TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD);

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
        m_stopped = false;

        // wait until we actually start acquiring data
        waitCondition->wait(this);

        // the framerate is measured over a sliding window
        auto windowStartTime = currentTimePoint();
        uint windowFrameCount = 0;

        while (m_running) {
            Frame frame;
            auto res = m_camera->getFrame(frame, m_clockSync.get());
            if (!res) {
                // a genuine error means the camera is in serious trouble (it could not
                // deliver a frame within 10 seconds, etc.) - abort the run immediately
                m_running = false;
                raiseError(QStringLiteral("Failed to record frames from this camera: %1").arg(res.error()));
                continue;
            }
            if (!res.value()) {
                // succeeded, but no frame was produced this iteration (e.g. a pre-start
                // frame was discarded) - this is benign, just try again
                continue;
            }

            // emit this frame on our output port
            m_outStream->push(frame);

            // evaluate the average framerate roughly once per second
            windowFrameCount++;
            const auto windowMsec = timeDiffToNowMsec(windowStartTime).count();
            if (windowMsec >= 1000) {
                const auto currentFps = (windowFrameCount * 1000.0) / static_cast<double>(windowMsec);
                windowStartTime = currentTimePoint();
                windowFrameCount = 0;

                // warn only if the sustained average framerate is too low
                if (currentFps < (m_fps - 2)) {
                    fpsLow = true;
                    setStatusMessage(QStringLiteral("<b><font color=\"red\">Framerate (%1 fps) is too low!</font></b>")
                                         .arg(currentFps, 0, 'f', 1));
                } else if (fpsLow) {
                    fpsLow = false;
                    statusMessage("Acquiring frames...");
                }
            }
        }

        m_stopped = true;
    }

    void stop() override
    {
        statusMessage("Cleaning up...");
        AbstractModule::stop();

        while (!m_stopped)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        m_camera->disconnect();
        m_camSettingsWindow->setRunning(false);
        safeStopSynchronizer(m_clockSync);
        statusMessage("Camera disconnected.");
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert("camera", m_camera->cameraId());
        settings.insert("capture_format", m_camSettingsWindow->pixelFormatName());
        settings.insert("width", m_camSettingsWindow->resolution().width);
        settings.insert("height", m_camSettingsWindow->resolution().height);
        settings.insert("fps", m_camSettingsWindow->framerate());
        settings.insert("auto_exposure", m_camera->autoExposure());
        settings.insert("exposure", m_camera->exposureTime());
        settings.insert("gain", m_camera->gain());
        settings.insert("brightness", m_camera->brightness());
        settings.insert("contrast", m_camera->contrast());
        settings.insert("saturation", m_camera->saturation());
        settings.insert("gamma", m_camera->gamma());
        settings.insert("power_line_frequency", m_camera->powerLineFrequency());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_camera->setCameraId(settings.value("camera").toString());
        m_camera->setResolution(cv::Size(settings.value("width").toInt(), settings.value("height").toInt()));
        m_camera->setAutoExposure(settings.value("auto_exposure", true).toBool());
        m_camera->setExposureTime(settings.value("exposure").toDouble());
        m_camera->setGain(settings.value("gain").toDouble());
        m_camera->setBrightness(settings.value("brightness").toDouble());
        m_camera->setContrast(settings.value("contrast").toDouble());
        m_camera->setSaturation(settings.value("saturation").toDouble());
        m_camera->setGamma(settings.value("gamma", 2.2).toDouble());
        m_camera->setPowerLineFrequency(settings.value("power_line_frequency", 1).toInt());
        m_camSettingsWindow->setFramerate(settings.value("fps").toDouble());
        m_camSettingsWindow->setPixelFormatName(settings.value("capture_format").toString());

        m_camSettingsWindow->updateValues();
        return true;
    }

    void usbHotplugEvent(UsbHotplugEventKind) override
    {
        if (!m_stopped)
            return;
        m_camSettingsWindow->updateValues();
    }
};

QString LibcameraModuleInfo::id() const
{
    return QStringLiteral("camera-lc");
}

QString LibcameraModuleInfo::name() const
{
    return QStringLiteral("Camera");
}

QString LibcameraModuleInfo::description() const
{
    return QStringLiteral("Record video using the modern libcamera Linux camera stack.");
}

ModuleCategories LibcameraModuleInfo::categories() const
{
    return ModuleCategory::DEVICES;
}

QColor LibcameraModuleInfo::color() const
{
    return QColor::fromRgba(qRgba(67, 192, 235, 180)).darker();
}

AbstractModule *LibcameraModuleInfo::createModule(QObject *parent)
{
    return new LibcameraModule(this, parent);
}

#include "libcameramodule.moc"

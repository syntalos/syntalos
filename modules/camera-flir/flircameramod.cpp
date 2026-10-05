/*
 * Copyright (C) 2020 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "flircameramod.h"

#include <QDebug>

#include "flircamera.h"
#include "flircamsettingsdialog.h"

SYNTALOS_MODULE(FLIRCameraModule)

class FLIRCameraMod : public AbstractModule
{
    Q_OBJECT
private:
    static bool s_libVersionPrinted;

    FLIRCamera *m_camera;
    FLIRCamSettingsDialog *m_camSettingsWindow;

    std::shared_ptr<DataStream<Frame>> m_outStream;

public:
    explicit FLIRCameraMod(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_camera = new FLIRCamera(),

        m_outStream = registerOutputPort<Frame>(QStringLiteral("video"), QStringLiteral("Video"));

        m_camSettingsWindow = new FLIRCamSettingsDialog(m_camera);
        addSettingsWindow(m_camSettingsWindow);

        // set initial window titles
        setName(name());

        // print some debug info, but only once if there are multiple camera modules
        if (!s_libVersionPrinted) {
            m_camera->printLibraryVersion();
            s_libVersionPrinted = true;
        }
    }

    std::expected<void, QString> initialize() override
    {
        m_camSettingsWindow->updateValues();
        return {};
    }

    ~FLIRCameraMod()
    {
        delete m_camera;
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

    bool prepare(const RunInfo &) override
    {
        const auto camSerial = m_camSettingsWindow->selectedCameraSerial();
        if (camSerial.isEmpty()) {
            raiseError("Unable to continue: No valid FLIR camera was selected!");
            return false;
        }

        // ensure the right serial is set at this point
        m_camera->setSerial(camSerial);

        const auto resolution = m_camSettingsWindow->resolution();
        const double framerate = m_camSettingsWindow->framerate();
        m_camera->setResolution(resolution);
        m_camera->setFramerate(framerate);

        m_camSettingsWindow->setRunning(true);

        // set the required stream metadata for video capture
        m_outStream->setMetadataValue("framerate", framerate);
        m_outStream->setMetadataValue("size", QSize(resolution.width, resolution.height));

        // start the stream
        m_outStream->start();

        // The clock synchronizer is configured and started by our thread, once
        // it has initialized the camera and knows its actual framerate.
        setWorker(
            Worker{
                .camera = m_camera,
                .outStream = m_outStream,
                .clockSync = initClockSynchronizer(),
            });

        statusMessage("Waiting.");
        return true;
    }

    /**
     * Runs the frame acquisition in the module's thread.
     * The Spinnaker API has to be used by one thread only, so the camera is
     * initialized and shut down here as well.
     */
    struct Worker {
        WorkerContext mod{};
        FLIRCamera *camera;
        std::shared_ptr<DataStream<Frame>> outStream;
        std::unique_ptr<SecondaryClockSynchronizer> clockSync;

        void run()
        {
            // initialize camera
            if (!camera->initAcquisition()) {
                mod.raiseError(camera->lastError());
                return;
            }
            const auto actualFramerate = camera->actualFramerate();

            // set up clock synchronizer, the framerate the camera actually runs at is only known now
            if (actualFramerate > 0)
                clockSync->setExpectedClockFrequencyHz(actualFramerate);
            clockSync->setStrategies(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD);

            // start the synchronizer
            if (!clockSync->start()) {
                mod.raiseError(QStringLiteral("Unable to set up clock synchronizer!"));
                return;
            }

            // wait until we actually start acquiring data
            mod.waitForStart();

            // set up remaining pieces now that we are running, then start retrieving frames
            mod.setStatusMessage(QStringLiteral("Recording (max %1 FPS)").arg(qRound(actualFramerate), 4));
            const auto clockSyncPtr = clockSync.get();
            camera->setStartTime(mod.timer->startTime());
            while (mod.running()) {
                Frame frame;
                if (!camera->acquireFrame(frame, clockSyncPtr)) {
                    mod.raiseError(QStringLiteral("Unable to acquire frame: %1").arg(camera->lastError()));
                    break;
                }

                // emit this frame on our output port
                outStream->push(frame);
            }

            // finalize clock synchronizer
            clockSync->stop();

            // stop camera
            camera->endAcquisition();
        }
    };

    void stop() override
    {
        m_camSettingsWindow->setRunning(false);
        statusMessage(QString());
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert("camera", m_camera->serial());
        settings.insert("width", m_camSettingsWindow->resolution().width);
        settings.insert("height", m_camSettingsWindow->resolution().height);
        settings.insert("fps", m_camSettingsWindow->framerate());
        settings.insert("exposure_us", QVariant::fromValue(m_camera->exposureTime().count()));
        settings.insert("gamma", m_camera->gamma());
        settings.insert("gain", m_camera->gain());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_camera->setResolution(cv::Size(settings.value("width").toInt(), settings.value("height").toInt()));
        m_camera->setExposureTime(microseconds_t(settings.value("exposure_us").toLongLong()));
        m_camera->setGamma(settings.value("gamma").toInt());
        m_camera->setGain(settings.value("gain").toInt());
        m_camSettingsWindow->setFramerate(settings.value("fps").toInt());

        m_camera->setSerial(settings.value("camera").toString());
        m_camSettingsWindow->updateValues();
        return true;
    }
};

bool FLIRCameraMod::s_libVersionPrinted = false;

QString FLIRCameraModuleInfo::id() const
{
    return QStringLiteral("camera-flir");
}

QString FLIRCameraModuleInfo::name() const
{
    return QStringLiteral("FLIR Camera");
}

QString FLIRCameraModuleInfo::description() const
{
    return QStringLiteral(
        "Capture video using a camera from FLIR Systems, Inc. that is accessible via their Spinnaker SDK.");
}

ModuleCategories FLIRCameraModuleInfo::categories() const
{
    return ModuleCategory::DEVICES;
}

AbstractModule *FLIRCameraModuleInfo::createModule(QObject *parent)
{
    return new FLIRCameraMod(parent);
}

#include "flircameramod.moc"

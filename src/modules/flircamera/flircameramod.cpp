/*
 * Copyright (C) 2020 Matthias Klumpp <matthias@tenstral.net>
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

#include "flircameramod.h"

#include <QDebug>

#include "flircamera.h"
#include "flircamsettingsdialog.h"

class FLIRCameraMod : public AbstractModule
{
    Q_OBJECT
private:
    spn::SystemPtr m_spnSystem;
    FLIRCamera *m_camera;
    FLIRCamSettingsDialog *m_camSettingsWindow;

    std::shared_ptr<DataStream<Frame>> m_outStream;

public:
    explicit FLIRCameraMod(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_spnSystem = spn::System::GetInstance();
        m_camera = new FLIRCamera(m_spnSystem),

        m_outStream = registerOutputPort<Frame>(QStringLiteral("video"), QStringLiteral("Video"));

        m_camSettingsWindow = new FLIRCamSettingsDialog(m_camera);
        addSettingsWindow(m_camSettingsWindow);

        // set initial window titles
        setName(name());

        // print some debug info
        m_camera->printLibraryVersion(m_spnSystem);
    }

    bool initialize() override
    {
        m_camSettingsWindow->updateValues();
        return true;
    }

    ~FLIRCameraMod()
    {
        delete m_camera;
        m_spnSystem->ReleaseInstance();
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
        const auto camSerial = m_camSettingsWindow->selectedCameraSerial();
        if (camSerial.isEmpty()) {
            raiseError("Unable to continue: No valid FLIR camera was selected!");
            return false;
        }

        if (!m_camera->isValid() && !m_camera->setup(camSerial)) {
            raiseError(QStringLiteral("Unable to setup FLIR camera (serial: %1), can not continue").arg(camSerial));
            return false;
        }

        const auto resolution = m_camSettingsWindow->resolution();
        const auto framerate = m_camSettingsWindow->framerate();
        m_camera->setResolution(resolution);
        m_camera->setFramerate(framerate);

        m_camSettingsWindow->setRunning(true);

        // set the required stream metadata for video capture
        m_outStream->setMetadataValue("size", QSize(resolution.width,
                                                    resolution.height));
        m_outStream->setMetadataValue("framerate", framerate);

        // start the stream
        m_outStream->start();

        statusMessage("Waiting.");
        return true;
    }

    void runThread(OptionalWaitCondition *waitCondition) override
    {
        if (!m_camera->initAcquisition()) {
            raiseError(m_camera->lastError());
            return;
        }

        // wait until we actually start acquiring data
        waitCondition->wait(this);

        statusMessage(QStringLiteral("Recording (max %1 FPS)").arg(qRound(m_camera->actualFramerate()), 4));
        while (m_running) {
            Frame frame;
            if (!m_camera->acquireFrame(frame)) {
                m_running = false;
                raiseError(QStringLiteral("Unable to acquire frame: %1").arg(m_camera->lastError()));
                continue;
            }

            // emit this frame on our output port
            m_outStream->push(frame);
        }

        m_camera->endAcquisition();
    }

    void stop() override
    {
        m_camSettingsWindow->setRunning(false);
    }

};

QString FLIRCameraModuleInfo::id() const
{
    return QStringLiteral("flir-camera");
}

QString FLIRCameraModuleInfo::name() const
{
    return QStringLiteral("FLIR Camera");
}

QString FLIRCameraModuleInfo::description() const
{
    return QStringLiteral("Capture video using a camera from FLIR Systems, Inc. that is accessible via their Spinnaker SDK.");
}

QPixmap FLIRCameraModuleInfo::pixmap() const
{
    return QPixmap(":/module/camera-flir");
}

AbstractModule *FLIRCameraModuleInfo::createModule(QObject *parent)
{
    return new FLIRCameraMod(parent);
}

#include "flircameramod.moc"

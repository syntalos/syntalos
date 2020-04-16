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
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tiscameramodule.h"

#include <QDebug>
#include <gst/app/gstappsink.h>
#include "streams/frametype.h"

#include "cdeviceselectiondlg.h"
#include "cpropertiesdialog.h"
#include "tcamcamera.h"

class TISCameraModule : public AbstractModule
{
    Q_OBJECT
private:
    CPropertiesDialog *m_propDialog;
    std::shared_ptr<DataStream<Frame>> m_outStream;

    gsttcam::TcamCamera *m_camera;
    QString m_camSerial;
    cv::Size m_resolution;

    double m_fps;
    int m_fpsNumerator;
    int m_fpsDenominator;

public:
    explicit TISCameraModule(QObject *parent = nullptr)
        : AbstractModule(parent),
          m_propDialog(nullptr),
          m_camera(nullptr)
    {
        m_outStream = registerOutputPort<Frame>(QStringLiteral("video"), QStringLiteral("Video"));

        createNewPropertiesDialog();
    }

    ~TISCameraModule()
    {
        if (m_propDialog != nullptr)
            delete m_propDialog;
        if (m_camera != nullptr) {
            m_camera->stop();
            delete m_camera;
        }
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::RUN_THREADED |
               ModuleFeature::SHOW_SETTINGS;
    }

    void createNewPropertiesDialog()
    {
        if (m_propDialog != nullptr) {
            m_propDialog->hide();
            m_propDialog->deleteLater();
        }
        m_propDialog = new CPropertiesDialog;
        connect(m_propDialog, &CPropertiesDialog::deviceSelectClicked, this, &TISCameraModule::onDeviceSelectClicked);
        m_propDialog->setWindowTitle(QStringLiteral("%1 - Settings").arg(name()));
    }

    void showSettingsUi() override
    {
        m_propDialog->setWindowTitle(QStringLiteral("%1 - Settings").arg(name()));
        m_propDialog->show();
        m_propDialog->raise();
    }

    bool isSettingsUiVisible() override
    {
        return m_propDialog->isVisible();
    }

    void hideSettingsUi() override
    {
        m_propDialog->hide();
    }

    bool resetCamera()
    {
        if (m_camSerial.isEmpty())
            return false;
        if ((m_fpsNumerator <= 0) || (m_fpsDenominator <= 0) || (m_resolution.empty()))
            return false;

        if (m_camera != nullptr) {
            m_camera->stop();
            delete m_camera;
        }

        // Instantiate the TcamCamera object with the serial number
        // of the selected device
        m_camera = new gsttcam::TcamCamera(m_camSerial.toStdString());

        // Set video format, resolution and frame rate. We display color.
        m_camera->set_capture_format("BGRx",
                                     gsttcam::FrameSize{m_resolution.width, m_resolution.height},
                                     gsttcam::FrameRate{m_fpsNumerator, m_fpsDenominator});
        return true;
    }

    void selectCamera(const QString &serial, int width, int height, int fps1, int fps2)
    {
        if (serial.isEmpty())
            return;
        if ((fps1 <= 0) || (fps2 <= 0) || (width <= 0) || (height <= 0))
            return;

        m_camSerial = serial;
        m_fpsNumerator = fps1;
        m_fpsDenominator = fps2;
        m_fps = (double) m_fpsNumerator / (double) m_fpsDenominator;
        m_resolution = cv::Size(width, height);

        // create a new camera, deleting the old one
        resetCamera();

        // delete our old properties dialog, we don't need it anymore
        // replace it with a new one for the newly selected camera/settings combo
        createNewPropertiesDialog();

        // Pass the tcambin element to the properties dialog
        // so in knows, which device do handle
        const auto camProp = TCAM_PROP(m_camera->getTcamBin());
        m_propDialog->setCamera(camProp);
    }

    void onDeviceSelectClicked()
    {
        if (m_running)
            return;
        m_propDialog->hide();
        CDeviceSelectionDlg devSelDlg(m_propDialog);
        if (devSelDlg.exec() != QDialog::Accepted) {
            m_propDialog->show();
            return;
        }

        selectCamera(QString::fromStdString(devSelDlg.getSerialNumber()),
                     devSelDlg.getWidth(),
                     devSelDlg.getHeight(),
                     devSelDlg.getFPSNominator(),
                     devSelDlg.getFPSDeNominator());
        m_propDialog->show();
    }

    bool prepare(const TestSubject &) override
    {
        if (m_camera == nullptr) {
            raiseError("Unable to continue: No valid camera was selected!");
            return false;
        }

        // don't permit selecting a different device from this point on
        m_propDialog->setRunning(true);

        // there are stream issues if we do not recreate the GStreamer pipeline
        // every single time
        // FIXME: Find out why restarting the pipeline after it has run once does
        // not work, so we don't have to recreate it every time.
        if (!resetCamera()) {
            raiseError("Unable to initialize camera video streaming pipeline. Is the right camera selected, and is it plugged in?");
            return false;
        }

        // set the required stream metadata for video capture
        m_outStream->setMetadataValue("size", QSize(m_resolution.width, m_resolution.height));
        m_outStream->setMetadataValue("framerate", m_fps);

        // start the stream
        m_outStream->start();

        statusMessage("Waiting.");
        return true;
    }

    void runThread(OptionalWaitCondition *waitCondition) override
    {
        bool m_firstTimestamp = true;

        // set up clock synchronizer
        const auto clockSync = initClockSynchronizer(m_fps);
        clockSync->setStrategies(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD);

        // permit tolerance of about a third of a frame
        clockSync->setTolerance(microseconds_t(static_cast<long>((1000.0 / m_fps) * 250)));

        // check accuracy roughly every 500msec
        clockSync->setCheckInterval(milliseconds_t(500));

        // start the synchronizer
        if (!clockSync->start()) {
            raiseError(QStringLiteral("Unable to set up clock synchronizer!"));
            return;
        }

        // wait until we actually start acquiring data
        waitCondition->wait(this);

        if (!m_camera->start()) {
            raiseError(QStringLiteral("Failed to start image acquisition pipeline."));
            return;
        }

        statusMessage("");
        const auto appsink = GST_APP_SINK(m_camera->getCaptureSink());
        while (m_running) {
            g_autoptr(GstSample) sample = nullptr;
            auto frameRecvTime = MTIMER_FUNC_TIMESTAMP(sample = gst_app_sink_pull_sample(appsink));
            if (sample == nullptr) {
                // check if the inout stream has ended
                if (gst_app_sink_is_eos(appsink)) {
                    if (m_running)
                        qWarning("TISCamera: Video stream has ended prematurely!");
                    break;
                }
                if (m_running)
                    qWarning("TISCamera: Received invalid sample.");
                continue;
            }

            const auto buffer = gst_sample_get_buffer(sample);
            GstMapInfo info;

            gst_buffer_map(buffer, &info, GST_MAP_READ);
            if (info.data != nullptr) {
                GstCaps *caps = gst_sample_get_caps(sample);
                const auto gStr = gst_caps_get_structure (caps, 0);

                // sanity check
                if (g_strcmp0 (gst_structure_get_string(gStr, "format"), "BGRx") != 0) {
                    qDebug() << "Received buffer with unsupported format: " << gst_structure_get_string(gStr, "format");
                    gst_buffer_unmap (buffer, &info);
                    continue;
                }

                // create our frame and push it to subscribers
                Frame frame;
                frame.mat.create(m_resolution, CV_8UC(4));
                memcpy(frame.mat.data, info.data, m_resolution.width * m_resolution.height * 4);

                // only do time adjustment if we have a valid timestamp
                const auto pts = buffer->pts;
                if (pts != GST_CLOCK_TIME_NONE) {
                    // mark as us not being able to do any time adjustments if no valid timestamps are received
                    if (m_firstTimestamp)
                        clockSync->setStrategies(TimeSyncStrategy::NONE);
                    clockSync->processTimestamp(frameRecvTime, std::chrono::duration_cast<milliseconds_t>(nanoseconds_t(pts)));
                }
                frame.time = frameRecvTime;
                m_firstTimestamp = false;

                m_outStream->push(frame);
            }

            // unmap our buffer - all other resources are cleaned up automatically
            gst_buffer_unmap (buffer, &info);
        }

        statusMessage("Stopped.");
        m_camera->stop();
    }

    void stop() override
    {
        // we may have been called after a failure because no camera was selected...
        // if that's the case, there is already nothing for us left to do
        if (m_camera == nullptr)
            return;

        // we may still be blocking on the GStreamer buffer pull, so
        // we need to stop the pipeline here as well to make sure
        // we don't deadlock
        m_running = false;
        m_camera->stop();

        // we are not running anymore, so new device selections are possible again
        m_propDialog->setRunning(false);
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert("camera", m_camSerial);
        settings.insert("width", m_resolution.width);
        settings.insert("height", m_resolution.height);
        settings.insert("fps_numerator", m_fpsNumerator);
        settings.insert("fps_denominator", m_fpsDenominator);
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        selectCamera(settings.value("camera").toString(),
                     settings.value("width").toInt(),
                     settings.value("height").toInt(),
                     settings.value("fps_numerator").toInt(),
                     settings.value("fps_denominator").toInt());
        return true;
    }
};

QString TISCameraModuleInfo::id() const
{
    return QStringLiteral("camera-tis");
}

QString TISCameraModuleInfo::name() const
{
    return QStringLiteral("TIS Camera");
}

QString TISCameraModuleInfo::description() const
{
    return QStringLiteral("Capture video with a camera from The Imaging Source.");
}

QPixmap TISCameraModuleInfo::pixmap() const
{
    return QPixmap(":/module/camera-tis");
}

QString TISCameraModuleInfo::license() const
{
    return QStringLiteral("This module embeds code from <a href=\"https://www.theimagingsource.com/\">The Imaging Source</a> "
                          "which is distributed under the terms of the Apache-2.0 license.");
}

AbstractModule *TISCameraModuleInfo::createModule(QObject *parent)
{
    return new TISCameraModule(parent);
}

#include "tiscameramodule.moc"

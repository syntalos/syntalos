/*
 * Copyright (C) 2020-2022 Matthias Klumpp <matthias@tenstral.net>
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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#include <gst/app/gstappsink.h>
#include <gst/video/video-format.h>
#pragma GCC diagnostic pop
#include "streams/frametype.h"

#include "cdeviceselectiondlg.h"
#include "cpropertiesdialog.h"
#include "tcamcamera.h"

SYNTALOS_MODULE(TISCameraModule)

namespace Syntalos {
    Q_LOGGING_CATEGORY(logTISCam, "mod.tiscam")
}

class TISCameraModule : public AbstractModule
{
    Q_OBJECT
private:
    CPropertiesDialog *m_propDialog;
    std::shared_ptr<DataStream<Frame>> m_outStream;

    gsttcam::TcamCamera *m_camera;
    QString m_camSerial;
    QString m_imgFormat;
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

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::THREAD_DEDICATED;
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::REALTIME |
               ModuleFeature::SHOW_SETTINGS;
    }

    void createNewPropertiesDialog()
    {
        QRect oldGeometry;
        if (m_propDialog != nullptr) {
            oldGeometry = m_propDialog->geometry();
            m_propDialog->hide();
            m_propDialog->setCamera(nullptr);
            m_propDialog->deleteLater();
        }
        m_propDialog = new CPropertiesDialog;
        connect(m_propDialog, &CPropertiesDialog::deviceSelectClicked, this, &TISCameraModule::onDeviceSelectClicked);
        m_propDialog->setWindowTitle(QStringLiteral("%1 - Settings").arg(name()));
        if (!oldGeometry.isEmpty())
            m_propDialog->setGeometry(oldGeometry);
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
        m_camera->set_capture_format(m_imgFormat.toStdString(),
                                     gsttcam::FrameSize{m_resolution.width, m_resolution.height},
                                     gsttcam::FrameRate{m_fpsNumerator, m_fpsDenominator});
        return true;
    }

    void selectCamera(const QString &serial, const QString &format, int width, int height, int fps1, int fps2)
    {
        if (serial.isEmpty())
            return;
        if ((fps1 <= 0) || (fps2 <= 0) || (width <= 0) || (height <= 0))
            return;

        m_camSerial = serial;
        m_imgFormat = format;
        m_fpsNumerator = fps1;
        m_fpsDenominator = fps2;
        m_fps = (double) m_fpsNumerator / (double) m_fpsDenominator;
        m_resolution = cv::Size(width, height);

        // sanity check on image formats
        if ((m_imgFormat != QStringLiteral("BGRx")) &&
            (m_imgFormat != QStringLiteral("GRAY8")) &&
            (m_imgFormat != QStringLiteral("GRAY16_LE"))) {
            m_imgFormat = QStringLiteral("BGRx");
            qCWarning(logTISCam).noquote().nospace() << "Unknown/untested image format '" << format << "' selected, falling back to BGRx";
        }

        // create a new camera, deleting the old one
        if (!resetCamera()) {
            qWarning() << "TISCamera. Unable to reset camera.";
            return;
        }

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
        if (devSelDlg.exec() != QDialog::Accepted)
            return;

        selectCamera(QString::fromStdString(devSelDlg.getSerialNumber()),
                     QString::fromStdString(devSelDlg.getFormat()),
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

        // there are stream issues if we do not recreate the GStreamer pipeline
        // every single time
        // FIXME: Find out why restarting the pipeline after it has run once does
        // not work, so we don't have to recreate it every time.
        if (!resetCamera()) {
            raiseError("Unable to initialize camera video streaming pipeline. Is the right camera selected, and is it plugged in?");
            return false;
        }

        // we have just reset the camera, so we'll also have to recreate the
        // settings view - again
        createNewPropertiesDialog();
        const auto camProp = TCAM_PROP(m_camera->getTcamBin());
        m_propDialog->setCamera(camProp);

        // don't permit selecting a different device from this point on
        m_propDialog->setRunning(true);

        // set the required stream metadata for video capture
        m_outStream->setMetadataValue("size", QSize(m_resolution.width, m_resolution.height));
        m_outStream->setMetadataValue("framerate", m_fps);
        m_outStream->setMetadataValue("has_color", !m_imgFormat.startsWith("GRAY"));
        if (m_imgFormat.startsWith("GRAY16"))
            m_outStream->setMetadataValue("depth", CV_16U);

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

        // start the synchronizer
        if (!clockSync->start()) {
            raiseError(QStringLiteral("Unable to set up clock synchronizer!"));
            return;
        }

        // We don't want to hold more than one buffer in the appsink, as otherwise getting the master timestamp
        // on gst_app_sink_pull_sample() will be even less accurate than it already is (and we would have to correct
        // timestamps for pending buffer content, which is difficult to do right, and complicated).
        // If the appsink max buffer count is low, elements upstream in the pipeline will be blocked until we removed
        // one and free a buffer slot, which means camera DAQ will be delayed to the speed at which we can convert
        // data into Syntalos stream elements, which is exactly what we want.
        const auto appsink = GST_APP_SINK(m_camera->getCaptureSink());
        gst_app_sink_set_max_buffers (appsink, 1);

        // wait until we actually start acquiring data
        waitCondition->wait(this);

        if (!m_camera->start()) {
            raiseError(QStringLiteral("Failed to start image acquisition pipeline."));
            return;
        }

        statusMessage("");
        while (m_running) {
            g_autoptr(GstSample) sample = nullptr;
            auto frameRecvTime = MTIMER_FUNC_TIMESTAMP(sample = gst_app_sink_pull_sample(appsink));
            if (sample == nullptr) {
                // check if the inout stream has ended
                if (gst_app_sink_is_eos(appsink)) {
                    if (m_running)
                        raiseError(QStringLiteral("Video stream has ended prematurely!"));
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
                const auto gS = gst_caps_get_structure (caps, 0);
                const gchar *format_str = gst_structure_get_string(gS, "format");

                // create our frame and push it to subscribers
                Frame frame;
                if (g_strcmp0(format_str, "BGRx") == 0) {
                    frame.mat.create(m_resolution, CV_8UC(4));
                    memcpy(frame.mat.data, info.data, m_resolution.width * m_resolution.height * 4);
                } else if (g_strcmp0(format_str, "GRAY8") == 0) {
                    frame.mat.create(m_resolution, CV_8UC(1));
                    memcpy(frame.mat.data, info.data, m_resolution.width * m_resolution.height);
                } else if (g_strcmp0(format_str, "GRAY16_LE") == 0) {
                    frame.mat.create(m_resolution, CV_16UC(1));
                    memcpy(frame.mat.data, info.data, m_resolution.width * m_resolution.height);
                } else {
                    qCDebug(logTISCam).noquote().nospace() << m_camSerial << ": "
                                                           << "Received buffer with unsupported format: " << format_str;
                    gst_buffer_unmap (buffer, &info);
                    continue;
                }

                // only do time adjustment if we have a valid timestamp
                // NOTE: We use the DTS here as using PTS (as before) has stopped working with newer TIS camera versions.
                // For our purpose here, PTS and DTS should be identical anyway.
                const auto pts = GST_BUFFER_DTS(buffer);
                if (pts != GST_CLOCK_TIME_NONE) {
                    clockSync->processTimestamp(frameRecvTime, std::chrono::duration_cast<microseconds_t>(nanoseconds_t(pts)));
                    m_firstTimestamp = false;
                } else {
                    // mark as us not being able to do any time adjustments if no valid timestamps are received
                    if (m_firstTimestamp)
                        clockSync->setStrategies(TimeSyncStrategy::NONE);
                }
                frame.time = usecToMsec(frameRecvTime);

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
        settings.insert("camera_serial", m_camSerial);
        settings.insert("format", m_imgFormat);
        settings.insert("width", m_resolution.width);
        settings.insert("height", m_resolution.height);
        settings.insert("fps_numerator", m_fpsNumerator);
        settings.insert("fps_denominator", m_fpsDenominator);

        if (m_camera == nullptr) {
            qCWarning(logTISCam).noquote().nospace() << "No TIS camera selected, will not save settings to file.";
            return;
        }

        QVariantList camProps;
        for (const auto &prop : m_camera->get_camera_property_list()) {
            const auto ptype = QString::fromStdString(prop->type);
            bool success = false;
            QVariantHash sProp;
            sProp.insert("type", ptype);

            if (ptype == "integer") {
                const auto intProp = std::dynamic_pointer_cast<gsttcam::IntegerProperty>(prop);
                success = intProp != nullptr;
                if (success)
                    sProp.insert("value", intProp->value);
            } else if (ptype == "double") {
                const auto doubleProp = std::dynamic_pointer_cast<gsttcam::DoubleProperty>(prop);
                success = doubleProp != nullptr;
                if (success)
                    sProp.insert("value", doubleProp->value);
            } else if (ptype == "string") {
                const auto strProp = std::dynamic_pointer_cast<gsttcam::StringProperty>(prop);
                success = strProp != nullptr;
                if (success)
                    sProp.insert("value", QString::fromStdString(strProp->value));
            } else if (ptype == "enum") {
                const auto enumProp = std::dynamic_pointer_cast<gsttcam::EnumProperty>(prop);
                success = enumProp != nullptr;
                if (success)
                    sProp.insert("value", QString::fromStdString(enumProp->value));
            } else if (ptype == "boolean" || ptype == "button") {
                const auto boolProp = std::dynamic_pointer_cast<gsttcam::BooleanProperty>(prop);
                success = boolProp != nullptr;
                if (success)
                    sProp.insert("value", boolProp->value);
            } else {
                qCWarning(logTISCam).noquote().nospace() << m_camSerial << ": "
                                                         << "Can not save camera property of unknown type: " << QString::fromStdString(prop->to_string());
                continue;
            }

            if (!success) {
                qCWarning(logTISCam).noquote().nospace() << m_camSerial << ": "
                                                         << "Unable to save camera property:" << QString::fromStdString(prop->to_string());
                continue;
            }

            sProp.insert("name", QString::fromStdString(prop->name));
            sProp.insert("group", QString::fromStdString(prop->group));
            sProp.insert("category", QString::fromStdString(prop->category));
            camProps.append(sProp);
        }

        settings.insert("camera_properties", camProps);
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        selectCamera(settings.value("camera_serial").toString(),
                     settings.value("format").toString(),
                     settings.value("width").toInt(),
                     settings.value("height").toInt(),
                     settings.value("fps_numerator").toInt(),
                     settings.value("fps_denominator").toInt());

        // only continue loading camera settings if we selected the right camera
        if (m_camera == nullptr) {
            qCWarning(logTISCam).noquote().nospace() << "Unable to load find exact camera match for '" << settings.value("camera_serial").toString() << "' "
                                                     << "Will not load camera settings.";
            return true;
        }

        const auto camProps = settings.value("camera_properties").toList();
        for (const auto &cpropVar : camProps) {
            const auto cprop = cpropVar.toHash();
            const auto ptype = cprop.value("type").toString();
            const auto pname = cprop.value("name").toString();
            const auto valueVar = cprop.value("value");

            // sanity check for damaged configuration
            if (pname.isEmpty() || valueVar.isNull())
                continue;

            GValue gval = G_VALUE_INIT;
            if (ptype == "integer") {
                g_value_init(&gval, G_TYPE_INT);
                g_value_set_int(&gval, valueVar.toInt());
            } else if (ptype == "double") {
                g_value_init(&gval, G_TYPE_DOUBLE);
                g_value_set_double(&gval, valueVar.toDouble());
            } else if (ptype == "string") {
                g_value_init(&gval, G_TYPE_STRING);
                g_value_set_string(&gval, qPrintable(valueVar.toString()));
            } else if (ptype == "enum") {
                g_value_init(&gval, G_TYPE_STRING);
                g_value_set_string(&gval, qPrintable(valueVar.toString()));
            } else if (ptype == "boolean" || ptype == "button") {
                g_value_init(&gval, G_TYPE_BOOLEAN);
                g_value_set_boolean(&gval, valueVar.toBool());
            } else {
                qCWarning(logTISCam).noquote().nospace() << m_camSerial << ": "
                                                         << "Unable to load camera property of unknown type:" << ptype;
                continue;
            }

            bool ret = m_camera->set_property(pname.toStdString(), gval);
            if (!ret)
                qCWarning(logTISCam).noquote().nospace() << m_camSerial << ": "
                                                         << "Unable to set camera property '" << pname << "' to '" << valueVar << "'";
        }

        m_propDialog->updateProperties();
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

QIcon TISCameraModuleInfo::icon() const
{
    return QIcon(":/module/camera-tis");
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

/*
 * Copyright (C) 2020-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "datactl/frametype.h"
#include "utils/misc.h"
#include <QDebug>
#include <QMessageBox>
#include <gst/app/gstappsink.h>
#include <tcam-property-1.0.h>

#include "tcamcontroldialog.h"

SYNTALOS_MODULE(TISCameraModule)

namespace Syntalos
{
Q_LOGGING_CATEGORY(logTISCam, "mod.tiscam")
}

class TISCameraModule : public AbstractModule
{
    Q_OBJECT
private:
    std::shared_ptr<DataStream<Frame>> m_outStream;

    std::shared_ptr<TcamCaptureConfig> m_capConfig;
    TcamControlDialog *m_ctlDialog;

    Device m_device;
    GstElement *m_pipeline = nullptr;
    GstAppSink *m_appSink = nullptr;
    QSize m_resolution;

    double m_fps;
    QString m_imgFormat;
    std::atomic_bool m_deviceLost;

public:
    explicit TISCameraModule(QObject *parent = nullptr)
        : AbstractModule(parent),
          m_capConfig(std::make_shared<TcamCaptureConfig>())
    {
        m_outStream = registerOutputPort<Frame>(QStringLiteral("video"), QStringLiteral("Video"));

        m_ctlDialog = new TcamControlDialog(m_capConfig);
        connect(m_ctlDialog, &TcamControlDialog::deviceLost, this, &TISCameraModule::onDeviceLost);
    }

    ~TISCameraModule()
    {
        delete m_ctlDialog;
    }

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::THREAD_DEDICATED;
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::REALTIME | ModuleFeature::SHOW_SETTINGS;
    }

    bool initialize() override
    {
        // be nice and warn the user in case udev rules are missing
        if (!hostUdevRuleExists("80-theimagingsource-cameras.rules")) {
            QMessageBox::warning(
                nullptr,
                QStringLiteral("Hardware configuration not installed"),
                QStringLiteral("<html>The hardware definitions for The Imaging Source cameras are not installed.\n"
                               "To ensure the devices are detected and work properly, please "
                               "<a href=\"https://www.theimagingsource.com/support/download/\">download & install the "
                               "driver package</a> "
                               "from the Imaging Source website."),
                QMessageBox::Ok);
        }

        return true;
    }

    void showSettingsUi() override
    {
        m_ctlDialog->setWindowTitle(QStringLiteral("%1 - Settings").arg(name()));
        m_ctlDialog->show();
        m_ctlDialog->raise();
        setCameraNameStatus();
    }

    bool isSettingsUiVisible() override
    {
        return m_ctlDialog->isVisible();
    }

    void hideSettingsUi() override
    {
        m_ctlDialog->hide();
        setCameraNameStatus();
    }

    void setCameraNameStatus(const QString &prefix = nullptr)
    {
        if (m_device.model().empty()) {
            if (!prefix.isEmpty())
                statusMessage(QStringLiteral("<html>%1: Unknown").arg(prefix));
            return;
        }

        if (prefix.isEmpty())
            statusMessage(
                QStringLiteral("<html>%1 <small>%2</small>")
                    .arg(QString::fromStdString(m_device.model()), QString::fromStdString(m_device.serial())));
        else
            statusMessage(
                QStringLiteral("<html>%1: %2 <small>%3</small>")
                    .arg(prefix, QString::fromStdString(m_device.model()), QString::fromStdString(m_device.serial())));
    }

    bool prepare(const TestSubject &) override
    {
        m_deviceLost = false;
        m_device = m_ctlDialog->selectedDevice();
        if (m_device.serial().empty()) {
            raiseError("Unable to continue: No valid camera was selected!");
            return false;
        }

        m_ctlDialog->setRunning(true);
        auto caps = m_ctlDialog->currentCaps();
        GstStructure *structure = gst_caps_get_structure(caps, 0);

        int value;
        gst_structure_get_int(structure, "width", &value);
        m_resolution.setWidth(value);
        gst_structure_get_int(structure, "height", &value);
        m_resolution.setHeight(value);

        const auto framerate = gst_structure_get_value(structure, "framerate");
        const double fps_n = gst_value_get_fraction_numerator(framerate);
        const double fps_d = gst_value_get_fraction_denominator(framerate);
        m_fps = fps_n / fps_d;
        m_imgFormat = gst_structure_get_string(structure, "format");

        // set the required stream metadata for video capture
        m_outStream->setMetadataValue("size", m_resolution);
        m_outStream->setMetadataValue("framerate", m_fps);
        m_outStream->setMetadataValue("has_color", !m_imgFormat.toUpper().startsWith("GRAY"));
        if (m_imgFormat.toUpper().startsWith("GRAY16"))
            m_outStream->setMetadataValue("depth", VIPS_FORMAT_USHORT);

        // start the stream
        m_outStream->start();
        m_pipeline = m_ctlDialog->pipeline();
        m_appSink = m_ctlDialog->videoSink();

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

        setCameraNameStatus();

        // We don't want to hold more than one buffer in the appsink, as otherwise getting the master timestamp
        // on gst_app_sink_pull_sample() will be even less accurate than it already is (and we would have to correct
        // timestamps for pending buffer content, which is difficult to do right, and complicated).
        // If the appsink max buffer count is low, elements upstream in the pipeline will be blocked until we removed
        // one and free a buffer slot, which means camera DAQ will be delayed to the speed at which we can convert
        // data into Syntalos stream elements, which is exactly what we want.
        gst_app_sink_set_max_buffers(m_appSink, 1);

        // wait until we actually start acquiring data
        waitCondition->wait(this);

        if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
            raiseError(QStringLiteral("Failed to start image acquisition pipeline."));
            return;
        }

        while (m_running) {
            g_autoptr(GstSample) sample = nullptr;
            auto frameRecvTime = MTIMER_FUNC_TIMESTAMP(sample = gst_app_sink_pull_sample(m_appSink));
            if (sample == nullptr) {
                // check if the inout stream has ended
                if (gst_app_sink_is_eos(m_appSink)) {
                    if (m_running && !m_deviceLost)
                        raiseError(QStringLiteral("Video stream has ended prematurely!"));
                    break;
                }
                if (m_running)
                    qCWarning(logTISCam).noquote() << "Received invalid sample.";
                continue;
            }

            const auto buffer = gst_sample_get_buffer(sample);
            GstMapInfo info;

            gst_buffer_map(buffer, &info, GST_MAP_READ);
            if (info.data != nullptr) {
                GstCaps *caps = gst_sample_get_caps(sample);
                const auto gS = gst_caps_get_structure(caps, 0);
                const gchar *format_str = gst_structure_get_string(gS, "format");

                // create our frame and push it to subscribers
                Frame frame;
                if (g_strcmp0(format_str, "BGRx") == 0) {
                    frame.mat = vips::VImage::new_from_memory_copy(
                        info.data,
                        m_resolution.width() * m_resolution.height() * 4,
                        m_resolution.width(),
                        m_resolution.height(),
                        4,
                        VIPS_FORMAT_UCHAR);

                    // BGR to RGB
                    frame.mat = frame.mat.bandjoin({
                        frame.mat[2], // Red channel
                        frame.mat[1], // Green channel
                        frame.mat[0]  // Blue channel
                    });
                } else if (g_strcmp0(format_str, "GRAY8") == 0) {
                    frame.mat = vips::VImage::new_from_memory_copy(
                        info.data,
                        m_resolution.width() * m_resolution.height(),
                        m_resolution.width(),
                        m_resolution.height(),
                        1,
                        VIPS_FORMAT_UCHAR);
                } else if (g_strcmp0(format_str, "GRAY16_LE") == 0) {
                    frame.mat = vips::VImage::new_from_memory_copy(
                        info.data,
                        m_resolution.width() * m_resolution.height(),
                        m_resolution.width(),
                        m_resolution.height(),
                        1,
                        VIPS_FORMAT_USHORT);
                } else {
                    qCDebug(logTISCam).noquote().nospace() << QString::fromStdString(m_device.str()) << ": "
                                                           << "Received buffer with unsupported format: " << format_str;
                    gst_buffer_unmap(buffer, &info);
                    continue;
                }

                // only do time adjustment if we have a valid timestamp
                // NOTE: We use the DTS here as using PTS (as before) has stopped working with newer TIS camera
                // versions. For our purpose here, PTS and DTS should be identical anyway.
                const auto pts = GST_BUFFER_DTS(buffer);
                if (pts != GST_CLOCK_TIME_NONE) {
                    clockSync->processTimestamp(
                        frameRecvTime, std::chrono::duration_cast<microseconds_t>(nanoseconds_t(pts)));
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
            gst_buffer_unmap(buffer, &info);
        }

        if (!m_deviceLost) {
            gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
            gst_element_set_state(m_pipeline, GST_STATE_READY);
        }
    }

    void stop() override
    {
        // we may still be blocking on the GStreamer buffer pull, so
        // we need to stop the pipeline here as well to make sure
        // we don't deadlock
        m_running = false;

        // we are not running anymore, so new device selections are possible again
        m_ctlDialog->setRunning(false);
    }

    void onDeviceLost(const QString &message)
    {
        m_deviceLost = true;
        stop();
        m_ctlDialog->closePipeline();
        raiseError(message);
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        m_device = m_ctlDialog->selectedDevice();
        if (m_device.serial().empty()) {
            qCWarning(logTISCam).noquote().nospace() << "No TIS camera selected, will not save settings to file.";
            return;
        }

        settings.insert("camera_serial", QString::fromStdString(m_device.serial()));
        settings.insert("camera_model", QString::fromStdString(m_device.model()));
        settings.insert("camera_type", QString::fromStdString(m_device.type()));
        g_autofree gchar *caps_str = gst_caps_to_string(m_ctlDialog->currentCaps());
        settings.insert("caps", QString::fromUtf8(caps_str));

        QVariantList camProps;
        auto collection = m_ctlDialog->tcamCollection();
        auto names = collection->get_names();
        if (collection == nullptr) {
            qCWarning(logTISCam, "Unable to save camera properties: No collection for active camera.");
            return;
        }

        for (const std::string &name : names) {
            g_autoptr(GError) error = nullptr;
            g_autoptr(TcamPropertyBase) prop = collection->get_property(name);

            if (!prop) {
                qCWarning(logTISCam, "Unable to retrieve property: %s", name.c_str());
                continue;
            }

            // we don't want to store values for read-only or write-only properties, as we could either
            // not write them back later, or load their values to save now.
            const auto accessLevel = tcam_property_base_get_access(prop);
            if (accessLevel == TCAM_PROPERTY_ACCESS_RO || accessLevel == TCAM_PROPERTY_ACCESS_WO)
                continue;

            QVariantHash sProp;
            const auto typeId = tcam_property_base_get_property_type(prop);
            sProp.insert("type_id", typeId);
            sProp.insert("name", QString::fromStdString(name));

            switch (typeId) {
            case TCAM_PROPERTY_TYPE_FLOAT: {
                double value = tcam_property_float_get_value(TCAM_PROPERTY_FLOAT(prop), &error);
                if (error)
                    break;

                sProp.insert("value", value);
                break;
            }
            case TCAM_PROPERTY_TYPE_INTEGER: {
                auto value = tcam_property_integer_get_value(TCAM_PROPERTY_INTEGER(prop), &error);
                if (error)
                    break;

                sProp.insert("value", QVariant::fromValue(value));
                break;
            }
            case TCAM_PROPERTY_TYPE_ENUMERATION: {
                auto value = tcam_property_enumeration_get_value(TCAM_PROPERTY_ENUMERATION(prop), &error);
                if (error)
                    break;

                sProp.insert("value", QString::fromUtf8(value));
                break;
            }
            case TCAM_PROPERTY_TYPE_BOOLEAN: {
                bool value = tcam_property_boolean_get_value(TCAM_PROPERTY_BOOLEAN(prop), &error);
                if (error)
                    break;

                sProp.insert("value", value);
                break;
            }
            case TCAM_PROPERTY_TYPE_STRING: {
                auto value = tcam_property_string_get_value(TCAM_PROPERTY_STRING(prop), &error);
                if (error)
                    break;

                sProp.insert("value", value);
                break;
            }
            case TCAM_PROPERTY_TYPE_COMMAND:
                break;
            }

            if (error != nullptr) {
                qCWarning(logTISCam).noquote().nospace()
                    << QString::fromStdString(m_device.serial()) << ": "
                    << "Unable to save camera property: " << QString::fromUtf8(error->message);
                continue;
            }

            sProp.insert("category", QString::fromStdString(tcam_property_base_get_category(prop)));
            camProps.append(sProp);
        }

        settings.insert("camera_properties", camProps);
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        const auto capsStr = settings.value("caps").toString();
        g_autoptr(GstCaps) caps = gst_caps_from_string(qPrintable(capsStr));
        bool ret = m_ctlDialog->setDevice(
            settings.value("camera_model").toString(),
            settings.value("camera_serial").toString(),
            settings.value("camera_type").toString(),
            caps);

        // only continue loading camera settings if we selected the right camera
        if (!ret) {
            qCWarning(logTISCam).noquote().nospace()
                << "Unable to load find exact camera match for '" << settings.value("camera_model").toString() << " "
                << settings.value("camera_serial").toString() << " [" << settings.value("camera_type").toString() << "]"
                << "' "
                << "Will not load camera settings.";
            setStatusMessage(
                QStringLiteral("<html><font color=\"red\">Missing:</font> %1 <small>%2</small>")
                    .arg(settings.value("camera_model").toString(), settings.value("camera_serial").toString()));
            return true;
        }

        m_device = m_ctlDialog->selectedDevice();
        setCameraNameStatus();

        const auto camProps = settings.value("camera_properties").toList();
        auto collection = m_ctlDialog->tcamCollection();
        if (collection == nullptr) {
            qCWarning(logTISCam, "Unable to load camera properties: No collection for active camera.");
            return true;
        }
        for (const auto &cpropVar : camProps) {
            bool ok;
            const auto cprop = cpropVar.toHash();
            const auto typeId = cprop.value("type_id").toInt(&ok);
            const auto name = cprop.value("name").toString();
            const auto valueVar = cprop.value("value");

            // sanity check for damaged configuration
            if (!ok || valueVar.isNull())
                continue;

            g_autoptr(GError) error = nullptr;
            g_autoptr(TcamPropertyBase) prop = collection->get_property(qPrintable(name));

            // skip unknown properties
            if (prop == nullptr)
                continue;

            // only load values for properties that we can write to
            if (tcam_property_base_get_access(prop) == TCAM_PROPERTY_ACCESS_RO) {
                qCDebug(logTISCam).noquote().nospace() << QString::fromStdString(m_device.serial()) << ": "
                                                       << "Skipped loading read-only property '" << name << "'";
                continue;
            }
            if (tcam_property_base_is_locked(prop, nullptr)) {
                qCDebug(logTISCam).noquote().nospace() << QString::fromStdString(m_device.serial()) << ": "
                                                       << "Skipped loading locked property '" << name << "'";
                continue;
            }

            switch (typeId) {
            case TCAM_PROPERTY_TYPE_FLOAT: {
                tcam_property_float_set_value(TCAM_PROPERTY_FLOAT(prop), valueVar.toDouble(), &error);
                break;
            }
            case TCAM_PROPERTY_TYPE_INTEGER: {
                tcam_property_integer_set_value(TCAM_PROPERTY_INTEGER(prop), valueVar.toInt(), &error);
                break;
            }
            case TCAM_PROPERTY_TYPE_ENUMERATION: {
                tcam_property_enumeration_set_value(
                    TCAM_PROPERTY_ENUMERATION(prop), qPrintable(valueVar.toString()), &error);
                break;
            }
            case TCAM_PROPERTY_TYPE_BOOLEAN: {
                tcam_property_boolean_set_value(TCAM_PROPERTY_BOOLEAN(prop), valueVar.toBool(), &error);
                break;
            }
            case TCAM_PROPERTY_TYPE_STRING: {
                tcam_property_string_set_value(TCAM_PROPERTY_STRING(prop), qPrintable(valueVar.toString()), &error);
                break;
            }
            case TCAM_PROPERTY_TYPE_COMMAND:
                break;
            }

            if (error != nullptr) {
                qCWarning(logTISCam).noquote().nospace()
                    << QString::fromStdString(m_device.serial()) << ": "
                    << "Unable to load camera property '" << name << "': " << QString::fromUtf8(error->message);
                continue;
            }
        }

        m_ctlDialog->refreshPropertiesInfo();

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

ModuleCategories TISCameraModuleInfo::categories() const
{
    return ModuleCategory::DEVICES;
}

QString TISCameraModuleInfo::license() const
{
    return QStringLiteral(
        "This module embeds code from <a href=\"https://www.theimagingsource.com/\">The Imaging Source</a> "
        "which is distributed under the terms of the Apache-2.0 license.");
}

AbstractModule *TISCameraModuleInfo::createModule(QObject *parent)
{
    return new TISCameraModule(parent);
}

#include "tiscameramodule.moc"

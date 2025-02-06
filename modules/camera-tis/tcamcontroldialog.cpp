/**
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0+
 */

#include "tcamcontroldialog.h"
#include "ui_tcamcontroldialog.h"

#include <QMessageBox>
#include <gst/app/gstappsink.h>

#include "caps.h"
#include "capswidget.h"
#include "devicedialog.h"
#include "tiscameramodule.h"

TcamControlDialog::TcamControlDialog(std::shared_ptr<TcamCaptureConfig> config, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::TcamControlDialog),
      m_tcamCollection(nullptr)
{
    ui->setupUi(this);

    m_capConfig = config;
    m_index = std::make_shared<Indexer>();
    connect(m_index.get(), &Indexer::device_lost, this, &TcamControlDialog::emitDeviceLost);
}

TcamControlDialog::~TcamControlDialog()
{
    delete ui;
    if (m_currentCaps)
        gst_caps_unref(m_currentCaps);
    if (m_tcamCollection)
        delete m_tcamCollection;
}

GstElement *TcamControlDialog::pipeline() const
{
    return m_pipeline;
}

GstAppSink *TcamControlDialog::videoSink() const
{
    return m_videoSink;
}

GstCaps *TcamControlDialog::currentCaps() const
{
    return m_currentCaps;
}

Device TcamControlDialog::selectedDevice() const
{
    return m_selectedDevice;
}

bool TcamControlDialog::setDevice(const QString &model, const QString &serial, const QString &type, GstCaps *caps)
{
    Q_UNUSED(model)
    closePipeline();

    m_selectedDevice = Device();
    const auto serialStr = serial.toStdString();
    const auto typeStr = type.toStdString();
    for (auto device : m_index->get_device_list()) {
        if (device.serial() == serialStr && device.type() == typeStr) {
            m_selectedDevice = device;
            break;
        }
    }

    // return false if device was not found
    if (m_selectedDevice.serial().empty())
        return false;

    if (m_selectedCaps)
        gst_caps_unref(m_selectedCaps);
    m_selectedCaps = gst_caps_ref(caps);

    openPipeline(FormatHandling::Static);
    createPropertiesBox();

    return true;
}

TcamCollection *TcamControlDialog::tcamCollection() const
{
    return m_tcamCollection;
}

void TcamControlDialog::refreshPropertiesInfo()
{
    if (m_propsBox)
        m_propsBox->refresh();
}

void TcamControlDialog::setRunning(bool running)
{
    ui->selectDeviceButton->setEnabled(!running);
    ui->selectFormatButton->setEnabled(!running);
}

void TcamControlDialog::emitDeviceLostBySerial(const QString &serial)
{
    if (!serial.isEmpty())
        qCWarning(logTISCam, "Device lost: %s", qPrintable(serial));

    if (serial != QString::fromStdString(m_selectedDevice.serial()))
        return;

    deletePropertiesBox();
    m_selectedDevice = {};

    emit deviceLost(QStringLiteral("Device %1 has been lost. Please reconnect/restart it.").arg(serial));
}

void TcamControlDialog::emitDeviceLost(const Device &dev)
{
    if (dev == m_selectedDevice)
        emitDeviceLostBySerial(dev.serial().c_str());
}

void TcamControlDialog::showEvent(QShowEvent *event)
{
    if (m_selectedDevice.serial().empty())
        on_selectDeviceButton_clicked();
    QWidget::showEvent(event);
}

static gboolean bus_callback(GstBus * /*bus*/, GstMessage *message, gpointer user_data)
{
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_INFO: {
        char *str;
        GError *err = nullptr;
        gst_message_parse_info(message, &err, &str);

        // qCInfo(logTISCam, "INFO: %s", str);

        QString s = str;

        // infos concerning the caps that are actually set
        // set infos for users
        if (s.startsWith("Working with src caps:")) {
            s = s.remove(QRegularExpression("\\(\\w*\\)"));
            s = s.section(":", 1);
            qCInfo(logTISCam, "%s", str);
        }

        if (err) {
            qCInfo(logTISCam, "%s", err->message);
            g_clear_error(&err);
        }
        break;
    }
    case GST_MESSAGE_ERROR: {

        char *str = nullptr;
        GError *err = nullptr;
        gst_message_parse_error(message, &err, &str);
        QString s = err->message;
        if (s.startsWith("Device lost (")) {
            int start = s.indexOf("(") + 1;
            QString serial = s.mid(start, s.indexOf(")") - start);
            ((TcamControlDialog *)user_data)->emitDeviceLostBySerial(serial);
        }

        g_error_free(err);
        g_free(str);

        break;
    }
    case GST_MESSAGE_EOS: {
        qCInfo(logTISCam, "Received EOS");

        break;
    }
    case GST_MESSAGE_STREAM_START: {
        // all sink elements are playing.
        // stream actually started

        break;
    }
    case GST_MESSAGE_STREAM_STATUS: {
        break;
    }
    case GST_MESSAGE_STATE_CHANGED: {
        // GstState oldstate;
        // GstState newstate;
        // GstState pending;
        // gst_message_parse_state_changed(message, &oldstate, &newstate, &pending);

        // qCInfo(logTISCam, "State change: old: %s new: %s pend: %s",
        //       gst_element_state_get_name(oldstate),
        //       gst_element_state_get_name(newstate),
        //       gst_element_state_get_name(pending));

        break;
    }
    case GST_MESSAGE_ELEMENT: {
        break;
    }
    case GST_MESSAGE_ASYNC_DONE: { // ignore
        break;
    }
    case GST_MESSAGE_NEW_CLOCK: { // ignore
        break;
    }
    default: {
        qCInfo(logTISCam, "Message handling not implemented: %s", GST_MESSAGE_TYPE_NAME(message));
        break;
    }
    }

    return TRUE;
}

void TcamControlDialog::openPipeline(FormatHandling handling)
{
    std::string pipeline_string = m_capConfig->pipeline.toStdString();
    g_autoptr(GError) error = nullptr;

    bool set_device = false;

    if (m_pipeline) {
        GstState state;
        GstState pending;
        // wait 0.1 sec
        GstStateChangeReturn change_ret = gst_element_get_state(m_pipeline, &state, &pending, 100000000);

        if (change_ret == GST_STATE_CHANGE_SUCCESS) {
            if (state == GST_STATE_PAUSED || state == GST_STATE_PLAYING) {
                gst_element_set_state(m_pipeline, GST_STATE_READY);
            }
        } else {
            qWarning("Unable to determine pipeline state. Attempting restart.");
            closePipeline();
        }
    } else {
        set_device = true;
        m_pipeline = gst_parse_launch(pipeline_string.c_str(), &error);

        auto bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
        [[maybe_unused]] auto gst_bus_id = gst_bus_add_watch(bus, bus_callback, this);
        gst_object_unref(bus);
    }

    if (!m_pipeline) {
        qCWarning(logTISCam, "Unable to start pipeline!");
        if (error)
            qCWarning(logTISCam, "Reason: %s", error->message);

        return;
    }

    auto has_property = [](GstElement *element, const char *name) {
        return g_object_class_find_property(G_OBJECT_GET_CLASS(element), name) != nullptr;
    };

    if (set_device) {
        m_source = gst_bin_get_by_name(GST_BIN(m_pipeline), "tcam0");

        if (!m_source) {
            // TODO throw error to user
            qCInfo(logTISCam, "NO source for you");
            return;
        }

        // if (has_property(m_source, "tcam-device"))
        // {
        //     g_object_set(m_source, "tcam-device", m_selectedDevice, nullptr);
        // }
        // else
        if (has_property(m_source, "serial")) {
            std::string serial = m_selectedDevice.serial_long();
            g_object_set(m_source, "serial", serial.c_str(), nullptr);
        }

        if (has_property(m_source, "conversion-element")) {
            qCDebug(
                logTISCam,
                "Setting 'conversion-element' property to '%s'",
                conversion_element_to_string(m_capConfig->conversion_element));
            g_object_set(m_source, "conversion-element", m_capConfig->conversion_element, nullptr);
        }
    }

    // must not be freed
    GstElementFactory *factory = gst_element_get_factory(m_source);
    GType element_type = gst_element_factory_get_element_type(factory);

    // attach a bus to the pipeline to capture error messages when starting the pipeline
    g_autoptr(GstBus) bus = gst_element_get_bus(m_pipeline);

    auto src_change_ret = gst_element_set_state(m_source, GST_STATE_READY);

    if (src_change_ret == GST_STATE_CHANGE_ASYNC) {
        GstState state;
        GstState pending;
        // wait 0.1 sec
        GstStateChangeReturn change_ret = gst_element_get_state(m_source, &state, &pending, 100000000);

        if (change_ret == GST_STATE_CHANGE_SUCCESS) {
            if (state == GST_STATE_PAUSED || state == GST_STATE_PLAYING) {
                gst_element_set_state(m_source, GST_STATE_READY);
            }
        } else {
            qCWarning(logTISCam, "Unable to start pipeline. Stopping.");

            g_autoptr(GstMessage) msg = gst_bus_timed_pop_filtered(
                bus, 100 * GST_MSECOND, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_STATE_CHANGED));
            if (msg) {
                g_autofree gchar *debug_info = nullptr;
                gst_message_parse_error(msg, &error, &debug_info);

                // emit a detailed error message
                if (error)
                    qCWarning(logTISCam, "Error: %s", error->message);
                if (debug_info)
                    qCWarning(logTISCam, "Debug info: %s", debug_info);
            }

            closePipeline();
            return;
        }
    } else if (src_change_ret == GST_STATE_CHANGE_FAILURE) {
        g_autoptr(GstMessage) msg = gst_bus_timed_pop_filtered(
            bus, 100 * GST_MSECOND, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_STATE_CHANGED));
        if (msg) {
            g_autofree gchar *debug_info = nullptr;
            gst_message_parse_error(msg, &error, &debug_info);

            if (error)
                QMessageBox::critical(this, "Unable to open device", QString("Error: %1").arg(error->message));
            if (debug_info)
                qCWarning(logTISCam, "Unable to open device: %s", debug_info);

        } else {
            QMessageBox::critical(this, "Unable to open device", "Failed to set pipeline state to READY.");
            qCWarning(logTISCam, "Unable to open device: Failed to set pipeline state to READY.");
        }

        closePipeline();
        return;
    }
    // we want the device caps
    // there are two scenarios
    // tcambin -> get subelement
    // source element -> use that
    GstElement *tcamsrc = nullptr;
    if (element_type == g_type_from_name("GstTcamBin")) {
        tcamsrc = gst_bin_get_by_name(GST_BIN(m_source), "tcambin-source");
    } else {
        tcamsrc = m_source;
        gst_object_ref(tcamsrc);
    }

    GstCaps *src_caps = nullptr;
    if (has_property(m_source, "available-caps")) {
        const char *available_caps = nullptr;
        g_object_get(m_source, "available-caps", &available_caps, NULL);

        if (available_caps) {
            src_caps = gst_caps_from_string(available_caps);
            m_selectedDevice.set_caps(src_caps);
        }
    } else {
        GstPad *src_pad = gst_element_get_static_pad(tcamsrc, "src");

        src_caps = gst_pad_query_caps(src_pad, nullptr);

        gst_object_unref(src_pad);

        m_selectedDevice.set_caps(src_caps);

        gst_object_unref(tcamsrc);
    }

    GstCaps *caps = nullptr;
    if (handling == FormatHandling::Dialog) {
        m_selectedCaps = showFormatDialog();

        if (!m_selectedCaps) {
            closePipeline();
            return;
        }

        caps = gst_caps_copy(m_selectedCaps);
    } else if (handling == FormatHandling::Static) {
        caps = gst_caps_copy(m_selectedCaps);
    } else {
        caps = Caps::get_default_caps(src_caps);
    }

    if (src_caps) {
        gst_caps_unref(src_caps);
    }

    if (has_property(m_source, "device-caps")) {
        qCInfo(logTISCam, "setting caps to: %s", gst_caps_to_string(caps));
        g_object_set(m_source, "device-caps", gst_caps_to_string(caps), nullptr);
    } else {
        auto capsfilter = gst_bin_get_by_name(GST_BIN(m_pipeline), "device-caps");

        if (!capsfilter) {
            qWarning("Source does not have property 'device-caps'.");
            qWarning("Alternative of capsfilter named 'device-caps' does not exist.");
        } else {
            // TODO check if element really is a capsfilter
            g_object_set(capsfilter, "caps", caps, nullptr);
        }
    }

    if (m_currentCaps)
        gst_caps_unref(m_currentCaps);
    m_currentCaps = caps;

    // do this last
    // Class queries elements automatically
    // at this point all properties have to be available
    if (m_tcamCollection != nullptr)
        delete m_tcamCollection;
    m_tcamCollection = new TcamCollection(GST_BIN(m_pipeline));

    m_videoSink = GST_APP_SINK(gst_bin_get_by_name(GST_BIN(m_pipeline), "sink"));
    g_object_set(m_videoSink, "max-buffers", 4, "drop", true, nullptr);
    if (!m_videoSink) {
        qErrnoWarning("Unable to find sink element. Potentially unable to stream...");
    }
}

void TcamControlDialog::closePipeline()
{
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;

        m_device_caps.clear();

        m_source = nullptr;
    }
}

void TcamControlDialog::on_selectDeviceButton_clicked()
{
    DeviceDialog dialog(m_index);

    const auto res = dialog.exec();
    if (res == QDialog::Accepted) {
        setEnabled(false);
        if (m_selectedCaps) {
            gst_caps_unref(m_selectedCaps);
            m_selectedCaps = nullptr;
        }
        if (m_pipeline) {
            if (m_propsBox) {
                m_propsBox->hide();

                delete m_propsBox;
                m_propsBox = nullptr;
            }
            closePipeline();
        }

        m_selectedDevice = dialog.get_selected_device();
        qCInfo(logTISCam, "device selected: %s", m_selectedDevice.str().c_str());

        openPipeline(m_capConfig->format_selection_type);
        createPropertiesBox();
        setEnabled(true);
    } else {
        qCInfo(logTISCam, "No device selected");
    }
}

void TcamControlDialog::createPropertiesBox()
{
    if (m_propsBox)
        return;

    if (!m_pipeline)
        return;

    m_propsBox = new PropertiesBox(*m_tcamCollection, ui->propsContainer);

    setWindowTitle(QStringLiteral("%1 - %2: Properties")
                       .arg(m_selectedDevice.model().c_str(), m_selectedDevice.serial_long().c_str()));
    ui->propsContainer->layout()->addWidget(m_propsBox);
}

void TcamControlDialog::deletePropertiesBox()
{
    if (m_propsBox == nullptr)
        return;

    ui->propsContainer->layout()->removeWidget(m_propsBox);
    delete m_propsBox;
    m_propsBox = nullptr;
}

GstCaps *TcamControlDialog::showFormatDialog()
{
    auto format_dialog = QDialog();

    format_dialog.setWindowFlags(format_dialog.windowFlags() | Qt::Tool);

    auto layout = new QVBoxLayout();

    format_dialog.setLayout(layout);

    auto factory = gst_element_get_factory(m_source);

    // dependending on the pipeline we want to use a different element
    // tcambin will change GstQueries we send
    // always prefer tcamsrc when dealing with device caps
    GstElement *caps_element = m_source;
    bool must_free = false;
    if (strcmp("GstTcamBin", g_type_name(gst_element_factory_get_element_type(factory))) == 0) {
        caps_element = gst_bin_get_by_name(GST_BIN(m_source), "tcambin-source");
        must_free = true;
    }

    auto fmt_widget = new CapsWidget(Caps(m_selectedDevice.caps(), *caps_element));

    layout->addWidget(fmt_widget);

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    connect(buttonBox, &QDialogButtonBox::accepted, &format_dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &format_dialog, &QDialog::reject);

    layout->addWidget(buttonBox);

    QString window_title = "Caps - ";
    window_title += m_selectedDevice.model().c_str();
    window_title += " - ";
    window_title += m_selectedDevice.serial_long().c_str();
    format_dialog.setWindowTitle(window_title);
    format_dialog.setWindowIcon(windowIcon());

    format_dialog.setMinimumSize(320, 240);
    format_dialog.setMaximumSize(640, 480);

    if (!m_device_caps.isEmpty()) {
        GstCaps *c = gst_caps_from_string(m_device_caps.toStdString().c_str());
        fmt_widget->set_caps(c, *caps_element);
        gst_caps_unref(c);
    }

    if (must_free) {
        gst_object_unref(caps_element);
    }

    if (format_dialog.exec() == QDialog::Accepted) {
        GstCaps *new_caps = fmt_widget->get_caps();
        return new_caps;
    }
    return nullptr;
}

void TcamControlDialog::on_selectFormatButton_clicked()
{
    auto caps = showFormatDialog();
    if (!caps)
        return;

    if (m_selectedCaps) {
        gst_caps_unref(m_selectedCaps);
        m_selectedCaps = nullptr;
    }

    m_selectedCaps = caps;
    openPipeline(FormatHandling::Static);
}

void TcamControlDialog::on_refreshButton_clicked()
{
    if (m_propsBox != nullptr)
        m_propsBox->refresh();
}

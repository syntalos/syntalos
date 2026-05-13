/**
 * Copyright (C) 2020-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "audiosrcmodule.h"
#include "QtSvg/qsvgrenderer.h"

#include <QDir>
#include <QMessageBox>
#include <QPainter>
#include <QSvgRenderer>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#include <gst/gst.h>
#pragma GCC diagnostic pop

#include "audiosettingsdialog.h"
#include "utils/style.h"

SYNTALOS_MODULE(AudioSourceModule)

static gboolean audiosrc_pipeline_watch_func(GstBus *bus, GstMessage *message, gpointer udata);

class AudioSourceModule : public AbstractModule
{
    Q_OBJECT

private:
    AudioSettingsDialog *m_settingsDialog;

    std::shared_ptr<StreamInputPort<ControlCommand>> m_ctlPort;
    std::shared_ptr<StreamSubscription<ControlCommand>> m_ctlIn;

    ControlCommandKind m_prevCommand;

    GstElement *m_audioSource;
    GstElement *m_audioSink;
    GstElement *m_pipeline;
    GstBus *m_bus;
    guint m_busWatchId;

public:
    explicit AudioSourceModule(ModuleInfo *modInfo, QObject *parent = nullptr)
        : AbstractModule(parent),
          m_settingsDialog(nullptr),
          m_prevCommand(ControlCommandKind::STOP),
          m_audioSource(nullptr),
          m_audioSink(nullptr),
          m_pipeline(nullptr),
          m_bus(nullptr),
          m_busWatchId(0)
    {
        m_ctlPort = registerInputPort<ControlCommand>(QStringLiteral("control-in"), QStringLiteral("Control"));

        m_settingsDialog = new AudioSettingsDialog;
        addSettingsWindow(m_settingsDialog);
        setName(name());
    }

    ~AudioSourceModule() override
    {
        deletePipeline();
    }

    bool initialize() override
    {
        setupPipeline();
        return AbstractModule::initialize();
    }

    void setName(const QString &name) override
    {
        AbstractModule::setName(name);
        m_settingsDialog->setWindowTitle(QStringLiteral("Settings for %1").arg(name));
    }

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::EVENTS_SHARED;
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_SETTINGS;
    }

    GstElement *createSinkFromConfiguredDevice()
    {
        const QString wantedId = m_settingsDialog->deviceId();
        if (wantedId.isEmpty())
            return nullptr;

        GstElement *sink = nullptr;
        GstDeviceMonitor *monitor = gst_device_monitor_new();
        GstCaps *caps = gst_caps_new_empty_simple("audio/x-raw");
        gst_device_monitor_add_filter(monitor, "Audio/Sink", caps);
        gst_caps_unref(caps);

        if (gst_device_monitor_start(monitor)) {
            GList *devices = gst_device_monitor_get_devices(monitor);
            for (GList *it = devices; it != nullptr; it = it->next) {
                g_autoptr(GstDevice) dev = GST_DEVICE(it->data);
                if (sink != nullptr)
                    continue;

                QString id;
                g_autoptr(GstStructure) props = gst_device_get_properties(dev);
                if (props != nullptr) {
                    for (const char *key : {"node.name", "device.bus_path", "device.name", "alsa.card_name"}) {
                        const gchar *val = gst_structure_get_string(props, key);
                        if (val != nullptr && *val != '\0') {
                            id = QString::fromUtf8(val);
                            break;
                        }
                    }
                }
                if (id.isEmpty()) {
                    g_autofree gchar *displayName = gst_device_get_display_name(dev);
                    id = displayName != nullptr ? QString::fromUtf8(displayName) : QString();
                }
                if (id == wantedId)
                    sink = gst_device_create_element(dev, "output");
            }
            g_list_free(devices);
            gst_device_monitor_stop(monitor);
        }
        gst_object_unref(monitor);

        if (sink == nullptr)
            LOG_WARNING(m_log, "Configured audio output device '{}' not found, using default.", wantedId);
        return sink;
    }

    bool setupPipeline()
    {
        if (m_pipeline != nullptr) {
            LOG_CRITICAL(m_log, "Tried to re-setup pipeline that already existed!");
            return true;
        }
        m_pipeline = gst_pipeline_new("sy_audiogen");
        m_audioSource = gst_element_factory_make("audiotestsrc", "source");

        m_audioSink = createSinkFromConfiguredDevice();
        if (m_audioSink == nullptr)
            m_audioSink = gst_element_factory_make("pipewiresink", "output");
        if (m_audioSink == nullptr)
            m_audioSink = gst_element_factory_make("pulsesink", "output");
        if (m_audioSink == nullptr) {
            raiseError(
                QStringLiteral("Failed to create an audio output sink (no PipeWire/PulseAudio sink available)."));
            return false;
        }

        if (g_object_class_find_property(G_OBJECT_GET_CLASS(m_audioSink), "client-name") != nullptr)
            g_object_set(m_audioSink, "client-name", qPrintable(QStringLiteral("Syntalos: %1").arg(name())), NULL);

        gst_bin_add_many(GST_BIN(m_pipeline), m_audioSource, m_audioSink, NULL);
        gst_element_link(m_audioSource, m_audioSink);

        m_bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
        m_busWatchId = gst_bus_add_watch(m_bus, audiosrc_pipeline_watch_func, this);

        return true;
    }

    void deletePipeline()
    {
        if (m_busWatchId != 0) {
            g_source_remove(m_busWatchId);
            m_busWatchId = 0;
        }
        if (m_pipeline == nullptr)
            return;
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        g_clear_pointer(&m_pipeline, gst_object_unref);
        g_clear_pointer(&m_bus, gst_object_unref);
        m_audioSource = nullptr;
        m_audioSink = nullptr;
    }

    bool resetPipeline()
    {
        deletePipeline();
        return setupPipeline();
    }

    void failPipeline(const QString &errorMessage)
    {
        deletePipeline();
        raiseError(errorMessage);
    }

    ControlCommandKind prevCommand() const
    {
        return m_prevCommand;
    }

    void setPlayStateFromCommand(ControlCommandKind kind)
    {
        if (kind == ControlCommandKind::START) {
            gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
        } else if (kind == ControlCommandKind::STOP || kind == ControlCommandKind::PAUSE) {
            gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
        }
    }

    bool prepare(const TestSubject &) override
    {
        if (m_pipeline == nullptr)
            setupPipeline();

        if (m_ctlPort->hasSubscription()) {
            m_ctlIn = m_ctlPort->subscription();
            if (m_ctlIn.get() != nullptr)
                registerDataReceivedEvent(&AudioSourceModule::onControlReceived, m_ctlIn);
        } else {
            m_ctlIn.reset();
        }

        if (!resetPipeline())
            return false;
        g_object_set(m_audioSource, "wave", m_settingsDialog->waveKind(), NULL);
        g_object_set(m_audioSource, "freq", m_settingsDialog->frequency(), NULL);
        g_object_set(m_audioSource, "volume", m_settingsDialog->volume(), NULL);
        LOG_INFO(
            m_log,
            "Playing wave {} @ {} Hz, volume: {}",
            m_settingsDialog->waveKindName().toStdString(),
            m_settingsDialog->frequency(),
            m_settingsDialog->volume());

        return true;
    }

    void start() override
    {
        if (m_settingsDialog->startImmediately()) {
            gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
            m_prevCommand = ControlCommandKind::START;
        } else {
            gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
            m_prevCommand = ControlCommandKind::STOP;
        }
        AbstractModule::start();
    }

    void stop() override
    {
        // this will terminate the thread
        m_running = false;

        gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
    }

    static gboolean onResetTimerTimeout(gpointer udata)
    {
        auto self = static_cast<AudioSourceModule *>(udata);
        self->setPlayStateFromCommand(self->prevCommand());
        return G_SOURCE_REMOVE;
    }

    void onControlReceived()
    {
        const auto maybeCtl = m_ctlIn->peekNext();
        if (!maybeCtl.has_value())
            return;

        const auto &ctl = maybeCtl.value();

        setPlayStateFromCommand(ctl.kind);
        if (ctl.duration.count() == 0)
            m_prevCommand = ctl.kind;
        else
            g_timeout_add_full(G_PRIORITY_HIGH, ctl.duration.count(), &onResetTimerTimeout, this, nullptr);
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert("play_immediately", m_settingsDialog->startImmediately());

        settings.insert("device_id", m_settingsDialog->deviceId());
        settings.insert("wave_type", m_settingsDialog->waveKind());
        settings.insert("frequency", m_settingsDialog->frequency());
        settings.insert("volume", m_settingsDialog->volume());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_settingsDialog->setStartImmediately(settings.value("play_immediately", false).toBool());
        m_settingsDialog->setDeviceId(settings.value("device_id").toString());
        m_settingsDialog->setWaveKind(settings.value("wave_type", 0).toInt());
        m_settingsDialog->setFrequency(settings.value("frequency", 100.0).toDouble());
        m_settingsDialog->setVolume(settings.value("volume", 0.8).toDouble());

        return true;
    }
};

static gboolean audiosrc_pipeline_watch_func(GstBus *bus, GstMessage *message, gpointer udata)
{
    auto self = static_cast<AudioSourceModule *>(udata);

    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
        g_autoptr(GError) err = NULL;

        gst_message_parse_error(message, &err, NULL);
        self->failPipeline(QString::fromUtf8(err->message));

        return FALSE;
    }

    return TRUE;
}

QString AudioSourceModuleInfo::id() const
{
    return QStringLiteral("audiosource");
}

QString AudioSourceModuleInfo::name() const
{
    return QStringLiteral("Audio Source");
}

QString AudioSourceModuleInfo::description() const
{
    return QStringLiteral("Play various acoustic signals.");
}

ModuleCategories AudioSourceModuleInfo::categories() const
{
    return ModuleCategory::GENERATORS;
}

void AudioSourceModuleInfo::refreshIcon()
{
    const QString audioSrcIconFname = QDir(rootDir()).filePath("audiosource.svg");
    bool isDark = currentThemeIsDark();
    if (!isDark) {
        setIcon(QIcon(audioSrcIconFname));
        return;
    }

    // convert our bright-mode icon into something that's visible easier
    // on a dark background
    QFile f(audioSrcIconFname);
    if (!f.open(QFile::ReadOnly | QFile::Text)) {
        LOG_WARNING(logRoot, "Failed to find audiosrc module icon: {}", f.errorString());
        setIcon(QIcon(audioSrcIconFname));
        return;
    }

    QTextStream in(&f);
    auto data = in.readAll();
    QSvgRenderer renderer(data.replace(QStringLiteral("#4d4d4d"), QStringLiteral("#bdc3c7")).toLocal8Bit());
    QPixmap pix(96, 96);
    pix.fill(QColor(0, 0, 0, 0));
    QPainter painter(&pix);
    renderer.render(&painter, pix.rect());

    setIcon(QIcon(pix));
    return;
}

AbstractModule *AudioSourceModuleInfo::createModule(QObject *parent)
{
    return new AudioSourceModule(this, parent);
}

#include "audiosrcmodule.moc"

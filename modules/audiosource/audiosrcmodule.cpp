/**
 * Copyright (C) 2020-2026 Matthias Klumpp <matthias@tenstral.net>
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
#include <QFileInfo>
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
static void audiosrc_decodebin_pad_added(GstElement *decodebin, GstPad *pad, gpointer udata);

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

    bool m_fileMode;
    bool m_loopFile;

public:
    explicit AudioSourceModule(ModuleInfo *modInfo, QObject *parent = nullptr)
        : AbstractModule(parent),
          m_settingsDialog(nullptr),
          m_prevCommand(ControlCommandKind::STOP),
          m_audioSource(nullptr),
          m_audioSink(nullptr),
          m_pipeline(nullptr),
          m_bus(nullptr),
          m_busWatchId(0),
          m_fileMode(false),
          m_loopFile(false)
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

    /**
     * Check whether the configured output device currently exists, and
     * return its stable ID if it does (or an empty string otherwise).
     */
    QString findConfiguredDeviceId()
    {
        const QString wantedId = m_settingsDialog->deviceId();
        if (wantedId.isEmpty())
            return QString();

        bool found = false;
        GstDeviceMonitor *monitor = gst_device_monitor_new();
        GstCaps *caps = gst_caps_new_empty_simple("audio/x-raw");
        gst_device_monitor_add_filter(monitor, "Audio/Sink", caps);
        gst_caps_unref(caps);

        if (gst_device_monitor_start(monitor)) {
            GList *devices = gst_device_monitor_get_devices(monitor);
            for (GList *it = devices; it != nullptr; it = it->next) {
                g_autoptr(GstDevice) dev = GST_DEVICE(it->data);
                if (found)
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
                    found = true;
            }
            g_list_free(devices);
            gst_device_monitor_stop(monitor);
        }
        gst_object_unref(monitor);

        if (!found) {
            LOG_WARNING(m_log, "Configured audio output device '{}' not found, using default.", wantedId);
            return QString();
        }
        return wantedId;
    }

    /**
     * Create the audio output sink.
     *
     * We deliberately prefer pulsesink (which talks to PipeWire via pipewire-pulse) over
     * pipewiresink: the latter waits for its buffer pool while holding the PipeWire loop
     * lock in its param-changed callback, which can deadlock pipeline teardown and stall
     * the whole PipeWire graph (observed with PipeWire 1.6.8). PulseAudio sink names are
     * identical to PipeWire node names, so the configured device ID works for both.
     */
    GstElement *createAudioSink()
    {
        const QString deviceId = findConfiguredDeviceId();

        GstElement *sink = gst_element_factory_make("pulsesink", "output");
        if (sink != nullptr) {
            if (!deviceId.isEmpty())
                g_object_set(sink, "device", qPrintable(deviceId), NULL);
            return sink;
        }

        LOG_INFO(m_log, "PulseAudio sink not available, falling back to the PipeWire sink.");
        sink = gst_element_factory_make("pipewiresink", "output");
        if (sink != nullptr && !deviceId.isEmpty())
            g_object_set(sink, "target-object", qPrintable(deviceId), NULL);
        return sink;
    }

    bool setupPipeline()
    {
        if (m_pipeline != nullptr) {
            LOG_CRITICAL(m_log, "Tried to re-setup pipeline that already existed!");
            return true;
        }
        m_fileMode = m_settingsDialog->sourceKind() == AudioSourceKind::AUDIO_FILE;
        m_loopFile = m_fileMode && m_settingsDialog->loopPlayback();
        if (m_fileMode && !checkAudioFile())
            return false;

        m_pipeline = gst_pipeline_new("sy_audiogen");
        m_audioSink = createAudioSink();
        if (m_audioSink == nullptr) {
            g_clear_pointer(&m_pipeline, gst_object_unref);
            raiseError(
                QStringLiteral("Failed to create an audio output sink (no PipeWire/PulseAudio sink available)."));
            return false;
        }

        if (g_object_class_find_property(G_OBJECT_GET_CLASS(m_audioSink), "client-name") != nullptr)
            g_object_set(m_audioSink, "client-name", qPrintable(QStringLiteral("Syntalos: %1").arg(name())), NULL);

        if (m_fileMode) {
            if (!setupFilePipelineElements())
                return false;
        } else {
            m_audioSource = gst_element_factory_make("audiotestsrc", "source");
            gst_bin_add_many(GST_BIN(m_pipeline), m_audioSource, m_audioSink, NULL);
            gst_element_link(m_audioSource, m_audioSink);
        }

        m_bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
        m_busWatchId = gst_bus_add_watch(m_bus, audiosrc_pipeline_watch_func, this);

        return true;
    }

    bool checkAudioFile()
    {
        const QString filePath = m_settingsDialog->audioFilePath();
        if (filePath.isEmpty()) {
            raiseError(QStringLiteral("No audio file selected to play. Please choose a file in the module settings."));
            return false;
        }
        const QFileInfo fi(filePath);
        if (!fi.isFile() || !fi.isReadable()) {
            raiseError(QStringLiteral("The selected audio file '%1' does not exist or is not readable.").arg(filePath));
            return false;
        }
        return true;
    }

    /**
     * Build filesrc ! decodebin ! audioconvert ! audioresample ! volume ! sink
     * The sink must already have been created and m_pipeline must exist.
     * On failure, all elements are owned by the pipeline and will be cleaned up with it.
     */
    bool setupFilePipelineElements()
    {
        GstElement *fileSrc = gst_element_factory_make("filesrc", "source");
        GstElement *decoder = gst_element_factory_make("decodebin", "decoder");
        GstElement *convert = gst_element_factory_make("audioconvert", "convert");
        GstElement *resample = gst_element_factory_make("audioresample", "resample");
        m_audioSource = gst_element_factory_make("volume", "volume");

        // the pipeline takes ownership of the elements; missing ones are skipped by gst_bin_add
        for (GstElement *e : {fileSrc, decoder, convert, resample, m_audioSource, m_audioSink}) {
            if (e != nullptr)
                gst_bin_add(GST_BIN(m_pipeline), e);
        }
        if (fileSrc == nullptr || decoder == nullptr || convert == nullptr || resample == nullptr
            || m_audioSource == nullptr) {
            raiseError(QStringLiteral(
                "Failed to create GStreamer elements for audio file playback. "
                "Please check that the GStreamer base plugins are installed."));
            return false;
        }

        const QFileInfo fi(m_settingsDialog->audioFilePath());
        g_object_set(fileSrc, "location", qPrintable(fi.absoluteFilePath()), NULL);

        if (!gst_element_link(fileSrc, decoder)
            || !gst_element_link_many(convert, resample, m_audioSource, m_audioSink, NULL)) {
            raiseError(QStringLiteral("Failed to link GStreamer elements for audio file playback."));
            return false;
        }
        // decodebin only exposes its source pad once the stream type is known
        g_signal_connect(decoder, "pad-added", G_CALLBACK(audiosrc_decodebin_pad_added), convert);

        return true;
    }

    /**
     * Seek back to the start of the file. When looping is enabled, a segment seek is
     * performed, so we get a SEGMENT_DONE message instead of EOS and can continue
     * playback without a gap.
     */
    bool seekToStart(bool flush)
    {
        if (m_pipeline == nullptr || !m_fileMode)
            return false;

        auto flags = static_cast<GstSeekFlags>(m_loopFile ? GST_SEEK_FLAG_SEGMENT : GST_SEEK_FLAG_NONE);
        if (flush)
            flags = static_cast<GstSeekFlags>(flags | GST_SEEK_FLAG_FLUSH);
        return gst_element_seek(
            m_pipeline,
            1.0,
            GST_FORMAT_TIME,
            flags,
            GST_SEEK_TYPE_SET,
            0,
            GST_SEEK_TYPE_NONE,
            GST_CLOCK_TIME_NONE);
    }

    void onSegmentDone()
    {
        if (!m_fileMode)
            return;
        if (m_loopFile) {
            // continue playing from the start without flushing, for gapless looping
            LOG_DEBUG(m_log, "Audio file segment finished, looping.");
            seekToStart(false);
        }
    }

    void onEndOfStream()
    {
        if (!m_fileMode)
            return;
        if (m_loopFile) {
            // should not happen with segment seeks, but handle gracefully
            seekToStart(true);
            return;
        }

        // sample was played once: stop and rewind, so the next START plays it again
        LOG_DEBUG(m_log, "Reached end of audio file, stopping and rewinding.");
        gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
        seekToStart(true);
        m_prevCommand = ControlCommandKind::STOP;
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
        if (m_pipeline == nullptr)
            return;
        if (kind == ControlCommandKind::START) {
            gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
        } else if (kind == ControlCommandKind::PAUSE) {
            gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
        } else if (kind == ControlCommandKind::STOP) {
            gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
            // in file mode, STOP rewinds the sample, while PAUSE keeps its position
            seekToStart(true);
        }
    }

    bool prepare(const RunInfo &) override
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

        if (m_fileMode) {
            g_object_set(m_audioSource, "volume", m_settingsDialog->volume(), NULL);

            // preroll the pipeline now, so we can seek and start playback quickly later
            gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
            const auto ret = gst_element_get_state(m_pipeline, nullptr, nullptr, 10 * GST_SECOND);
            if (ret == GST_STATE_CHANGE_FAILURE) {
                // fetch the detailed error from the bus, the bus watch will not run before we return
                QString details;
                g_autoptr(GstMessage) msg = gst_bus_pop_filtered(m_bus, GST_MESSAGE_ERROR);
                if (msg != nullptr) {
                    g_autoptr(GError) err = NULL;
                    gst_message_parse_error(msg, &err, NULL);
                    if (err != nullptr)
                        details = QStringLiteral(" %1").arg(QString::fromUtf8(err->message));
                }
                failPipeline(QStringLiteral("Failed to prepare audio file '%1' for playback.%2")
                                 .arg(m_settingsDialog->audioFilePath(), details));
                return false;
            }
            if (!seekToStart(true))
                LOG_WARNING(m_log, "Unable to seek in audio file, looping may not work as expected.");

            LOG_INFO(
                m_log,
                "Playing file {} (loop: {}), volume: {}",
                m_settingsDialog->audioFilePath().toStdString(),
                m_loopFile,
                m_settingsDialog->volume());
        } else {
            g_object_set(m_audioSource, "wave", m_settingsDialog->waveKind(), NULL);
            g_object_set(m_audioSource, "freq", m_settingsDialog->frequency(), NULL);
            g_object_set(m_audioSource, "volume", m_settingsDialog->volume(), NULL);
            LOG_INFO(
                m_log,
                "Playing wave {} @ {} Hz, volume: {}",
                m_settingsDialog->waveKindName().toStdString(),
                m_settingsDialog->frequency(),
                m_settingsDialog->volume());
        }

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

        if (m_pipeline != nullptr)
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
        settings.insert("source_kind", static_cast<int>(m_settingsDialog->sourceKind()));
        settings.insert("audio_file", m_settingsDialog->audioFilePath());
        settings.insert("loop", m_settingsDialog->loopPlayback());
        settings.insert("wave_type", m_settingsDialog->waveKind());
        settings.insert("frequency", m_settingsDialog->frequency());
        settings.insert("volume", m_settingsDialog->volume());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_settingsDialog->setStartImmediately(settings.value("play_immediately", false).toBool());
        m_settingsDialog->setDeviceId(settings.value("device_id").toString());
        m_settingsDialog->setSourceKind(
            static_cast<AudioSourceKind>(
                settings.value("source_kind", static_cast<int>(AudioSourceKind::TEST_SIGNAL)).toInt()));
        m_settingsDialog->setAudioFilePath(settings.value("audio_file").toString());
        m_settingsDialog->setLoopPlayback(settings.value("loop", false).toBool());
        m_settingsDialog->setWaveKind(settings.value("wave_type", 0).toInt());
        m_settingsDialog->setFrequency(settings.value("frequency", 100.0).toDouble());
        m_settingsDialog->setVolume(settings.value("volume", 0.8).toDouble());

        return true;
    }
};

static gboolean audiosrc_pipeline_watch_func(GstBus *bus, GstMessage *message, gpointer udata)
{
    auto self = static_cast<AudioSourceModule *>(udata);

    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR: {
        g_autoptr(GError) err = NULL;

        gst_message_parse_error(message, &err, NULL);
        self->failPipeline(QString::fromUtf8(err->message));

        return FALSE;
    }
    case GST_MESSAGE_SEGMENT_DONE:
        self->onSegmentDone();
        break;
    case GST_MESSAGE_EOS:
        self->onEndOfStream();
        break;
    default:
        break;
    }

    return TRUE;
}

static void audiosrc_decodebin_pad_added(GstElement *, GstPad *pad, gpointer udata)
{
    auto convert = GST_ELEMENT(udata);

    // only link audio pads, ignore anything else the file may contain
    g_autoptr(GstCaps) caps = gst_pad_get_current_caps(pad);
    if (caps == nullptr)
        caps = gst_pad_query_caps(pad, nullptr);
    if (caps != nullptr && gst_caps_get_size(caps) > 0) {
        const gchar *name = gst_structure_get_name(gst_caps_get_structure(caps, 0));
        if (name == nullptr || !g_str_has_prefix(name, "audio/"))
            return;
    }

    g_autoptr(GstPad) sinkPad = gst_element_get_static_pad(convert, "sink");
    if (gst_pad_is_linked(sinkPad))
        return;
    if (gst_pad_link(pad, sinkPad) != GST_PAD_LINK_OK)
        GST_ERROR_OBJECT(convert, "Failed to link decoded audio pad to audio converter.");
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

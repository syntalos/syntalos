/**
 * Copyright (C) 2020-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include <QMessageBox>
#include <QDebug>
#include <QSvgRenderer>
#include <QPainter>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#include <gst/gst.h>
#pragma GCC diagnostic pop

#include "utils/style.h"
#include "audiosettingsdialog.h"

SYNTALOS_MODULE(AudioSourceModule)
Q_LOGGING_CATEGORY(logModAudio, "mod.audiosource")

static gboolean audiosrc_pipeline_watch_func (GstBus *bus, GstMessage *message, gpointer udata);

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

public:
    explicit AudioSourceModule(QObject *parent = nullptr)
        : AbstractModule(parent),
          m_settingsDialog(nullptr),
          m_pipeline(nullptr)
    {
        m_ctlPort = registerInputPort<ControlCommand>(QStringLiteral("control-in"), QStringLiteral("Control"));

        m_settingsDialog = new AudioSettingsDialog;
        addSettingsWindow(m_settingsDialog);
        setName(name());
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

    bool setupPipeline()
    {
        if (m_pipeline != nullptr) {
            qCCritical(logModAudio).noquote() << "Tried to re-setup pipeline that already existed!";
            return true;
        }
        m_pipeline = gst_pipeline_new("sy_audiogen");
        m_audioSource = gst_element_factory_make("audiotestsrc", "source");
        m_audioSink = gst_element_factory_make("pulsesink", "output");
        g_object_set(m_audioSink,
                     "client-name",
                     qPrintable(QStringLiteral("Syntalos: %1").arg(name())),
                     NULL);

        gst_bin_add_many(GST_BIN (m_pipeline), m_audioSource, m_audioSink, NULL);
        gst_element_link(m_audioSource, m_audioSink);

        m_bus = gst_pipeline_get_bus(GST_PIPELINE (m_pipeline));
        gst_bus_add_watch(m_bus, audiosrc_pipeline_watch_func, this);

        return true;
    }

    void deletePipeline()
    {
        if (m_pipeline == nullptr)
            return;
        gst_element_set_state (m_pipeline, GST_STATE_NULL);
        g_clear_pointer(&m_pipeline, g_object_unref);
        g_clear_pointer(&m_bus, g_object_unref);
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
            gst_element_set_state (m_pipeline, GST_STATE_PLAYING);
        } else if (kind == ControlCommandKind::STOP || kind == ControlCommandKind::PAUSE) {
            gst_element_set_state (m_pipeline, GST_STATE_PAUSED);
        }
    }

    bool prepare(const TestSubject &) override
    {
        if (m_pipeline == nullptr)
            setupPipeline();

        m_ctlIn = m_ctlPort->subscription();
        if (m_ctlIn.get() != nullptr)
            registerDataReceivedEvent(&AudioSourceModule::onControlReceived, m_ctlIn);

        resetPipeline();
        g_object_set(m_audioSource, "wave", m_settingsDialog->waveKind(), NULL);
        g_object_set(m_audioSource, "freq", m_settingsDialog->frequency(), NULL);
        g_object_set(m_audioSource, "volume", m_settingsDialog->volume(), NULL);
        qCDebug(logModAudio).noquote() << "Playing wave" << m_settingsDialog->waveKind() << "@" << m_settingsDialog->frequency() << "Hz, volume:" << m_settingsDialog->volume();

        return true;
    }

    void start() override
    {
        if (m_settingsDialog->startImmediately()) {
            gst_element_set_state (m_pipeline, GST_STATE_PLAYING);
            m_prevCommand = ControlCommandKind::START;
        } else {
            gst_element_set_state (m_pipeline, GST_STATE_PAUSED);
            m_prevCommand = ControlCommandKind::STOP;
        }
        AbstractModule::start();
    }

    void stop() override
    {
        // this will terminate the thread
        m_running = false;

        gst_element_set_state (m_pipeline, GST_STATE_PAUSED);

        // permit settings canges again
        m_settingsDialog->setEnabled(true);
    }

    static gboolean onResetTimerTimeout(gpointer udata)
    {
        auto self = static_cast<AudioSourceModule*>(udata);
        self->setPlayStateFromCommand(self->prevCommand());
        return G_SOURCE_REMOVE;
    }

    void onControlReceived()
    {
        const auto maybeCtl = m_ctlIn->peekNext();
        if (!maybeCtl.has_value())
            return;

        auto ctl = maybeCtl.value();

        setPlayStateFromCommand(ctl.kind);
        if (ctl.duration.count() == 0)
            m_prevCommand = ctl.kind;
        else
            g_timeout_add_full(G_PRIORITY_HIGH,
                               ctl.duration.count(),
                               &onResetTimerTimeout,
                               this,
                               nullptr);
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert("play_immediately", m_settingsDialog->startImmediately());

        settings.insert("wave_type", m_settingsDialog->waveKind());
        settings.insert("frequency", m_settingsDialog->frequency());
        settings.insert("volume", m_settingsDialog->volume());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_settingsDialog->setStartImmediately(settings.value("play_immediately", false).toBool());
        m_settingsDialog->setWaveKind(settings.value("wave_type", 0).toInt());
        m_settingsDialog->setFrequency(settings.value("frequency", 100.0).toDouble());
        m_settingsDialog->setVolume(settings.value("volume", 0.8).toDouble());

        return true;
    }
};

static gboolean
audiosrc_pipeline_watch_func (GstBus *bus, GstMessage *message, gpointer udata)
{
    auto self = static_cast<AudioSourceModule*>(udata);

    if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR) {
        g_autoptr(GError) err = NULL;

        gst_message_parse_error (message, &err, NULL);
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

QIcon AudioSourceModuleInfo::icon() const
{
    const auto audioSrcIconResStr = QStringLiteral(":/module/audiosource");
    bool isDark = currentThemeIsDark();
    if (!isDark)
        return QIcon(audioSrcIconResStr);

    // convert our bright-mode icon into something that's visible easier
    // on a dark background
    QFile f(audioSrcIconResStr);
    if (!f.open(QFile::ReadOnly | QFile::Text)) {
        qWarning().noquote() << "Failed to find audiosrc module icon: " << f.errorString();
        return QIcon(audioSrcIconResStr);
    }

    QTextStream in(&f);
    auto data = in.readAll();
    QSvgRenderer renderer(data.replace(QStringLiteral("#4d4d4d"), QStringLiteral("#bdc3c7")).toLocal8Bit());
    QPixmap pix(96, 96);
    pix.fill(QColor(0, 0, 0, 0));
    QPainter painter(&pix);
    renderer.render(&painter, pix.rect());

    return QIcon(pix);
}

AbstractModule *AudioSourceModuleInfo::createModule(QObject *parent)
{
    return new AudioSourceModule(parent);
}

#include "audiosrcmodule.moc"

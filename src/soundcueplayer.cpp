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

#include "soundcueplayer.h"

#include <cstring>
#include <QAudio>
#include <QAudioOutput>
#include <QBuffer>
#include <QFile>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QUrl>
#include <QtEndian>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#include <pipewire/pipewire.h>
#pragma GCC diagnostic pop

using namespace Syntalos;

namespace
{

/**
 * Length of the silent lead-in played to wake up a suspended output device.
 */
constexpr int LEAD_IN_MSEC = 250;

/**
 * If we finished playing something this recently, the output device can not
 * have been suspended again yet (sound servers wait a few seconds for that).
 */
constexpr auto DEVICE_WARM_PERIOD = std::chrono::seconds(3);

/**
 * Maximum time we wait for the sound server to answer a node state query.
 */
constexpr int PW_QUERY_TIMEOUT_SEC = 1;

/**
 * Build a mono 16-bit PCM WAV file of silence in memory.
 */
QByteArray makeSilenceWav(int msec, int sampleRate = 48000)
{
    const quint32 dataSize = static_cast<quint32>(sampleRate * msec / 1000) * 2;
    QByteArray wav;
    wav.reserve(44 + dataSize);

    auto appendU32 = [&wav](quint32 v) {
        char buf[4];
        qToLittleEndian(v, buf);
        wav.append(buf, 4);
    };
    auto appendU16 = [&wav](quint16 v) {
        char buf[2];
        qToLittleEndian(v, buf);
        wav.append(buf, 2);
    };

    wav.append("RIFF", 4);
    appendU32(36 + dataSize);
    wav.append("WAVE", 4);
    wav.append("fmt ", 4);
    appendU32(16); // chunk size
    appendU16(1);  // PCM
    appendU16(1);  // channels
    appendU32(static_cast<quint32>(sampleRate));
    appendU32(static_cast<quint32>(sampleRate) * 2); // byte rate
    appendU16(2);                                    // block align
    appendU16(16);                                   // bits per sample
    wav.append("data", 4);
    appendU32(dataSize);
    wav.append(static_cast<int>(dataSize), '\0');

    return wav;
}

/**
 * Helper to look up the state of a single PipeWire node by name.
 */
struct PwNodeStateQuery {
    QByteArray nodeName;
    pw_thread_loop *loop{nullptr};
    pw_core *core{nullptr};
    pw_registry *registry{nullptr};
    pw_proxy *nodeProxy{nullptr};
    spa_hook coreListener{};
    spa_hook registryListener{};
    spa_hook nodeListener{};
    int pendingSeq{0};
    bool done{false};
    std::optional<pw_node_state> state;
};

void pwOnNodeInfo(void *data, const struct pw_node_info *info)
{
    auto q = static_cast<PwNodeStateQuery *>(data);
    if (info != nullptr)
        q->state = info->state;
}

const pw_node_events PW_NODE_EVENTS = {
    .version = PW_VERSION_NODE_EVENTS,
    .info = pwOnNodeInfo,
    .param = nullptr,
};

void pwOnRegistryGlobal(
    void *data,
    uint32_t id,
    uint32_t /*permissions*/,
    const char *type,
    uint32_t /*version*/,
    const struct spa_dict *props)
{
    auto q = static_cast<PwNodeStateQuery *>(data);
    if (q->nodeProxy != nullptr || props == nullptr || std::strcmp(type, PW_TYPE_INTERFACE_Node) != 0)
        return;

    const char *name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
    if (name == nullptr || q->nodeName != name)
        return;

    q->nodeProxy = static_cast<pw_proxy *>(pw_registry_bind(q->registry, id, type, PW_VERSION_NODE, 0));
    if (q->nodeProxy == nullptr)
        return;
    pw_node_add_listener(reinterpret_cast<pw_node *>(q->nodeProxy), &q->nodeListener, &PW_NODE_EVENTS, q);

    // we need one more roundtrip to receive the node info
    q->pendingSeq = pw_core_sync(q->core, PW_ID_CORE, q->pendingSeq);
}

const pw_registry_events PW_REGISTRY_EVENTS = {
    .version = PW_VERSION_REGISTRY_EVENTS,
    .global = pwOnRegistryGlobal,
    .global_remove = nullptr,
};

void pwOnCoreDone(void *data, uint32_t id, int seq)
{
    auto q = static_cast<PwNodeStateQuery *>(data);
    if (id != PW_ID_CORE || seq != q->pendingSeq)
        return;
    q->done = true;
    pw_thread_loop_signal(q->loop, false);
}

void pwOnCoreError(void *data, uint32_t /*id*/, int /*seq*/, int /*res*/, const char * /*message*/)
{
    auto q = static_cast<PwNodeStateQuery *>(data);
    q->done = true;
    pw_thread_loop_signal(q->loop, false);
}

const pw_core_events PW_CORE_EVENTS = {
    .version = PW_VERSION_CORE_EVENTS,
    .info = nullptr,
    .done = pwOnCoreDone,
    .ping = nullptr,
    .error = pwOnCoreError,
    .remove_id = nullptr,
    .bound_id = nullptr,
    .add_mem = nullptr,
    .remove_mem = nullptr,
    .bound_props = nullptr,
};

/**
 * Query the state of the PipeWire node with the given name.
 * Returns nothing if PipeWire is unavailable, the node was not found, or the query timed out.
 */
std::optional<pw_node_state> queryPipeWireNodeState(const QByteArray &nodeName)
{
    // safe to call multiple times, and needed in case the host app didn't do it
    pw_init(nullptr, nullptr);

    PwNodeStateQuery q;
    q.nodeName = nodeName;

    q.loop = pw_thread_loop_new("sy-sndquery", nullptr);
    if (q.loop == nullptr)
        return std::nullopt;

    auto context = pw_context_new(pw_thread_loop_get_loop(q.loop), nullptr, 0);
    if (context == nullptr) {
        pw_thread_loop_destroy(q.loop);
        return std::nullopt;
    }

    q.core = pw_context_connect(context, nullptr, 0);
    if (q.core == nullptr) {
        pw_context_destroy(context);
        pw_thread_loop_destroy(q.loop);
        return std::nullopt;
    }

    pw_core_add_listener(q.core, &q.coreListener, &PW_CORE_EVENTS, &q);
    q.registry = pw_core_get_registry(q.core, PW_VERSION_REGISTRY, 0);
    pw_registry_add_listener(q.registry, &q.registryListener, &PW_REGISTRY_EVENTS, &q);

    pw_thread_loop_lock(q.loop);
    q.pendingSeq = pw_core_sync(q.core, PW_ID_CORE, 0);
    if (pw_thread_loop_start(q.loop) == 0) {
        while (!q.done) {
            if (pw_thread_loop_timed_wait(q.loop, PW_QUERY_TIMEOUT_SEC) != 0)
                break;
        }
    }
    pw_thread_loop_unlock(q.loop);
    pw_thread_loop_stop(q.loop);

    if (q.nodeProxy != nullptr) {
        spa_hook_remove(&q.nodeListener);
        pw_proxy_destroy(q.nodeProxy);
    }
    spa_hook_remove(&q.registryListener);
    pw_proxy_destroy(reinterpret_cast<pw_proxy *>(q.registry));
    spa_hook_remove(&q.coreListener);
    pw_core_disconnect(q.core);
    pw_context_destroy(context);
    pw_thread_loop_destroy(q.loop);

    return q.state;
}

} // namespace

SoundCuePlayer::SoundCuePlayer(QObject *parent)
    : QObject(parent)
{
    m_log = getLogger("sounds");
    m_gc = new GlobalConfig(this);
}

SoundCuePlayer::~SoundCuePlayer()
{
    if (m_player != nullptr)
        m_player->stop();
}

void SoundCuePlayer::ensureBackend()
{
    if (m_player != nullptr)
        return;

    // Loading the multimedia backend probes hardware and connects to the sound server,
    // so we only do this once sounds are actually used (they are off by default).
    m_devices = new QMediaDevices(this);
    m_output = new QAudioOutput(this);
    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_output);

    connect(m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &errorString) {
        LOG_ERROR(m_log, "Sound cue playback failed: {}", errorString);
        m_pendingCue.reset();
        finishPlayback();
    });

    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status != QMediaPlayer::EndOfMedia && status != QMediaPlayer::InvalidMedia)
            return;

        // the lead-in has finished, now play the actual cue
        if (m_pendingCue.has_value()) {
            const auto cue = *m_pendingCue;
            m_pendingCue.reset();
            playCueFile(cue);
            return;
        }

        finishPlayback();
    });

    // devices may come and go, so we re-resolve the configured device when that happens
    connect(m_devices, &QMediaDevices::audioOutputsChanged, this, [this]() {
        reloadSettings();
        Q_EMIT outputDevicesChanged();
    });

    reloadSettings();
}

QList<QAudioDevice> SoundCuePlayer::availableOutputDevices()
{
    ensureBackend();
    return QMediaDevices::audioOutputs();
}

void SoundCuePlayer::reloadSettings()
{
    // nothing to do if the backend isn't up: the settings are read when it is created
    if (m_player == nullptr)
        return;

    const auto wantedId = m_gc->soundOutputDeviceId();
    QAudioDevice device;

    if (!wantedId.isEmpty()) {
        for (const auto &dev : QMediaDevices::audioOutputs()) {
            if (dev.id() == wantedId) {
                device = dev;
                break;
            }
        }

        if (device.isNull()) {
            // only warn once per missing device, this may be called frequently
            if (m_missingDeviceWarned != wantedId) {
                LOG_WARNING(
                    m_log,
                    "Configured sound output device '{}' is not available, using the system default instead.",
                    m_gc->soundOutputDeviceName());
                m_missingDeviceWarned = wantedId;
            }
        } else {
            m_missingDeviceWarned.clear();
        }
    }

    if (device.isNull())
        device = QMediaDevices::defaultAudioOutput();
    m_output->setDevice(device);

    m_output->setVolume(
        QAudio::convertVolume(
            m_gc->soundVolumePercent() / 100.0,
            QAudio::LogarithmicVolumeScale,
            QAudio::LinearVolumeScale));
}

bool SoundCuePlayer::isPlaying() const
{
    return m_playing;
}

void SoundCuePlayer::play(SoundCue cue)
{
    if (!m_gc->soundCueEnabled(cue))
        return;
    startPlayback(cue);
}

void SoundCuePlayer::playPreview(SoundCue cue)
{
    startPlayback(cue);
}

bool SoundCuePlayer::outputDeviceNeedsWakeUp()
{
    // we just played something, so the device can not have gone to sleep yet
    if (std::chrono::steady_clock::now() - m_lastPlaybackEnd < DEVICE_WARM_PERIOD)
        return false;

    // Qt's device ID is the PipeWire node name (or the identical PulseAudio sink name)
    const auto state = queryPipeWireNodeState(m_output->device().id());
    if (!state.has_value()) {
        LOG_DEBUG(m_log, "Unable to determine output device state, assuming it needs a wake-up.");
        return true;
    }

    return *state != PW_NODE_STATE_RUNNING && *state != PW_NODE_STATE_IDLE;
}

void SoundCuePlayer::startPlayback(SoundCue cue)
{
    ensureBackend();

    // a new cue always interrupts whatever is currently playing
    m_pendingCue.reset();
    m_player->stop();

    if (outputDeviceNeedsWakeUp()) {
        // A suspended device swallows the first bit of audio while it wakes up, so we
        // play some silence first and start the actual cue once that has finished.
        LOG_DEBUG(m_log, "Output device is suspended, playing a lead-in first.");
        m_pendingCue = cue;
        auto silence = std::make_unique<QBuffer>();
        silence->setData(makeSilenceWav(LEAD_IN_MSEC));
        silence->open(QIODevice::ReadOnly);
        playSource(std::move(silence), QUrl(QStringLiteral("lead-in.wav")));
        return;
    }

    playCueFile(cue);
}

void SoundCuePlayer::playCueFile(SoundCue cue)
{
    const auto cueId = soundCueId(cue);
    const auto resPath = QStringLiteral(":/sounds/%1.opus").arg(cueId);

    auto file = std::make_unique<QFile>(resPath);
    if (!file->open(QIODevice::ReadOnly)) {
        LOG_ERROR(m_log, "Unable to open sound cue '{}': {}", cueId, file->errorString());
        finishPlayback();
        return;
    }

    LOG_DEBUG(m_log, "Playing sound cue '{}' on '{}'", cueId, m_output->device().description());
    playSource(std::move(file), QUrl(QStringLiteral("qrc") + resPath));
}

void SoundCuePlayer::playSource(std::unique_ptr<QIODevice> source, const QUrl &hint)
{
    // the player reads from the device asynchronously, so the source must outlive playback
    m_player->setSourceDevice(source.get(), hint);
    m_source = std::move(source);
    m_playing = true;
    m_player->play();
}

void SoundCuePlayer::finishPlayback()
{
    if (!m_playing)
        return;
    m_playing = false;
    m_lastPlaybackEnd = std::chrono::steady_clock::now();
    Q_EMIT playbackFinished();
}

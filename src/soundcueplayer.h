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

#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <QObject>
#include <QAudioDevice>

#include "globalconfig.h"

class QBuffer;
class QIODevice;
class QMediaPlayer;
class QAudioOutput;
class QMediaDevices;

namespace Syntalos
{

/**
 * @brief Plays short sound cues to alert the experimenter about events.
 */
class SoundCuePlayer : public QObject
{
    Q_OBJECT
public:
    explicit SoundCuePlayer(QObject *parent = nullptr);
    ~SoundCuePlayer() override;

    /**
     * @brief Play a cue if the user has enabled it.
     */
    void play(SoundCue cue);

    /**
     * @brief Play a cue regardless of whether it is enabled (for previews).
     */
    void playPreview(SoundCue cue);

    /**
     * @brief Re-read output device and volume from the global configuration.
     */
    void reloadSettings();

    /**
     * @brief Whether a cue (or its lead-in) is currently playing.
     */
    bool isPlaying() const;

    /**
     * @brief List all audio output devices currently available.
     */
    QList<QAudioDevice> availableOutputDevices();

signals:
    /**
     * @brief Emitted when the set of available output devices has changed.
     */
    void outputDevicesChanged();

    /**
     * @brief Emitted when a cue has finished (or failed) playing.
     */
    void playbackFinished();

private:
    void ensureBackend();
    void startPlayback(SoundCue cue);
    void playCueFile(SoundCue cue);
    void playSource(std::unique_ptr<QIODevice> source, const QUrl &hint);
    void finishPlayback();
    bool outputDeviceNeedsWakeUp();

    QuillLogger *m_log;
    GlobalConfig *m_gc;
    QMediaDevices *m_devices{nullptr};
    QMediaPlayer *m_player{nullptr};
    QAudioOutput *m_output{nullptr};
    std::unique_ptr<QIODevice> m_source;
    QByteArray m_missingDeviceWarned;

    bool m_playing{false};
    std::optional<SoundCue> m_pendingCue;
    std::chrono::steady_clock::time_point m_lastPlaybackEnd;
};

} // namespace Syntalos

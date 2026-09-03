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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QDialog>
#include <QList>
#include <QStringList>

#include "fabric/moduleapi.h"

namespace Ui
{
class SpikeGLXSettingsDialog;
}

/**
 * @brief Settings dialog for the SpikeGLX remote-control module.
 *
 * The dialog is a passive widget: it holds the settings values and emits
 * change signals. Anything that talks to SpikeGLX is done by the module.
 */
class SpikeGLXSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    enum RunControlMode {
        FullControl = 0, /// Syntalos starts and stops the SpikeGLX run
        GateOnly = 1,    /// SpikeGLX is already running; Syntalos only toggles recording
        Monitor = 2,     /// Never change SpikeGLX state, only observe
        Automatic = 3    /// FullControl if SpikeGLX is idle, GateOnly if it is already running
    };

    enum OverrunPolicy {
        AbortRun = 0, /// fail the run (default)
        SkipAhead = 1 /// jump to the newest data, log and count the gap
    };

    struct FetchEntry {
        QString stream;   /// e.g. "imec0"
        QString group;    /// e.g. "AP"
        QString channels; /// channel subset relative to the group, empty = all
    };

    explicit SpikeGLXSettingsDialog(ModuleInfo *modInfo, QWidget *parent = nullptr);
    ~SpikeGLXSettingsDialog() override;

    QString host() const;
    void setHost(const QString &host);
    int port() const;
    void setPort(int port);
    int connectTimeoutMs() const;
    void setConnectTimeoutMs(int msec);

    RunControlMode runControlMode() const;
    void setRunControlMode(RunControlMode mode);
    QString deviceString() const;
    void setDeviceString(const QString &devString);
    QString runNameExtra() const;
    void setRunNameExtra(const QString &extra);
    bool pushMetadata() const;
    void setPushMetadata(bool enabled);
    bool storeParams() const;
    void setStoreParams(bool enabled);

    int syncIntervalMs() const;
    void setSyncIntervalMs(int msec);
    QStringList syncStreams() const;
    void setSyncStreams(const QStringList &streams);

    bool fetchEnabled() const;
    void setFetchEnabled(bool enabled);
    int fetchIntervalMs() const;
    void setFetchIntervalMs(int msec);
    int fetchMaxBlockMs() const;
    void setFetchMaxBlockMs(int msec);
    OverrunPolicy overrunPolicy() const;
    void setOverrunPolicy(OverrunPolicy policy);
    QList<FetchEntry> fetchEntries() const;
    void setFetchEntries(const QList<FetchEntry> &entries);

    /** Disable interactive controls while a run is active. */
    void setRunActive(bool active);
    /** Show the outcome of a connection test. */
    void setConnectionStatus(const QString &text, bool ok);
    /** Show a description of the streams SpikeGLX offers. */
    void setStreamsInfo(const QString &text, const QStringList &streamNames);

signals:
    /** Emitted when any plain setting changed. */
    void settingsChanged();
    /** Emitted when the live-data entries changed and ports need to be rebuilt. */
    void fetchEntriesChanged();
    void testConnectionRequested();
    void queryStreamsRequested();

private:
    void appendFetchRow(const FetchEntry &entry);
    QString modeDescription(RunControlMode mode) const;
    FetchEntry fetchEntryAt(int row) const;

    Ui::SpikeGLXSettingsDialog *ui;
    QStringList m_knownStreams;
    bool m_updating = false;
};

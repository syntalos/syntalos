/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "devices/Headstage.h"

#include <QDialog>
#include <QHash>
#include <QString>
#include <vector>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;

/**
 * Settings dialog for the Open Ephys Acquisition Board module.
 *
 * The dialog is a passive widget over a settings struct: it emits change
 * signals when the user touches a control. The owning module is responsible
 * for applying the values to the live board.
 */
class OeAcqSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OeAcqSettingsDialog(QWidget *parent = nullptr);
    ~OeAcqSettingsDialog() override = default;

    /** Populate the sample-rate combo. Call once after the board is detected. */
    void setAvailableSampleRates(const std::vector<int> &ratesHz);

    /** Update the read-only "detected headstages" panel. */
    void setHeadstageSummary(const std::vector<const Headstage *> &headstages);

    int sampleRateHz() const;
    void setSampleRateHz(int rate);

    ChannelNamingScheme namingScheme() const;
    void setNamingScheme(ChannelNamingScheme scheme);

    /** Disable interactive controls while a run is active. */
    void setRunActive(bool active);

    /** Update the "Active backend:" label. */
    void setActiveBackendLabel(const QString &text);

    /** Backend selection. */
    enum BackendChoice {
        BackendAuto = 0,
        BackendDevice = 1,
        BackendSimulated = 2,
    };

    BackendChoice backendChoice() const;
    void setBackendChoice(BackendChoice choice);

    /** One row of the per-channel selector. */
    struct ChannelEntry {
        QString id;          /// canonical channel ID
        QString label;       /// display label
        QString group;       /// sub-group label, e.g. "Headstage A1" or "Board ADC"
        bool enabled = true; /// checked state
    };

    /** Rebuild the channel panel from the given inventory. */
    void setChannelInventory(const std::vector<ChannelEntry> &entries);

signals:
    void sampleRateChanged(int rateHz);
    void namingSchemeChanged(int scheme);
    void rescanRequested();
    void measureImpedancesRequested();
    void backendChoiceChanged(BackendChoice choice);
    void reconnectRequested();
    void channelEnabledChanged(const QString &channelId, bool enabled);

private:
    void buildUi();
    void rebuildChannelPanelControls();

    QComboBox *m_sampleRateCombo = nullptr;
    QComboBox *m_namingCombo = nullptr;
    QLabel *m_headstageLabel = nullptr;
    QPushButton *m_scanButton = nullptr;
    QPushButton *m_impedanceButton = nullptr;
    QComboBox *m_backendCombo = nullptr;
    QPushButton *m_reconnectButton = nullptr;
    QLabel *m_activeBackendLabel = nullptr;

    // Channels panel
    QGroupBox *m_channelsGroup = nullptr;
    QWidget *m_channelsHost = nullptr; /// widget inside the scroll area
    QVBoxLayout *m_channelsLayout = nullptr;
    QPushButton *m_enableAllButton = nullptr;
    QPushButton *m_disableAllButton = nullptr;
    QHash<QString, QCheckBox *> m_channelChecks;
    std::vector<ChannelEntry> m_channelEntries;

    bool m_emitSignals = true;
};

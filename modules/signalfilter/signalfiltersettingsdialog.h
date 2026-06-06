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
#include <vector>

#include "filterpipeline.h"

namespace Ui
{
class SignalFilterSettingsDialog;
}

/**
 * @brief Settings UI for the signalfilter module.
 *
 * Lets the user pick the input signal type, restrict filtering to a subset of
 * channels, and build a chain of filter stages.
 */
class SignalFilterSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SignalFilterSettingsDialog(QWidget *parent = nullptr);
    ~SignalFilterSettingsDialog();

    // input type
    int selectedTypeId() const;
    QString selectedTypeName() const;
    void setSelectedTypeName(const QString &typeName);

    // channel selection
    bool useAllChannels() const;
    QString channelSelectionText() const;
    void setChannelSelection(bool useAll, const QString &ranges);

    // filter stages
    std::vector<FilterStage> stages() const;
    void setStages(const std::vector<FilterStage> &stages);

    /// Disable all topology-affecting controls while a run is in progress.
    void setRunning(bool running);

Q_SIGNALS:
    /// Emitted when the input type changes (requires port reconfiguration).
    void settingsChanged();

    /// Emitted when the channel selection changes (can be applied live).
    void channelsChanged();

    /// Emitted when the filter stage list or any stage parameter changes (live).
    void stagesChanged();

private:
    void rebuildStageList();
    void selectStage(int index);
    void populateEditorFromStage(int index);
    void writeEditorToStage();
    void updateEditorVisibility();
    void updateSosStatus();
    QString stageLabel(const FilterStage &stage) const;

    Ui::SignalFilterSettingsDialog *ui;
    std::vector<FilterStage> m_stages;
    int m_currentIndex;
    bool m_loading;
};

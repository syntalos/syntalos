/*
 * Copyright (C) 2024-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include <QDialog>

namespace Ui
{
class PrismCtlDialog;
}

/**
 * @brief Operating mode of the Prism module.
 */
enum class PrismMode {
    SPLIT,    /// Split one multi-channel frame into per-channel streams
    COMBINE,  /// Merge separate per-channel streams into one multi-channel frame
    GRAYSCALE /// Convert an incoming color frame to grayscale
};

/**
 * @brief Settings dialog for the Prism module.
 */
class PrismCtlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PrismCtlDialog(QWidget *parent = nullptr);
    ~PrismCtlDialog() override;

    void setRunning(bool running);

    PrismMode mode() const;
    void setMode(PrismMode mode);

    /// Returns whether channel @p ch (0=R, 1=G, 2=B, 3=A) is enabled.
    bool channelEnabled(int ch) const;
    void setChannelEnabled(int ch, bool enabled);

    QVariantHash serializeSettings() const;
    void loadSettings(const QVariantHash &settings);

signals:
    /// Emitted whenever the mode or channel selection changes.
    void settingsChanged();

private slots:
    void onModeChanged();
    void onChannelChanged();
    void updateUiState();

private:
    Ui::PrismCtlDialog *ui;
};

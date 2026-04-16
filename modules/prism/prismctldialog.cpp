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

#include "prismctldialog.h"
#include "ui_prismctldialog.h"

PrismCtlDialog::PrismCtlDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::PrismCtlDialog)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window);

    connect(ui->rbSplit, &QRadioButton::toggled, this, &PrismCtlDialog::onModeChanged);
    connect(ui->rbCombine, &QRadioButton::toggled, this, &PrismCtlDialog::onModeChanged);
    connect(ui->rbGrayscale, &QRadioButton::toggled, this, &PrismCtlDialog::onModeChanged);

    connect(ui->cbRed, &QCheckBox::toggled, this, &PrismCtlDialog::onChannelChanged);
    connect(ui->cbGreen, &QCheckBox::toggled, this, &PrismCtlDialog::onChannelChanged);
    connect(ui->cbBlue, &QCheckBox::toggled, this, &PrismCtlDialog::onChannelChanged);
    connect(ui->cbAlpha, &QCheckBox::toggled, this, &PrismCtlDialog::onChannelChanged);

    updateUiState();
}

PrismCtlDialog::~PrismCtlDialog()
{
    delete ui;
}

void PrismCtlDialog::setRunning(bool running)
{
    ui->modeGroupBox->setEnabled(!running);
    ui->channelGroupBox->setEnabled(!running);
}

PrismMode PrismCtlDialog::mode() const
{
    if (ui->rbCombine->isChecked())
        return PrismMode::COMBINE;
    if (ui->rbGrayscale->isChecked())
        return PrismMode::GRAYSCALE;
    return PrismMode::SPLIT;
}

void PrismCtlDialog::setMode(PrismMode m)
{
    switch (m) {
    case PrismMode::SPLIT:
        ui->rbSplit->setChecked(true);
        break;
    case PrismMode::COMBINE:
        ui->rbCombine->setChecked(true);
        break;
    case PrismMode::GRAYSCALE:
        ui->rbGrayscale->setChecked(true);
        break;
    }
}

bool PrismCtlDialog::channelEnabled(int ch) const
{
    switch (ch) {
    case 0:
        return ui->cbRed->isChecked();
    case 1:
        return ui->cbGreen->isChecked();
    case 2:
        return ui->cbBlue->isChecked();
    case 3:
        return ui->cbAlpha->isChecked();
    default:
        return false;
    }
}

void PrismCtlDialog::setChannelEnabled(int ch, bool enabled)
{
    switch (ch) {
    case 0:
        ui->cbRed->setChecked(enabled);
        break;
    case 1:
        ui->cbGreen->setChecked(enabled);
        break;
    case 2:
        ui->cbBlue->setChecked(enabled);
        break;
    case 3:
        ui->cbAlpha->setChecked(enabled);
        break;
    }
}

QVariantHash PrismCtlDialog::serializeSettings() const
{
    QVariantHash s;
    s["mode"] = static_cast<int>(mode());
    s["ch_r"] = ui->cbRed->isChecked();
    s["ch_g"] = ui->cbGreen->isChecked();
    s["ch_b"] = ui->cbBlue->isChecked();
    s["ch_a"] = ui->cbAlpha->isChecked();
    return s;
}

void PrismCtlDialog::loadSettings(const QVariantHash &settings)
{
    // Block signals during bulk load so we don't get partial settingsChanged emissions
    const QSignalBlocker b1(ui->rbSplit);
    const QSignalBlocker b2(ui->rbCombine);
    const QSignalBlocker b3(ui->rbGrayscale);
    const QSignalBlocker b4(ui->cbRed);
    const QSignalBlocker b5(ui->cbGreen);
    const QSignalBlocker b6(ui->cbBlue);
    const QSignalBlocker b7(ui->cbAlpha);

    setMode(static_cast<PrismMode>(settings.value("mode", 0).toInt()));
    ui->cbRed->setChecked(settings.value("ch_r", true).toBool());
    ui->cbGreen->setChecked(settings.value("ch_g", true).toBool());
    ui->cbBlue->setChecked(settings.value("ch_b", true).toBool());
    ui->cbAlpha->setChecked(settings.value("ch_a", false).toBool());

    updateUiState();
}

void PrismCtlDialog::onModeChanged()
{
    updateUiState();
    emit settingsChanged();
}

void PrismCtlDialog::onChannelChanged()
{
    emit settingsChanged();
}

void PrismCtlDialog::updateUiState()
{
    const auto m = mode();
    const bool isGrayscale = (m == PrismMode::GRAYSCALE);

    // Channel selection is irrelevant in grayscale mode
    ui->channelGroupBox->setEnabled(!isGrayscale);

    switch (m) {
    case PrismMode::SPLIT:
        ui->lblDescription->setText(
            QStringLiteral("Each enabled channel produces a single-channel (grayscale) output stream."));
        break;
    case PrismMode::COMBINE:
        ui->lblDescription->setText(QStringLiteral(
            "Each enabled channel accepts a single-channel input stream. "
            "All connected channels are merged into one frame when data from all of them is available. "
            "Inputs are expected to be synchronized for best results."));
        break;
    case PrismMode::GRAYSCALE:
        ui->lblDescription->setText(
            QStringLiteral("Converts an incoming color frame (BGR or BGRA) to a single-channel grayscale frame."));
        break;
    }
}

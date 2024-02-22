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

#include "audiosettingsdialog.h"
#include "ui_audiosettingsdialog.h"

#include <QVariant>

AudioSettingsDialog::AudioSettingsDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::AudioSettingsDialog)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-config"));

    ui->waveComboBox->addItem("Sine", 0);
    ui->waveComboBox->addItem("Square", 1);
    ui->waveComboBox->addItem("Saw", 2);
    ui->waveComboBox->addItem("Triangle", 3);
    ui->waveComboBox->addItem("Silence", 4);
    ui->waveComboBox->addItem("White Uniform Noise", 5);
    ui->waveComboBox->addItem("Pink Noise", 6);
    ui->waveComboBox->addItem("Sine Table", 7);
    ui->waveComboBox->addItem("Periodic Ticks", 8);
    ui->waveComboBox->addItem("White Gaussian Noise", 9);
    ui->waveComboBox->addItem("Red (Brownian) Noise", 10);
    ui->waveComboBox->addItem("Blue Noise", 11);
    ui->waveComboBox->addItem("Violet Noise", 12);
}

AudioSettingsDialog::~AudioSettingsDialog()
{
    delete ui;
}

bool AudioSettingsDialog::startImmediately() const
{
    return ui->immediatePlayCheckBox->isChecked();
}

void AudioSettingsDialog::setStartImmediately(bool value)
{
    ui->immediatePlayCheckBox->setChecked(value);
}

int AudioSettingsDialog::waveKind() const
{
    return ui->waveComboBox->currentData().toInt();
}

void AudioSettingsDialog::setWaveKind(int value)
{
    ui->waveComboBox->setCurrentIndex(ui->waveComboBox->findData(value));
}

double AudioSettingsDialog::frequency() const
{
    return ui->freqSpinBox->value();
}

void AudioSettingsDialog::setFrequency(double value)
{
    ui->freqSpinBox->setValue(value);
}

double AudioSettingsDialog::volume() const
{
    return ui->volumeSlider->value() / 100.0;
}

void AudioSettingsDialog::setVolume(double value)
{
    ui->volumeSlider->setValue(value * 100.0);
}

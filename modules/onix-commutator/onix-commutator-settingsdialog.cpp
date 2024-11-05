/*
 * Copyright (C) 2023-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "onix-commutator-settingsdialog.h"
#include "ui_onix-commutator-settingsdialog.h"

#include <QSerialPort>
#include <QSerialPortInfo>
#include <QMessageBox>

#include "utils/misc.h"

ONIXCommutatorSettingsDialog::ONIXCommutatorSettingsDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::ONIXCommutatorSettingsDialog)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-config"));

    scanDevices();
}

ONIXCommutatorSettingsDialog::~ONIXCommutatorSettingsDialog()
{
    delete ui;
}

void ONIXCommutatorSettingsDialog::scanDevices()
{
    const auto selectedSerialPort = serialPort();

    // find Arduino / SP210 I/O
    auto allPorts = QSerialPortInfo::availablePorts();
    for (auto &port : allPorts) {
        if (port.description().contains("SP210"))
            ui->portsComboBox->addItem(
                QString("%1 (%2)").arg(port.portName()).arg(port.description()), port.systemLocation());
    }

    if (selectedSerialPort.isEmpty())
        return;

    // select previous entry
    for (int i = 0; i < ui->portsComboBox->count(); i++) {
        if (ui->portsComboBox->itemData(i).toString() == selectedSerialPort) {
            ui->portsComboBox->setCurrentIndex(i);
            break;
        }
    }
}

void ONIXCommutatorSettingsDialog::setRunning(bool running)
{
    ui->portsComboBox->setEnabled(!running);
    ui->statusLedCheckBox->setEnabled(!running);
    ui->speedSpinBox->setEnabled(!running);
    ui->accelerationSpinBox->setEnabled(!running);
}

QString ONIXCommutatorSettingsDialog::serialPort() const
{
    return ui->portsComboBox->currentData().toString();
}

void ONIXCommutatorSettingsDialog::setSerialPort(QString port)
{
    // select the right port
    for (int i = 0; i < ui->portsComboBox->count(); i++) {
        if (ui->portsComboBox->itemData(i).toString() == port) {
            ui->portsComboBox->setCurrentIndex(i);
            break;
        }
    }
}

bool ONIXCommutatorSettingsDialog::statusLedEnabled() const
{
    return ui->statusLedCheckBox->isChecked();
}

void ONIXCommutatorSettingsDialog::setStatusLedEnabled(bool enabled)
{
    ui->statusLedCheckBox->setChecked(enabled);
}

double ONIXCommutatorSettingsDialog::speed() const
{
    return ui->speedSpinBox->value();
}

void ONIXCommutatorSettingsDialog::setSpeed(double speed)
{
    ui->speedSpinBox->setValue(speed);
}

double ONIXCommutatorSettingsDialog::acceleration() const
{
    return ui->accelerationSpinBox->value();
}

void ONIXCommutatorSettingsDialog::setAcceleration(double acceleration)
{
    ui->accelerationSpinBox->setValue(acceleration);
}

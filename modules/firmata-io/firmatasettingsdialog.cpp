/*
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "firmatasettingsdialog.h"
#include "ui_firmatasettingsdialog.h"

#include <QSerialPortInfo>

FirmataSettingsDialog::FirmataSettingsDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::FirmataSettingsDialog)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-config"));

    // Arduino / Firmata I/O
    auto allPorts = QSerialPortInfo::availablePorts();
    for (auto &port : allPorts) {
        ui->portsComboBox->addItem(
            QString("%1 (%2)").arg(port.portName()).arg(port.description()), port.systemLocation()
        );
    }
}

FirmataSettingsDialog::~FirmataSettingsDialog()
{
    delete ui;
}

void FirmataSettingsDialog::setRunning(bool running)
{
    ui->portsComboBox->setEnabled(!running);
}

QString FirmataSettingsDialog::serialPort() const
{
    return ui->portsComboBox->currentData().toString();
}

void FirmataSettingsDialog::setSerialPort(QString port)
{
    // select the right port
    for (int i = 0; i < ui->portsComboBox->count(); i++) {
        if (ui->portsComboBox->itemData(i).toString() == port) {
            ui->portsComboBox->setCurrentIndex(i);
            break;
        }
    }
}

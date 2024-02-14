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

#include "sp210settingsdialog.h"
#include "ui_sp210settingsdialog.h"

#include <QSerialPort>
#include <QSerialPortInfo>
#include <QMessageBox>

#include "utils/misc.h"

SP210SettingsDialog::SP210SettingsDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::SP210SettingsDialog)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-config"));

    scanDevices();

    ui->zeroModeComboBox->addItem("Standard", "standard");
    ui->zeroModeComboBox->addItem("Z-Track", "ztrack");

    ui->rateComboBox->addItem("250 Hz", 250);
    ui->rateComboBox->addItem("180 Hz", 180);
    ui->rateComboBox->addItem("130 Hz", 130);
    ui->rateComboBox->addItem("100 Hz", 100);
    ui->rateComboBox->addItem("65 Hz", 65);
    ui->rateComboBox->addItem("50 Hz", 50);
    ui->rateComboBox->addItem("35 Hz", 35);
    ui->rateComboBox->addItem("25 Hz", 25);
}

SP210SettingsDialog::~SP210SettingsDialog()
{
    delete ui;
}

void SP210SettingsDialog::scanDevices()
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

void SP210SettingsDialog::setRunning(bool running)
{
    ui->portsComboBox->setEnabled(!running);
    ui->sensorInfoWidget->setEnabled(!running);
    ui->zeroModeComboBox->setEnabled(!running);
    ui->zeroNoiseSuppressionCheckBox->setEnabled(!running);
    ui->rateComboBox->setEnabled(!running);
}

QString SP210SettingsDialog::serialPort() const
{
    return ui->portsComboBox->currentData().toString();
}

void SP210SettingsDialog::setSerialPort(QString port)
{
    // select the right port
    for (int i = 0; i < ui->portsComboBox->count(); i++) {
        if (ui->portsComboBox->itemData(i).toString() == port) {
            ui->portsComboBox->setCurrentIndex(i);
            break;
        }
    }
}

QString SP210SettingsDialog::zeroMode() const
{
    return ui->zeroModeComboBox->currentData().toString();
}

void SP210SettingsDialog::setZeroMode(const QString &mode)
{
    for (int i = 0; i < ui->zeroModeComboBox->count(); i++) {
        if (ui->zeroModeComboBox->itemData(i).toString() == mode) {
            ui->zeroModeComboBox->setCurrentIndex(i);
            break;
        }
    }
}

bool SP210SettingsDialog::zeroNoiseSuppression() const
{
    return ui->zeroNoiseSuppressionCheckBox->isChecked();
}

void SP210SettingsDialog::setZeroNoiseSuppression(bool enabled)
{
    return ui->zeroNoiseSuppressionCheckBox->setChecked(enabled);
}

int SP210SettingsDialog::samplingRate() const
{
    return ui->rateComboBox->currentData().toInt();
}

void SP210SettingsDialog::setSamplingRate(int rate)
{
    for (int i = 0; i < ui->rateComboBox->count(); i++) {
        if (ui->rateComboBox->itemData(i).toString() == rate) {
            ui->rateComboBox->setCurrentIndex(i);
            break;
        }
    }
}

static void serialSendNoReplyCommand(QSerialPort &serial, const char *cmd)
{
    serial.write(cmd);
    serial.waitForReadyRead(500);
    serial.readAll();
}

void SP210SettingsDialog::on_readInfoBtn_clicked()
{
    QSerialPort serial;
    serial.close();
    serial.setBaudRate(QSerialPort::Baud115200);
    serial.setStopBits(QSerialPort::OneStop);
    serial.setPortName(serialPort());

    if (!serial.open(QIODevice::ReadWrite)) {
        QMessageBox::warning(
            this,
            "Unable to connect",
            QStringLiteral("Can't open %1, error code %2").arg(serial.portName()).arg(serial.error()));
        return;
    }

    serialSendNoReplyCommand(serial, "STOP\n");
    serial.write("INFO\n");

    QByteArray sensorDataRaw;
    while (serial.waitForReadyRead(500))
        sensorDataRaw += serial.readAll();

    QString infoStr;
    for (const auto &line : sensorDataRaw.split('\n')) {
        if (line.startsWith("I:"))
            infoStr += line.mid(2).trimmed() + "\n";
    }
    infoStr = infoStr.trimmed();
    if (infoStr.isEmpty())
        infoStr = QStringLiteral("No information received!");

    QMessageBox::information(
        this, "Device Sensor Information", "Information about the sensor in this device:\n\n" + infoStr);
}

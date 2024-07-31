/*
 * Copyright (C) 2017 Matthias Klumpp <matthias@tenstral.net>
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

#include "galdursettingsdialog.h"
#include "ui_galdursettingsdialog.h"

#include <QMessageBox>
#include <QScrollBar>
#include <QLabel>
#include <QtSerialPort>

#include "labrstimclient.h"

GaldurSettingsDialog::GaldurSettingsDialog(LabrstimClient *client, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::GaldurSettingsDialog),
      m_client(client)
{
    ui->setupUi(this);

    // be safe on what is selected
    ui->stackedWidget->setCurrentIndex(0);
    ui->stimTypeComboBox->setCurrentIndex(0);
    m_client->setMode(LabrstimClient::ModeSwr);

    // connect(m_client, &LabrstimClient::newRawData, this, &GaldurSettingsDialog::readRawData);

    // collapse the log view
    QList<int> list;
    list.append(1);
    list.append(0);
    ui->splitter->setSizes(list);

    // set default values (which should trigger value-change slots to set the values on m_client)
    ui->samplingRateSpinBox->setValue(20000);
    ui->trialDurationTimeEdit->setTime(QTime(0, 7, 0));
    ui->pulseDurationSpinBox->setValue(20);
    ui->laserIntensitySpinBox->setValue(2);
    ui->minimumIntervalSpinBox->setValue(10);
    ui->maximumIntervalSpinBox->setValue(20);

    ui->swrPowerThresholdDoubleSpinBox->setValue(3);
    ui->thetaPhaseSpinBox->setValue(90);
    ui->trainFrequencySpinBox->setValue(6);
}

GaldurSettingsDialog::~GaldurSettingsDialog()
{
    delete ui;
}

void GaldurSettingsDialog::updatePortList()
{
    const auto selectedPort = serialPort();
    ui->portsComboBox->clear();

    // List all serial ports
    auto allPorts = QSerialPortInfo::availablePorts();
    for (auto &port : allPorts) {
        ui->portsComboBox->addItem(
            QString("%1 (%2)").arg(port.portName()).arg(port.description()), port.systemLocation());
    }

    setSerialPort(selectedPort);
}

QString GaldurSettingsDialog::serialPort() const
{
    return ui->portsComboBox->currentData().toString();
}

void GaldurSettingsDialog::setSerialPort(const QString &port)
{
    // select the right port
    for (int i = 0; i < ui->portsComboBox->count(); i++) {
        if (ui->portsComboBox->itemData(i).toString() == port) {
            ui->portsComboBox->setCurrentIndex(i);
            break;
        }
    }
}

void GaldurSettingsDialog::readRawData(const QString &data)
{
    ui->logViewBox->insertPlainText(QString(data));
    auto vbar = ui->logViewBox->verticalScrollBar();
    vbar->setValue(vbar->maximum());
}

void GaldurSettingsDialog::setRunning(bool running)
{
    ui->generalBox->setEnabled(!running);
    ui->stackedWidget->setEnabled(!running);
    ui->generalWidget->setEnabled(!running);
}

bool GaldurSettingsDialog::startImmediately() const
{
    return ui->cbStartImmediately->isChecked();
}

void GaldurSettingsDialog::setStartImmediately(bool start)
{
    ui->cbStartImmediately->setChecked(start);
}

void GaldurSettingsDialog::on_stimTypeComboBox_currentIndexChanged(int index)
{
    ui->randomIntervalCheckBox->setEnabled(true);
    ui->randomIntervalLabel->setEnabled(true);
    switch (index) {
    case 0:
        m_client->setMode(LabrstimClient::ModeSwr);
        ui->randomIntervalCheckBox->setEnabled(false);
        ui->randomIntervalLabel->setEnabled(false);
        break;
    case 1:
        m_client->setMode(LabrstimClient::ModeTheta);
        break;
    case 2:
        m_client->setMode(LabrstimClient::ModeTrain);
        break;
    default:
        qWarning() << "Unknown mode selected!";
        m_client->setMode(LabrstimClient::ModeUnknown);
    }
}

void GaldurSettingsDialog::on_trialDurationTimeEdit_timeChanged(const QTime &time)
{
    m_client->setTrialDuration(QTime(0, 0).secsTo(time));
}

void GaldurSettingsDialog::on_pulseDurationSpinBox_valueChanged(double arg1)
{
    m_client->setPulseDuration(arg1);
}

void GaldurSettingsDialog::on_laserIntensitySpinBox_valueChanged(double arg1)
{
    m_client->setLaserIntensity(arg1);
}

void GaldurSettingsDialog::on_randomIntervalCheckBox_toggled(bool checked)
{
    m_client->setRandomIntervals(checked);
}

void GaldurSettingsDialog::on_minimumIntervalSpinBox_valueChanged(double arg1)
{
    m_client->setMinimumInterval(arg1);
    if (ui->maximumIntervalSpinBox->value() <= arg1) {
        ui->maximumIntervalSpinBox->setValue(arg1 + 1);
    }
}

void GaldurSettingsDialog::on_maximumIntervalSpinBox_valueChanged(double arg1)
{
    m_client->setMaximumInterval(arg1);
    if (ui->minimumIntervalSpinBox->value() >= arg1) {
        ui->minimumIntervalSpinBox->setValue(arg1 - 1);
    }
}

void GaldurSettingsDialog::on_swrRefractoryTimeSpinBox_valueChanged(double arg1)
{
    m_client->setSwrRefractoryTime(arg1);
}

void GaldurSettingsDialog::on_swrPowerThresholdDoubleSpinBox_valueChanged(double arg1)
{
    m_client->setSwrPowerThreshold(arg1);
}

void GaldurSettingsDialog::on_convolutionPeakThresholdSpinBox_valueChanged(double arg1)
{
    m_client->setConvolutionPeakThreshold(arg1);
}

void GaldurSettingsDialog::on_thetaPhaseSpinBox_valueChanged(double arg1)
{
    m_client->setThetaPhase(arg1);
}

void GaldurSettingsDialog::on_trainFrequencySpinBox_valueChanged(double arg1)
{
    m_client->setTrainFrequency(arg1);
}

void GaldurSettingsDialog::on_samplingRateSpinBox_valueChanged(int arg1)
{
    m_client->setSamplingFrequency(arg1);
}

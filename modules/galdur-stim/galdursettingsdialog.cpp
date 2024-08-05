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

GaldurSettingsDialog::GaldurSettingsDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::GaldurSettingsDialog),
      m_currentMode(LabrstimClient::ModeUnknown)
{
    ui->setupUi(this);

    // be safe on what is selected
    ui->stackedWidget->setCurrentIndex(0);
    ui->stimTypeComboBox->setCurrentIndex(0);

    // collapse the log view
    QList<int> list;
    list.append(1);
    list.append(0);
    ui->splitter->setSizes(list);

    // set default values
    setMode(LabrstimClient::ModeSwr);
    ui->samplingRateSpinBox->setValue(20000);
    ui->pulseDurationSpinBox->setValue(20);
    ui->laserIntensitySpinBox->setValue(2);
    ui->minimumIntervalSpinBox->setValue(10);
    ui->maximumIntervalSpinBox->setValue(20);

    ui->swrPowerThresholdDoubleSpinBox->setValue(3);
    ui->thetaPhaseSpinBox->setValue(90);
    ui->trainFrequencySpinBox->setValue(6);

    updatePortList();
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

void GaldurSettingsDialog::addRawData(const QString &data)
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

LabrstimClient::Mode GaldurSettingsDialog::mode() const
{
    return m_currentMode;
}

void GaldurSettingsDialog::setMode(LabrstimClient::Mode mode)
{
    m_currentMode = mode;
    switch (mode) {
    case LabrstimClient::ModeSwr:
        ui->stimTypeComboBox->setCurrentIndex(0);
        break;
    case LabrstimClient::ModeTheta:
        ui->stimTypeComboBox->setCurrentIndex(1);
        break;
    case LabrstimClient::ModeTrain:
        ui->stimTypeComboBox->setCurrentIndex(2);
        break;
    default:
        qWarning() << "Unknown mode selected!";
    }
}

double GaldurSettingsDialog::pulseDuration() const
{
    return ui->pulseDurationSpinBox->value();
}

void GaldurSettingsDialog::setPulseDuration(double val)
{
    ui->pulseDurationSpinBox->setValue(val);
}

double GaldurSettingsDialog::laserIntensity() const
{
    return ui->laserIntensitySpinBox->value();
}

void GaldurSettingsDialog::setLaserIntensity(double val)
{
    ui->laserIntensitySpinBox->setValue(val);
}

int GaldurSettingsDialog::samplingFrequency() const
{
    return ui->samplingRateSpinBox->value();
}

void GaldurSettingsDialog::setSamplingFrequency(int hz)
{
    ui->samplingRateSpinBox->setValue(hz);
}

bool GaldurSettingsDialog::randomIntervals() const
{
    return ui->randomIntervalCheckBox->isChecked();
}

void GaldurSettingsDialog::setRandomIntervals(bool random)
{
    ui->randomIntervalCheckBox->setChecked(random);
}

double GaldurSettingsDialog::minimumInterval() const
{
    return ui->minimumIntervalSpinBox->value();
}

void GaldurSettingsDialog::setMinimumInterval(double min)
{
    ui->minimumIntervalSpinBox->setValue(min);
}

double GaldurSettingsDialog::maximumInterval() const
{
    return ui->maximumIntervalSpinBox->value();
}

void GaldurSettingsDialog::setMaximumInterval(double max)
{
    ui->maximumIntervalSpinBox->setValue(max);
}

double GaldurSettingsDialog::swrRefractoryTime() const
{
    return ui->swrRefractoryTimeSpinBox->value();
}

void GaldurSettingsDialog::setSwrRefractoryTime(double val)
{
    ui->swrRefractoryTimeSpinBox->setValue(val);
}

double GaldurSettingsDialog::swrPowerThreshold() const
{
    return ui->swrPowerThresholdDoubleSpinBox->value();
}

void GaldurSettingsDialog::setSwrPowerThreshold(double val)
{
    ui->swrPowerThresholdDoubleSpinBox->setValue(val);
}

double GaldurSettingsDialog::convolutionPeakThreshold() const
{
    return ui->convolutionPeakThresholdSpinBox->value();
}

void GaldurSettingsDialog::setConvolutionPeakThreshold(double val)
{
    ui->convolutionPeakThresholdSpinBox->setValue(val);
}

double GaldurSettingsDialog::thetaPhase() const
{
    return ui->thetaPhaseSpinBox->value();
}

void GaldurSettingsDialog::setThetaPhase(double val)
{
    ui->thetaPhaseSpinBox->setValue(val);
}

double GaldurSettingsDialog::trainFrequency() const
{
    return ui->trainFrequencySpinBox->value();
}

void GaldurSettingsDialog::setTrainFrequency(double val)
{
    ui->trainFrequencySpinBox->setValue(val);
}

void GaldurSettingsDialog::on_stimTypeComboBox_currentIndexChanged(int index)
{
    ui->randomIntervalCheckBox->setEnabled(true);
    ui->randomIntervalLabel->setEnabled(true);
    switch (index) {
    case 0:
        m_currentMode = LabrstimClient::ModeSwr;
        ui->randomIntervalCheckBox->setEnabled(false);
        ui->randomIntervalLabel->setEnabled(false);
        break;
    case 1:
        m_currentMode = LabrstimClient::ModeTheta;
        break;
    case 2:
        m_currentMode = LabrstimClient::ModeTrain;
        break;
    default:
        qWarning() << "Unknown mode selected!";
        m_currentMode = LabrstimClient::ModeUnknown;
    }
}

void GaldurSettingsDialog::on_minimumIntervalSpinBox_valueChanged(double arg1)
{
    if (ui->maximumIntervalSpinBox->value() <= arg1) {
        ui->maximumIntervalSpinBox->setValue(arg1 + 1);
    }
}

void GaldurSettingsDialog::on_maximumIntervalSpinBox_valueChanged(double arg1)
{
    if (ui->minimumIntervalSpinBox->value() >= arg1) {
        ui->minimumIntervalSpinBox->setValue(arg1 - 1);
    }
}

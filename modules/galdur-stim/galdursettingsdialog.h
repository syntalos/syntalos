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

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QtSerialPort/QSerialPort>

#include "labrstimclient.h"

namespace Ui
{
class GaldurSettingsDialog;
}
class QLabel;

class GaldurSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GaldurSettingsDialog(QWidget *parent = nullptr);
    ~GaldurSettingsDialog();

    void updatePortList();
    QString serialPort() const;
    void setSerialPort(const QString &port);

    void setRunning(bool running);
    void addRawData(const QString &data);

    bool startImmediately() const;
    void setStartImmediately(bool start);

    LabrstimClient::Mode mode() const;
    void setMode(LabrstimClient::Mode mode);

    double pulseDuration() const;
    void setPulseDuration(double val);

    double laserIntensity() const;
    void setLaserIntensity(double val);

    int samplingFrequency() const;
    void setSamplingFrequency(int hz);

    bool randomIntervals() const;
    void setRandomIntervals(bool random);

    double minimumInterval() const;
    void setMinimumInterval(double min);
    double maximumInterval() const;
    void setMaximumInterval(double max);

    double swrRefractoryTime() const;
    void setSwrRefractoryTime(double val);

    double swrPowerThreshold() const;
    void setSwrPowerThreshold(double val);

    double convolutionPeakThreshold() const;
    void setConvolutionPeakThreshold(double val);

    double thetaPhase() const;
    void setThetaPhase(double val);

    double trainFrequency() const;
    void setTrainFrequency(double val);

private slots:
    void on_stimTypeComboBox_currentIndexChanged(int index);
    void on_minimumIntervalSpinBox_valueChanged(double arg1);
    void on_maximumIntervalSpinBox_valueChanged(double arg1);

private:
    Ui::GaldurSettingsDialog *ui;

    LabrstimClient::Mode m_currentMode;
};

#endif // SETTINGSDIALOG_H

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

#include <QtCore/QtGlobal>

#include <QDialog>
#include <QtSerialPort/QSerialPort>

namespace Ui
{
class GaldurSettingsDialog;
}
class QLabel;
class GaldurSettingsDialog;
class LabrstimClient;

class GaldurSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GaldurSettingsDialog(LabrstimClient *client, QWidget *parent = nullptr);
    ~GaldurSettingsDialog();

    void updatePortList();
    QString serialPort() const;
    void setSerialPort(const QString &port);

    void setRunning(bool running);

    bool startImmediately() const;
    void setStartImmediately(bool start);

private slots:
    void readRawData(const QString &data);

    void on_stimTypeComboBox_currentIndexChanged(int index);
    void on_trialDurationTimeEdit_timeChanged(const QTime &time);
    void on_pulseDurationSpinBox_valueChanged(double arg1);
    void on_laserIntensitySpinBox_valueChanged(double arg1);
    void on_samplingRateSpinBox_valueChanged(int arg1);

    void on_randomIntervalCheckBox_toggled(bool checked);
    void on_minimumIntervalSpinBox_valueChanged(double arg1);
    void on_maximumIntervalSpinBox_valueChanged(double arg1);
    void on_swrRefractoryTimeSpinBox_valueChanged(double arg1);
    void on_swrPowerThresholdDoubleSpinBox_valueChanged(double arg1);
    void on_convolutionPeakThresholdSpinBox_valueChanged(double arg1);
    void on_thetaPhaseSpinBox_valueChanged(double arg1);
    void on_trainFrequencySpinBox_valueChanged(double arg1);

private:
    Ui::GaldurSettingsDialog *ui;
    LabrstimClient *m_client;
};

#endif // SETTINGSDIALOG_H

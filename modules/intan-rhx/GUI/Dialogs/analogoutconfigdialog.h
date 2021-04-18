//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.0.3
//
//  Copyright (c) 2020-2021 Intan Technologies
//
//  This file is part of the Intan Technologies RHX Data Acquisition Software.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <http://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

#ifndef ANALOGOUTCONFIGDIALOG_H
#define ANALOGOUTCONFIGDIALOG_H

#include <QDialog>
#include "controllerinterface.h"
#include "systemstate.h"

class QLabel;
class QPushButton;
class QSpinBox;
class QCheckBox;
class QDialogButtonBox;

class AnalogOutConfigDialog : public QDialog
{
    Q_OBJECT
public:
    AnalogOutConfigDialog(SystemState* state_, ControllerInterface* controllerInterface_, QWidget* parent = nullptr);
    void updateFromState(bool oneAmpSelected);

private slots:
    void dac1LockToSelected(bool enable);
    void dac1SetChannel();
    void dac2SetChannel();
    void dac3SetChannel();
    void dac4SetChannel();
    void dac5SetChannel();
    void dac6SetChannel();
    void dac7SetChannel();
    void dac8SetChannel();
    void dacSetRefChannel();
    void dac1Disable();
    void dac2Disable();
    void dac3Disable();
    void dac4Disable();
    void dac5Disable();
    void dac6Disable();
    void dac7Disable();
    void dac8Disable();
    void dacRefDisable();
    void setDac1Threshold(int threshold);
    void setDac2Threshold(int threshold);
    void setDac3Threshold(int threshold);
    void setDac4Threshold(int threshold);
    void setDac5Threshold(int threshold);
    void setDac6Threshold(int threshold);
    void setDac7Threshold(int threshold);
    void setDac8Threshold(int threshold);
    void enableDac1Threshold(bool enable);
    void enableDac2Threshold(bool enable);
    void enableDac3Threshold(bool enable);
    void enableDac4Threshold(bool enable);
    void enableDac5Threshold(bool enable);
    void enableDac6Threshold(bool enable);
    void enableDac7Threshold(bool enable);
    void enableDac8Threshold(bool enable);

private:
    SystemState* state;
    ControllerInterface* controllerInterface;
    bool eightAnalogOuts;

    QLabel* dac1Label;
    QLabel* dac2Label;
    QLabel* dac3Label;
    QLabel* dac4Label;
    QLabel* dac5Label;
    QLabel* dac6Label;
    QLabel* dac7Label;
    QLabel* dac8Label;
    QLabel* dacRefLabel;

    QPushButton* dac1SetButton;
    QPushButton* dac2SetButton;
    QPushButton* dac3SetButton;
    QPushButton* dac4SetButton;
    QPushButton* dac5SetButton;
    QPushButton* dac6SetButton;
    QPushButton* dac7SetButton;
    QPushButton* dac8SetButton;

    QPushButton* dac1DisableButton;
    QPushButton* dac2DisableButton;
    QPushButton* dac3DisableButton;
    QPushButton* dac4DisableButton;
    QPushButton* dac5DisableButton;
    QPushButton* dac6DisableButton;
    QPushButton* dac7DisableButton;
    QPushButton* dac8DisableButton;

    QPushButton* dacRefSetButton;
    QPushButton* dacRefDisableButton;

    QSpinBox *dac1ThresholdSpinBox;
    QSpinBox *dac2ThresholdSpinBox;
    QSpinBox *dac3ThresholdSpinBox;
    QSpinBox *dac4ThresholdSpinBox;
    QSpinBox *dac5ThresholdSpinBox;
    QSpinBox *dac6ThresholdSpinBox;
    QSpinBox *dac7ThresholdSpinBox;
    QSpinBox *dac8ThresholdSpinBox;
    QSpinBox *displayMarkerSpinBox;

    QCheckBox *dacLockToSelectedCheckBox;
    QCheckBox *dac1ThresholdEnableCheckBox;
    QCheckBox *dac2ThresholdEnableCheckBox;
    QCheckBox *dac3ThresholdEnableCheckBox;
    QCheckBox *dac4ThresholdEnableCheckBox;
    QCheckBox *dac5ThresholdEnableCheckBox;
    QCheckBox *dac6ThresholdEnableCheckBox;
    QCheckBox *dac7ThresholdEnableCheckBox;
    QCheckBox *dac8ThresholdEnableCheckBox;

    QDialogButtonBox* buttonBox;
};

#endif // ANALOGOUTCONFIGDIALOG_H

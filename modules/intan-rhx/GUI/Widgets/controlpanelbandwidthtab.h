//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.4.0
//
//  Copyright (c) 2020-2025 Intan Technologies
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

#ifndef CONTROLPANELBANDWIDTHTAB_H
#define CONTROLPANELBANDWIDTHTAB_H

#include <QtWidgets>
#include "controllerinterface.h"
#include "systemstate.h"
#include "viewfilterswindow.h"

class ControlPanelBandwidthTab : public QWidget
{
    Q_OBJECT
public:
    explicit ControlPanelBandwidthTab(ControllerInterface* controllerInterface_, SystemState* state_, QWidget *parent = nullptr);
    ~ControlPanelBandwidthTab();

    void updateFromState();

private slots:
    void simpleBandwidthDialog();
    void advancedBandwidthDialog();
    void viewFiltersSlot();
    void changeNotchFilter(int filterIndex) { state->notchFreq->setIndex(filterIndex); }
    void changeLowType(int lowType) { state->lowType->setIndex(lowType); }
    void changeHighType(int highType) { state->highType->setIndex(highType); }
    void changeLowOrder(int lowOrder) { state->lowOrder->setValueWithLimits(lowOrder); }
    void changeHighOrder(int highOrder) { state->highOrder->setValueWithLimits(highOrder); }
    void changeLowCutoff() { state->lowSWCutoffFreq->setValueWithLimits(lpCutoffLineEdit->text().toDouble()); }
    void changeHighCutoff() { state->highSWCutoffFreq->setValueWithLimits(hpCutoffLineEdit->text().toDouble()); }

private:
    SystemState* state;
    ControllerInterface* controllerInterface;

    ViewFiltersWindow *viewFiltersWindow;

    QLabel *bandwidthLabel;

    QPushButton *changeBandwidthButton;
    QPushButton *advancedBandwidthButton;

    QComboBox *notchFilterComboBox;

    QComboBox *lpTypeComboBox;
    QLabel *lpOrderLabel;
    QSpinBox *lpOrderSpinBox;
    QLineEdit *lpCutoffLineEdit;

    QComboBox *hpTypeComboBox;
    QLabel *hpOrderLabel;
    QSpinBox *hpOrderSpinBox;
    QLineEdit *hpCutoffLineEdit;
    QPushButton *viewFiltersButton;

    static double lower3dBPoint(double hpf1Cutoff, double hpf2Cutoff, bool hpf2Enabled);
    static double secondPoleLocation(double target3dBPoint, double hpf1Cutoff);
};

#endif // CONTROLPANELBANDWIDTHTAB_H

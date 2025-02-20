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

#ifndef STIMPARAMDIALOG_H
#define STIMPARAMDIALOG_H

#include <QDialog>
#include "signalsources.h"
#include "systemstate.h"
#include "stimfigure.h"
#include "smartspinbox.h"

class QDialogButtonBox;
class QWidget;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QLabel;
class QCheckBox;
class CurrentSpinBox;
class StimFigure;

class StimParamDialog : public QDialog
{
    Q_OBJECT
public:
    StimParamDialog(SystemState* state_, Channel* channel_, QWidget *parent = nullptr);

    void updateParametersFromState(StimParameters* parameters);
    void activate();

public slots:
    void accept() override;

private slots:
    void notifyFocusChanged(QWidget* lostFocus, QWidget* gainedFocus);
    void updateFromState();

private:
    SystemState* state;
    Channel* channel;

    QDialogButtonBox *buttonBox;
    StimFigure *stimFigure;

    StimParameters *parameters;

    QComboBox *stimShapeComboBox;
    QLabel *stimShapeLabel;
    QComboBox *stimPolarityComboBox;
    QLabel *stimPolarityLabel;
    TimeSpinBox *firstPhaseDurationSpinBox;
    QLabel *firstPhaseDurationLabel;
    TimeSpinBox *secondPhaseDurationSpinBox;
    QLabel *secondPhaseDurationLabel;
    TimeSpinBox *interphaseDelaySpinBox;
    QLabel *interphaseDelayLabel;
    CurrentSpinBox *firstPhaseAmplitudeSpinBox;
    QLabel *firstPhaseAmplitudeLabel;
    CurrentSpinBox *secondPhaseAmplitudeSpinBox;
    QLabel *secondPhaseAmplitudeLabel;
    QLabel *totalPosChargeLabel;
    QLabel *totalNegChargeLabel;
    QLabel *chargeBalanceLabel;

    QCheckBox *enableStimCheckBox;
    QComboBox *triggerSourceComboBox;
    QLabel *triggerSourceLabel;
    QComboBox *triggerEdgeOrLevelComboBox;
    QLabel *triggerEdgeOrLevelLabel;
    QComboBox *triggerHighOrLowComboBox;
    QLabel *triggerHighOrLowLabel;
    TimeSpinBox *postTriggerDelaySpinBox;
    QLabel *postTriggerDelayLabel;

    QComboBox *pulseOrTrainComboBox;
    QLabel *pulseOrTrainLabel;
    QSpinBox *numberOfStimPulsesSpinBox;
    QLabel *numberOfStimPulsesLabel;
    TimeSpinBox *pulseTrainPeriodSpinBox;
    QLabel *pulseTrainPeriodLabel;
    QLabel *pulseTrainFrequencyLabel;
    TimeSpinBox *refractoryPeriodSpinBox;
    QLabel *refractoryPeriodLabel;

    TimeSpinBox *preStimAmpSettleSpinBox;
    QLabel *preStimAmpSettleLabel;
    TimeSpinBox *postStimAmpSettleSpinBox;
    QLabel *postStimAmpSettleLabel;
    QCheckBox *maintainAmpSettleCheckBox;
    QCheckBox *enableAmpSettleCheckBox;

    TimeSpinBox *postStimChargeRecovOnSpinBox;
    QLabel *postStimChargeRecovOnLabel;
    TimeSpinBox *postStimChargeRecovOffSpinBox;
    QLabel *postStimChargeRecovOffLabel;
    QCheckBox *enableChargeRecoveryCheckBox;

    double timestep;
    double currentstep;

private slots:
    void enableWidgets();
    void calculateCharge();
    void calculatePulseTrainFrequency();
    void constrainPostTriggerDelay();
    void constrainPostStimChargeRecovery();
    void constrainRefractoryPeriod();
    void constrainPulseTrainPeriod();
    void roundTimeInputs();
    void roundCurrentInputs();

signals:
    // Signal that is emitted when the DoubleSpinBoxes that control charge have been changed
    void chargeChanged();

    // Signal that is emitted when the DoubleSpinBoxes or ComboBox that control the minimum pulse train period have been changed
    void minimumPeriodChanged();

    // Signals that are emitted when various widgets that control the Stimulation Parameters are selected
    void highlightFirstPhaseDuration(bool highlight);
    void highlightSecondPhaseDuration(bool highlight);
    void highlightInterphaseDelay(bool highlight);
    void highlightFirstPhaseAmplitude(bool highlight);
    void highlightSecondPhaseAmplitude(bool highlight);
    void highlightPostTriggerDelay(bool highlight);
    void highlightPulseTrainPeriod(bool highlight);
    void highlightRefractoryPeriod(bool highlight);
    void highlightPreStimAmpSettle(bool highlight);
    void highlightPostStimAmpSettle(bool highlight);
    void highlightPostStimChargeRecovOn(bool highlight);
    void highlightPostStimChargeRecovOff(bool highlight);
};

#endif // STIMPARAMDIALOG_H

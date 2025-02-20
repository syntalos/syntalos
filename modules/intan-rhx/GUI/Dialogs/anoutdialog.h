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

#ifndef ANOUTDIALOG_H
#define ANOUTDIALOG_H

#include <QDialog>
#include "systemstate.h"
#include "stimparameters.h"

class QDialogButtonBox;
class QWidget;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QLabel;
class QCheckBox;
class StimParameters;
class TimeSpinBox;
class VoltageSpinBox;
class AnOutFigure;

class AnOutDialog : public QDialog
{
    Q_OBJECT
public:
    AnOutDialog(SystemState* state_, Channel* channel_, QWidget *parent = nullptr);
    void loadParameters(StimParameters* parameters);

public slots:
    void accept() override;
    void notifyFocusChanged(QWidget *lostFocus, QWidget *gainedFocus);

private:
    SystemState* state;
    Channel* channel;

    QDialogButtonBox *buttonBox;

    AnOutFigure *anOutFigure;

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
    VoltageSpinBox *firstPhaseAmplitudeSpinBox;
    QLabel *firstPhaseAmplitudeLabel;
    VoltageSpinBox *secondPhaseAmplitudeSpinBox;
    QLabel *secondPhaseAmplitudeLabel;
    VoltageSpinBox *baselineVoltageSpinBox;
    QLabel *baselineVoltageLabel;

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

    double timestep, currentstep;

private slots:
    void enableWidgets();
    void calculatePulseTrainFrequency();
    void constrainPulseTrainPeriod();
    void roundTimeInputs();
    void roundVoltageInputs();
    void updateMonophasicAndPositive();

signals:
    // Signal that is emitted when the DoubleSpinBoxes or ComboBox that control the minimum pulse train period have been changed
    void minimumPeriodChanged();

    // Signals that are emitted when various widgets that control the Stimulation Parameters are selected
    void monophasicAndPositiveSignalChanged(bool logicValue);
    void highlightBaselineVoltage(bool highlight);
    void highlightFirstPhaseDuration(bool highlight);
    void highlightSecondPhaseDuration(bool highlight);
    void highlightInterphaseDelay(bool highlight);
    void highlightFirstPhaseAmplitude(bool highlight);
    void highlightSecondPhaseAmplitude(bool highlight);
    void highlightPostTriggerDelay(bool highlight);
    void highlightPulseTrainPeriod(bool highlight);
    void highlightRefractoryPeriod(bool highlight);
};

#endif // ANOUTDIALOG_H

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

#ifndef DIGOUTDIALOG_H
#define DIGOUTDIALOG_H

#include <QDialog>
#include "systemstate.h"
#include "stimparameters.h"
#include "rhxglobals.h"

class QDialogButtonBox;
class QWidget;
class QComboBox;
class QSpinBox;
class QLabel;
class QCheckBox;
class StimParameters;
class TimeSpinBox;
class DigFigure;

class DigOutDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DigOutDialog(SystemState* state_, Channel* channel_, QWidget *parent = nullptr);
    void loadParameters(StimParameters* parameters);

public slots:
    void accept();
    void notifyFocusChanged(QWidget *lostFocus, QWidget *gainedFocus);

private:
    SystemState* state;
    Channel* channel;

    QDialogButtonBox *buttonBox;

    DigFigure *digFigure;

    StimParameters *parameters;

    QCheckBox *enablePulseCheckBox;
    QComboBox *triggerSourceComboBox;
    QLabel *triggerSourceLabel;
    QComboBox *triggerEdgeOrLevelComboBox;
    QLabel *triggerEdgeOrLevelLabel;
    QComboBox *triggerHighOrLowComboBox;
    QLabel *triggerHighOrLowLabel;
    TimeSpinBox *postTriggerDelaySpinBox;
    QLabel *postTriggerDelayLabel;

    TimeSpinBox *pulseDurationSpinBox;
    QLabel *pulseDurationLabel;
    QComboBox *pulseRepetitionComboBox;
    QLabel *pulseRepetitionLabel;
    QSpinBox *numPulsesSpinBox;
    QLabel *numPulsesLabel;
    TimeSpinBox *pulseTrainPeriodSpinBox;
    QLabel *pulseTrainPeriodLabel;
    QLabel *pulseTrainFrequencyLabel;
    TimeSpinBox *refractoryPeriodSpinBox;
    QLabel *refractoryPeriodLabel;

    double timestep;

private slots:
    void enableWidgets();
    void roundTimeInputs();
    void calculatePulseTrainFrequency();
    void constrainPulseTrainPeriod();

signals:
    void highlightFirstPhaseDuration(bool highlight);
    void highlightPostTriggerDelay(bool highlight);
    void highlightPulseTrainPeriod(bool highlight);
    void highlightRefractoryPeriod(bool highlight);

};

#endif // DIGOUTDIALOG_H

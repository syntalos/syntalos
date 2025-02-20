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

#include <QtWidgets>
#include "rhxregisters.h"
#include "stimparamdialog.h"

StimParamDialog::StimParamDialog(SystemState* state_, Channel* channel_, QWidget *parent) :
    QDialog(parent),
    state(state_),
    channel(channel_)
{
    parameters = channel->stimParameters;
    timestep = 1.0e6 / state->sampleRate->getNumericValue();  // time step in microseconds
    currentstep = RHXRegisters::stimStepSizeToDouble(state->getStimStepSizeEnum()) * 1.0e6;  // current step in microamps

    stimFigure = new StimFigure(parameters, this);

    QGroupBox* stimWaveFormGroupBox = new QGroupBox(tr("Stimulation Waveform"), this);

    stimShapeLabel = new QLabel(tr("Stimulation Shape:"), this);
    stimShapeComboBox = new QComboBox(this);
    QStringList stimShapes;
    stimShapes << tr("Biphasic") << tr("Biphasic with Delay") << tr("Triphasic");
    stimShapeComboBox->addItems(stimShapes);

    stimPolarityLabel = new QLabel(tr("Stimulation Polarity:"), this);
    stimPolarityComboBox = new QComboBox(this);
    QStringList stimPolarities;
    stimPolarities << tr("Cathodic Current First") << tr("Anodic Current First");
    stimPolarityComboBox->addItems(stimPolarities);

    firstPhaseDurationLabel = new QLabel(tr("First Phase Duration (D1):"), this);
    firstPhaseDurationSpinBox = new TimeSpinBox(timestep, this);
    firstPhaseDurationSpinBox->setRange(0, 5000);
    connect(qApp, SIGNAL(focusChanged(QWidget*,QWidget*)), this, SLOT(notifyFocusChanged(QWidget*,QWidget*)));

    secondPhaseDurationLabel = new QLabel(tr("Second Phase Duration (D2):"), this);
    secondPhaseDurationSpinBox = new TimeSpinBox(timestep, this);
    secondPhaseDurationSpinBox->setRange(0, 5000);

    interphaseDelayLabel = new QLabel(tr("Interphase Delay (DP):"), this);
    interphaseDelaySpinBox = new TimeSpinBox(timestep, this);
    interphaseDelaySpinBox->setRange(0, 5000);

    firstPhaseAmplitudeLabel = new QLabel(tr("First Phase Amplitude (A1):"), this);
    firstPhaseAmplitudeSpinBox = new CurrentSpinBox(currentstep, this);
    firstPhaseAmplitudeSpinBox->setRange(0, 255 * currentstep);

    secondPhaseAmplitudeLabel = new QLabel(tr("Second Phase Amplitude (A2):"), this);
    secondPhaseAmplitudeSpinBox = new CurrentSpinBox(currentstep, this);
    secondPhaseAmplitudeSpinBox->setRange(0, 255 * currentstep);

    totalPosChargeLabel = new QLabel(this);
    totalNegChargeLabel = new QLabel(this);
    chargeBalanceLabel = new QLabel(tr("Charge Balance Placeholder"), this);
    chargeBalanceLabel->setAlignment(Qt::AlignRight);

    // Create trigger information widgets.
    QGroupBox* triggerGroupBox = new QGroupBox(tr("Trigger"), this);

    enableStimCheckBox = new QCheckBox(tr("Enable"), this);

    triggerSourceLabel = new QLabel(tr("Trigger Source:"), this);
    triggerSourceComboBox = new QComboBox(this);
    QStringList triggerSources;
    triggerSources << "DIGITAL IN 1" << "DIGITAL IN 2" << "DIGITAL IN 3" << "DIGITAL IN 4" << "DIGITAL IN 5"
                   << "DIGITAL IN 6" << "DIGITAL IN 7" << "DIGITAL IN 8" << "DIGITAL IN 9" << "DIGITAL IN 10"
                   << "DIGITAL IN 11" << "DIGITAL IN 12" << "DIGITAL IN 13" << "DIGITAL IN 14" << "DIGITAL IN 15"
                   << "DIGITAL IN 16" << "ANALOG IN 1" << "ANALOG IN 2" << "ANALOG IN 3" << "ANALOG IN 4"
                   << "ANALOG IN 5" << "ANALOG IN 6" << "ANALOG IN 7" << "ANALOG IN 8" << "KEYPRESS: F1"
                   << "KEYPRESS: F2" << "KEYPRESS: F3" << "KEYPRESS: F4" << "KEYPRESS: F5" << "KEYPRESS: F6"
                   << "KEYPRESS: F7" << "KEYPRESS: F8";
    triggerSourceComboBox->addItems(triggerSources);

    triggerEdgeOrLevelLabel = new QLabel(tr(" "), this);
    triggerEdgeOrLevelComboBox = new QComboBox(this);
    QStringList triggerEdgeOrLevels;
    triggerEdgeOrLevels << tr("Edge Triggered") << tr("Level Triggered");
    triggerEdgeOrLevelComboBox->addItems(triggerEdgeOrLevels);

    triggerHighOrLowLabel = new QLabel(tr(" "), this);
    triggerHighOrLowComboBox = new QComboBox(this);
    QStringList triggerHighOrLows;
    triggerHighOrLows << tr("Trigger on High") << tr("Trigger on Low");
    triggerHighOrLowComboBox->addItems(triggerHighOrLows);

    postTriggerDelayLabel = new QLabel(tr("Post Trigger Delay:"), this);
    postTriggerDelaySpinBox = new TimeSpinBox(timestep, this);
    postTriggerDelaySpinBox->setRange(0, 500000);

    // Create pulse train information widgets.
    QGroupBox* pulseTrainGroupBox = new QGroupBox(tr("Pulse Train"), this);

    pulseOrTrainLabel = new QLabel(tr("Pulse Repetition:"), this);
    pulseOrTrainComboBox = new QComboBox(this);
    QStringList pulseOrTrains;
    pulseOrTrains << tr("Single Stim Pulse") << tr("Stim Pulse Train");
    pulseOrTrainComboBox->addItems(pulseOrTrains);

    numberOfStimPulsesLabel = new QLabel(tr("Number of Stim Pulses"), this);
    numberOfStimPulsesSpinBox = new QSpinBox(this);
    numberOfStimPulsesSpinBox->setMaximumWidth(numberOfStimPulsesSpinBox->fontMetrics().horizontalAdvance("99999  "));
    numberOfStimPulsesSpinBox->setRange(2, 256);

    pulseTrainPeriodLabel = new QLabel(tr("Pulse Train Period:"), this);
    pulseTrainPeriodSpinBox = new TimeSpinBox(timestep, this);
    pulseTrainPeriodSpinBox->setRange(0, 1000000);

    pulseTrainFrequencyLabel = new QLabel(this);

    refractoryPeriodLabel = new QLabel(tr("Post-Stim Refractory Period:"), this);
    refractoryPeriodSpinBox = new TimeSpinBox(timestep, this);
    refractoryPeriodSpinBox->setRange(0, 1000000);

    // Create Amp Settle widgets.
    QGroupBox* ampSettleGroupBox = new QGroupBox(tr("Amp Settle"), this);

    preStimAmpSettleLabel = new QLabel(tr("Pre Stim Amp Settle:"), this);
    preStimAmpSettleSpinBox = new TimeSpinBox(timestep, this);
    preStimAmpSettleSpinBox->setRange(0, 500000);

    postStimAmpSettleLabel = new QLabel(tr("Post Stim Amp Settle:"), this);
    postStimAmpSettleSpinBox = new TimeSpinBox(timestep, this);
    postStimAmpSettleSpinBox->setRange(0, 500000);

    maintainAmpSettleCheckBox = new QCheckBox(tr("Maintain amp settle during pulse train"), this);

    enableAmpSettleCheckBox = new QCheckBox(tr("Enable Amp Settle"), this);

    // Create Charge Recovery widgets.
    QGroupBox* chargeRecoveryGroupBox = new QGroupBox(tr("Charge Recovery"), this);

    postStimChargeRecovOnLabel = new QLabel(tr("Post Stim Charge Recovery On:"), this);
    postStimChargeRecovOnSpinBox = new TimeSpinBox(timestep, this);
    postStimChargeRecovOnSpinBox->setRange(0, 1000000);

    postStimChargeRecovOffLabel = new QLabel(tr("Post Stim Charge Recovery Off:"), this);
    postStimChargeRecovOffSpinBox = new TimeSpinBox(timestep, this);
    postStimChargeRecovOffSpinBox->setRange(0, 1000000);

    enableChargeRecoveryCheckBox = new QCheckBox(tr("Enable Charge Recovery"), this);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    // Connect internal signals and slots.
    connect(enableChargeRecoveryCheckBox, SIGNAL(checkStateChanged(Qt::CheckState)), this, SLOT(enableWidgets()));
    connect(enableAmpSettleCheckBox, SIGNAL(checkStateChanged(Qt::CheckState)), this, SLOT(enableWidgets()));
    connect(enableStimCheckBox, SIGNAL(checkStateChanged(Qt::CheckState)), this, SLOT(enableWidgets()));
    connect(pulseOrTrainComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(enableWidgets()));
    connect(stimShapeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(enableWidgets()));

    connect(stimShapeComboBox, SIGNAL(currentIndexChanged(int)), this, SIGNAL(chargeChanged()));
    connect(stimPolarityComboBox, SIGNAL(currentIndexChanged(int)), this, SIGNAL(chargeChanged()));
    connect(firstPhaseDurationSpinBox, SIGNAL(valueChanged(double)), this, SIGNAL(chargeChanged()));
    connect(secondPhaseDurationSpinBox, SIGNAL(valueChanged(double)), this, SIGNAL(chargeChanged()));
    connect(firstPhaseAmplitudeSpinBox, SIGNAL(valueChanged(double)), this, SIGNAL(chargeChanged()));
    connect(secondPhaseAmplitudeSpinBox, SIGNAL(valueChanged(double)), this, SIGNAL(chargeChanged()));
    connect(this, SIGNAL(chargeChanged()), this, SLOT(calculateCharge()));
    connect(pulseTrainPeriodSpinBox, SIGNAL(valueChanged(double)), this, SLOT(calculatePulseTrainFrequency()));
    connect(preStimAmpSettleSpinBox, SIGNAL(trueValueChanged(double)), this, SLOT(constrainPostTriggerDelay()));
    connect(postStimChargeRecovOnSpinBox, SIGNAL(trueValueChanged(double)), this, SLOT(constrainPostStimChargeRecovery()));
    connect(postStimAmpSettleSpinBox, SIGNAL(trueValueChanged(double)), this, SLOT(constrainRefractoryPeriod()));
    connect(postStimChargeRecovOffSpinBox, SIGNAL(trueValueChanged(double)), this, SLOT(constrainRefractoryPeriod()));
    connect(firstPhaseDurationSpinBox, SIGNAL(editingFinished()), this, SLOT(roundTimeInputs()));
    connect(secondPhaseDurationSpinBox, SIGNAL(editingFinished()), this, SLOT(roundTimeInputs()));
    connect(interphaseDelaySpinBox, SIGNAL(editingFinished()), this, SLOT(roundTimeInputs()));
    connect(postTriggerDelaySpinBox, SIGNAL(editingFinished()), this, SLOT(roundTimeInputs()));
    connect(pulseTrainPeriodSpinBox, SIGNAL(editingFinished()), this, SLOT(roundTimeInputs()));
    connect(refractoryPeriodSpinBox, SIGNAL(editingFinished()), this, SLOT(roundTimeInputs()));
    connect(preStimAmpSettleSpinBox, SIGNAL(editingFinished()), this, SLOT(roundTimeInputs()));
    connect(postStimAmpSettleSpinBox, SIGNAL(editingFinished()), this, SLOT(roundTimeInputs()));
    connect(postStimChargeRecovOffSpinBox, SIGNAL(editingFinished()), this, SLOT(roundTimeInputs()));
    connect(postStimChargeRecovOnSpinBox, SIGNAL(editingFinished()), this, SLOT(roundTimeInputs()));
    connect(firstPhaseAmplitudeSpinBox, SIGNAL(editingFinished()), this, SLOT(roundCurrentInputs()));
    connect(secondPhaseAmplitudeSpinBox, SIGNAL(editingFinished()), this, SLOT(roundCurrentInputs()));
    connect(enableStimCheckBox, SIGNAL(toggled(bool)), stimFigure, SLOT(highlightStimTrace(bool)));

    // Connect signals to stimFigure's non-highlight slots.
    connect(stimShapeComboBox, SIGNAL(currentIndexChanged(int)), stimFigure, SLOT(updateStimShape(int)));
    connect(stimPolarityComboBox, SIGNAL(currentIndexChanged(int)), stimFigure, SLOT(updateStimPolarity(int)));
    connect(pulseOrTrainComboBox, SIGNAL(currentIndexChanged(int)), stimFigure, SLOT(updatePulseOrTrain(int)));
    connect(enableAmpSettleCheckBox, SIGNAL(toggled(bool)), stimFigure, SLOT(updateEnableAmpSettle(bool)));
    connect(maintainAmpSettleCheckBox, SIGNAL(toggled(bool)), stimFigure, SLOT(updateMaintainAmpSettle(bool)));
    connect(enableChargeRecoveryCheckBox, SIGNAL(toggled(bool)), stimFigure, SLOT(updateEnableChargeRecovery(bool)));

    // Connect signals to stimFigure's highlight slots.
    connect(this, SIGNAL(highlightFirstPhaseDuration(bool)), stimFigure, SLOT(highlightFirstPhaseDuration(bool)));
    connect(this, SIGNAL(highlightSecondPhaseDuration(bool)), stimFigure, SLOT(highlightSecondPhaseDuration(bool)));
    connect(this, SIGNAL(highlightInterphaseDelay(bool)), stimFigure, SLOT(highlightInterphaseDelay(bool)));
    connect(this, SIGNAL(highlightFirstPhaseAmplitude(bool)), stimFigure, SLOT(highlightFirstPhaseAmplitude(bool)));
    connect(this, SIGNAL(highlightSecondPhaseAmplitude(bool)), stimFigure, SLOT(highlightSecondPhaseAmplitude(bool)));
    connect(this, SIGNAL(highlightPostTriggerDelay(bool)), stimFigure, SLOT(highlightPostTriggerDelay(bool)));
    connect(this, SIGNAL(highlightPulseTrainPeriod(bool)), stimFigure, SLOT(highlightPulseTrainPeriod(bool)));
    connect(this, SIGNAL(highlightRefractoryPeriod(bool)), stimFigure, SLOT(highlightRefractoryPeriod(bool)));
    connect(this, SIGNAL(highlightPreStimAmpSettle(bool)), stimFigure, SLOT(highlightPreStimAmpSettle(bool)));
    connect(this, SIGNAL(highlightPostStimAmpSettle(bool)), stimFigure, SLOT(highlightPostStimAmpSettle(bool)));
    connect(this, SIGNAL(highlightPostStimChargeRecovOn(bool)), stimFigure, SLOT(highlightPostStimChargeRecovOn(bool)));
    connect(this, SIGNAL(highlightPostStimChargeRecovOff(bool)), stimFigure, SLOT(highlightPostStimChargeRecovOff(bool)));

    // Update widgets' states to be different than the default states, so state changes are detected properly.
    stimShapeComboBox->setCurrentIndex(Triphasic);
    stimPolarityComboBox->setCurrentIndex(PositiveFirst);
    firstPhaseDurationSpinBox->setValue(1);
    secondPhaseDurationSpinBox->setValue(1);
    interphaseDelaySpinBox->setValue(1);
    firstPhaseAmplitudeSpinBox->setValue(1);
    secondPhaseAmplitudeSpinBox->setValue(1);
    enableStimCheckBox->setChecked(true);
    triggerSourceComboBox->setCurrentIndex(AnalogIn1);
    triggerEdgeOrLevelComboBox->setCurrentIndex(TriggerLevel);
    triggerHighOrLowComboBox->setCurrentIndex(TriggerLow);
    postTriggerDelaySpinBox->setValue(2);
    pulseOrTrainComboBox->setCurrentIndex(SinglePulse);
    numberOfStimPulsesSpinBox->setValue(2);
    pulseTrainPeriodSpinBox->setValue(1);
    refractoryPeriodSpinBox->setValue(1);
    preStimAmpSettleSpinBox->setValue(1);
    postStimAmpSettleSpinBox->setValue(1);
    maintainAmpSettleCheckBox->setChecked(true);
    enableAmpSettleCheckBox->setChecked(false);
    postStimChargeRecovOffSpinBox->setValue(1);
    postStimChargeRecovOnSpinBox->setValue(1);
    enableChargeRecoveryCheckBox->setChecked(false);

    // Connect these signals only after initial values have been set.
    connect(stimShapeComboBox, SIGNAL(currentIndexChanged(int)), this, SIGNAL(minimumPeriodChanged()));
    connect(firstPhaseDurationSpinBox, SIGNAL(valueChanged(double)), this, SIGNAL(minimumPeriodChanged()));
    connect(secondPhaseDurationSpinBox, SIGNAL(valueChanged(double)), this, SIGNAL(minimumPeriodChanged()));
    connect(interphaseDelaySpinBox, SIGNAL(valueChanged(double)), this, SIGNAL(minimumPeriodChanged()));
    connect(this, SIGNAL(minimumPeriodChanged()), this, SLOT(constrainPulseTrainPeriod()));

    // Update dialog's state based on structParameters.
    updateParametersFromState(parameters);

    // Stimulation waveform widgets' layout
    QHBoxLayout *stimShapeRow = new QHBoxLayout;
    stimShapeRow->addWidget(stimShapeLabel);
    stimShapeRow->addStretch();
    stimShapeRow->addWidget(stimShapeComboBox);

    QHBoxLayout *stimPolarityRow = new QHBoxLayout;
    stimPolarityRow->addWidget(stimPolarityLabel);
    stimPolarityRow->addStretch();
    stimPolarityRow->addWidget(stimPolarityComboBox);

    QHBoxLayout *firstPhaseDurationRow = new QHBoxLayout;
    firstPhaseDurationRow->addWidget(firstPhaseDurationLabel);
    firstPhaseDurationRow->addStretch();
    firstPhaseDurationRow->addWidget(firstPhaseDurationSpinBox);

    QHBoxLayout *secondPhaseDurationRow = new QHBoxLayout;
    secondPhaseDurationRow->addWidget(secondPhaseDurationLabel);
    secondPhaseDurationRow->addStretch();
    secondPhaseDurationRow->addWidget(secondPhaseDurationSpinBox);

    QHBoxLayout *interphaseDelayRow = new QHBoxLayout;
    interphaseDelayRow->addWidget(interphaseDelayLabel);
    interphaseDelayRow->addStretch();
    interphaseDelayRow->addWidget(interphaseDelaySpinBox);

    QHBoxLayout *firstPhaseAmplitudeRow = new QHBoxLayout;
    firstPhaseAmplitudeRow->addWidget(firstPhaseAmplitudeLabel);
    firstPhaseAmplitudeRow->addStretch();
    firstPhaseAmplitudeRow->addWidget(firstPhaseAmplitudeSpinBox);

    QHBoxLayout *secondPhaseAmplitudeRow = new QHBoxLayout;
    secondPhaseAmplitudeRow->addWidget(secondPhaseAmplitudeLabel);
    secondPhaseAmplitudeRow->addStretch();
    secondPhaseAmplitudeRow->addWidget(secondPhaseAmplitudeSpinBox);

    QHBoxLayout *totalPositiveChargeRow = new QHBoxLayout;
    totalPositiveChargeRow->addWidget(totalPosChargeLabel);

    QHBoxLayout *totalNegativeChargeRow = new QHBoxLayout;
    totalNegativeChargeRow->addWidget(totalNegChargeLabel);

    QHBoxLayout *chargeBalanceRow = new QHBoxLayout;
    chargeBalanceRow->addWidget(chargeBalanceLabel);

    QVBoxLayout *stimWaveFormLayout = new QVBoxLayout;
    stimWaveFormLayout->addLayout(stimShapeRow);
    stimWaveFormLayout->addLayout(stimPolarityRow);
    stimWaveFormLayout->addLayout(firstPhaseDurationRow);
    stimWaveFormLayout->addLayout(secondPhaseDurationRow);
    stimWaveFormLayout->addLayout(interphaseDelayRow);
    stimWaveFormLayout->addLayout(firstPhaseAmplitudeRow);
    stimWaveFormLayout->addLayout(secondPhaseAmplitudeRow);
    stimWaveFormLayout->addLayout(totalPositiveChargeRow);
    stimWaveFormLayout->addLayout(totalNegativeChargeRow);
    stimWaveFormLayout->addLayout(chargeBalanceRow);
    stimWaveFormLayout->addStretch();
    stimWaveFormGroupBox->setLayout(stimWaveFormLayout);

    // Trigger information widgets' layout
    QHBoxLayout *enableStimRow = new QHBoxLayout;
    enableStimRow->addWidget(enableStimCheckBox);

    QHBoxLayout *triggerSourceRow = new QHBoxLayout;
    triggerSourceRow->addWidget(triggerSourceLabel);
    triggerSourceRow->addStretch();
    triggerSourceRow->addWidget(triggerSourceComboBox);

    QHBoxLayout *triggerEdgeOrLevelRow = new QHBoxLayout;
    triggerEdgeOrLevelRow->addWidget(triggerEdgeOrLevelLabel);
    triggerEdgeOrLevelRow->addStretch();
    triggerEdgeOrLevelRow->addWidget(triggerEdgeOrLevelComboBox);

    QHBoxLayout *triggerHighOrLowRow = new QHBoxLayout;
    triggerHighOrLowRow->addWidget(triggerHighOrLowLabel);
    triggerHighOrLowRow->addStretch();
    triggerHighOrLowRow->addWidget(triggerHighOrLowComboBox);

    QHBoxLayout *postTriggerDelayRow = new QHBoxLayout;
    postTriggerDelayRow->addWidget(postTriggerDelayLabel);
    postTriggerDelayRow->addStretch();
    postTriggerDelayRow->addWidget(postTriggerDelaySpinBox);

    QVBoxLayout *triggerLayout = new QVBoxLayout;
    triggerLayout->addLayout(enableStimRow);
    triggerLayout->addStretch();
    triggerLayout->addLayout(triggerSourceRow);
    triggerLayout->addStretch();
    triggerLayout->addLayout(triggerEdgeOrLevelRow);
    triggerLayout->addStretch();
    triggerLayout->addLayout(triggerHighOrLowRow);
    triggerLayout->addStretch();
    triggerLayout->addLayout(postTriggerDelayRow);
    triggerGroupBox->setLayout(triggerLayout);

    // Pulse train information widgets' layout
    QHBoxLayout *pulseOrTrainRow = new QHBoxLayout;
    pulseOrTrainRow->addWidget(pulseOrTrainLabel);
    pulseOrTrainRow->addStretch();
    pulseOrTrainRow->addWidget(pulseOrTrainComboBox);

    QHBoxLayout *numberOfStimPulsesRow = new QHBoxLayout;
    numberOfStimPulsesRow->addWidget(numberOfStimPulsesLabel);
    numberOfStimPulsesRow->addStretch();
    numberOfStimPulsesRow->addWidget(numberOfStimPulsesSpinBox);

    QHBoxLayout *pulseTrainPeriodRow = new QHBoxLayout;
    pulseTrainPeriodRow->addWidget(pulseTrainPeriodLabel);
    pulseTrainPeriodRow->addStretch();
    pulseTrainPeriodRow->addWidget(pulseTrainPeriodSpinBox);

    QHBoxLayout *pulseTrainFrequencyRow = new QHBoxLayout;
    pulseTrainFrequencyRow->addWidget(pulseTrainFrequencyLabel);

    QHBoxLayout *refractoryPeriodRow = new QHBoxLayout;
    refractoryPeriodRow->addWidget(refractoryPeriodLabel);
    refractoryPeriodRow->addStretch();
    refractoryPeriodRow->addWidget(refractoryPeriodSpinBox);

    QVBoxLayout *pulseTrainLayout = new QVBoxLayout;
    pulseTrainLayout->addLayout(pulseOrTrainRow);
    pulseTrainLayout->addLayout(numberOfStimPulsesRow);
    pulseTrainLayout->addLayout(pulseTrainPeriodRow);
    pulseTrainLayout->addLayout(pulseTrainFrequencyRow);
    pulseTrainLayout->addLayout(refractoryPeriodRow);
    pulseTrainGroupBox->setLayout(pulseTrainLayout);

    // Amp Settle widgets' layout
    QHBoxLayout *preStimAmpSettleRow = new QHBoxLayout;
    preStimAmpSettleRow->addWidget(preStimAmpSettleLabel);
    preStimAmpSettleRow->addStretch();
    preStimAmpSettleRow->addWidget(preStimAmpSettleSpinBox);

    QHBoxLayout *postStimAmpSettleRow = new QHBoxLayout;
    postStimAmpSettleRow->addWidget(postStimAmpSettleLabel);
    postStimAmpSettleRow->addStretch();
    postStimAmpSettleRow->addWidget(postStimAmpSettleSpinBox);

    QHBoxLayout *maintainAmpSettleRow = new QHBoxLayout;
    maintainAmpSettleRow->addWidget(maintainAmpSettleCheckBox);

    QHBoxLayout *enableAmpSettleRow = new QHBoxLayout;
    enableAmpSettleRow->addWidget(enableAmpSettleCheckBox);

    QVBoxLayout *ampSettleLayout = new QVBoxLayout;
    ampSettleLayout->addLayout(enableAmpSettleRow);
    ampSettleLayout->addLayout(preStimAmpSettleRow);
    ampSettleLayout->addLayout(postStimAmpSettleRow);
    ampSettleLayout->addLayout(maintainAmpSettleRow);
    ampSettleGroupBox->setLayout(ampSettleLayout);

    // Charge Recovery widgets' layout
    QHBoxLayout *postStimChargeRecovOnRow = new QHBoxLayout;
    postStimChargeRecovOnRow->addWidget(postStimChargeRecovOnLabel);
    postStimChargeRecovOnRow->addStretch();
    postStimChargeRecovOnRow->addWidget(postStimChargeRecovOnSpinBox);

    QHBoxLayout *postStimChargeRecovOffRow = new QHBoxLayout;
    postStimChargeRecovOffRow->addWidget(postStimChargeRecovOffLabel);
    postStimChargeRecovOffRow->addStretch();
    postStimChargeRecovOffRow->addWidget(postStimChargeRecovOffSpinBox);

    QHBoxLayout *enableChargeRecoveryRow = new QHBoxLayout;
    enableChargeRecoveryRow->addWidget(enableChargeRecoveryCheckBox);

    QVBoxLayout *chargeRecoveryLayout = new QVBoxLayout;
    chargeRecoveryLayout->addLayout(enableChargeRecoveryRow);
    chargeRecoveryLayout->addLayout(postStimChargeRecovOnRow);
    chargeRecoveryLayout->addLayout(postStimChargeRecovOffRow);
    chargeRecoveryGroupBox->setLayout(chargeRecoveryLayout);

    QVBoxLayout *firstColumn = new QVBoxLayout;
    firstColumn->addWidget(triggerGroupBox);
    firstColumn->addWidget(pulseTrainGroupBox);
    firstColumn->addStretch();

    QVBoxLayout *secondColumn = new QVBoxLayout;
    secondColumn->addWidget(stimWaveFormGroupBox);
    secondColumn->addStretch();

    QVBoxLayout *thirdColumn = new QVBoxLayout;
    thirdColumn->addWidget(ampSettleGroupBox);
    thirdColumn->addWidget(chargeRecoveryGroupBox);
    thirdColumn->addStretch();

    QHBoxLayout *finalRow = new QHBoxLayout;
    finalRow->addWidget(buttonBox);

    QHBoxLayout *columns = new QHBoxLayout;
    columns->addLayout(firstColumn);
    columns->addLayout(secondColumn);
    columns->addLayout(thirdColumn);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(stimFigure);
    mainLayout->addLayout(columns);
    mainLayout->addStretch();
    mainLayout->addLayout(finalRow);

    QWidget *mainWidget = new QWidget(this);
    mainWidget->setLayout(mainLayout);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(mainWidget);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QVBoxLayout *scrollLayout = new QVBoxLayout;
    scrollLayout->addWidget(scrollArea);

    setLayout(scrollLayout);

    // Set dialog initial size to 5% larger than scrollArea's sizeHint - should avoid scroll bars for default size.
    int initialWidth = round(mainWidget->sizeHint().width() * 1.05);
    int initialHeight = round(mainWidget->sizeHint().height() * 1.05);
    resize(initialWidth, initialHeight);

    setWindowTitle("Stimulation Parameters: " + channel->getNativeName() + " (" + channel->getCustomName() + ")");
}

void StimParamDialog::updateParametersFromState(StimParameters *parameters)
{
    // Set zero values initially to bring all ranges to microseconds, so values can be set properly.
    preStimAmpSettleSpinBox->setValue(0);
    postStimChargeRecovOnSpinBox->setValue(0);
    postStimAmpSettleSpinBox->setValue(0);
    postStimChargeRecovOffSpinBox->setValue(0);
    firstPhaseDurationSpinBox->setValue(0);
    secondPhaseDurationSpinBox->setValue(0);
    interphaseDelaySpinBox->setValue(0);
    postTriggerDelaySpinBox->setValue(0);
    pulseTrainPeriodSpinBox->setValue(0);
    refractoryPeriodSpinBox->setValue(0);
    firstPhaseAmplitudeSpinBox->setValue(0);
    secondPhaseAmplitudeSpinBox->setValue(0);

    // Load parameters that affect the ranges of other parameters first:
    // preStimAmpSettle, postStimChargeRecovOn, postStimAmpSettle, and postStimChargeRecovOff.
    preStimAmpSettleSpinBox->loadValue(parameters->preStimAmpSettle->getValue());
    postStimAmpSettleSpinBox->loadValue(parameters->postStimAmpSettle->getValue());
    postStimChargeRecovOnSpinBox->loadValue(parameters->postStimChargeRecovOn->getValue());
    postStimChargeRecovOffSpinBox->loadValue(parameters->postStimChargeRecovOff->getValue());


    // Load the rest of the parameters.
    stimShapeComboBox->setCurrentIndex(parameters->stimShape->getIndex());
    stimPolarityComboBox->setCurrentIndex(parameters->stimPolarity->getIndex());
    firstPhaseDurationSpinBox->loadValue(parameters->firstPhaseDuration->getValue());
    secondPhaseDurationSpinBox->loadValue(parameters->secondPhaseDuration->getValue());
    interphaseDelaySpinBox->loadValue(parameters->interphaseDelay->getValue());
    firstPhaseAmplitudeSpinBox->loadValue(parameters->firstPhaseAmplitude->getValue());
    secondPhaseAmplitudeSpinBox->loadValue(parameters->secondPhaseAmplitude->getValue());
    enableStimCheckBox->setChecked(parameters->enabled->getValue());
    triggerSourceComboBox->setCurrentIndex(parameters->triggerSource->getIndex());
    triggerEdgeOrLevelComboBox->setCurrentIndex(parameters->triggerEdgeOrLevel->getIndex());
    triggerHighOrLowComboBox->setCurrentIndex(parameters->triggerHighOrLow->getIndex());
    postTriggerDelaySpinBox->loadValue(parameters->postTriggerDelay->getValue());
    pulseOrTrainComboBox->setCurrentIndex(parameters->pulseOrTrain->getIndex());
    numberOfStimPulsesSpinBox->setValue(parameters->numberOfStimPulses->getValue());
    pulseTrainPeriodSpinBox->loadValue(parameters->pulseTrainPeriod->getValue());
    refractoryPeriodSpinBox->loadValue(parameters->refractoryPeriod->getValue());
    maintainAmpSettleCheckBox->setChecked(parameters->maintainAmpSettle->getValue());
    enableAmpSettleCheckBox->setChecked(parameters->enableAmpSettle->getValue());
    enableChargeRecoveryCheckBox->setChecked(parameters->enableChargeRecovery->getValue());

    // Constrain time periods so that the first values the user sees are valid.
    constrainPulseTrainPeriod();
    constrainPostStimChargeRecovery();
    constrainPostTriggerDelay();
    constrainRefractoryPeriod();

    // Calculate frequency and charge so that the first labels that are displayed correspond to the loaded parameters.
    calculatePulseTrainFrequency();
    calculateCharge();
}

void StimParamDialog::activate()
{
    updateFromState();
    show();
    raise();
    activateWindow();
}

void StimParamDialog::updateFromState()
{
    updateParametersFromState(parameters);
}

void StimParamDialog::accept()
{
    // Save the values of the parameters from the dialog box into the object.
    parameters->stimShape->setIndex(stimShapeComboBox->currentIndex());
    parameters->stimPolarity->setIndex(stimPolarityComboBox->currentIndex());
    parameters->firstPhaseDuration->setValue(firstPhaseDurationSpinBox->getTrueValue());
    parameters->secondPhaseDuration->setValue(secondPhaseDurationSpinBox->getTrueValue());
    parameters->interphaseDelay->setValue(interphaseDelaySpinBox->getTrueValue());
    parameters->firstPhaseAmplitude->setValue(firstPhaseAmplitudeSpinBox->getTrueValue());
    parameters->secondPhaseAmplitude->setValue(secondPhaseAmplitudeSpinBox->getTrueValue());
    parameters->enabled->setValue(enableStimCheckBox->isChecked());
    parameters->triggerSource->setIndex(triggerSourceComboBox->currentIndex());
    parameters->triggerEdgeOrLevel->setIndex(triggerEdgeOrLevelComboBox->currentIndex());
    parameters->triggerHighOrLow->setIndex(triggerHighOrLowComboBox->currentIndex());
    parameters->postTriggerDelay->setValue(postTriggerDelaySpinBox->getTrueValue());
    parameters->pulseOrTrain->setIndex(pulseOrTrainComboBox->currentIndex());
    parameters->numberOfStimPulses->setValue(numberOfStimPulsesSpinBox->value());
    parameters->pulseTrainPeriod->setValue(pulseTrainPeriodSpinBox->getTrueValue());
    parameters->refractoryPeriod->setValue(refractoryPeriodSpinBox->getTrueValue());
    parameters->maintainAmpSettle->setValue(maintainAmpSettleCheckBox->isChecked());
    parameters->enableAmpSettle->setValue(enableAmpSettleCheckBox->isChecked());
    parameters->enableChargeRecovery->setValue(enableChargeRecoveryCheckBox->isChecked());
    parameters->preStimAmpSettle->setValue(preStimAmpSettleSpinBox->getTrueValue());
    parameters->postStimChargeRecovOn->setValue(postStimChargeRecovOnSpinBox->getTrueValue());
    parameters->postStimAmpSettle->setValue(postStimAmpSettleSpinBox->getTrueValue());
    parameters->postStimChargeRecovOff->setValue(postStimChargeRecovOffSpinBox->getTrueValue());

    // Close the window.
    done(Accepted);
}

// Emit signals when widgets that can be highlighted gain or lose focus.
void StimParamDialog::notifyFocusChanged(QWidget *lostFocus, QWidget *gainedFocus)
{
    // Emit signals when a widget loses focus.
    if (lostFocus == firstPhaseDurationSpinBox->pointer()) emit highlightFirstPhaseDuration(false);
    if (lostFocus == secondPhaseDurationSpinBox->pointer()) emit highlightSecondPhaseDuration(false);
    if (lostFocus == interphaseDelaySpinBox->pointer()) emit highlightInterphaseDelay(false);
    if (lostFocus == firstPhaseAmplitudeSpinBox->pointer()) emit highlightFirstPhaseAmplitude(false);
    if (lostFocus == secondPhaseAmplitudeSpinBox->pointer()) emit highlightSecondPhaseAmplitude(false);
    if (lostFocus == postTriggerDelaySpinBox->pointer()) emit highlightPostTriggerDelay(false);
    if (lostFocus == pulseTrainPeriodSpinBox->pointer()) emit highlightPulseTrainPeriod(false);
    if (lostFocus == refractoryPeriodSpinBox->pointer()) emit highlightRefractoryPeriod(false);
    if (lostFocus == preStimAmpSettleSpinBox->pointer()) emit highlightPreStimAmpSettle(false);
    if (lostFocus == postStimAmpSettleSpinBox->pointer()) emit highlightPostStimAmpSettle(false);
    if (lostFocus == postStimChargeRecovOnSpinBox->pointer()) emit highlightPostStimChargeRecovOn(false);
    if (lostFocus == postStimChargeRecovOffSpinBox->pointer()) emit highlightPostStimChargeRecovOff(false);

    // Emit signals when a widget gains focus.
    if (gainedFocus == firstPhaseDurationSpinBox->pointer()) emit highlightFirstPhaseDuration(true);
    if (gainedFocus == secondPhaseDurationSpinBox->pointer()) emit highlightSecondPhaseDuration(true);
    if (gainedFocus == interphaseDelaySpinBox->pointer()) emit highlightInterphaseDelay(true);
    if (gainedFocus == firstPhaseAmplitudeSpinBox->pointer()) emit highlightFirstPhaseAmplitude(true);
    if (gainedFocus == secondPhaseAmplitudeSpinBox->pointer()) emit highlightSecondPhaseAmplitude(true);
    if (gainedFocus == postTriggerDelaySpinBox->pointer()) emit highlightPostTriggerDelay(true);
    if (gainedFocus == pulseTrainPeriodSpinBox->pointer()) emit highlightPulseTrainPeriod(true);
    if (gainedFocus == refractoryPeriodSpinBox->pointer()) emit highlightRefractoryPeriod(true);
    if (gainedFocus == preStimAmpSettleSpinBox->pointer()) emit highlightPreStimAmpSettle(true);
    if (gainedFocus == postStimAmpSettleSpinBox->pointer()) emit highlightPostStimAmpSettle(true);
    if (gainedFocus == postStimChargeRecovOnSpinBox->pointer()) emit highlightPostStimChargeRecovOn(true);
    if (gainedFocus == postStimChargeRecovOffSpinBox->pointer()) emit highlightPostStimChargeRecovOff(true);
}

// Look at each widget individually and the state of its control widgets to see if it should be enabled or disabled.
void StimParamDialog::enableWidgets()
{
    // Boolean conditional statements reflect if each widget should be enabled.

    // Trigger Group Box
    triggerSourceLabel->setEnabled(enableStimCheckBox->isChecked());
    triggerSourceComboBox->setEnabled(enableStimCheckBox->isChecked());
    triggerEdgeOrLevelComboBox->setEnabled(enableStimCheckBox->isChecked());
    triggerHighOrLowComboBox->setEnabled(enableStimCheckBox->isChecked());
    postTriggerDelayLabel->setEnabled(enableStimCheckBox->isChecked());
    postTriggerDelaySpinBox->setEnabled(enableStimCheckBox->isChecked());

    // Pulse Train Group Box
    pulseOrTrainLabel->setEnabled(enableStimCheckBox->isChecked());
    pulseOrTrainComboBox->setEnabled(enableStimCheckBox->isChecked());
    numberOfStimPulsesLabel->setEnabled(enableStimCheckBox->isChecked() && pulseOrTrainComboBox->currentIndex() == PulseTrain);
    numberOfStimPulsesSpinBox->setEnabled(enableStimCheckBox->isChecked() && pulseOrTrainComboBox->currentIndex() == PulseTrain);
    pulseTrainPeriodLabel->setEnabled(enableStimCheckBox->isChecked() && pulseOrTrainComboBox->currentIndex() == PulseTrain);
    pulseTrainPeriodSpinBox->setEnabled(enableStimCheckBox->isChecked() && pulseOrTrainComboBox->currentIndex() == PulseTrain);
    pulseTrainFrequencyLabel->setEnabled(enableStimCheckBox->isChecked() && pulseOrTrainComboBox->currentIndex() == PulseTrain);
    refractoryPeriodLabel->setEnabled(enableStimCheckBox->isChecked());
    refractoryPeriodSpinBox->setEnabled(enableStimCheckBox->isChecked());

    // Stimulation Waveform
    stimShapeLabel->setEnabled(enableStimCheckBox->isChecked());
    stimShapeComboBox->setEnabled(enableStimCheckBox->isChecked());
    stimPolarityLabel->setEnabled(enableStimCheckBox->isChecked());
    stimPolarityComboBox->setEnabled(enableStimCheckBox->isChecked());
    firstPhaseDurationLabel->setEnabled(enableStimCheckBox->isChecked());
    firstPhaseDurationSpinBox->setEnabled(enableStimCheckBox->isChecked());
    secondPhaseDurationLabel->setEnabled(enableStimCheckBox->isChecked());
    secondPhaseDurationSpinBox->setEnabled(enableStimCheckBox->isChecked());
    interphaseDelayLabel->setEnabled(enableStimCheckBox->isChecked() && stimShapeComboBox->currentIndex() == BiphasicWithInterphaseDelay);
    interphaseDelaySpinBox->setEnabled(enableStimCheckBox->isChecked() && stimShapeComboBox->currentIndex() == BiphasicWithInterphaseDelay);
    firstPhaseAmplitudeLabel->setEnabled(enableStimCheckBox->isChecked());
    firstPhaseAmplitudeSpinBox->setEnabled(enableStimCheckBox->isChecked());
    secondPhaseAmplitudeLabel->setEnabled(enableStimCheckBox->isChecked());
    secondPhaseAmplitudeSpinBox->setEnabled(enableStimCheckBox->isChecked());
    totalPosChargeLabel->setEnabled(enableStimCheckBox->isChecked());
    totalNegChargeLabel->setEnabled(enableStimCheckBox->isChecked());

    // Amp Settle
    enableAmpSettleCheckBox->setEnabled(enableStimCheckBox->isChecked());
    preStimAmpSettleLabel->setEnabled(enableStimCheckBox->isChecked() && enableAmpSettleCheckBox->isChecked());
    preStimAmpSettleSpinBox->setEnabled(enableStimCheckBox->isChecked() && enableAmpSettleCheckBox->isChecked());
    postStimAmpSettleLabel->setEnabled(enableStimCheckBox->isChecked() && enableAmpSettleCheckBox->isChecked());
    postStimAmpSettleSpinBox->setEnabled(enableStimCheckBox->isChecked() && enableAmpSettleCheckBox->isChecked());
    maintainAmpSettleCheckBox->setEnabled(enableStimCheckBox->isChecked() && enableAmpSettleCheckBox->isChecked() && pulseOrTrainComboBox->currentIndex() == PulseTrain);

    // Charge Recovery
    enableChargeRecoveryCheckBox->setEnabled(enableStimCheckBox->isChecked());
    postStimChargeRecovOnLabel->setEnabled(enableStimCheckBox->isChecked() && enableChargeRecoveryCheckBox->isChecked());
    postStimChargeRecovOnSpinBox->setEnabled(enableStimCheckBox->isChecked() && enableChargeRecoveryCheckBox->isChecked());
    postStimChargeRecovOffLabel->setEnabled(enableStimCheckBox->isChecked() && enableChargeRecoveryCheckBox->isChecked());
    postStimChargeRecovOffSpinBox->setEnabled(enableStimCheckBox->isChecked() && enableChargeRecoveryCheckBox->isChecked());

    // Reset Text for First Phase Labels
    if (stimShapeComboBox->currentIndex() == Biphasic || stimShapeComboBox->currentIndex() == BiphasicWithInterphaseDelay) {
        firstPhaseDurationLabel->setText(tr("First Phase Duration (D1):"));
        firstPhaseAmplitudeLabel->setText(tr("First Phase Amplitude (A1):"));
    } else if (stimShapeComboBox->currentIndex() == Triphasic) {
        firstPhaseDurationLabel->setText(tr("First/Third Phase Duration (D1):"));
        firstPhaseAmplitudeLabel->setText(tr("First/Third Phase Amplitude (A1):"));
    }
}

// Calculate the positive and negative charges generated by the current phase amplitudes and durations.
void StimParamDialog::calculateCharge()
{
    // Calculate Qmin and Qmax.
    double firstCharge = (firstPhaseAmplitudeSpinBox->getTrueValue()) * (firstPhaseDurationSpinBox->getTrueValue());
    double secondCharge = (secondPhaseAmplitudeSpinBox->getTrueValue()) * (secondPhaseDurationSpinBox->getTrueValue());
    if (stimShapeComboBox->currentIndex() == Triphasic)
        firstCharge = firstCharge * 2;
    double Qmin = qMin(firstCharge, secondCharge);
    double Qmax = qMax(firstCharge, secondCharge);

    // Calculate imbalance as a percentage.
    double imbalance;
    if (Qmax == 0.0) imbalance = 0;
    else imbalance = 100.0 * (1.0 - Qmin / Qmax);

    if (imbalance < .001) {  // If there's no imbalance, display text in a green font.
        chargeBalanceLabel->setText("Charge is perfectly balanced.");
        chargeBalanceLabel->setStyleSheet("QLabel {color: green}");
    } else if (imbalance < 10.0) {  // If there's an imbalance of less than 10%, display text in a yellow font.
        chargeBalanceLabel->setText("Charge is imbalanced by " + QString::number(imbalance, 'f', 1) + "%.");
        chargeBalanceLabel->setStyleSheet("QLabel {color: orange}");
    } else {  // If there's an imbalance of more than 10%, display text in a red font.
        chargeBalanceLabel->setText("Charge is imbalanced by " + QString::number(imbalance, 'f', 1) + "%.");
        chargeBalanceLabel->setStyleSheet("QLabel {color: red}");
    }

    // Update total positive and negative charge labels.
    double totalPositive;
    double totalNegative;
    if (stimPolarityComboBox->currentIndex() == PositiveFirst) {
        totalPositive = firstCharge;
        totalNegative = secondCharge;
    } else {
        totalPositive = secondCharge;
        totalNegative = firstCharge;
    }

    QString totalPosChargeString = tr("Total positive charge injected per pulse: ");
    QString totalNegChargeString = tr("Total negative charge injected per pulse: ");

    if (totalPositive < 999.0)
        totalPosChargeLabel->setText(totalPosChargeString + QString::number(totalPositive, 'f', 1) + " pC");
    else if (totalPositive < 999000.0)
        totalPosChargeLabel->setText(totalPosChargeString + QString::number(1.0e-3 * totalPositive, 'f', 1) + " nC");
    else
        totalPosChargeLabel->setText(totalPosChargeString + QString::number(1.0e-6 * totalPositive, 'f', 1) + " " + MuSymbol + "C");

    if (totalNegative < 999.0)
        totalNegChargeLabel->setText(totalNegChargeString + QString::number(totalNegative, 'f', 1) + " pC");
    else if (totalNegative < 999000.0)
        totalNegChargeLabel->setText(totalNegChargeString + QString::number(1.0e-3 * totalNegative, 'f', 1) + " nC");
    else
        totalNegChargeLabel->setText(totalNegChargeString + QString::number(1.0e-6 * totalNegative, 'f', 1) + " " + MuSymbol + "C");
}

// Calculate the frequency of the pulse train, given the user-selected period.
void StimParamDialog::calculatePulseTrainFrequency()
{
    double frequency;
    QString pulseTrainString = tr("Pulse Train Frequency: ");

    if (pulseTrainPeriodSpinBox->getTrueValue() == 0.0) {
        pulseTrainFrequencyLabel->setText(pulseTrainString + "-");
    } else if (pulseTrainPeriodSpinBox->getTrueValue() < 1000.0) {
        frequency = 1.0 / pulseTrainPeriodSpinBox->getTrueValue();
        pulseTrainFrequencyLabel->setText(pulseTrainString + QString::number(frequency * 1.0e3, 'f', 2) + " kHz");
    } else {
        frequency = 1.0 / pulseTrainPeriodSpinBox->getTrueValue();
        pulseTrainFrequencyLabel->setText(pulseTrainString + QString::number(frequency * 1.0e6, 'f', 2) + " Hz");
    }
}

// Constrain postTriggerDelay's lowest possible value to the current value of preStimAmpSettle.
void StimParamDialog::constrainPostTriggerDelay()
{
    postTriggerDelaySpinBox->setTrueMinimum(preStimAmpSettleSpinBox->getTrueValue());
}

// Constrain postStimChargeRecovOff's lowest possible value to the current value of postStimChargeRecovOn.
void StimParamDialog::constrainPostStimChargeRecovery()
{
    postStimChargeRecovOffSpinBox->setTrueMinimum(postStimChargeRecovOnSpinBox->getTrueValue());
}

// Constrain refractoryPeriod's lowest possible value to the higher of postStimAmpSettle and postStimChargeRecovOff. In other
// words, postStimAmpSettle, postStimChargeRecovOff, and postStimChargeRecovOn cannot surpass the value of refractoryPeriod.
void StimParamDialog::constrainRefractoryPeriod()
{
    refractoryPeriodSpinBox->setTrueMinimum(qMax(postStimAmpSettleSpinBox->getTrueValue(), postStimChargeRecovOffSpinBox->getTrueValue()));
}

// Private slot that constrains pulseTrainPeriod's lowest possible value to the sum of the durations of active phases.
void StimParamDialog::constrainPulseTrainPeriod()
{
    double minimum;
    if (stimShapeComboBox->currentIndex() == Biphasic) {
        minimum = firstPhaseDurationSpinBox->getTrueValue() + secondPhaseDurationSpinBox->getTrueValue();
    } else if (stimShapeComboBox->currentIndex() == BiphasicWithInterphaseDelay) {
        minimum = firstPhaseDurationSpinBox->getTrueValue() + secondPhaseDurationSpinBox->getTrueValue() + interphaseDelaySpinBox->getTrueValue();
    } else {
        minimum = (2.0 * firstPhaseDurationSpinBox->getTrueValue()) + secondPhaseDurationSpinBox->getTrueValue();
    }
    pulseTrainPeriodSpinBox->setTrueMinimum(minimum);
}

void StimParamDialog::roundTimeInputs()
{
    // Round all time inputs to the nearest multiple of timestep.
    firstPhaseDurationSpinBox->roundValue();
    secondPhaseDurationSpinBox->roundValue();
    interphaseDelaySpinBox->roundValue();
    postTriggerDelaySpinBox->roundValue();
    pulseTrainPeriodSpinBox->roundValue();
    refractoryPeriodSpinBox->roundValue();
    preStimAmpSettleSpinBox->roundValue();
    postStimAmpSettleSpinBox->roundValue();
    postStimChargeRecovOnSpinBox->roundValue();
    postStimChargeRecovOffSpinBox->roundValue();
}

void StimParamDialog::roundCurrentInputs()
{
    // Round all current inputs to the nearest multiple of currentstep.
    firstPhaseAmplitudeSpinBox->roundValue();
    secondPhaseAmplitudeSpinBox->roundValue();
}

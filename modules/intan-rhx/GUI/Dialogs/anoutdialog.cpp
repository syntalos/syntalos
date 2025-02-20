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
#include <cmath>
#include "signalsources.h"
#include "smartspinbox.h"
#include "voltagespinbox.h"
#include "anoutfigure.h"
#include "anoutdialog.h"

AnOutDialog::AnOutDialog(SystemState* state_, Channel* channel_, QWidget *parent) :
    QDialog(parent),
    state(state_),
    channel(channel_)
{
    parameters = channel->stimParameters;
    timestep = 1.0e6 / state->sampleRate->getNumericValue();  // time step in microseconds

    anOutFigure = new AnOutFigure(parameters, this);

    QGroupBox* stimWaveFormGroupBox = new QGroupBox(tr("Stimulation Waveform"), this);

    stimShapeLabel = new QLabel(tr("Stimulation Shape:"), this);
    stimShapeComboBox = new QComboBox(this);
    QStringList stimShapes;
    stimShapes << tr("Biphasic") << tr("Biphasic with Delay") << tr("Triphasic") << tr("Monophasic");
    stimShapeComboBox->addItems(stimShapes);

    stimPolarityLabel = new QLabel(tr("Stimulation Polarity:"), this);
    stimPolarityComboBox = new QComboBox(this);
    stimPolarityComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    QStringList stimPolarities;
    stimPolarities << tr("Negative Voltage First") << tr("Positive Voltage First");
    stimPolarityComboBox->addItems(stimPolarities);

    firstPhaseDurationLabel = new QLabel(tr("First Phase Duration (D1):"), this);
    firstPhaseDurationSpinBox = new TimeSpinBox(timestep, this);
    firstPhaseDurationSpinBox->setRange(0, 5000);

    secondPhaseDurationLabel = new QLabel(tr("Second Phase Duration (D2):"), this);
    secondPhaseDurationSpinBox = new TimeSpinBox(timestep, this);
    secondPhaseDurationSpinBox->setRange(0, 5000);

    interphaseDelayLabel = new QLabel(tr("Interphase Delay (DP):"), this);
    interphaseDelaySpinBox = new TimeSpinBox(timestep, this);
    interphaseDelaySpinBox->setRange(0, 5000);

    firstPhaseAmplitudeLabel = new QLabel(tr("First Phase Amplitude (A1):"), this);
    firstPhaseAmplitudeSpinBox = new VoltageSpinBox(this);
    firstPhaseAmplitudeSpinBox->setRange(0, 10.24);

    secondPhaseAmplitudeLabel = new QLabel(tr("Second Phase Amplitude (A2):"), this);
    secondPhaseAmplitudeSpinBox = new VoltageSpinBox(this);
    secondPhaseAmplitudeSpinBox->setRange(0, 10.24);

    baselineVoltageLabel = new QLabel(tr("Baseline Voltage:"));
    baselineVoltageSpinBox = new VoltageSpinBox(this);
    baselineVoltageSpinBox->setRange(-10.24, 10.24);

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

    // triggerEdgeOrLevelLabel = new QLabel(tr("Stimulation is:"), this);
    triggerEdgeOrLevelLabel = new QLabel(tr(" "), this);
    triggerEdgeOrLevelComboBox = new QComboBox(this);
    QStringList triggerEdgeOrLevels;
    triggerEdgeOrLevels << tr("Edge Triggered") << tr("Level Triggered");
    triggerEdgeOrLevelComboBox->addItems(triggerEdgeOrLevels);

    // triggerHighOrLowLabel = new QLabel(tr("Stimulation Will:"), this);
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
    numberOfStimPulsesSpinBox->setRange(2, 256);

    pulseTrainPeriodLabel = new QLabel(tr("Pulse Train Period:"), this);
    pulseTrainPeriodSpinBox = new TimeSpinBox(timestep, this);
    pulseTrainPeriodSpinBox->setRange(0, 1000000);

    pulseTrainFrequencyLabel = new QLabel();

    refractoryPeriodLabel = new QLabel(tr("Post-Stim Refractory Period:"), this);
    refractoryPeriodSpinBox = new TimeSpinBox(timestep, this);
    refractoryPeriodSpinBox->setRange(0, 1000000);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    // Connect internal signals and slots.
    connect(enableStimCheckBox, SIGNAL(checkStateChanged(Qt::CheckState)), this, SLOT(enableWidgets()));
    connect(pulseOrTrainComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(enableWidgets()));
    connect(stimShapeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(enableWidgets()));

    connect(stimShapeComboBox, SIGNAL(currentIndexChanged(int)), this, SIGNAL(minimumPeriodChanged()));
    connect(firstPhaseDurationSpinBox, SIGNAL(valueChanged(double)), this, SIGNAL(minimumPeriodChanged()));
    connect(secondPhaseDurationSpinBox, SIGNAL(valueChanged(double)), this, SIGNAL(minimumPeriodChanged()));

    connect(pulseTrainPeriodSpinBox, SIGNAL(valueChanged(double)), this, SLOT(calculatePulseTrainFrequency()));
    connect(this, SIGNAL(minimumPeriodChanged()), this, SLOT(constrainPulseTrainPeriod()));
    connect(firstPhaseDurationSpinBox, SIGNAL(editingFinished()), this, SLOT(roundTimeInputs()));
    connect(secondPhaseDurationSpinBox, SIGNAL(editingFinished()), this, SLOT(roundTimeInputs()));
    connect(interphaseDelaySpinBox, SIGNAL(editingFinished()), this, SLOT(roundTimeInputs()));
    connect(interphaseDelaySpinBox, SIGNAL(valueChanged(double)), this, SIGNAL(minimumPeriodChanged()));
    connect(postTriggerDelaySpinBox, SIGNAL(editingFinished()), this, SLOT(roundTimeInputs()));
    connect(pulseTrainPeriodSpinBox, SIGNAL(editingFinished()), this, SLOT(roundTimeInputs()));
    connect(refractoryPeriodSpinBox, SIGNAL(editingFinished()), this, SLOT(roundTimeInputs()));
    connect(firstPhaseAmplitudeSpinBox, SIGNAL(editingFinished()), this, SLOT(roundVoltageInputs()));
    connect(secondPhaseAmplitudeSpinBox, SIGNAL(editingFinished()), this, SLOT(roundVoltageInputs()));
    connect(baselineVoltageSpinBox, SIGNAL(editingFinished()), this, SLOT(roundVoltageInputs()));
    connect(stimShapeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateMonophasicAndPositive()));
    connect(stimPolarityComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateMonophasicAndPositive()));
    connect(enableStimCheckBox, SIGNAL(toggled(bool)), anOutFigure, SLOT(highlightStimTrace(bool)));

    // Connect signals to anOutFigure's non-highlight slots.
    connect(stimShapeComboBox, SIGNAL(currentIndexChanged(int)), anOutFigure, SLOT(updateStimShape(int)));
    connect(stimPolarityComboBox, SIGNAL(currentIndexChanged(int)), anOutFigure, SLOT(updateStimPolarity(int)));
    connect(pulseOrTrainComboBox, SIGNAL(currentIndexChanged(int)), anOutFigure, SLOT(updatePulseOrTrain(int)));
    connect(this, SIGNAL(monophasicAndPositiveSignalChanged(bool)), anOutFigure, SLOT(updateMonophasicAndPositive(bool)));

    // Connect signals to anOutFigure's highlight slots.
    connect(this, SIGNAL(highlightBaselineVoltage(bool)), anOutFigure, SLOT(highlightBaselineVoltage(bool)));
    connect(this, SIGNAL(highlightFirstPhaseDuration(bool)), anOutFigure, SLOT(highlightFirstPhaseDuration(bool)));
    connect(this, SIGNAL(highlightSecondPhaseDuration(bool)), anOutFigure, SLOT(highlightSecondPhaseDuration(bool)));
    connect(this, SIGNAL(highlightInterphaseDelay(bool)), anOutFigure, SLOT(highlightInterphaseDelay(bool)));
    connect(this, SIGNAL(highlightFirstPhaseAmplitude(bool)), anOutFigure, SLOT(highlightFirstPhaseAmplitude(bool)));
    connect(this, SIGNAL(highlightSecondPhaseAmplitude(bool)), anOutFigure, SLOT(highlightSecondPhaseAmplitude(bool)));
    connect(this, SIGNAL(highlightPostTriggerDelay(bool)), anOutFigure, SLOT(highlightPostTriggerDelay(bool)));
    connect(this, SIGNAL(highlightPulseTrainPeriod(bool)), anOutFigure, SLOT(highlightPulseTrainPeriod(bool)));
    connect(this, SIGNAL(highlightRefractoryPeriod(bool)), anOutFigure, SLOT(highlightRefractoryPeriod(bool)));

    // Update widgets' states to be different than the default states, so state changes are detected properly.

    stimShapeComboBox->setCurrentIndex(Triphasic);
    stimPolarityComboBox->setCurrentIndex(PositiveFirst);
    firstPhaseDurationSpinBox->setValue(1);
    secondPhaseDurationSpinBox->setValue(1);
    interphaseDelaySpinBox->setValue(1);
    firstPhaseAmplitudeSpinBox->setValue(1);
    secondPhaseAmplitudeSpinBox->setValue(1);
    baselineVoltageSpinBox->setValue(1);
    enableStimCheckBox->setChecked(true);
    triggerSourceComboBox->setCurrentIndex(AnalogIn1);
    triggerEdgeOrLevelComboBox->setCurrentIndex(TriggerLevel);
    triggerHighOrLowComboBox->setCurrentIndex(TriggerLow);
    postTriggerDelaySpinBox->setValue(2);
    pulseOrTrainComboBox->setCurrentIndex(SinglePulse);
    numberOfStimPulsesSpinBox->setValue(2);
    pulseTrainPeriodSpinBox->setValue(1);
    refractoryPeriodSpinBox->setValue(1);

    // Update dialog's state based on structParameters
    loadParameters(parameters);

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

    QHBoxLayout *baselineVoltageRow = new QHBoxLayout;
    baselineVoltageRow->addWidget(baselineVoltageLabel);
    baselineVoltageRow->addStretch();
    baselineVoltageRow->addWidget(baselineVoltageSpinBox);

    QVBoxLayout *stimWaveFormLayout = new QVBoxLayout;
    stimWaveFormLayout->addLayout(stimShapeRow);
    stimWaveFormLayout->addLayout(stimPolarityRow);
    stimWaveFormLayout->addLayout(firstPhaseDurationRow);
    stimWaveFormLayout->addLayout(secondPhaseDurationRow);
    stimWaveFormLayout->addLayout(interphaseDelayRow);
    stimWaveFormLayout->addLayout(firstPhaseAmplitudeRow);
    stimWaveFormLayout->addLayout(secondPhaseAmplitudeRow);
    stimWaveFormLayout->addLayout(baselineVoltageRow);
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

    QVBoxLayout *firstColumn = new QVBoxLayout;
    firstColumn->addWidget(triggerGroupBox);
    firstColumn->addStretch();

    QVBoxLayout *secondColumn = new QVBoxLayout;
    secondColumn->addWidget(pulseTrainGroupBox);
    secondColumn->addStretch();

    QVBoxLayout *thirdColumn = new QVBoxLayout;
    thirdColumn->addWidget(stimWaveFormGroupBox);
    thirdColumn->addStretch();

    QHBoxLayout *finalRow = new QHBoxLayout;
    finalRow->addWidget(buttonBox);

    QHBoxLayout *columns = new QHBoxLayout;
    columns->addLayout(firstColumn);
    columns->addLayout(secondColumn);
    columns->addLayout(thirdColumn);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(anOutFigure);
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

    setWindowTitle("Analog Output Parameters: " + channel->getNativeName() + " (" + channel->getCustomName() + ")");
}

void AnOutDialog::loadParameters(StimParameters *parameters)
{
    // Set zero values initially to bring all ranges to microseconds, so values can be set properly.
    firstPhaseDurationSpinBox->setValue(0);
    secondPhaseDurationSpinBox->setValue(0);
    interphaseDelaySpinBox->setValue(0);
    postTriggerDelaySpinBox->setValue(0);
    pulseTrainPeriodSpinBox->setValue(0);
    refractoryPeriodSpinBox->setValue(0);
    firstPhaseAmplitudeSpinBox->setValue(0);
    secondPhaseAmplitudeSpinBox->setValue(0);
    baselineVoltageSpinBox->setValue(0);

    // Load parameters that affect the ranges of other parameters first:

    // Load the rest of the parameters
    stimShapeComboBox->setCurrentIndex(parameters->stimShape->getIndex());
    stimPolarityComboBox->setCurrentIndex(parameters->stimPolarity->getIndex());
    firstPhaseDurationSpinBox->loadValue(parameters->firstPhaseDuration->getValue());
    secondPhaseDurationSpinBox->loadValue(parameters->secondPhaseDuration->getValue());
    interphaseDelaySpinBox->loadValue(parameters->interphaseDelay->getValue());
    firstPhaseAmplitudeSpinBox->setValue(parameters->firstPhaseAmplitude->getValue());
    secondPhaseAmplitudeSpinBox->setValue(parameters->secondPhaseAmplitude->getValue());
    baselineVoltageSpinBox->setValue(parameters->baselineVoltage->getValue());
    enableStimCheckBox->setChecked(parameters->enabled->getValue());
    triggerSourceComboBox->setCurrentIndex(parameters->triggerSource->getIndex());
    triggerEdgeOrLevelComboBox->setCurrentIndex(parameters->triggerEdgeOrLevel->getIndex());
    triggerHighOrLowComboBox->setCurrentIndex(parameters->triggerHighOrLow->getIndex());
    postTriggerDelaySpinBox->loadValue(parameters->postTriggerDelay->getValue());
    pulseOrTrainComboBox->setCurrentIndex(parameters->pulseOrTrain->getIndex());
    numberOfStimPulsesSpinBox->setValue(parameters->numberOfStimPulses->getValue());
    pulseTrainPeriodSpinBox->loadValue(parameters->pulseTrainPeriod->getValue());
    refractoryPeriodSpinBox->loadValue(parameters->refractoryPeriod->getValue());

    // Constrain time periods so that the first values the user sees are valid
    constrainPulseTrainPeriod();

    // Calculate frequency so that the first labels that are displayed correspond to the loaded parameters
    calculatePulseTrainFrequency();
}

void AnOutDialog::accept()
{
    // Save the values of the parameters from the dialog box into the object.
    parameters->stimShape->setIndex(stimShapeComboBox->currentIndex());
    parameters->stimPolarity->setIndex(stimPolarityComboBox->currentIndex());
    parameters->firstPhaseDuration->setValue(firstPhaseDurationSpinBox->getTrueValue());
    parameters->secondPhaseDuration->setValue(secondPhaseDurationSpinBox->getTrueValue());
    parameters->interphaseDelay->setValue(interphaseDelaySpinBox->getTrueValue());
    parameters->firstPhaseAmplitude->setValue(firstPhaseAmplitudeSpinBox->getValue());
    parameters->secondPhaseAmplitude->setValue(secondPhaseAmplitudeSpinBox->getValue());
    parameters->baselineVoltage->setValue(baselineVoltageSpinBox->getValue());
    parameters->enabled->setValue(enableStimCheckBox->isChecked());
    parameters->triggerSource->setIndex(triggerSourceComboBox->currentIndex());
    parameters->triggerEdgeOrLevel->setIndex(triggerEdgeOrLevelComboBox->currentIndex());
    parameters->triggerHighOrLow->setIndex(triggerHighOrLowComboBox->currentIndex());
    parameters->postTriggerDelay->setValue(postTriggerDelaySpinBox->getTrueValue());
    parameters->pulseOrTrain->setIndex(pulseOrTrainComboBox->currentIndex());
    parameters->numberOfStimPulses->setValue(numberOfStimPulsesSpinBox->value());
    parameters->pulseTrainPeriod->setValue(pulseTrainPeriodSpinBox->getTrueValue());
    parameters->refractoryPeriod->setValue(refractoryPeriodSpinBox->getTrueValue());

    // Close the window.
    done(Accepted);
}


// Emit signals when widgets that can be highlighted gain or lose focus.
void AnOutDialog::notifyFocusChanged(QWidget *lostFocus, QWidget *gainedFocus)
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
    if (lostFocus == baselineVoltageSpinBox->pointer()) emit highlightBaselineVoltage(false);

    // Emit signals when a widget gains focus.
    if (gainedFocus == firstPhaseDurationSpinBox->pointer()) emit highlightFirstPhaseDuration(true);
    if (gainedFocus == secondPhaseDurationSpinBox->pointer()) emit highlightSecondPhaseDuration(true);
    if (gainedFocus == interphaseDelaySpinBox->pointer()) emit highlightInterphaseDelay(true);
    if (gainedFocus == firstPhaseAmplitudeSpinBox->pointer()) emit highlightFirstPhaseAmplitude(true);
    if (gainedFocus == secondPhaseAmplitudeSpinBox->pointer()) emit highlightSecondPhaseAmplitude(true);
    if (gainedFocus == postTriggerDelaySpinBox->pointer()) emit highlightPostTriggerDelay(true);
    if (gainedFocus == pulseTrainPeriodSpinBox->pointer()) emit highlightPulseTrainPeriod(true);
    if (gainedFocus == refractoryPeriodSpinBox->pointer()) emit highlightRefractoryPeriod(true);
    if (gainedFocus == baselineVoltageSpinBox->pointer()) emit highlightBaselineVoltage(true);
}

// Look at each widget individually and the state of its control widgets to see if it should be enabled or disabled.
void AnOutDialog::enableWidgets()
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
    if (stimShapeComboBox->currentIndex() == Monophasic) {
        stimPolarityComboBox->setItemText(NegativeFirst, tr("Negative Voltage"));
        stimPolarityComboBox->setItemText(PositiveFirst, tr("Positive Voltage"));
    } else {
        stimPolarityComboBox->setItemText(NegativeFirst, tr("Negative Voltage First"));
        stimPolarityComboBox->setItemText(PositiveFirst, tr("Positive Voltage First"));
    }
    firstPhaseDurationLabel->setEnabled(enableStimCheckBox->isChecked());
    firstPhaseDurationSpinBox->setEnabled(enableStimCheckBox->isChecked());
    secondPhaseDurationLabel->setEnabled(enableStimCheckBox->isChecked() && stimShapeComboBox->currentIndex() != Monophasic);
    secondPhaseDurationSpinBox->setEnabled(enableStimCheckBox->isChecked() && stimShapeComboBox->currentIndex() != Monophasic);
    interphaseDelayLabel->setEnabled(enableStimCheckBox->isChecked() && stimShapeComboBox->currentIndex() == BiphasicWithInterphaseDelay);
    interphaseDelaySpinBox->setEnabled(enableStimCheckBox->isChecked() && stimShapeComboBox->currentIndex() == BiphasicWithInterphaseDelay);
    firstPhaseAmplitudeLabel->setEnabled(enableStimCheckBox->isChecked());
    firstPhaseAmplitudeSpinBox->setEnabled(enableStimCheckBox->isChecked());
    secondPhaseAmplitudeLabel->setEnabled(enableStimCheckBox->isChecked() && stimShapeComboBox->currentIndex() != Monophasic);
    secondPhaseAmplitudeSpinBox->setEnabled(enableStimCheckBox->isChecked() && stimShapeComboBox->currentIndex() != Monophasic);
    baselineVoltageLabel->setEnabled(enableStimCheckBox->isChecked());
    baselineVoltageSpinBox->setEnabled(enableStimCheckBox->isChecked());

    // Reset text for first phase labels
    if (stimShapeComboBox->currentIndex() != Triphasic) {
        firstPhaseDurationLabel->setText(tr("First Phase Duration (D1):"));
        firstPhaseAmplitudeLabel->setText(tr("First Phase Amplitude (A1):"));
    } else {
        firstPhaseDurationLabel->setText(tr("First/Third Phase Duration (D1):"));
        firstPhaseAmplitudeLabel->setText(tr("First/Third Phase Amplitude (A1):"));
    }
}

void AnOutDialog::calculatePulseTrainFrequency()
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

// Constrain pulseTrainPeriod's lowest possible value to the sum of the durations of active phases.
void AnOutDialog::constrainPulseTrainPeriod()
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

void AnOutDialog::roundTimeInputs()
{
    // Round all time inputs to the nearest multiple of timestep.
    firstPhaseDurationSpinBox->roundValue();
    secondPhaseDurationSpinBox->roundValue();
    interphaseDelaySpinBox->roundValue();
    postTriggerDelaySpinBox->roundValue();
    pulseTrainPeriodSpinBox->roundValue();
    refractoryPeriodSpinBox->roundValue();
}

void AnOutDialog::roundVoltageInputs()
{
    // Round all voltage inputs to the nearest multiple of voltagestep.
    firstPhaseAmplitudeSpinBox->roundValue();
    secondPhaseAmplitudeSpinBox->roundValue();
    baselineVoltageSpinBox->roundValue();
}

void AnOutDialog::updateMonophasicAndPositive()
{
    emit monophasicAndPositiveSignalChanged(stimShapeComboBox->currentIndex() == Monophasic &&
                                            stimPolarityComboBox->currentIndex() == PositiveFirst);
}

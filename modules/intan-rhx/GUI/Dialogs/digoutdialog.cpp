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

#include <QtWidgets>
#include "signalsources.h"
#include "smartspinbox.h"
#include "digfigure.h"
#include "digoutdialog.h"

DigOutDialog::DigOutDialog(SystemState* state_, Channel* channel_, QWidget *parent) :
    QDialog(parent),
    state(state_),
    channel(channel_)
{
    parameters = channel->stimParameters;
    timestep = 1.0e6 / state->sampleRate->getNumericValue();  // time step in microseconds

    digFigure = new DigFigure(parameters, this);

    QGroupBox* triggerGroupBox = new QGroupBox(tr("Trigger"), this);

    enablePulseCheckBox = new QCheckBox(tr("Enable"), this);

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

    postTriggerDelaySpinBox = new TimeSpinBox(timestep, this);
    postTriggerDelaySpinBox->setRange(0, 500000);
    postTriggerDelayLabel = new QLabel(tr("Post Trigger Delay:"), this);

    QGroupBox* pulseGroupBox = new QGroupBox(tr("Pulse"), this);

    pulseDurationSpinBox = new TimeSpinBox(timestep, this);
    pulseDurationSpinBox->setRange(0, 100000);
    pulseDurationLabel = new QLabel(tr("Pulse Duration (D1):"), this);

    pulseRepetitionComboBox = new QComboBox(this);
    QStringList pulseRepetitions;
    pulseRepetitions << tr("Single Pulse") << tr("Pulse Train");
    pulseRepetitionComboBox->addItems(pulseRepetitions);
    pulseRepetitionLabel = new QLabel(tr("Pulse Repetition:"), this);

    numPulsesSpinBox = new QSpinBox(this);
    numPulsesSpinBox->setRange(2, 256);
    numPulsesLabel = new QLabel(tr("Number of Pulses:"), this);

    pulseTrainPeriodSpinBox = new TimeSpinBox(timestep, this);
    pulseTrainPeriodSpinBox->setRange(0, 1000000);
    pulseTrainPeriodLabel = new QLabel(tr("Pulse Train Period:"), this);

    pulseTrainFrequencyLabel = new QLabel(tr("Pulse Train Frequency: -- Hz"), this);

    refractoryPeriodSpinBox = new TimeSpinBox(timestep, this);
    refractoryPeriodSpinBox->setRange(0, 1000000);
    refractoryPeriodLabel = new QLabel(tr("Refractory Period:"), this);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    // Connect internal signals and slots (for rounding and graying out).
    connect(enablePulseCheckBox, SIGNAL(stateChanged(int)), this, SLOT(enableWidgets()));
    connect(pulseRepetitionComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(enableWidgets()));
    connect(postTriggerDelaySpinBox, SIGNAL(editingFinished()), this, SLOT(roundTimeInputs()));
    connect(pulseDurationSpinBox, SIGNAL(editingFinished()), this, SLOT(roundTimeInputs()));
    connect(pulseTrainPeriodSpinBox, SIGNAL(editingFinished()), this, SLOT(roundTimeInputs()));
    connect(refractoryPeriodSpinBox, SIGNAL(editingFinished()), this, SLOT(roundTimeInputs()));
    connect(pulseTrainPeriodSpinBox, SIGNAL(valueChanged(double)), this, SLOT(calculatePulseTrainFrequency()));
    connect(pulseDurationSpinBox, SIGNAL(valueChanged(double)), this, SLOT(constrainPulseTrainPeriod()));
    connect(enablePulseCheckBox, SIGNAL(toggled(bool)), digFigure, SLOT(highlightStimTrace(bool)));

    // Connect signals to stimFigure's non-highlight slots.
    connect(pulseRepetitionComboBox, SIGNAL(currentIndexChanged(int)), digFigure, SLOT(updatePulseOrTrain(int)));

    // Connect signals to digFigure's highlight slots.
    connect(this, SIGNAL(highlightFirstPhaseDuration(bool)), digFigure, SLOT(highlightFirstPhaseDuration(bool)));
    connect(this, SIGNAL(highlightPostTriggerDelay(bool)), digFigure, SLOT(highlightPostTriggerDelay(bool)));
    connect(this, SIGNAL(highlightPulseTrainPeriod(bool)), digFigure, SLOT(highlightPulseTrainPeriod(bool)));
    connect(this, SIGNAL(highlightRefractoryPeriod(bool)), digFigure, SLOT(highlightRefractoryPeriod(bool)));

    // Update widgets' states to be different than the default states, so state changes are detected properly.
    enablePulseCheckBox->setChecked(true);
    pulseRepetitionComboBox->setCurrentIndex(SinglePulse);

    // Update dialog's state based on structParameters
    loadParameters(parameters);

    // Trigger information widgets' layout
    QHBoxLayout *enablePulseRow = new QHBoxLayout;
    enablePulseRow->addWidget(enablePulseCheckBox);

    QHBoxLayout *triggerSourceRow = new QHBoxLayout;
    triggerSourceRow->addWidget(triggerSourceLabel);
    triggerSourceRow->addStretch();
    triggerSourceRow->addWidget(triggerSourceComboBox);

    QHBoxLayout *triggerEdgeOrLevelRow = new QHBoxLayout;
    triggerEdgeOrLevelRow->addWidget(triggerEdgeOrLevelLabel);
    triggerEdgeOrLevelRow->addWidget(triggerEdgeOrLevelComboBox);

    QHBoxLayout *triggerHighOrLowRow = new QHBoxLayout;
    triggerHighOrLowRow->addWidget(triggerHighOrLowLabel);
    triggerHighOrLowRow->addWidget(triggerHighOrLowComboBox);

    QHBoxLayout *postTriggerDelayRow = new QHBoxLayout;
    postTriggerDelayRow->addWidget(postTriggerDelayLabel);
    postTriggerDelayRow->addStretch();
    postTriggerDelayRow->addWidget(postTriggerDelaySpinBox);

    QVBoxLayout *triggerLayout = new QVBoxLayout;
    triggerLayout->addLayout(enablePulseRow);
    triggerLayout->addLayout(triggerSourceRow);
    triggerLayout->addStretch();
    triggerLayout->addLayout(triggerEdgeOrLevelRow);
    triggerLayout->addStretch();
    triggerLayout->addLayout(triggerHighOrLowRow);
    triggerLayout->addStretch();
    triggerLayout->addLayout(postTriggerDelayRow);
    triggerGroupBox->setLayout(triggerLayout);

    // Pulse train information widgets' layout
    QHBoxLayout *pulseDurationRow = new QHBoxLayout;
    pulseDurationRow->addWidget(pulseDurationLabel);
    pulseDurationRow->addStretch();
    pulseDurationRow->addWidget(pulseDurationSpinBox);

    QHBoxLayout *pulseRepetitionRow = new QHBoxLayout;
    pulseRepetitionRow->addWidget(pulseRepetitionLabel);
    pulseRepetitionRow->addStretch();
    pulseRepetitionRow->addWidget(pulseRepetitionComboBox);

    QHBoxLayout *numPulsesRow = new QHBoxLayout;
    numPulsesRow->addWidget(numPulsesLabel);
    numPulsesRow->addStretch();
    numPulsesRow->addWidget(numPulsesSpinBox);

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

    QVBoxLayout *pulseLayout = new QVBoxLayout;
    pulseLayout->addLayout(pulseDurationRow);
    pulseLayout->addLayout(pulseRepetitionRow);
    pulseLayout->addLayout(numPulsesRow);
    pulseLayout->addLayout(pulseTrainPeriodRow);
    pulseLayout->addLayout(pulseTrainFrequencyRow);
    pulseLayout->addLayout(refractoryPeriodRow);
    pulseGroupBox->setLayout(pulseLayout);

    QVBoxLayout *firstColumn = new QVBoxLayout;
    firstColumn->addWidget(triggerGroupBox);
    firstColumn->addStretch();

    QVBoxLayout *secondColumn = new QVBoxLayout;
    secondColumn->addWidget(pulseGroupBox);

    QHBoxLayout *finalRow = new QHBoxLayout;
    finalRow->addWidget(buttonBox);

    QHBoxLayout *columns = new QHBoxLayout;
    columns->addLayout(firstColumn);
    columns->addLayout(secondColumn);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(digFigure);
    mainLayout->addLayout(columns);
    mainLayout->addLayout(finalRow);

    QWidget *mainWidget = new QWidget(this);
    mainWidget->setLayout(mainLayout);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(mainWidget);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QVBoxLayout *scrollLayout = new QVBoxLayout;
    scrollLayout->addWidget(scrollArea);

    setLayout(scrollLayout);

    // Set dialog initial size to 10% larger than scrollArea's sizeHint - should avoid scroll bars for default size.
    int initialWidth = round(mainWidget->sizeHint().width() * 1.1);
    int initialHeight = round(mainWidget->sizeHint().height() * 1.1);
    resize(initialWidth, initialHeight);

    setWindowTitle("Digital Output Parameters: " + channel->getNativeName() + " (" + channel->getCustomName() + ")");
}

void DigOutDialog::loadParameters(StimParameters *parameters)
{
    // Set zero values initially to bring all ranges to microseconds, so values can be set properly.
    postTriggerDelaySpinBox->setValue(0);
    pulseDurationSpinBox->setValue(0);
    pulseTrainPeriodSpinBox->setValue(0);
    refractoryPeriodSpinBox->setValue(0);

    // Load parameters.
    enablePulseCheckBox->setChecked(parameters->enabled->getValue());
    triggerSourceComboBox->setCurrentIndex(parameters->triggerSource->getIndex());
    triggerEdgeOrLevelComboBox->setCurrentIndex(parameters->triggerEdgeOrLevel->getIndex());
    triggerHighOrLowComboBox->setCurrentIndex(parameters->triggerHighOrLow->getIndex());
    postTriggerDelaySpinBox->loadValue(parameters->postTriggerDelay->getValue());
    pulseDurationSpinBox->loadValue(parameters->firstPhaseDuration->getValue());
    pulseRepetitionComboBox->setCurrentIndex(parameters->pulseOrTrain->getIndex());
    numPulsesSpinBox->setValue(parameters->numberOfStimPulses->getValue());
    pulseTrainPeriodSpinBox->loadValue(parameters->pulseTrainPeriod->getValue());
    refractoryPeriodSpinBox->loadValue(parameters->refractoryPeriod->getValue());

    // Calculate frequency so that the first label that is displayed corresponds to the loaded parameters.
    calculatePulseTrainFrequency();

    // Constrain pulse period so that when first loaded, the widget has a proper minimum.
    constrainPulseTrainPeriod();
}

void DigOutDialog::accept()
{
    // Save the values of the parameters from the dialog box into the object.
    parameters->enabled->setValue(enablePulseCheckBox->isChecked());
    parameters->triggerSource->setIndex(triggerSourceComboBox->currentIndex());
    parameters->triggerEdgeOrLevel->setIndex(triggerEdgeOrLevelComboBox->currentIndex());
    parameters->triggerHighOrLow->setIndex(triggerHighOrLowComboBox->currentIndex());
    parameters->postTriggerDelay->setValue(postTriggerDelaySpinBox->getTrueValue());
    parameters->firstPhaseDuration->setValue(pulseDurationSpinBox->getTrueValue());
    parameters->pulseOrTrain->setIndex(pulseRepetitionComboBox->currentIndex());
    parameters->numberOfStimPulses->setValue(numPulsesSpinBox->value());
    parameters->pulseTrainPeriod->setValue(pulseTrainPeriodSpinBox->getTrueValue());
    parameters->refractoryPeriod->setValue(refractoryPeriodSpinBox->getTrueValue());

    // Close the window.
    done(Accepted);
}

void DigOutDialog::notifyFocusChanged(QWidget *lostFocus, QWidget *gainedFocus)
{
    // Emit signals when a widget loses focus.
    if (lostFocus == pulseDurationSpinBox->pointer()) emit highlightFirstPhaseDuration(false);
    if (lostFocus == postTriggerDelaySpinBox->pointer()) emit highlightPostTriggerDelay(false);
    if (lostFocus == pulseTrainPeriodSpinBox->pointer()) emit highlightPulseTrainPeriod(false);
    if (lostFocus == refractoryPeriodSpinBox->pointer()) emit highlightRefractoryPeriod(false);

    // Emit signals when a widget gains focus.
    if (gainedFocus == pulseDurationSpinBox->pointer()) emit highlightFirstPhaseDuration(true);
    if (gainedFocus == postTriggerDelaySpinBox->pointer()) emit highlightPostTriggerDelay(true);
    if (gainedFocus == pulseTrainPeriodSpinBox->pointer()) emit highlightPulseTrainPeriod(true);
    if (gainedFocus == refractoryPeriodSpinBox->pointer()) emit highlightRefractoryPeriod(true);
}

void DigOutDialog::calculatePulseTrainFrequency()
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

void DigOutDialog::roundTimeInputs()
{
    // Round all time inputs to the nearest multiple of timestep.
    postTriggerDelaySpinBox->roundValue();
    pulseDurationSpinBox->roundValue();
    pulseTrainPeriodSpinBox->roundValue();
    refractoryPeriodSpinBox->roundValue();

}

void DigOutDialog::enableWidgets()
{
    // Trigger Group Box
    triggerSourceLabel->setEnabled(enablePulseCheckBox->isChecked());
    triggerSourceComboBox->setEnabled(enablePulseCheckBox->isChecked());
    triggerEdgeOrLevelComboBox->setEnabled(enablePulseCheckBox->isChecked());
    triggerHighOrLowComboBox->setEnabled(enablePulseCheckBox->isChecked());
    postTriggerDelayLabel->setEnabled(enablePulseCheckBox->isChecked());
    postTriggerDelaySpinBox->setEnabled(enablePulseCheckBox->isChecked());

    // Pulse Group Box
    pulseDurationLabel->setEnabled(enablePulseCheckBox->isChecked());
    pulseDurationSpinBox->setEnabled(enablePulseCheckBox->isChecked());
    pulseRepetitionLabel->setEnabled(enablePulseCheckBox->isChecked());
    pulseRepetitionComboBox->setEnabled(enablePulseCheckBox->isChecked());
    numPulsesLabel->setEnabled(enablePulseCheckBox->isChecked() && pulseRepetitionComboBox->currentIndex() == PulseTrain);
    numPulsesSpinBox->setEnabled(enablePulseCheckBox->isChecked() && pulseRepetitionComboBox->currentIndex() == PulseTrain);
    pulseTrainPeriodLabel->setEnabled(enablePulseCheckBox->isChecked() && pulseRepetitionComboBox->currentIndex() == PulseTrain);
    pulseTrainPeriodSpinBox->setEnabled(enablePulseCheckBox->isChecked() && pulseRepetitionComboBox->currentIndex() == PulseTrain);
    pulseTrainFrequencyLabel->setEnabled(enablePulseCheckBox->isChecked() && pulseRepetitionComboBox->currentIndex() == PulseTrain);
    refractoryPeriodLabel->setEnabled(enablePulseCheckBox->isChecked());
    refractoryPeriodSpinBox->setEnabled(enablePulseCheckBox->isChecked());
}

void DigOutDialog::constrainPulseTrainPeriod()
{
    pulseTrainPeriodSpinBox->setTrueMinimum(pulseDurationSpinBox->getTrueValue());
}

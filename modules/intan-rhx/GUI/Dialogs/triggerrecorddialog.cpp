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

#include "triggerrecorddialog.h"

// Triggered recording dialog - this allows users to select a digital input channel for a triggered recording session.
TriggerRecordDialog::TriggerRecordDialog(SystemState* state_, QWidget *parent) :
    QDialog(parent)
{
    state = state_;

    setWindowTitle(tr("Episodic Triggered Recording Control"));

    QLabel *label1 = new QLabel(tr("Digital or analog inputs lines may be used to trigger "
                                   "recording."), this);
    label1->setWordWrap(true);

    digitalInputComboBox = new QComboBox(this);
    state->triggerSource->setupComboBox(digitalInputComboBox);
    digitalInput = state->triggerSource->getIndex();

    connect(digitalInputComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setDigitalInput(int)));

    triggerPolarityComboBox = new QComboBox(this);
    state->triggerPolarity->setupComboBox(triggerPolarityComboBox);
    triggerPolarity = state->triggerPolarity->getIndex();

    connect(triggerPolarityComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(setTriggerPolarity(int)));

    triggerSaveCheckBox = new QCheckBox(tr("Automatically Save Trigger Channel"), this);
    triggerSaveCheckBox->setChecked(state->saveTriggerSource->getValue());

    QVBoxLayout *triggerControls = new QVBoxLayout;
    triggerControls->addWidget(digitalInputComboBox);
    triggerControls->addWidget(triggerPolarityComboBox);
    triggerControls->addWidget(triggerSaveCheckBox);

    QHBoxLayout *triggerHBox = new QHBoxLayout;
    triggerHBox->addLayout(triggerControls);
    triggerHBox->addStretch(1);

    QVBoxLayout *triggerLayout = new QVBoxLayout;
    triggerLayout->addWidget(label1);
    triggerLayout->addLayout(triggerHBox);

    QGroupBox *triggerGroupBox = new QGroupBox(tr("Trigger Source"), this);
    triggerGroupBox->setLayout(triggerLayout);

    QHBoxLayout *triggerHLayout = new QHBoxLayout;
    triggerHLayout->addWidget(triggerGroupBox);

    recordBufferSpinBox = new QSpinBox(this);
    state->preTriggerBuffer->setupSpinBox(recordBufferSpinBox);

    QHBoxLayout *bufferSpinBoxLayout = new QHBoxLayout;
    bufferSpinBoxLayout->addWidget(recordBufferSpinBox);
    bufferSpinBoxLayout->addWidget(new QLabel(tr("seconds"), this));
    bufferSpinBoxLayout->addStretch(1);

    QLabel *label2 = new QLabel(tr("If a pretrigger buffer size of N seconds is selected, "
                                   "slightly more than N seconds of pretrigger data will be "
                                   "saved to disk when a trigger is detected, assuming that "
                                   "data acquisition has been running for at least N seconds."), this);
    label2->setWordWrap(true);

    QVBoxLayout *bufferSelectLayout = new QVBoxLayout;
    bufferSelectLayout->addWidget(new QLabel(tr("Pretrigger data saved (range: 1-30 seconds):"), this));
    bufferSelectLayout->addLayout(bufferSpinBoxLayout);
    bufferSelectLayout->addWidget(label2);

    QGroupBox *bufferGroupBox = new QGroupBox(tr("Pretrigger Buffer"), this);
    bufferGroupBox->setLayout(bufferSelectLayout);

    QHBoxLayout *bufferHLayout = new QHBoxLayout;
    bufferHLayout->addWidget(bufferGroupBox);

    postTriggerSpinBox = new QSpinBox(this);
    state->postTriggerBuffer->setupSpinBox(postTriggerSpinBox);

    QHBoxLayout *postTriggerSpinBoxLayout = new QHBoxLayout;
    postTriggerSpinBoxLayout->addWidget(postTriggerSpinBox);
    postTriggerSpinBoxLayout->addWidget(new QLabel(tr("seconds"), this));
    postTriggerSpinBoxLayout->addStretch(1);

    QLabel *label4 = new QLabel(tr("If a posttrigger time of M seconds is selected, "
                                   "slightly more than M seconds of data will be "
                                   "saved to disk after the trigger is de-asserted."), this);
    label4->setWordWrap(true);

    QVBoxLayout *postTriggerSelectLayout = new QVBoxLayout;
    postTriggerSelectLayout->addWidget(new QLabel(tr("Posttrigger data saved (range: 1-9999 seconds):"), this));
    postTriggerSelectLayout->addLayout(postTriggerSpinBoxLayout);
    postTriggerSelectLayout->addWidget(label4);

    QGroupBox *postTriggerGroupBox = new QGroupBox(tr("Posttrigger Buffer"), this);
    postTriggerGroupBox->setLayout(postTriggerSelectLayout);

    QHBoxLayout *postTriggerHLayout = new QHBoxLayout;
    postTriggerHLayout->addWidget(postTriggerGroupBox);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QLabel *label3 = new QLabel(tr("Press OK to start triggered recording with selected settings.  "
                                   "Waveforms will be displayed in real time, but recording will "
                                   "not start until the trigger is detected.  A tone will indicate "
                                   "when the trigger has been detected.  A different tone indicates "
                                   "that recording has stopped after a trigger has been de-asserted.  "
                                   "Successive trigger events will create new saved data files.  "
                                   "Press the Stop button to exit triggered recording mode."), this);
    label3->setWordWrap(true);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(triggerHLayout);
    mainLayout->addLayout(bufferHLayout);
    mainLayout->addLayout(postTriggerHLayout);
    mainLayout->addWidget(label3);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
}

void TriggerRecordDialog::updateFromState()
{
    digitalInputComboBox->setCurrentIndex(state->triggerSource->getIndex());
    triggerPolarityComboBox->setCurrentIndex(state->triggerPolarity->getIndex());
    recordBufferSpinBox->setValue(state->preTriggerBuffer->getValue());
    postTriggerSpinBox->setValue(state->postTriggerBuffer->getValue());
    triggerSaveCheckBox->setChecked(state->saveTriggerSource->getValue());
}

QString TriggerRecordDialog::getTriggerSave()
{
    return (triggerSaveCheckBox->isChecked() ? "True" : "False");
}

QString TriggerRecordDialog::getTriggerInput()
{
    return state->triggerSource->getValue(digitalInput);
}

QString TriggerRecordDialog::getTriggerPolarity()
{
    return state->triggerPolarity->getValue(triggerPolarity);
}

QString TriggerRecordDialog::getRecordBuffer()
{
    return QString::number(recordBufferSpinBox->value());
}

QString TriggerRecordDialog::getPostTriggerBufferSeconds()
{
    return QString::number(postTriggerSpinBox->value());
}

void TriggerRecordDialog::setDigitalInput(int index)
{
    digitalInput = index;
}

void TriggerRecordDialog::setTriggerPolarity(int index)
{
    triggerPolarity = index;
}

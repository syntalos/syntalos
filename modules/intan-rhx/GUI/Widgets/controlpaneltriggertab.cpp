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

#include "controlpaneltriggertab.h"

ControlPanelTriggerTab::ControlPanelTriggerTab(ControllerInterface* controllerInterface_, SystemState* state_,
                                               QWidget *parent) :
    QWidget(parent),
    state(state_),
    controllerInterface(controllerInterface_),
    triggerEnableCheckBox(nullptr),
    triggerSourceComboBox(nullptr),
    triggerPolarityComboBox(nullptr),
    triggerPositionComboBox(nullptr)
{
    triggerEnableCheckBox = new QCheckBox(tr("Enable"), this);
    connect(triggerEnableCheckBox, SIGNAL(clicked(bool)), this, SLOT(enableTrigger(bool)));

    triggerSourceComboBox = new QComboBox(this);
    state->triggerSourceDisplay->setupComboBox(triggerSourceComboBox);
    connect(triggerSourceComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setTriggerSource(int)));

    triggerPolarityComboBox = new QComboBox(this);
    state->triggerPolarityDisplay->setupComboBox(triggerPolarityComboBox);
    connect(triggerPolarityComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setTriggerPolarity(int)));

    triggerPositionComboBox = new QComboBox(this);
    state->triggerPositionDisplay->setupComboBox(triggerPositionComboBox);
    connect(triggerPositionComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setTriggerPosition(int)));

    QHBoxLayout *triggerLayout1 = new QHBoxLayout;
    triggerLayout1->addWidget(new QLabel(tr("Trigger Source"), this));
    triggerLayout1->addWidget(triggerSourceComboBox);
    triggerLayout1->addStretch(1);

    QHBoxLayout *triggerLayout2 = new QHBoxLayout;
    triggerLayout2->addWidget(new QLabel(tr("Trigger Polarity"), this));
    triggerLayout2->addWidget(triggerPolarityComboBox);
    triggerLayout2->addStretch(1);

    QHBoxLayout *triggerLayout3 = new QHBoxLayout;
    triggerLayout3->addWidget(new QLabel(tr("Trigger Position"), this));
    triggerLayout3->addWidget(triggerPositionComboBox);
    triggerLayout3->addStretch(1);

    QVBoxLayout *triggerLayout = new QVBoxLayout;
    triggerLayout->addWidget(triggerEnableCheckBox);
    triggerLayout->addLayout(triggerLayout1);
    triggerLayout->addLayout(triggerLayout2);
    triggerLayout->addLayout(triggerLayout3);

    QGroupBox *triggerGroupBox = new QGroupBox(tr("Display Trigger"), this);
    triggerGroupBox->setLayout(triggerLayout);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(triggerGroupBox);
    mainLayout->addStretch(1);
    setLayout(mainLayout);
}

void ControlPanelTriggerTab::updateFromState()
{
    triggerEnableCheckBox->setChecked(state->triggerModeDisplay->getValue());
    triggerSourceComboBox->setCurrentIndex(state->triggerSourceDisplay->getIndex());
    triggerPolarityComboBox->setCurrentIndex(state->triggerPolarityDisplay->getIndex());
    triggerPositionComboBox->setCurrentIndex(state->triggerPositionDisplay->getIndex());
}

void ControlPanelTriggerTab::enableTrigger(bool enable)
{
    if (enable) state->rollMode->setValue(false);   // Triggering display only works in sweep mode.
    state->triggerModeDisplay->setValue(enable);
}

void ControlPanelTriggerTab::setTriggerSource(int index)
{
    state->triggerSourceDisplay->setIndex(index);
}

void ControlPanelTriggerTab::setTriggerPolarity(int index)
{
    state->triggerPolarityDisplay->setIndex(index);
}

void ControlPanelTriggerTab::setTriggerPosition(int index)
{
    state->triggerPositionDisplay->setIndex(index);
}

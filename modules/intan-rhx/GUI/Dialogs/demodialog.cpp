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

#include "demodialog.h"

DemoDialog::DemoDialog(DemoSelections *demoSelection_, QWidget *parent) :
    QDialog(parent),
    demoSelection(demoSelection_),
    message(nullptr),
    usbInterfaceButton(nullptr),
    recordControllerButton(nullptr),
    stimControllerButton(nullptr),
    playbackButton(nullptr)
{
    message = new QLabel(tr("No Intan controllers have been detected. Ensure devices are powered on and connected to this machine.\n"
                            "You may also run this software in demonstration mode or play back a saved data file."));

    usbInterfaceButton = new QPushButton(tr("RHD USB Interface Board Demo"), this);
    recordControllerButton = new QPushButton(tr("RHD Recording Controller Demo"), this);
    stimControllerButton = new QPushButton(tr("RHS Stim/Record Controller Demo"), this);
    playbackButton = new QPushButton(tr("Data File Playback"));

    stimControllerButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    int buttonWidth = stimControllerButton->sizeHint().width() + 10;
    stimControllerButton->setFixedWidth(buttonWidth);
    usbInterfaceButton->setFixedWidth(buttonWidth);
    recordControllerButton->setFixedWidth(buttonWidth);
    playbackButton->setFixedWidth(buttonWidth);

    connect(usbInterfaceButton, SIGNAL(clicked(bool)), this, SLOT(usbInterface()));
    connect(recordControllerButton, SIGNAL(clicked(bool)), this, SLOT(recordController()));
    connect(stimControllerButton, SIGNAL(clicked(bool)), this, SLOT(stimController()));
    connect(playbackButton, SIGNAL(clicked(bool)), this, SLOT(playback()));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(message);
    mainLayout->addWidget(usbInterfaceButton);
    mainLayout->addWidget(recordControllerButton);
    mainLayout->addWidget(stimControllerButton);
    mainLayout->addSpacing(8);
    mainLayout->addWidget(playbackButton);

    mainLayout->setAlignment(usbInterfaceButton, Qt::AlignHCenter);
    mainLayout->setAlignment(recordControllerButton, Qt::AlignHCenter);
    mainLayout->setAlignment(stimControllerButton, Qt::AlignHCenter);
    mainLayout->setAlignment(playbackButton, Qt::AlignHCenter);

    setWindowTitle(tr("No Intan Controllers Detected"));
    setLayout(mainLayout);
}

void DemoDialog::closeEvent(QCloseEvent *)
{
    exit(EXIT_FAILURE);
}

void DemoDialog::usbInterface()
{
    *demoSelection = DemoUSBInterfaceBoard;
    accept();
}

void DemoDialog::recordController()
{
    *demoSelection = DemoRecordingController;
    accept();
}

void DemoDialog::stimController()
{
    *demoSelection = DemoStimRecordController;
    accept();
}

void DemoDialog::playback()
{
    *demoSelection = DemoPlayback;
    accept();
}

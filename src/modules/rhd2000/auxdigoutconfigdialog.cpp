//  ------------------------------------------------------------------------
//
//  This file is part of the Intan Technologies RHD2000 Interface
//  Version 1.4
//  Copyright (C) 2013 Intan Technologies
//
//  ------------------------------------------------------------------------
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <QtGui>
#include <iostream>

#include "qtincludes.h"

#include "auxdigoutconfigdialog.h"

using namespace std;

// Auxiliary digital output configuration dialog.
// This dialog allows users to configure real-time control of the auxiliary digital
// output pin (auxout) on each RHD2000 chip using selected digital input signals
// on the USB interface board.

AuxDigOutConfigDialog::AuxDigOutConfigDialog(QVector<bool> &auxOutEnabledIn, QVector<int> &auxOutChannelIn, QWidget *parent) :
    QDialog(parent)
{
    int port;

    auxOutEnabled.resize(4);
    auxOutChannel.resize(4);
    for (port = 0; port < 4; ++port) {
        auxOutEnabled[port] = auxOutEnabledIn[port];
        auxOutChannel[port] = auxOutChannelIn[port];
    }

    enablePortACheckBox = new QCheckBox(tr("Control auxiliary digital output on Port A from"));
    connect(enablePortACheckBox, SIGNAL(toggled(bool)), this, SLOT(enablePortAChanged(bool)));
    enablePortACheckBox->setChecked(auxOutEnabled[0]);

    enablePortBCheckBox = new QCheckBox(tr("Control auxiliary digital output on Port B from"));
    connect(enablePortBCheckBox, SIGNAL(toggled(bool)), this, SLOT(enablePortBChanged(bool)));
    enablePortBCheckBox->setChecked(auxOutEnabled[1]);

    enablePortCCheckBox = new QCheckBox(tr("Control auxiliary digital output on Port C from"));
    connect(enablePortCCheckBox, SIGNAL(toggled(bool)), this, SLOT(enablePortCChanged(bool)));
    enablePortCCheckBox->setChecked(auxOutEnabled[2]);

    enablePortDCheckBox = new QCheckBox(tr("Control auxiliary digital output on Port D from"));
    connect(enablePortDCheckBox, SIGNAL(toggled(bool)), this, SLOT(enablePortDChanged(bool)));
    enablePortDCheckBox->setChecked(auxOutEnabled[3]);

    channelPortAComboBox = new QComboBox();
    channelPortAComboBox->addItem(tr("Digital Input 0"));
    channelPortAComboBox->addItem(tr("Digital Input 1"));
    channelPortAComboBox->addItem(tr("Digital Input 2"));
    channelPortAComboBox->addItem(tr("Digital Input 3"));
    channelPortAComboBox->addItem(tr("Digital Input 4"));
    channelPortAComboBox->addItem(tr("Digital Input 5"));
    channelPortAComboBox->addItem(tr("Digital Input 6"));
    channelPortAComboBox->addItem(tr("Digital Input 7"));
    channelPortAComboBox->addItem(tr("Digital Input 8"));
    channelPortAComboBox->addItem(tr("Digital Input 9"));
    channelPortAComboBox->addItem(tr("Digital Input 10"));
    channelPortAComboBox->addItem(tr("Digital Input 11"));
    channelPortAComboBox->addItem(tr("Digital Input 12"));
    channelPortAComboBox->addItem(tr("Digital Input 13"));
    channelPortAComboBox->addItem(tr("Digital Input 14"));
    channelPortAComboBox->addItem(tr("Digital Input 15"));
    connect(channelPortAComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(channelPortAChanged(int)));
    channelPortAComboBox->setCurrentIndex(auxOutChannel[0]);

    channelPortBComboBox = new QComboBox();
    channelPortBComboBox->addItem(tr("Digital Input 0"));
    channelPortBComboBox->addItem(tr("Digital Input 1"));
    channelPortBComboBox->addItem(tr("Digital Input 2"));
    channelPortBComboBox->addItem(tr("Digital Input 3"));
    channelPortBComboBox->addItem(tr("Digital Input 4"));
    channelPortBComboBox->addItem(tr("Digital Input 5"));
    channelPortBComboBox->addItem(tr("Digital Input 6"));
    channelPortBComboBox->addItem(tr("Digital Input 7"));
    channelPortBComboBox->addItem(tr("Digital Input 8"));
    channelPortBComboBox->addItem(tr("Digital Input 9"));
    channelPortBComboBox->addItem(tr("Digital Input 10"));
    channelPortBComboBox->addItem(tr("Digital Input 11"));
    channelPortBComboBox->addItem(tr("Digital Input 12"));
    channelPortBComboBox->addItem(tr("Digital Input 13"));
    channelPortBComboBox->addItem(tr("Digital Input 14"));
    channelPortBComboBox->addItem(tr("Digital Input 15"));
    connect(channelPortBComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(channelPortBChanged(int)));
    channelPortBComboBox->setCurrentIndex(auxOutChannel[1]);

    channelPortCComboBox = new QComboBox();
    channelPortCComboBox->addItem(tr("Digital Input 0"));
    channelPortCComboBox->addItem(tr("Digital Input 1"));
    channelPortCComboBox->addItem(tr("Digital Input 2"));
    channelPortCComboBox->addItem(tr("Digital Input 3"));
    channelPortCComboBox->addItem(tr("Digital Input 4"));
    channelPortCComboBox->addItem(tr("Digital Input 5"));
    channelPortCComboBox->addItem(tr("Digital Input 6"));
    channelPortCComboBox->addItem(tr("Digital Input 7"));
    channelPortCComboBox->addItem(tr("Digital Input 8"));
    channelPortCComboBox->addItem(tr("Digital Input 9"));
    channelPortCComboBox->addItem(tr("Digital Input 10"));
    channelPortCComboBox->addItem(tr("Digital Input 11"));
    channelPortCComboBox->addItem(tr("Digital Input 12"));
    channelPortCComboBox->addItem(tr("Digital Input 13"));
    channelPortCComboBox->addItem(tr("Digital Input 14"));
    channelPortCComboBox->addItem(tr("Digital Input 15"));
    connect(channelPortCComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(channelPortCChanged(int)));
    channelPortCComboBox->setCurrentIndex(auxOutChannel[2]);

    channelPortDComboBox = new QComboBox();
    channelPortDComboBox->addItem(tr("Digital Input 0"));
    channelPortDComboBox->addItem(tr("Digital Input 1"));
    channelPortDComboBox->addItem(tr("Digital Input 2"));
    channelPortDComboBox->addItem(tr("Digital Input 3"));
    channelPortDComboBox->addItem(tr("Digital Input 4"));
    channelPortDComboBox->addItem(tr("Digital Input 5"));
    channelPortDComboBox->addItem(tr("Digital Input 6"));
    channelPortDComboBox->addItem(tr("Digital Input 7"));
    channelPortDComboBox->addItem(tr("Digital Input 8"));
    channelPortDComboBox->addItem(tr("Digital Input 9"));
    channelPortDComboBox->addItem(tr("Digital Input 10"));
    channelPortDComboBox->addItem(tr("Digital Input 11"));
    channelPortDComboBox->addItem(tr("Digital Input 12"));
    channelPortDComboBox->addItem(tr("Digital Input 13"));
    channelPortDComboBox->addItem(tr("Digital Input 14"));
    channelPortDComboBox->addItem(tr("Digital Input 15"));
    connect(channelPortDComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(channelPortDChanged(int)));
    channelPortDComboBox->setCurrentIndex(auxOutChannel[3]);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QHBoxLayout *portALayout = new QHBoxLayout;
    portALayout->addWidget(enablePortACheckBox);
    portALayout->addWidget(channelPortAComboBox);
    portALayout->addStretch(1);

    QHBoxLayout *portBLayout = new QHBoxLayout;
    portBLayout->addWidget(enablePortBCheckBox);
    portBLayout->addWidget(channelPortBComboBox);
    portBLayout->addStretch(1);

    QHBoxLayout *portCLayout = new QHBoxLayout;
    portCLayout->addWidget(enablePortCCheckBox);
    portCLayout->addWidget(channelPortCComboBox);
    portCLayout->addStretch(1);

    QHBoxLayout *portDLayout = new QHBoxLayout;
    portDLayout->addWidget(enablePortDCheckBox);
    portDLayout->addWidget(channelPortDComboBox);
    portDLayout->addStretch(1);

    QLabel *label1 = new QLabel(
                tr("All RHD2000 chips have an auxiliary digital output pin <b>auxout</b> that "
                   "can be controlled via the SPI interface.  This pin is brought out to a solder "
                   "point <b>DO</b> on some RHD2000 amplifier boards.  This dialog enables real-time "
                   "control of this pin from a user-selected digital input on the USB interface board.  "
                   "A logic signal on the selected digital input will control the selected <b>auxout</b> "
                   "pin with a latency of 4-5 amplifier sampling periods.  For example, if the sampling "
                   "frequency is 20 kS/s, the control latency will be 200-250 microseconds."));
    label1->setWordWrap(true);

    QLabel *label2 = new QLabel(
                tr("Note that the auxiliary output pin will only be controlled while data "
                   "acquisition is running, and will be pulled to ground when acquisition stops."));
    label2->setWordWrap(true);

    QLabel *label3 = new QLabel(
                tr("The <b>auxout</b> pin is capable of driving up to 2 mA of current from the 3.3V "
                   "supply.  An external transistor can be added for additional current drive or voltage "
                   "range."));
    label3->setWordWrap(true);

    QVBoxLayout *controlLayout = new QVBoxLayout;
    controlLayout->addLayout(portALayout);
    controlLayout->addLayout(portBLayout);
    controlLayout->addLayout(portCLayout);
    controlLayout->addLayout(portDLayout);

    QGroupBox *controlBox = new QGroupBox();
    controlBox->setLayout(controlLayout);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(label1);
    mainLayout->addWidget(controlBox);
    mainLayout->addWidget(label2);
    mainLayout->addWidget(label3);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);

    setWindowTitle(tr("Configure Auxiliary Digital Output Control"));
}

void AuxDigOutConfigDialog::enablePortAChanged(bool enable)
{
    auxOutEnabled[0] = enable;
}

void AuxDigOutConfigDialog::enablePortBChanged(bool enable)
{
    auxOutEnabled[1] = enable;
}

void AuxDigOutConfigDialog::enablePortCChanged(bool enable)
{
    auxOutEnabled[2] = enable;
}

void AuxDigOutConfigDialog::enablePortDChanged(bool enable)
{
    auxOutEnabled[3] = enable;
}

void AuxDigOutConfigDialog::channelPortAChanged(int channel)
{
    auxOutChannel[0] = channel;
}

void AuxDigOutConfigDialog::channelPortBChanged(int channel)
{
    auxOutChannel[1] = channel;
}

void AuxDigOutConfigDialog::channelPortCChanged(int channel)
{
    auxOutChannel[2] = channel;
}

void AuxDigOutConfigDialog::channelPortDChanged(int channel)
{
    auxOutChannel[3] = channel;
}

bool AuxDigOutConfigDialog::enabled(int port)
{
    return auxOutEnabled[port];
}

int AuxDigOutConfigDialog::channel(int port)
{
    return auxOutChannel[port];
}


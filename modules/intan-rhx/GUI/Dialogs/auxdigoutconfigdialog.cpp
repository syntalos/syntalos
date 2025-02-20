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
#include "auxdigoutconfigdialog.h"

AuxDigOutConfigDialog::AuxDigOutConfigDialog(std::vector<bool> &auxOutEnabledIn, std::vector<int> &auxOutChannelIn, int numPorts,
                                             QWidget *parent) :
    QDialog(parent)
{
    const int MaxNumSpiPorts = 8;
    auxOutEnabled.resize(MaxNumSpiPorts);
    auxOutChannel.resize(MaxNumSpiPorts);

    for (int port = 0; port < MaxNumSpiPorts; ++port) {
        auxOutEnabled[port] = auxOutEnabledIn[port];
        auxOutChannel[port] = auxOutChannelIn[port];
    }

    enablePortACheckBox = new QCheckBox(tr("Control auxiliary digital output on Port A from"), this);
    connect(enablePortACheckBox, SIGNAL(toggled(bool)), this, SLOT(enablePortAChanged(bool)));
    enablePortACheckBox->setChecked(auxOutEnabled[0]);

    enablePortBCheckBox = new QCheckBox(tr("Control auxiliary digital output on Port B from"), this);
    connect(enablePortBCheckBox, SIGNAL(toggled(bool)), this, SLOT(enablePortBChanged(bool)));
    enablePortBCheckBox->setChecked(auxOutEnabled[1]);

    enablePortCCheckBox = new QCheckBox(tr("Control auxiliary digital output on Port C from"), this);
    connect(enablePortCCheckBox, SIGNAL(toggled(bool)), this, SLOT(enablePortCChanged(bool)));
    enablePortCCheckBox->setChecked(auxOutEnabled[2]);

    enablePortDCheckBox = new QCheckBox(tr("Control auxiliary digital output on Port D from"), this);
    connect(enablePortDCheckBox, SIGNAL(toggled(bool)), this, SLOT(enablePortDChanged(bool)));
    enablePortDCheckBox->setChecked(auxOutEnabled[3]);

    channelPortAComboBox = new QComboBox(this);
    populatePortComboBox(channelPortAComboBox);
    connect(channelPortAComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(channelPortAChanged(int)));
    channelPortAComboBox->setCurrentIndex(auxOutChannel[0]);

    channelPortBComboBox = new QComboBox(this);
    populatePortComboBox(channelPortBComboBox);
    connect(channelPortBComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(channelPortBChanged(int)));
    channelPortBComboBox->setCurrentIndex(auxOutChannel[1]);

    channelPortCComboBox = new QComboBox(this);
    populatePortComboBox(channelPortCComboBox);
    connect(channelPortCComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(channelPortCChanged(int)));
    channelPortCComboBox->setCurrentIndex(auxOutChannel[2]);

    channelPortDComboBox = new QComboBox(this);
    populatePortComboBox(channelPortDComboBox);
    connect(channelPortDComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(channelPortDChanged(int)));
    channelPortDComboBox->setCurrentIndex(auxOutChannel[3]);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
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
                tr("All RHD amplifier chips have an auxiliary digital output pin <b>auxout</b> that "
                   "can be controlled via the SPI interface.  This pin is brought out to a solder "
                   "point <b>DO</b> on some RHD headstages.  This dialog enables real-time "
                   "control of this pin from a user-selected digital input on the Intan controller.  "
                   "A logic signal on the selected digital input will control the selected <b>auxout</b> "
                   "pin with a latency of 4-5 amplifier sampling periods."), this);
    label1->setWordWrap(true);

    QLabel *label2 = new QLabel(
                tr("Note that the auxiliary output pin will only be controlled while data "
                   "acquisition is running, and will be pulled to ground when acquisition stops."), this);
    label2->setWordWrap(true);

    QLabel *label3 = new QLabel(
                tr("The <b>auxout</b> pin is capable of driving up to 2 mA of current from the 3.3V "
                   "supply.  An external transistor can be added for additional current drive or voltage "
                   "range."), this);
    label3->setWordWrap(true);

    QVBoxLayout *controlLayout = new QVBoxLayout;
    controlLayout->addLayout(portALayout);
    controlLayout->addLayout(portBLayout);
    controlLayout->addLayout(portCLayout);
    controlLayout->addLayout(portDLayout);

    if (numPorts == 8) {
        enablePortECheckBox = new QCheckBox(tr("Control auxiliary digital output on Port E from"), this);
        connect(enablePortECheckBox, SIGNAL(toggled(bool)), this, SLOT(enablePortEChanged(bool)));
        enablePortECheckBox->setChecked(auxOutEnabled[4]);

        enablePortFCheckBox = new QCheckBox(tr("Control auxiliary digital output on Port F from"), this);
        connect(enablePortFCheckBox, SIGNAL(toggled(bool)), this, SLOT(enablePortFChanged(bool)));
        enablePortFCheckBox->setChecked(auxOutEnabled[5]);

        enablePortGCheckBox = new QCheckBox(tr("Control auxiliary digital output on Port G from"), this);
        connect(enablePortGCheckBox, SIGNAL(toggled(bool)), this, SLOT(enablePortGChanged(bool)));
        enablePortGCheckBox->setChecked(auxOutEnabled[6]);

        enablePortHCheckBox = new QCheckBox(tr("Control auxiliary digital output on Port H from"), this);
        connect(enablePortHCheckBox, SIGNAL(toggled(bool)), this, SLOT(enablePortHChanged(bool)));
        enablePortHCheckBox->setChecked(auxOutEnabled[7]);

        channelPortEComboBox = new QComboBox(this);
        populatePortComboBox(channelPortEComboBox);
        connect(channelPortEComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(channelPortEChanged(int)));
        channelPortEComboBox->setCurrentIndex(auxOutChannel[4]);

        channelPortFComboBox = new QComboBox(this);
        populatePortComboBox(channelPortFComboBox);
        connect(channelPortFComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(channelPortFChanged(int)));
        channelPortFComboBox->setCurrentIndex(auxOutChannel[5]);

        channelPortGComboBox = new QComboBox(this);
        populatePortComboBox(channelPortGComboBox);
        connect(channelPortGComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(channelPortGChanged(int)));
        channelPortGComboBox->setCurrentIndex(auxOutChannel[6]);

        channelPortHComboBox = new QComboBox(this);
        populatePortComboBox(channelPortHComboBox);
        connect(channelPortHComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(channelPortHChanged(int)));
        channelPortHComboBox->setCurrentIndex(auxOutChannel[7]);

        QHBoxLayout *portELayout = new QHBoxLayout;
        portELayout->addWidget(enablePortECheckBox);
        portELayout->addWidget(channelPortEComboBox);
        portELayout->addStretch(1);

        QHBoxLayout *portFLayout = new QHBoxLayout;
        portFLayout->addWidget(enablePortFCheckBox);
        portFLayout->addWidget(channelPortFComboBox);
        portFLayout->addStretch(1);

        QHBoxLayout *portGLayout = new QHBoxLayout;
        portGLayout->addWidget(enablePortGCheckBox);
        portGLayout->addWidget(channelPortGComboBox);
        portGLayout->addStretch(1);

        QHBoxLayout *portHLayout = new QHBoxLayout;
        portHLayout->addWidget(enablePortHCheckBox);
        portHLayout->addWidget(channelPortHComboBox);
        portHLayout->addStretch(1);

        controlLayout->addLayout(portELayout);
        controlLayout->addLayout(portFLayout);
        controlLayout->addLayout(portGLayout);
        controlLayout->addLayout(portHLayout);
    }

    QGroupBox *controlBox = new QGroupBox(this);
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

void AuxDigOutConfigDialog::enablePortEChanged(bool enable)
{
    auxOutEnabled[4] = enable;
}

void AuxDigOutConfigDialog::enablePortFChanged(bool enable)
{
    auxOutEnabled[5] = enable;
}

void AuxDigOutConfigDialog::enablePortGChanged(bool enable)
{
    auxOutEnabled[6] = enable;
}

void AuxDigOutConfigDialog::enablePortHChanged(bool enable)
{
    auxOutEnabled[7] = enable;
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

void AuxDigOutConfigDialog::channelPortEChanged(int channel)
{
    auxOutChannel[4] = channel;
}

void AuxDigOutConfigDialog::channelPortFChanged(int channel)
{
    auxOutChannel[5] = channel;
}

void AuxDigOutConfigDialog::channelPortGChanged(int channel)
{
    auxOutChannel[6] = channel;
}

void AuxDigOutConfigDialog::channelPortHChanged(int channel)
{
    auxOutChannel[7] = channel;
}

bool AuxDigOutConfigDialog::enabled(int port)
{
    return auxOutEnabled[port];
}

int AuxDigOutConfigDialog::channel(int port)
{
    return auxOutChannel[port];
}

void AuxDigOutConfigDialog::populatePortComboBox(QComboBox* channelPortComboBox)
{
    channelPortComboBox->addItem(tr("DIGITAL IN 1"));
    channelPortComboBox->addItem(tr("DIGITAL IN 2"));
    channelPortComboBox->addItem(tr("DIGITAL IN 3"));
    channelPortComboBox->addItem(tr("DIGITAL IN 4"));
    channelPortComboBox->addItem(tr("DIGITAL IN 5"));
    channelPortComboBox->addItem(tr("DIGITAL IN 6"));
    channelPortComboBox->addItem(tr("DIGITAL IN 7"));
    channelPortComboBox->addItem(tr("DIGITAL IN 8"));
    channelPortComboBox->addItem(tr("DIGITAL IN 9"));
    channelPortComboBox->addItem(tr("DIGITAL IN 10"));
    channelPortComboBox->addItem(tr("DIGITAL IN 11"));
    channelPortComboBox->addItem(tr("DIGITAL IN 12"));
    channelPortComboBox->addItem(tr("DIGITAL IN 13"));
    channelPortComboBox->addItem(tr("DIGITAL IN 14"));
    channelPortComboBox->addItem(tr("DIGITAL IN 15"));
    channelPortComboBox->addItem(tr("DIGITAL IN 16"));
}

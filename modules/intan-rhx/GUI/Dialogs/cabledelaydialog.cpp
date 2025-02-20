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
#include "cabledelaydialog.h"

CableDelayDialog::CableDelayDialog(std::vector<bool> &manualDelayEnabled, std::vector<int> &currentDelay, int numPorts,
                                   QWidget *parent) :
    QDialog(parent)
{
    QLabel *label1 = new QLabel(
                tr("Intan hardware can compensate for the nanosecond-scale time delays "
                   "resulting from finite signal velocities on the headstage interface cables.  "
                   "Each time the interface software is opened or the <b>Rescan Ports</b> button is "
                   "clicked, the software attempts to determine the optimum delay settings for each headstage "
                   "port.  Sometimes this delay-setting algorithm fails, particularly when using 64- or "
                   "128-channel RHD headstages, which use a double-data-rate SPI protocol."));
    label1->setWordWrap(true);

    QLabel *label2 = new QLabel(
                tr("This dialog box allows users to override this algorithm and set delays manually.  "
                   "If a particular headstage port is returning noisy signals with large discontinuities, "
                   "try checking the manual delay box for that port and adjust the delay setting up or "
                   "down by one."));
    label2->setWordWrap(true);

    QLabel *label3 = new QLabel(
                tr("Note that the optimum delay setting for a particular cable length will change if the "
                   "amplifier sampling rate is changed."));
    label3->setWordWrap(true);

    bool numPorts8 = numPorts == 8;

    manualPortACheckBox = new QCheckBox("Set manual delay for Port A");
    manualPortBCheckBox = new QCheckBox("Set manual delay for Port B");
    manualPortCCheckBox = new QCheckBox("Set manual delay for Port C");
    manualPortDCheckBox = new QCheckBox("Set manual delay for Port D");

    manualPortACheckBox->setChecked(manualDelayEnabled[0]);
    manualPortBCheckBox->setChecked(manualDelayEnabled[1]);
    manualPortCCheckBox->setChecked(manualDelayEnabled[2]);
    manualPortDCheckBox->setChecked(manualDelayEnabled[3]);

    delayPortASpinBox = new QSpinBox(this);
    delayPortASpinBox->setRange(0, 15);
    delayPortASpinBox->setValue(currentDelay[0]);

    delayPortBSpinBox = new QSpinBox(this);
    delayPortBSpinBox->setRange(0, 15);
    delayPortBSpinBox->setValue(currentDelay[1]);

    delayPortCSpinBox = new QSpinBox(this);
    delayPortCSpinBox->setRange(0, 15);
    delayPortCSpinBox->setValue(currentDelay[2]);

    delayPortDSpinBox = new QSpinBox(this);
    delayPortDSpinBox->setRange(0, 15);
    delayPortDSpinBox->setValue(currentDelay[3]);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QHBoxLayout *portARow = new QHBoxLayout;
    portARow->addWidget(manualPortACheckBox);
    portARow->addStretch(1);
    portARow->addWidget(new QLabel(tr("Current delay")));
    portARow->addWidget(delayPortASpinBox);

    QHBoxLayout *portBRow = new QHBoxLayout;
    portBRow->addWidget(manualPortBCheckBox);
    portBRow->addStretch(1);
    portBRow->addWidget(new QLabel(tr("Current delay")));
    portBRow->addWidget(delayPortBSpinBox);

    QHBoxLayout *portCRow = new QHBoxLayout;
    portCRow->addWidget(manualPortCCheckBox);
    portCRow->addStretch(1);
    portCRow->addWidget(new QLabel(tr("Current delay")));
    portCRow->addWidget(delayPortCSpinBox);

    QHBoxLayout *portDRow = new QHBoxLayout;
    portDRow->addWidget(manualPortDCheckBox);
    portDRow->addStretch(1);
    portDRow->addWidget(new QLabel(tr("Current delay")));
    portDRow->addWidget(delayPortDSpinBox);

    QVBoxLayout *controlLayout = new QVBoxLayout;
    controlLayout->addLayout(portARow);
    controlLayout->addLayout(portBRow);
    controlLayout->addLayout(portCRow);
    controlLayout->addLayout(portDRow);

    if (numPorts8) {
        manualPortECheckBox = new QCheckBox("Set manual delay for Port E");
        manualPortFCheckBox = new QCheckBox("Set manual delay for Port F");
        manualPortGCheckBox = new QCheckBox("Set manual delay for Port G");
        manualPortHCheckBox = new QCheckBox("Set manual delay for Port H");

        manualPortECheckBox->setChecked(manualDelayEnabled[4]);
        manualPortFCheckBox->setChecked(manualDelayEnabled[5]);
        manualPortGCheckBox->setChecked(manualDelayEnabled[6]);
        manualPortHCheckBox->setChecked(manualDelayEnabled[7]);

        delayPortESpinBox = new QSpinBox(this);
        delayPortESpinBox->setRange(0, 15);
        delayPortESpinBox->setValue(currentDelay[4]);

        delayPortFSpinBox = new QSpinBox(this);
        delayPortFSpinBox->setRange(0, 15);
        delayPortFSpinBox->setValue(currentDelay[5]);

        delayPortGSpinBox = new QSpinBox(this);
        delayPortGSpinBox->setRange(0, 15);
        delayPortGSpinBox->setValue(currentDelay[6]);

        delayPortHSpinBox = new QSpinBox(this);
        delayPortHSpinBox->setRange(0, 15);
        delayPortHSpinBox->setValue(currentDelay[7]);

        QHBoxLayout *portERow = new QHBoxLayout;
        portERow->addWidget(manualPortECheckBox);
        portERow->addStretch(1);
        portERow->addWidget(new QLabel(tr("Current delay")));
        portERow->addWidget(delayPortESpinBox);

        QHBoxLayout *portFRow = new QHBoxLayout;
        portFRow->addWidget(manualPortFCheckBox);
        portFRow->addStretch(1);
        portFRow->addWidget(new QLabel(tr("Current delay")));
        portFRow->addWidget(delayPortFSpinBox);

        QHBoxLayout *portGRow = new QHBoxLayout;
        portGRow->addWidget(manualPortGCheckBox);
        portGRow->addStretch(1);
        portGRow->addWidget(new QLabel(tr("Current delay")));
        portGRow->addWidget(delayPortGSpinBox);

        QHBoxLayout *portHRow = new QHBoxLayout;
        portHRow->addWidget(manualPortHCheckBox);
        portHRow->addStretch(1);
        portHRow->addWidget(new QLabel(tr("Current delay")));
        portHRow->addWidget(delayPortHSpinBox);

        controlLayout->addLayout(portERow);
        controlLayout->addLayout(portFRow);
        controlLayout->addLayout(portGRow);
        controlLayout->addLayout(portHRow);
    }

    QGroupBox *controlBox = new QGroupBox();
    controlBox->setLayout(controlLayout);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(label1);
    mainLayout->addWidget(label2);
    mainLayout->addWidget(label3);
    mainLayout->addWidget(controlBox);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);

    setWindowTitle(tr("Manual Headstage Cable Delay Configuration"));
}


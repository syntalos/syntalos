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

#include "cabledelaydialog.h"

using namespace std;

// Manual SPI cable delay configuration dialog.
// This dialog allows users to select fixed values that the FPGA uses to compensate
// for signal propagation delays in the SPI interface cables.

CableDelayDialog::CableDelayDialog(QVector<bool> &manualDelayEnabled, vector<int> &currentDelay, QWidget *parent) :
    QDialog(parent)
{
    QLabel *label1 = new QLabel(
                tr("The RHD2000 USB interface board can compensate for the nanosecond-scale time delays "
                   "resulting from finite signal velocities on the SPI interface cables.  "
                   "Each time the interface software is opened or the <b>Rescan Ports A-D</b> button is "
                   "clicked, the software attempts to determine the optimum delay settings for each SPI "
                   "port.  Sometimes this delay-setting algorithm fails, particularly when using RHD2164 "
                   "chips which use a double-data-rate SPI protocol."));
    label1->setWordWrap(true);

    QLabel *label2 = new QLabel(
                tr("This dialog box allows users to override this algorithm and set delays manually.  "
                   "If a particular SPI port is returning noisy signals with large discontinuities, "
                   "try checking the manual delay box for that port and adjust the delay setting up or "
                   "down by one."));
    label2->setWordWrap(true);

    QLabel *label3 = new QLabel(
                tr("Note that the optimum delay setting for a particular SPI cable length will change if the "
                   "amplifier sampling rate is changed."));
    label3->setWordWrap(true);

    manualPortACheckBox = new QCheckBox("Set manual delay for Port A");
    manualPortBCheckBox = new QCheckBox("Set manual delay for Port B");
    manualPortCCheckBox = new QCheckBox("Set manual delay for Port C");
    manualPortDCheckBox = new QCheckBox("Set manual delay for Port D");

    manualPortACheckBox->setChecked(manualDelayEnabled[0]);
    manualPortBCheckBox->setChecked(manualDelayEnabled[1]);
    manualPortCCheckBox->setChecked(manualDelayEnabled[2]);
    manualPortDCheckBox->setChecked(manualDelayEnabled[3]);

    delayPortASpinBox = new QSpinBox();
    delayPortASpinBox->setRange(0, 15);
    delayPortASpinBox->setValue(currentDelay[0]);

    delayPortBSpinBox = new QSpinBox();
    delayPortBSpinBox->setRange(0, 15);
    delayPortBSpinBox->setValue(currentDelay[1]);

    delayPortCSpinBox = new QSpinBox();
    delayPortCSpinBox->setRange(0, 15);
    delayPortCSpinBox->setValue(currentDelay[2]);

    delayPortDSpinBox = new QSpinBox();
    delayPortDSpinBox->setRange(0, 15);
    delayPortDSpinBox->setValue(currentDelay[3]);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QHBoxLayout *portARow = new QHBoxLayout;
    portARow->addWidget(manualPortACheckBox);
    portARow->addStretch(1);
    portARow->addWidget(new QLabel(tr("Current delay:")));
    portARow->addWidget(delayPortASpinBox);

    QHBoxLayout *portBRow = new QHBoxLayout;
    portBRow->addWidget(manualPortBCheckBox);
    portBRow->addStretch(1);
    portBRow->addWidget(new QLabel(tr("Current delay:")));
    portBRow->addWidget(delayPortBSpinBox);

    QHBoxLayout *portCRow = new QHBoxLayout;
    portCRow->addWidget(manualPortCCheckBox);
    portCRow->addStretch(1);
    portCRow->addWidget(new QLabel(tr("Current delay:")));
    portCRow->addWidget(delayPortCSpinBox);

    QHBoxLayout *portDRow = new QHBoxLayout;
    portDRow->addWidget(manualPortDCheckBox);
    portDRow->addStretch(1);
    portDRow->addWidget(new QLabel(tr("Current delay:")));
    portDRow->addWidget(delayPortDSpinBox);

    QVBoxLayout *controlLayout = new QVBoxLayout;
    controlLayout->addLayout(portARow);
    controlLayout->addLayout(portBRow);
    controlLayout->addLayout(portCRow);
    controlLayout->addLayout(portDRow);

    QGroupBox *controlBox = new QGroupBox();
    controlBox->setLayout(controlLayout);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(label1);
    mainLayout->addWidget(label2);
    mainLayout->addWidget(label3);
    mainLayout->addWidget(controlBox);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);

    setWindowTitle(tr("Manual SPI Cable Delay Configuration"));
}

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
#include "abstractrhxcontroller.h"
#include "analogoutconfigdialog.h"

AnalogOutConfigDialog::AnalogOutConfigDialog(SystemState* state_, ControllerInterface* controllerInterface_, QWidget* parent) :
    QDialog(parent),
    state(state_),
    controllerInterface(controllerInterface_)
{
    eightAnalogOuts = AbstractRHXController::numAnalogIO(state->getControllerTypeEnum(), state->expanderConnected->getValue()) == 8;

    dacLockToSelectedCheckBox = new QCheckBox(tr("Lock to Selected"), this);
    int dacLockToSelectedCheckBoxWidth = dacLockToSelectedCheckBox->minimumSizeHint().width();
    connect(dacLockToSelectedCheckBox, SIGNAL(clicked(bool)), this, SLOT(dac1LockToSelected(bool)));

    dac1Label = new QLabel(tr("off"), this);
    dac2Label = new QLabel(tr("off"), this);
    if (eightAnalogOuts) {
        dac3Label = new QLabel(tr("off"), this);
        dac4Label = new QLabel(tr("off"), this);
        dac5Label = new QLabel(tr("off"), this);
        dac6Label = new QLabel(tr("off"), this);
        dac7Label = new QLabel(tr("off"), this);
        dac8Label = new QLabel(tr("off"), this);
    }

    dac1SetButton = new QPushButton(tr("Set to Selected"), this);
    dac2SetButton = new QPushButton(tr("Set to Selected"), this);
    connect(dac1SetButton, SIGNAL(clicked()), this, SLOT(dac1SetChannel()));
    connect(dac2SetButton, SIGNAL(clicked()), this, SLOT(dac2SetChannel()));
    if (eightAnalogOuts) {
        dac3SetButton = new QPushButton(tr("Set to Selected"), this);
        dac4SetButton = new QPushButton(tr("Set to Selected"), this);
        dac5SetButton = new QPushButton(tr("Set to Selected"), this);
        dac6SetButton = new QPushButton(tr("Set to Selected"), this);
        dac7SetButton = new QPushButton(tr("Set to Selected"), this);
        dac8SetButton = new QPushButton(tr("Set to Selected"), this);
        connect(dac3SetButton, SIGNAL(clicked()), this, SLOT(dac3SetChannel()));
        connect(dac4SetButton, SIGNAL(clicked()), this, SLOT(dac4SetChannel()));
        connect(dac5SetButton, SIGNAL(clicked()), this, SLOT(dac5SetChannel()));
        connect(dac6SetButton, SIGNAL(clicked()), this, SLOT(dac6SetChannel()));
        connect(dac7SetButton, SIGNAL(clicked()), this, SLOT(dac7SetChannel()));
        connect(dac8SetButton, SIGNAL(clicked()), this, SLOT(dac8SetChannel()));
    }

    dac1DisableButton = new QPushButton(tr("Disable"), this);
    dac1DisableButton->setFixedWidth(dac1DisableButton->fontMetrics().horizontalAdvance(dac1DisableButton->text()) + 14);
    connect(dac1DisableButton, SIGNAL(clicked()), this, SLOT(dac1Disable()));
    dac2DisableButton = new QPushButton(tr("Disable"), this);
    dac2DisableButton->setFixedWidth(dac2DisableButton->fontMetrics().horizontalAdvance(dac2DisableButton->text()) + 14);
    connect(dac2DisableButton, SIGNAL(clicked()), this, SLOT(dac2Disable()));
    if (eightAnalogOuts) {
        dac3DisableButton = new QPushButton(tr("Disable"), this);
        dac3DisableButton->setFixedWidth(dac3DisableButton->fontMetrics().horizontalAdvance(dac3DisableButton->text()) + 14);
        connect(dac3DisableButton, SIGNAL(clicked()), this, SLOT(dac3Disable()));
        dac4DisableButton = new QPushButton(tr("Disable"), this);
        dac4DisableButton->setFixedWidth(dac4DisableButton->fontMetrics().horizontalAdvance(dac4DisableButton->text()) + 14);
        connect(dac4DisableButton, SIGNAL(clicked()), this, SLOT(dac4Disable()));
        dac5DisableButton = new QPushButton(tr("Disable"), this);
        dac5DisableButton->setFixedWidth(dac5DisableButton->fontMetrics().horizontalAdvance(dac5DisableButton->text()) + 14);
        connect(dac5DisableButton, SIGNAL(clicked()), this, SLOT(dac5Disable()));
        dac6DisableButton = new QPushButton(tr("Disable"), this);
        dac6DisableButton->setFixedWidth(dac6DisableButton->fontMetrics().horizontalAdvance(dac6DisableButton->text()) + 14);
        connect(dac6DisableButton, SIGNAL(clicked()), this, SLOT(dac6Disable()));
        dac7DisableButton = new QPushButton(tr("Disable"), this);
        dac7DisableButton->setFixedWidth(dac7DisableButton->fontMetrics().horizontalAdvance(dac7DisableButton->text()) + 14);
        connect(dac7DisableButton, SIGNAL(clicked()), this, SLOT(dac7Disable()));
        dac8DisableButton = new QPushButton(tr("Disable"), this);
        dac8DisableButton->setFixedWidth(dac8DisableButton->fontMetrics().horizontalAdvance(dac8DisableButton->text()) + 14);
        connect(dac8DisableButton, SIGNAL(clicked()), this, SLOT(dac8Disable()));
    }

    dac1ThresholdSpinBox = new QSpinBox(this);
    dac1ThresholdSpinBox->setRange(-6000, 6000);
    dac1ThresholdSpinBox->setSingleStep(5);
    dac1ThresholdSpinBox->setValue(0);
    dac1ThresholdSpinBox->setSuffix(" " + MicroVoltsSymbol);
    connect(dac1ThresholdSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setDac1Threshold(int)));

    dac2ThresholdSpinBox = new QSpinBox(this);
    dac2ThresholdSpinBox->setRange(-6000, 6000);
    dac2ThresholdSpinBox->setSingleStep(5);
    dac2ThresholdSpinBox->setValue(0);
    dac2ThresholdSpinBox->setSuffix(" " + MicroVoltsSymbol);
    connect(dac2ThresholdSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setDac2Threshold(int)));

    if (eightAnalogOuts) {
        dac3ThresholdSpinBox = new QSpinBox(this);
        dac3ThresholdSpinBox->setRange(-6000, 6000);
        dac3ThresholdSpinBox->setSingleStep(5);
        dac3ThresholdSpinBox->setValue(0);
        dac3ThresholdSpinBox->setSuffix(" " + MicroVoltsSymbol);
        connect(dac3ThresholdSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setDac3Threshold(int)));

        dac4ThresholdSpinBox = new QSpinBox(this);
        dac4ThresholdSpinBox->setRange(-6000, 6000);
        dac4ThresholdSpinBox->setSingleStep(5);
        dac4ThresholdSpinBox->setValue(0);
        dac4ThresholdSpinBox->setSuffix(" " + MicroVoltsSymbol);
        connect(dac4ThresholdSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setDac4Threshold(int)));

        dac5ThresholdSpinBox = new QSpinBox(this);
        dac5ThresholdSpinBox->setRange(-6000, 6000);
        dac5ThresholdSpinBox->setSingleStep(5);
        dac5ThresholdSpinBox->setValue(0);
        dac5ThresholdSpinBox->setSuffix(" " + MicroVoltsSymbol);
        connect(dac5ThresholdSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setDac5Threshold(int)));

        dac6ThresholdSpinBox = new QSpinBox(this);
        dac6ThresholdSpinBox->setRange(-6000, 6000);
        dac6ThresholdSpinBox->setSingleStep(5);
        dac6ThresholdSpinBox->setValue(0);
        dac6ThresholdSpinBox->setSuffix(" " + MicroVoltsSymbol);
        connect(dac6ThresholdSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setDac6Threshold(int)));

        dac7ThresholdSpinBox = new QSpinBox(this);
        dac7ThresholdSpinBox->setRange(-6000, 6000);
        dac7ThresholdSpinBox->setSingleStep(5);
        dac7ThresholdSpinBox->setValue(0);
        dac7ThresholdSpinBox->setSuffix(" " + MicroVoltsSymbol);
        connect(dac7ThresholdSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setDac7Threshold(int)));

        dac8ThresholdSpinBox = new QSpinBox(this);
        dac8ThresholdSpinBox->setRange(-6000, 6000);
        dac8ThresholdSpinBox->setSingleStep(5);
        dac8ThresholdSpinBox->setValue(0);
        dac8ThresholdSpinBox->setSuffix(" " + MicroVoltsSymbol);
        connect(dac8ThresholdSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setDac8Threshold(int)));
    }

    dac1ThresholdEnableCheckBox = new QCheckBox(tr("Enable"), this);
    connect(dac1ThresholdEnableCheckBox, SIGNAL(clicked(bool)), this, SLOT(enableDac1Threshold(bool)));
    dac2ThresholdEnableCheckBox = new QCheckBox(tr("Enable"), this);
    connect(dac2ThresholdEnableCheckBox, SIGNAL(clicked(bool)), this, SLOT(enableDac2Threshold(bool)));
    if (eightAnalogOuts) {
        dac3ThresholdEnableCheckBox = new QCheckBox(tr("Enable"), this);
        connect(dac3ThresholdEnableCheckBox, SIGNAL(clicked(bool)), this, SLOT(enableDac3Threshold(bool)));
        dac4ThresholdEnableCheckBox = new QCheckBox(tr("Enable"), this);
        connect(dac4ThresholdEnableCheckBox, SIGNAL(clicked(bool)), this, SLOT(enableDac4Threshold(bool)));
        dac5ThresholdEnableCheckBox = new QCheckBox(tr("Enable"), this);
        connect(dac5ThresholdEnableCheckBox, SIGNAL(clicked(bool)), this, SLOT(enableDac5Threshold(bool)));
        dac6ThresholdEnableCheckBox = new QCheckBox(tr("Enable"), this);
        connect(dac6ThresholdEnableCheckBox, SIGNAL(clicked(bool)), this, SLOT(enableDac6Threshold(bool)));
        dac7ThresholdEnableCheckBox = new QCheckBox(tr("Enable"), this);
        connect(dac7ThresholdEnableCheckBox, SIGNAL(clicked(bool)), this, SLOT(enableDac7Threshold(bool)));
        dac8ThresholdEnableCheckBox = new QCheckBox(tr("Enable"), this);
        connect(dac8ThresholdEnableCheckBox, SIGNAL(clicked(bool)), this, SLOT(enableDac8Threshold(bool)));
    }

    if (state->getControllerTypeEnum() != ControllerStimRecordUSB2) {
        dac1ThresholdEnableCheckBox->setChecked(true);
        dac2ThresholdEnableCheckBox->setChecked(true);
        if (eightAnalogOuts) {
            dac3ThresholdEnableCheckBox->setChecked(true);
            dac4ThresholdEnableCheckBox->setChecked(true);
            dac5ThresholdEnableCheckBox->setChecked(true);
            dac6ThresholdEnableCheckBox->setChecked(true);
            dac7ThresholdEnableCheckBox->setChecked(true);
            dac8ThresholdEnableCheckBox->setChecked(true);
        }

        dac1ThresholdEnableCheckBox->setEnabled(false);
        dac2ThresholdEnableCheckBox->setEnabled(false);
        if (eightAnalogOuts) {
            dac3ThresholdEnableCheckBox->setEnabled(false);
            dac4ThresholdEnableCheckBox->setEnabled(false);
            dac5ThresholdEnableCheckBox->setEnabled(false);
            dac6ThresholdEnableCheckBox->setEnabled(false);
            dac7ThresholdEnableCheckBox->setEnabled(false);
            dac8ThresholdEnableCheckBox->setEnabled(false);
        }
    }

    QHBoxLayout *dac1Layout1, *dac2Layout1, *dac3Layout1, *dac4Layout1, *dac5Layout1, *dac6Layout1, *dac7Layout1, *dac8Layout1;
    dac1Layout1 = new QHBoxLayout;
    dac1Layout1->addWidget(new QLabel(tr("ANALOG OUT 1 (Audio L):"), this));
    dac1Layout1->addWidget(dac1Label);
    dac1Layout1->addStretch(1);
    dac2Layout1 = new QHBoxLayout;
    dac2Layout1->addWidget(new QLabel(tr("ANALOG OUT 2 (Audio R):"), this));
    dac2Layout1->addWidget(dac2Label);
    dac2Layout1->addStretch(1);
    if (eightAnalogOuts) {
        dac3Layout1 = new QHBoxLayout;
        dac3Layout1->addWidget(new QLabel(tr("ANALOG OUT 3:"), this));
        dac3Layout1->addWidget(dac3Label);
        dac3Layout1->addStretch(1);
        dac4Layout1 = new QHBoxLayout;
        dac4Layout1->addWidget(new QLabel(tr("ANALOG OUT 4:"), this));
        dac4Layout1->addWidget(dac4Label);
        dac4Layout1->addStretch(1);
        dac5Layout1 = new QHBoxLayout;
        dac5Layout1->addWidget(new QLabel(tr("ANALOG OUT 5:"), this));
        dac5Layout1->addWidget(dac5Label);
        dac5Layout1->addStretch(1);
        dac6Layout1 = new QHBoxLayout;
        dac6Layout1->addWidget(new QLabel(tr("ANALOG OUT 6:"), this));
        dac6Layout1->addWidget(dac6Label);
        dac6Layout1->addStretch(1);
        dac7Layout1 = new QHBoxLayout;
        dac7Layout1->addWidget(new QLabel(tr("ANALOG OUT 7:"), this));
        dac7Layout1->addWidget(dac7Label);
        dac7Layout1->addStretch(1);
        dac8Layout1 = new QHBoxLayout;
        dac8Layout1->addWidget(new QLabel(tr("ANALOG OUT 8:"), this));
        dac8Layout1->addWidget(dac8Label);
        dac8Layout1->addStretch(1);
    }

    const int HSpacing = 146;
    QHBoxLayout *dac1Layout2, *dac2Layout2, *dac3Layout2, *dac4Layout2, *dac5Layout2, *dac6Layout2, *dac7Layout2, *dac8Layout2;

    dac1Layout2 = new QHBoxLayout;
    dac1Layout2->addWidget(dac1SetButton);
    dac1Layout2->addWidget(dac1DisableButton);
    dac1Layout2->addWidget(dacLockToSelectedCheckBox);
    dac1Layout2->addStretch();
    dac1Layout2->addSpacing(HSpacing - dacLockToSelectedCheckBoxWidth);
    dac1Layout2->addWidget(new QLabel(tr("DIGITAL OUT 1 Threshold"), this));
    dac1Layout2->addWidget(dac1ThresholdSpinBox);
    dac1Layout2->addWidget(dac1ThresholdEnableCheckBox);

    dac2Layout2 = new QHBoxLayout;
    dac2Layout2->addWidget(dac2SetButton);
    dac2Layout2->addWidget(dac2DisableButton);
    dac2Layout2->addStretch();
    dac2Layout2->addSpacing(HSpacing);
    dac2Layout2->addWidget(new QLabel(tr("DIGITAL OUT 2 Threshold"), this));
    dac2Layout2->addWidget(dac2ThresholdSpinBox);
    dac2Layout2->addWidget(dac2ThresholdEnableCheckBox);

    if (eightAnalogOuts) {
        dac3Layout2 = new QHBoxLayout;
        dac3Layout2->addWidget(dac3SetButton);
        dac3Layout2->addWidget(dac3DisableButton);
        dac3Layout2->addStretch();
        dac3Layout2->addSpacing(HSpacing);
        dac3Layout2->addWidget(new QLabel(tr("DIGITAL OUT 3 Threshold"), this));
        dac3Layout2->addWidget(dac3ThresholdSpinBox);
        dac3Layout2->addWidget(dac3ThresholdEnableCheckBox);

        dac4Layout2 = new QHBoxLayout;
        dac4Layout2->addWidget(dac4SetButton);
        dac4Layout2->addWidget(dac4DisableButton);
        dac4Layout2->addStretch();
        dac4Layout2->addSpacing(HSpacing);
        dac4Layout2->addWidget(new QLabel(tr("DIGITAL OUT 4 Threshold"), this));
        dac4Layout2->addWidget(dac4ThresholdSpinBox);
        dac4Layout2->addWidget(dac4ThresholdEnableCheckBox);

        dac5Layout2 = new QHBoxLayout;
        dac5Layout2->addWidget(dac5SetButton);
        dac5Layout2->addWidget(dac5DisableButton);
        dac5Layout2->addStretch();
        dac5Layout2->addSpacing(HSpacing);
        dac5Layout2->addWidget(new QLabel(tr("DIGITAL OUT 5 Threshold"), this));
        dac5Layout2->addWidget(dac5ThresholdSpinBox);
        dac5Layout2->addWidget(dac5ThresholdEnableCheckBox);

        dac6Layout2 = new QHBoxLayout;
        dac6Layout2->addWidget(dac6SetButton);
        dac6Layout2->addWidget(dac6DisableButton);
        dac6Layout2->addStretch();
        dac6Layout2->addSpacing(HSpacing);
        dac6Layout2->addWidget(new QLabel(tr("DIGITAL OUT 6 Threshold"), this));
        dac6Layout2->addWidget(dac6ThresholdSpinBox);
        dac6Layout2->addWidget(dac6ThresholdEnableCheckBox);

        dac7Layout2 = new QHBoxLayout;
        dac7Layout2->addWidget(dac7SetButton);
        dac7Layout2->addWidget(dac7DisableButton);
        dac7Layout2->addStretch();
        dac7Layout2->addSpacing(HSpacing);
        dac7Layout2->addWidget(new QLabel(tr("DIGITAL OUT 7 Threshold"), this));
        dac7Layout2->addWidget(dac7ThresholdSpinBox);
        dac7Layout2->addWidget(dac7ThresholdEnableCheckBox);

        dac8Layout2 = new QHBoxLayout;
        dac8Layout2->addWidget(dac8SetButton);
        dac8Layout2->addWidget(dac8DisableButton);
        dac8Layout2->addStretch();
        dac8Layout2->addSpacing(HSpacing);
        dac8Layout2->addWidget(new QLabel(tr("DIGITAL OUT 8 Threshold"), this));
        dac8Layout2->addWidget(dac8ThresholdSpinBox);
        dac8Layout2->addWidget(dac8ThresholdEnableCheckBox);
    }

    QVBoxLayout *dac1Layout, *dac2Layout, *dac3Layout, *dac4Layout, *dac5Layout, *dac6Layout, *dac7Layout, *dac8Layout;
    dac1Layout = new QVBoxLayout;
    dac1Layout->addLayout(dac1Layout1);
    dac1Layout->addLayout(dac1Layout2);
    dac2Layout = new QVBoxLayout;
    dac2Layout->addLayout(dac2Layout1);
    dac2Layout->addLayout(dac2Layout2);
    if (eightAnalogOuts) {
        dac3Layout = new QVBoxLayout;
        dac3Layout->addLayout(dac3Layout1);
        dac3Layout->addLayout(dac3Layout2);
        dac4Layout = new QVBoxLayout;
        dac4Layout->addLayout(dac4Layout1);
        dac4Layout->addLayout(dac4Layout2);
        dac5Layout = new QVBoxLayout;
        dac5Layout->addLayout(dac5Layout1);
        dac5Layout->addLayout(dac5Layout2);
        dac6Layout = new QVBoxLayout;
        dac6Layout->addLayout(dac6Layout1);
        dac6Layout->addLayout(dac6Layout2);
        dac7Layout = new QVBoxLayout;
        dac7Layout->addLayout(dac7Layout1);
        dac7Layout->addLayout(dac7Layout2);
        dac8Layout = new QVBoxLayout;
        dac8Layout->addLayout(dac8Layout1);
        dac8Layout->addLayout(dac8Layout2);
    }

    QFrame *frame1, *frame2, *frame3, *frame4, *frame5, *frame6, *frame7, *frame8, *frame9;
    frame1 = new QFrame(this);
    frame1->setFrameShape(QFrame::HLine);
    frame2 = new QFrame(this);
    frame2->setFrameShape(QFrame::HLine);
    if (eightAnalogOuts) {
        frame3 = new QFrame(this);
        frame3->setFrameShape(QFrame::HLine);
        frame4 = new QFrame(this);
        frame4->setFrameShape(QFrame::HLine);
        frame5 = new QFrame(this);
        frame5->setFrameShape(QFrame::HLine);
        frame6 = new QFrame(this);
        frame6->setFrameShape(QFrame::HLine);
        frame7 = new QFrame(this);
        frame7->setFrameShape(QFrame::HLine);
        frame8 = new QFrame(this);
        frame8->setFrameShape(QFrame::HLine);
    }

    QHBoxLayout *dacRefLabelLayout, *dacRefLayout;
    QLabel *labelRef;
    if (state->getControllerTypeEnum() != ControllerRecordUSB2) {
        frame9 = new QFrame(this);
        frame9->setFrameShape(QFrame::HLine);

        dacRefLabel = new QLabel("hardware", this);
        dacRefSetButton = new QPushButton(tr("Set to Selected"), this);
        dacRefDisableButton = new QPushButton(tr("Hardware Reference"), this);

        connect(dacRefSetButton, SIGNAL(clicked()), this, SLOT(dacSetRefChannel()));
        connect(dacRefDisableButton, SIGNAL(clicked()), this, SLOT(dacRefDisable()));

        dacRefLabelLayout = new QHBoxLayout;
        dacRefLabelLayout->addWidget(new QLabel(tr("Global ANALOG OUT Reference:"), this));
        dacRefLabelLayout->addWidget(dacRefLabel);
        dacRefLabelLayout->addStretch(1);

        dacRefLayout = new QHBoxLayout;
        dacRefLayout->addWidget(dacRefSetButton);
        dacRefLayout->addWidget(dacRefDisableButton);
        dacRefLayout->addStretch(1);

        labelRef = new QLabel(tr("Software referencing cannot be applied to Analog Out signals, but one amplifier signal "
                                 "may be selected as a global reference that is subtracted from all Analog Out signals in "
                                 "real time."), this);
        labelRef->setWordWrap(true);
    } else {
        dacRefLabel = nullptr;
        dacRefSetButton = nullptr;
        dacRefDisableButton = nullptr;
    }

    QLabel *label1 = new QLabel(tr("Selected wideband amplifier channels are routed to Analog Out ports on the Intan hardware.  "
                                   "Analog Out 1 and 2 are also connected to the left and right channels of Audio Line Out.  "
                                   "The typical latency from amplifier input to Analog Out port is less than 200 ") +
                                   MicroSecondsSymbol + ".", this);
    label1->setWordWrap(true);

    QLabel *label2 = new QLabel(tr("Intan hardware implements low-latency threshold comparators that "
                                   "signal on Digital Out ports when the signals routed to Analog Out ports "
                                   "exceed specified levels.  These comparators have latencies less than "
                                   "200 ") + MicroSecondsSymbol + tr(" and may be used for real-time triggering of other "
                                   "devices based on the detection of neural spikes."), this);
    label2->setWordWrap(true);

    QLabel *label3 = new QLabel(tr("If spike detection is performed on amplifier signals that include low-frequency local "
                                   "field potentials, the optional Analog Out high pass filter can be enabled to pass "
                                   "only spikes.  Go to the <b>Audio/Analog</b> tab to enable this filter."), this);
    label3->setWordWrap(true);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));

    QHBoxLayout *finalRow = new QHBoxLayout;
    finalRow->addStretch(1);
    finalRow->addWidget(buttonBox);

    QFrame *frame0 = new QFrame(this);
    frame0->setFrameShape(QFrame::HLine);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(label1);
    mainLayout->addWidget(frame0);
    mainLayout->addLayout(dac1Layout);
    mainLayout->addWidget(frame1);
    mainLayout->addLayout(dac2Layout);
    mainLayout->addWidget(frame2);
    if (eightAnalogOuts) {
        mainLayout->addLayout(dac3Layout);
        mainLayout->addWidget(frame3);
        mainLayout->addLayout(dac4Layout);
        mainLayout->addWidget(frame4);
        mainLayout->addLayout(dac5Layout);
        mainLayout->addWidget(frame5);
        mainLayout->addLayout(dac6Layout);
        mainLayout->addWidget(frame6);
        mainLayout->addLayout(dac7Layout);
        mainLayout->addWidget(frame7);
        mainLayout->addLayout(dac8Layout);
        mainLayout->addWidget(frame8);
    }
    if (state->getControllerTypeEnum() != ControllerRecordUSB2) {
        mainLayout->addLayout(dacRefLabelLayout);
        mainLayout->addLayout(dacRefLayout);
        mainLayout->addWidget(labelRef);
        mainLayout->addWidget(frame9);
    }
    mainLayout->addWidget(label2);
    mainLayout->addWidget(label3);
    mainLayout->addLayout(finalRow);
    setLayout(mainLayout);

    QWidget *mainWidget = new QWidget(this);
    mainWidget->setLayout(mainLayout);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(mainWidget);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QVBoxLayout *scrollLayout = new QVBoxLayout;
    scrollLayout->addWidget(scrollArea);

    setLayout(scrollLayout);

    // Set dialog initial size to 10% larger than scrollArea's sizeHint - should avoid scroll bars for default info.
    int initialWidth = round(mainWidget->sizeHint().width() * 1.1);
    int initialHeight = round(mainWidget->sizeHint().height() * 1.1);
    resize(initialWidth, initialHeight);

    setWindowTitle(tr("Analog Output Configuration"));
}

void AnalogOutConfigDialog::updateFromState(bool oneAmpSelected)
{
    if (state->analogOut1LockToSelected->getValue() != dacLockToSelectedCheckBox->isChecked()) {
        dacLockToSelectedCheckBox->setChecked(state->analogOut1LockToSelected->getValue());
    }

    dac1SetButton->setEnabled(oneAmpSelected && !dacLockToSelectedCheckBox->isChecked());
    dac2SetButton->setEnabled(oneAmpSelected);
    if (eightAnalogOuts) {
        dac3SetButton->setEnabled(oneAmpSelected);
        dac4SetButton->setEnabled(oneAmpSelected);
        dac5SetButton->setEnabled(oneAmpSelected);
        dac6SetButton->setEnabled(oneAmpSelected);
        dac7SetButton->setEnabled(oneAmpSelected);
        dac8SetButton->setEnabled(oneAmpSelected);
    }

    const QString offString = "Off";
    dac1DisableButton->setEnabled(state->analogOut1Channel->getValueString() != offString);
    dac2DisableButton->setEnabled(state->analogOut2Channel->getValueString() != offString);
    if (eightAnalogOuts) {
        dac3DisableButton->setEnabled(state->analogOut3Channel->getValueString() != offString);
        dac4DisableButton->setEnabled(state->analogOut4Channel->getValueString() != offString);
        dac5DisableButton->setEnabled(state->analogOut5Channel->getValueString() != offString);
        dac6DisableButton->setEnabled(state->analogOut6Channel->getValueString() != offString);
        dac7DisableButton->setEnabled(state->analogOut7Channel->getValueString() != offString);
        dac8DisableButton->setEnabled(state->analogOut8Channel->getValueString() != offString);
    }

    dac1Label->setText(state->analogOut1Channel->getValueString());
    dac2Label->setText(state->analogOut2Channel->getValueString());
    if (eightAnalogOuts) {
        dac3Label->setText(state->analogOut3Channel->getValueString());
        dac4Label->setText(state->analogOut4Channel->getValueString());
        dac5Label->setText(state->analogOut5Channel->getValueString());
        dac6Label->setText(state->analogOut6Channel->getValueString());
        dac7Label->setText(state->analogOut7Channel->getValueString());
        dac8Label->setText(state->analogOut8Channel->getValueString());
    }
    if (dacRefLabel) {
        dacRefLabel->setText(state->analogOutRefChannel->getValueString());
        dacRefSetButton->setEnabled(oneAmpSelected);
        dacRefDisableButton->setEnabled(state->analogOutRefChannel->getValueString().toLower() != "hardware");
    }

    dac1ThresholdSpinBox->setValue(state->analogOut1Threshold->getValue());
    dac2ThresholdSpinBox->setValue(state->analogOut2Threshold->getValue());
    if (eightAnalogOuts) {
        dac3ThresholdSpinBox->setValue(state->analogOut3Threshold->getValue());
        dac4ThresholdSpinBox->setValue(state->analogOut4Threshold->getValue());
        dac5ThresholdSpinBox->setValue(state->analogOut5Threshold->getValue());
        dac6ThresholdSpinBox->setValue(state->analogOut6Threshold->getValue());
        dac7ThresholdSpinBox->setValue(state->analogOut7Threshold->getValue());
        dac8ThresholdSpinBox->setValue(state->analogOut8Threshold->getValue());
    }

    dac1ThresholdEnableCheckBox->setChecked(state->analogOut1ThresholdEnabled->getValue());
    dac2ThresholdEnableCheckBox->setChecked(state->analogOut2ThresholdEnabled->getValue());
    if (eightAnalogOuts) {
        dac3ThresholdEnableCheckBox->setChecked(state->analogOut3ThresholdEnabled->getValue());
        dac4ThresholdEnableCheckBox->setChecked(state->analogOut4ThresholdEnabled->getValue());
        dac5ThresholdEnableCheckBox->setChecked(state->analogOut5ThresholdEnabled->getValue());
        dac6ThresholdEnableCheckBox->setChecked(state->analogOut6ThresholdEnabled->getValue());
        dac7ThresholdEnableCheckBox->setChecked(state->analogOut7ThresholdEnabled->getValue());
        dac8ThresholdEnableCheckBox->setChecked(state->analogOut8ThresholdEnabled->getValue());
    }
}

void AnalogOutConfigDialog::dac1LockToSelected(bool enable)
{
    state->analogOut1LockToSelected->setValue(enable);
}

void AnalogOutConfigDialog::dac1SetChannel()
{
    QString channelName = state->signalSources->singleSelectedAmplifierChannelName();
    if (!channelName.isEmpty()) {
        state->analogOut1Channel->setValue(channelName);
    }
}

void AnalogOutConfigDialog::dac2SetChannel()
{
    QString channelName = state->signalSources->singleSelectedAmplifierChannelName();
    if (!channelName.isEmpty()) {
        state->analogOut2Channel->setValue(channelName);
    }
}

void AnalogOutConfigDialog::dac3SetChannel()
{
    QString channelName = state->signalSources->singleSelectedAmplifierChannelName();
    if (!channelName.isEmpty()) {
        state->analogOut3Channel->setValue(channelName);
    }
}

void AnalogOutConfigDialog::dac4SetChannel()
{
    QString channelName = state->signalSources->singleSelectedAmplifierChannelName();
    if (!channelName.isEmpty()) {
        state->analogOut4Channel->setValue(channelName);
    }
}

void AnalogOutConfigDialog::dac5SetChannel()
{
    QString channelName = state->signalSources->singleSelectedAmplifierChannelName();
    if (!channelName.isEmpty()) {
        state->analogOut5Channel->setValue(channelName);
    }
}

void AnalogOutConfigDialog::dac6SetChannel()
{
    QString channelName = state->signalSources->singleSelectedAmplifierChannelName();
    if (!channelName.isEmpty()) {
        state->analogOut6Channel->setValue(channelName);
    }
}

void AnalogOutConfigDialog::dac7SetChannel()
{
    QString channelName = state->signalSources->singleSelectedAmplifierChannelName();
    if (!channelName.isEmpty()) {
        state->analogOut7Channel->setValue(channelName);
    }
}

void AnalogOutConfigDialog::dac8SetChannel()
{
    QString channelName = state->signalSources->singleSelectedAmplifierChannelName();
    if (!channelName.isEmpty()) {
        state->analogOut8Channel->setValue(channelName);
    }
}

void AnalogOutConfigDialog::dacSetRefChannel()
{
    QString channelName = state->signalSources->singleSelectedAmplifierChannelName();
    if (!channelName.isEmpty()) {
        state->analogOutRefChannel->setValue(channelName);
    }
}

void AnalogOutConfigDialog::dac1Disable()
{
    state->analogOut1Channel->setValue("Off");
}

void AnalogOutConfigDialog::dac2Disable()
{
    state->analogOut2Channel->setValue("Off");
}

void AnalogOutConfigDialog::dac3Disable()
{
    state->analogOut3Channel->setValue("Off");
}

void AnalogOutConfigDialog::dac4Disable()
{
    state->analogOut4Channel->setValue("Off");
}

void AnalogOutConfigDialog::dac5Disable()
{
    state->analogOut5Channel->setValue("Off");
}

void AnalogOutConfigDialog::dac6Disable()
{
    state->analogOut6Channel->setValue("Off");
}

void AnalogOutConfigDialog::dac7Disable()
{
    state->analogOut7Channel->setValue("Off");
}

void AnalogOutConfigDialog::dac8Disable()
{
    state->analogOut8Channel->setValue("Off");
}

void AnalogOutConfigDialog::dacRefDisable()
{
    state->analogOutRefChannel->setValue("Hardware");
}

void AnalogOutConfigDialog::setDac1Threshold(int threshold)
{
    state->analogOut1Threshold->setValue(threshold);
}

void AnalogOutConfigDialog::setDac2Threshold(int threshold)
{
    state->analogOut2Threshold->setValue(threshold);
}

void AnalogOutConfigDialog::setDac3Threshold(int threshold)
{
    state->analogOut3Threshold->setValue(threshold);
}

void AnalogOutConfigDialog::setDac4Threshold(int threshold)
{
    state->analogOut4Threshold->setValue(threshold);
}

void AnalogOutConfigDialog::setDac5Threshold(int threshold)
{
    state->analogOut5Threshold->setValue(threshold);
}

void AnalogOutConfigDialog::setDac6Threshold(int threshold)
{
    state->analogOut6Threshold->setValue(threshold);
}

void AnalogOutConfigDialog::setDac7Threshold(int threshold)
{
    state->analogOut7Threshold->setValue(threshold);
}

void AnalogOutConfigDialog::setDac8Threshold(int threshold)
{
    state->analogOut8Threshold->setValue(threshold);
}

void AnalogOutConfigDialog::enableDac1Threshold(bool enable)
{
    state->analogOut1ThresholdEnabled->setValue(enable);
}

void AnalogOutConfigDialog::enableDac2Threshold(bool enable)
{
    state->analogOut2ThresholdEnabled->setValue(enable);
}

void AnalogOutConfigDialog::enableDac3Threshold(bool enable)
{
    state->analogOut3ThresholdEnabled->setValue(enable);
}

void AnalogOutConfigDialog::enableDac4Threshold(bool enable)
{
    state->analogOut4ThresholdEnabled->setValue(enable);
}

void AnalogOutConfigDialog::enableDac5Threshold(bool enable)
{
    state->analogOut5ThresholdEnabled->setValue(enable);
}

void AnalogOutConfigDialog::enableDac6Threshold(bool enable)
{
    state->analogOut6ThresholdEnabled->setValue(enable);
}

void AnalogOutConfigDialog::enableDac7Threshold(bool enable)
{
    state->analogOut7ThresholdEnabled->setValue(enable);
}

void AnalogOutConfigDialog::enableDac8Threshold(bool enable)
{
    state->analogOut8ThresholdEnabled->setValue(enable);
}

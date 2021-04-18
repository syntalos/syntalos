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

#include "startupdialog.h"

#include <QtWidgets>

StartupDialog::StartupDialog(ControllerType controllerType_, AmplifierSampleRate* sampleRate_, StimStepSize* stimStepSize_,
                             bool* rememberSettings_, bool askToRememberSettings, QWidget *parent) :
    QDialog(parent),
    controllerType(controllerType_),
    sampleRate(sampleRate_),
    stimStepSize(stimStepSize_),
    rememberSettings(rememberSettings_)
{
    QGroupBox *sampleRateGroupBox;
    sampleRateComboBox = new QComboBox(this);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    if (controllerType == ControllerStimRecordUSB2) {
        sampleRateGroupBox = new QGroupBox(tr("Sample Rate / Stimulation Time Resolution"), this);
        sampleRateComboBox->addItem(tr("20 kHz sample rate (50 ") + MicroSecondsSymbol + tr(" stimulation time resolution)"));
        sampleRateComboBox->addItem(tr("25 kHz sample rate (40 ") + MicroSecondsSymbol + tr(" stimulation time resolution)"));
        sampleRateComboBox->addItem(tr("30 kHz sample rate (33.3 ") + MicroSecondsSymbol + tr(" stimulation time resolution)"));
        sampleRateComboBox->setCurrentIndex(2);
    } else {
        sampleRateComboBox->addItem(tr("1.00 kHz"));
        sampleRateComboBox->addItem(tr("1.25 kHz"));
        sampleRateComboBox->addItem(tr("1.50 kHz"));
        sampleRateComboBox->addItem(tr("2.00 kHz"));
        sampleRateComboBox->addItem(tr("2.50 kHz"));
        sampleRateComboBox->addItem(tr("3.00 kHz"));
        sampleRateComboBox->addItem(tr("3.33 kHz"));
        sampleRateComboBox->addItem(tr("4.00 kHz"));
        sampleRateComboBox->addItem(tr("5.00 kHz"));
        sampleRateComboBox->addItem(tr("6.25 kHz"));
        sampleRateComboBox->addItem(tr("8.00 kHz"));
        sampleRateComboBox->addItem(tr("10.0 kHz"));
        sampleRateComboBox->addItem(tr("12.5 kHz"));
        sampleRateComboBox->addItem(tr("15.0 kHz"));
        sampleRateComboBox->addItem(tr("20.0 kHz"));
        sampleRateComboBox->addItem(tr("25.0 kHz"));
        sampleRateComboBox->addItem(tr("30.0 kHz"));
        sampleRateComboBox->setCurrentIndex(14);
    }

    QHBoxLayout *sampleRateLayout = new QHBoxLayout();
    sampleRateLayout->addWidget(new QLabel(tr("Sample Rate"), this));
    sampleRateLayout->addWidget(sampleRateComboBox);
    sampleRateLayout->addStretch(1);
    if (controllerType == ControllerStimRecordUSB2) {
        sampleRateGroupBox->setLayout(sampleRateLayout);
    }

    QGroupBox *stimStepGroupBox;

    if (controllerType == ControllerStimRecordUSB2) {
        stimStepGroupBox = new QGroupBox(tr("Stimulation Range / Step Size"), this);
        stimStepComboBox = new QComboBox(this);
        stimStepComboBox->addItem(StimStepSizeString[1]);
        stimStepComboBox->addItem(StimStepSizeString[2]);
        stimStepComboBox->addItem(StimStepSizeString[3]);
        stimStepComboBox->addItem(StimStepSizeString[4]);
        stimStepComboBox->addItem(StimStepSizeString[5]);
        stimStepComboBox->addItem(StimStepSizeString[6]);
        stimStepComboBox->addItem(StimStepSizeString[7]);
        stimStepComboBox->addItem(StimStepSizeString[8]);
        stimStepComboBox->addItem(StimStepSizeString[9]);
        stimStepComboBox->addItem(StimStepSizeString[10]);
        stimStepComboBox->setCurrentIndex(5);

        QHBoxLayout *stimStepLayout1 = new QHBoxLayout();
        stimStepLayout1->addWidget(new QLabel(tr("Stimulation Range"), this));
        stimStepLayout1->addWidget(stimStepComboBox);
        stimStepLayout1->addStretch(1);

        QVBoxLayout *stimStepLayout = new QVBoxLayout();
        stimStepLayout->addLayout(stimStepLayout1);
        stimStepGroupBox->setLayout(stimStepLayout);
    }

    rememberSettingsCheckBox = new QCheckBox(this);
    if (controllerType == ControllerStimRecordUSB2) {
        rememberSettingsCheckBox->setText(tr("Always use these settings with ") + ControllerTypeString[(int)controllerType]);
    } else {
        rememberSettingsCheckBox->setText(tr("Always use this sample rate with ") + ControllerTypeString[(int)controllerType]);
    }
    rememberSettingsCheckBox->setVisible(askToRememberSettings);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));

    QHBoxLayout *buttonBoxRow = new QHBoxLayout();
    buttonBoxRow->addStretch();
    buttonBoxRow->addWidget(buttonBox);

    QVBoxLayout *mainLayout = new QVBoxLayout;

    if (controllerType == ControllerStimRecordUSB2) {
        mainLayout->addWidget(sampleRateGroupBox);
        mainLayout->addWidget(stimStepGroupBox);
    } else {
        mainLayout->addLayout(sampleRateLayout);
    }

    setWindowTitle(tr("Select Sample Rate"));
    mainLayout->addWidget(rememberSettingsCheckBox);
    mainLayout->addLayout(buttonBoxRow);
    setLayout(mainLayout);
}

void StartupDialog::closeEvent(QCloseEvent *)
{
    exit(EXIT_FAILURE);
}

void StartupDialog::accept()
{
    if (controllerType == ControllerStimRecordUSB2) {
        switch (sampleRateComboBox->currentIndex()) {
        case 0: *sampleRate = SampleRate20000Hz; break;
        case 1: *sampleRate = SampleRate25000Hz; break;
        case 2: *sampleRate = SampleRate30000Hz; break;
        }

        switch (stimStepComboBox->currentIndex()) {
        case 0: *stimStepSize = StimStepSize10nA; break;
        case 1: *stimStepSize = StimStepSize20nA; break;
        case 2: *stimStepSize = StimStepSize50nA; break;
        case 3: *stimStepSize = StimStepSize100nA; break;
        case 4: *stimStepSize = StimStepSize200nA; break;
        case 5: *stimStepSize = StimStepSize500nA; break;
        case 6: *stimStepSize = StimStepSize1uA; break;
        case 7: *stimStepSize = StimStepSize2uA; break;
        case 8: *stimStepSize = StimStepSize5uA; break;
        case 9: *stimStepSize = StimStepSize10uA; break;
        }
    } else {
        switch (sampleRateComboBox->currentIndex()) {
        case 0: *sampleRate = SampleRate1000Hz; break;
        case 1: *sampleRate = SampleRate1250Hz; break;
        case 2: *sampleRate = SampleRate1500Hz; break;
        case 3: *sampleRate = SampleRate2000Hz; break;
        case 4: *sampleRate = SampleRate2500Hz; break;
        case 5: *sampleRate = SampleRate3000Hz; break;
        case 6: *sampleRate = SampleRate3333Hz; break;
        case 7: *sampleRate = SampleRate4000Hz; break;
        case 8: *sampleRate = SampleRate5000Hz; break;
        case 9: *sampleRate = SampleRate6250Hz; break;
        case 10: *sampleRate = SampleRate8000Hz; break;
        case 11: *sampleRate = SampleRate10000Hz; break;
        case 12: *sampleRate = SampleRate12500Hz; break;
        case 13: *sampleRate = SampleRate15000Hz; break;
        case 14: *sampleRate = SampleRate20000Hz; break;
        case 15: *sampleRate = SampleRate25000Hz; break;
        case 16: *sampleRate = SampleRate30000Hz; break;
        }
        *stimStepSize = StimStepSizeMin;
    }
    *rememberSettings = rememberSettingsCheckBox->isChecked();

    done(Accepted);
}

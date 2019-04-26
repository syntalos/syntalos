//  ------------------------------------------------------------------------
//
//  This file is part of the Intan Technologies RHD2000 Interface
//  Version 1.3
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

#include "qtincludes.h"

#include "globalconstants.h"
#include "setsaveformatdialog.h"


// Save file format selection dialog.
// Allows users to select a new save file format, along with various options.

SetSaveFormatDialog::SetSaveFormatDialog(SaveFormat initSaveFormat, bool initSaveTemperature,
                                         bool initSaveTtlOut, int initNewSaveFilePeriodMinutes,
                                         QWidget *parent) :
    QDialog(parent)
{ 
    setWindowTitle(tr("Select Saved Data File Format"));

    saveFormatIntanButton = new QRadioButton(tr("Traditional Intan File Format"));
    saveFormatNeuroScopeButton = new QRadioButton(tr("\"One File Per Signal Type\" Format"));
    saveFormatOpenEphysButton = new QRadioButton(tr("\"One File Per Channel\" Format"));

    buttonGroup = new QButtonGroup();
    buttonGroup->addButton(saveFormatIntanButton);
    buttonGroup->addButton(saveFormatNeuroScopeButton);
    buttonGroup->addButton(saveFormatOpenEphysButton);
    buttonGroup->setId(saveFormatIntanButton, (int) SaveFormatIntan);
    buttonGroup->setId(saveFormatNeuroScopeButton, (int) SaveFormatFilePerSignalType);
    buttonGroup->setId(saveFormatOpenEphysButton, (int) SaveFormatFilePerChannel);

    switch (initSaveFormat) {
    case SaveFormatIntan:
        saveFormatIntanButton->setChecked(true);
        break;
    case SaveFormatFilePerSignalType:
        saveFormatNeuroScopeButton->setChecked(true);
        break;
    case SaveFormatFilePerChannel:
        saveFormatOpenEphysButton->setChecked(true);
        break;
    }

    recordTimeSpinBox = new QSpinBox();
    recordTimeSpinBox->setRange(1, 999);
    recordTimeSpinBox->setValue(initNewSaveFilePeriodMinutes);

    saveTemperatureCheckBox = new QCheckBox(tr("Save On-Chip Temperature Sensor Readings"));
    saveTemperatureCheckBox->setChecked(initSaveTemperature);

    saveTtlOutCheckBox = new QCheckBox(tr("Save Digital Ouputs"));
    saveTtlOutCheckBox->setChecked(initSaveTtlOut);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QHBoxLayout *newFileTimeLayout = new QHBoxLayout;
    newFileTimeLayout->addWidget(new QLabel(tr("Start new file every")));
    newFileTimeLayout->addWidget(recordTimeSpinBox);
    newFileTimeLayout->addWidget(new QLabel(tr("minutes")));
    newFileTimeLayout->addStretch(1);

    QLabel *label1 = new QLabel(tr("This option saves all waveforms in one file, along with records "
                                   "of sampling rate, amplifier bandwidth, channel names, etc.  To keep "
                                   "individual file size reasonable, a new file is created every N minutes.  "
                                   "These *.rhd data files may be read into MATLAB using "
                                   "read_Intan_RHD2000_file.m, provided on the Intan web site."));
    label1->setWordWrap(true);

    QLabel *label2 = new QLabel(tr("This option creates a subdirectory and saves raw data files for each "
                                   "signal type: amplifiers, auxiliary inputs, supply voltages, board "
                                   "ADC inputs, and board digital inputs.  For example, the amplifier.dat "
                                   "file contains waveform data from all enabled amplifier channels.  The "
                                   "time.dat file contains the timestamp vector, and an info.rhd file contains "
                                   "records of sampling rate, amplifier bandwidth, channel names, etc."));
    label2->setWordWrap(true);

    QLabel *label2b = new QLabel(tr("These raw data files are compatible with the NeuroScope software package."));
    label2b->setWordWrap(true);

    QLabel *label3 = new QLabel(tr("This option creates a subdirectory and saves each enabled waveform "
                                   "in its own *.dat raw data file.  The subdirectory also contains a time.dat "
                                   "file containing a timestamp vector, and an info.rhd file containing "
                                   "records of sampling rate, amplifier bandwidth, channel names, etc."));
    label3->setWordWrap(true);

    QVBoxLayout *boxLayout1 = new QVBoxLayout;
    boxLayout1->addWidget(saveFormatIntanButton);
    boxLayout1->addWidget(label1);
    boxLayout1->addLayout(newFileTimeLayout);
    boxLayout1->addWidget(saveTemperatureCheckBox);

    QVBoxLayout *boxLayout2 = new QVBoxLayout;
    boxLayout2->addWidget(saveFormatNeuroScopeButton);
    boxLayout2->addWidget(label2);
    boxLayout2->addWidget(label2b);

    QVBoxLayout *boxLayout3 = new QVBoxLayout;
    boxLayout3->addWidget(saveFormatOpenEphysButton);
    boxLayout3->addWidget(label3);

    QGroupBox *mainGroupBox1 = new QGroupBox();
    mainGroupBox1->setLayout(boxLayout1);
    QGroupBox *mainGroupBox2 = new QGroupBox();
    mainGroupBox2->setLayout(boxLayout2);
    QGroupBox *mainGroupBox3 = new QGroupBox();
    mainGroupBox3->setLayout(boxLayout3);

    QLabel *label4 = new QLabel(tr("To minimize the disk space required for data files, remember to "
                                   "disable all unused channels, including auxiliary input and supply "
                                   "voltage channels, which may be found by scrolling down below "
                                   "amplifier channels in the multi-waveform display."));

    label4->setWordWrap(true);

    QLabel *label5 = new QLabel(tr("For detailed information on file formats, see the "
                                   "<b>RHD2000 Application note: Data file formats</b>, "
                                   "available at <i>http://www.intantech.com/downloads.html</i>"));

    label5->setWordWrap(true);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(mainGroupBox1);
    mainLayout->addWidget(mainGroupBox2);
    mainLayout->addWidget(mainGroupBox3);
    mainLayout->addWidget(saveTtlOutCheckBox);
    mainLayout->addWidget(label4);
    mainLayout->addWidget(label5);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
}

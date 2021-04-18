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
#include "rhxglobals.h"
#include "setfileformatdialog.h"

// Save file format selection dialog - this allows users to select a new save file format,
// along with various options.
SetFileFormatDialog::SetFileFormatDialog(SystemState *state_, QWidget *parent) :
    QDialog(parent),
    state(state_)
{ 
    setWindowTitle(tr("Select Saved Data File Format"));

    fileFormatIntanButton = new QRadioButton(tr("Traditional Intan File Format"), this);
    fileFormatNeuroScopeButton = new QRadioButton(tr("\"One File Per Signal Type\" Format"), this);
    fileFormatOpenEphysButton = new QRadioButton(tr("\"One File Per Channel\" Format"), this);

    buttonGroup = new QButtonGroup(this);
    buttonGroup->addButton(fileFormatIntanButton);
    buttonGroup->addButton(fileFormatNeuroScopeButton);
    buttonGroup->addButton(fileFormatOpenEphysButton);
    buttonGroup->setId(fileFormatIntanButton, (int) FileFormatIntan);
    buttonGroup->setId(fileFormatNeuroScopeButton, (int) FileFormatFilePerSignalType);
    buttonGroup->setId(fileFormatOpenEphysButton, (int) FileFormatFilePerChannel);

    recordTimeSpinBox = new QSpinBox(this);
    state->newSaveFilePeriodMinutes->setupSpinBox(recordTimeSpinBox);    

    saveAuxInWithAmpCheckBox = new QCheckBox(tr("Save Auxiliary Inputs (Accelerometers) in Wideband Amplifier Data File"), this);
    saveWidebandAmplifierWaveformsCheckBox = new QCheckBox(tr("Save Wideband Amplifier Waveforms"), this);
    saveLowpassAmplifierWaveformsCheckBox = new QCheckBox(tr("Save Lowpass Amplifier Waveforms"), this);
    saveHighpassAmplifierWaveformsCheckBox = new QCheckBox(tr("Save Highpass Amplifier Waveforms"), this);
    saveSpikeDataCheckBox = new QCheckBox(tr("Save Spike Events"), this);
    saveSpikeSnapshotsCheckBox = new QCheckBox(tr("Save Snapshots"), this);
    spikeSnapshotPreDetectSpinBox = new QSpinBox(this);
    state->spikeSnapshotPreDetect->setupSpinBox(spikeSnapshotPreDetectSpinBox);
    spikeSnapshotPreDetectSpinBox->setSuffix(" ms");
    spikeSnapshotPostDetectSpinBox = new QSpinBox(this);
    state->spikeSnapshotPostDetect->setupSpinBox(spikeSnapshotPostDetectSpinBox);
    spikeSnapshotPostDetectSpinBox->setSuffix(" ms");

    fromLabel = new QLabel(tr("from"), this);
    toLabel = new QLabel(tr("to"), this);

    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        saveDCAmplifierWaveformsCheckBox = new QCheckBox(tr("Save DC Amplifier Waveforms"), this);
    }

    lowpassWaveformDownsampleRateComboBox = new QComboBox(this);
    state->lowpassWaveformDownsampleRate->setupComboBox(lowpassWaveformDownsampleRateComboBox);

    connect(saveLowpassAmplifierWaveformsCheckBox, SIGNAL(clicked(bool)), this, SLOT(updateSaveLowpass()));
    connect(saveSpikeDataCheckBox, SIGNAL(clicked(bool)), this, SLOT(updateSaveSpikes()));
    connect(saveSpikeSnapshotsCheckBox, SIGNAL(clicked(bool)), this, SLOT(updateSaveSnapshots()));
    connect(lowpassWaveformDownsampleRateComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateLowpassSampleRate()));
    connect(buttonGroup, SIGNAL(buttonClicked(QAbstractButton*)), this, SLOT(updateOldFileFormat()));

    downsampleLabel = new QLabel(tr("Downsample:"), this);
    lowpassSampleRateLabel = new QLabel(this);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QHBoxLayout *newFileTimeLayout = new QHBoxLayout;
    newFileTimeLayout->addWidget(new QLabel(tr("Start new file every"), this));
    newFileTimeLayout->addWidget(recordTimeSpinBox);
    newFileTimeLayout->addWidget(new QLabel(tr("minutes"), this));
    newFileTimeLayout->addStretch(1);

    QString fileSuffix = "rh" + (QString)(state->getControllerTypeEnum() == ControllerStimRecordUSB2 ? "s" : "d");

    QLabel *traditionalFormatDescription = new QLabel(tr("This option saves all waveforms in one file, along with records "
                                     "of sampling rate,\namplifier bandwidth, channel names, etc.  To keep "
                                     "individual file size reasonable, a\nnew file is created every N minutes.  "
                                     "These *.") + fileSuffix + tr(" data files may be read into\nMATLAB or Python using "
                                     "code provided on the Intan web site."), this);

    QLabel *traditionalFormatWarning = new QLabel(tr("<b>Note:</b> This file format does not support saving lowpass, highpass, or "
                                                     " spike data."), this);

    QLabel *oneFilePerSignalTypeDescription;
    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        oneFilePerSignalTypeDescription = new QLabel(tr("This option creates a subdirectory and saves raw data files for each "
                                     "signal type:\namplifiers and controller analog and digital I/O. For example, the amplifier.dat "
                                     "file\ncontains waveform data from all enabled amplifier channels.  The "
                                     "time.dat file\ncontains the timestamp vector, and an info.rhs file contains "
                                     "records of sampling\nrate, amplifier bandwidth, channel names, etc."), this);
    } else {
        oneFilePerSignalTypeDescription = new QLabel(tr("This option creates a subdirectory and saves raw data files for each "
                                     "signal type:\namplifiers, auxiliary inputs, supply voltages, and controller analog and digital\ninputs. For example, the amplifier.dat "
                                     "file contains waveform data from all enabled\namplifier channels.  The "
                                     "time.dat file contains the timestamp vector, and an\ninfo.rhd file contains "
                                     "records of sampling rate, amplifier bandwidth, channel names,\netc."), this);
    }

    QLabel *NeuroScopeDescription = new QLabel(tr("These raw data files are compatible with the NeuroScope software package."), this);

    QLabel *oneFilePerChannelDescription = new QLabel(tr("This option creates a subdirectory and saves each enabled waveform "
                                   "in its own\nraw data file.  The subdirectory also contains a time.dat "
                                   "file containing a timestamp\nvector, and an info.") + fileSuffix + tr(" file containing "
                                   "records of sampling rate, amplifier\nbandwidth, channel names, etc."), this);

    QVBoxLayout *traditionalBoxLayout = new QVBoxLayout;
    traditionalBoxLayout->addWidget(fileFormatIntanButton);
    traditionalBoxLayout->addWidget(traditionalFormatDescription);
    traditionalBoxLayout->addWidget(traditionalFormatWarning);
    traditionalBoxLayout->addLayout(newFileTimeLayout);

    QVBoxLayout *oneFilePerSignalTypeBoxLayout = new QVBoxLayout;
    oneFilePerSignalTypeBoxLayout->addWidget(fileFormatNeuroScopeButton);
    oneFilePerSignalTypeBoxLayout->addWidget(oneFilePerSignalTypeDescription);
    oneFilePerSignalTypeBoxLayout->addWidget(NeuroScopeDescription);
    oneFilePerSignalTypeBoxLayout->addWidget(saveAuxInWithAmpCheckBox);

    QVBoxLayout *oneFilePerChannelBoxLayout = new QVBoxLayout;
    oneFilePerChannelBoxLayout->addWidget(fileFormatOpenEphysButton);
    oneFilePerChannelBoxLayout->addWidget(oneFilePerChannelDescription);

    QGroupBox *traditionalBox = new QGroupBox();
    traditionalBox->setLayout(traditionalBoxLayout);
    QGroupBox *oneFilePerSignalTypeBox = new QGroupBox();
    oneFilePerSignalTypeBox->setLayout(oneFilePerSignalTypeBoxLayout);
    QGroupBox *oneFilePerChannelBox = new QGroupBox();
    oneFilePerChannelBox->setLayout(oneFilePerChannelBoxLayout);

    QHBoxLayout *lowpassSaveLayout = new QHBoxLayout;
    lowpassSaveLayout->addWidget(saveLowpassAmplifierWaveformsCheckBox);
    lowpassSaveLayout->addWidget(downsampleLabel);
    lowpassSaveLayout->addWidget(lowpassWaveformDownsampleRateComboBox);
    lowpassSaveLayout->addWidget(lowpassSampleRateLabel);
    lowpassSaveLayout->addStretch(1);

    QHBoxLayout *spikeSaveLayout = new QHBoxLayout;
    spikeSaveLayout->addWidget(saveSpikeDataCheckBox);
    spikeSaveLayout->addWidget(saveSpikeSnapshotsCheckBox);
    spikeSaveLayout->addWidget(fromLabel);
    spikeSaveLayout->addWidget(spikeSnapshotPreDetectSpinBox);
    spikeSaveLayout->addWidget(toLabel);
    spikeSaveLayout->addWidget(spikeSnapshotPostDetectSpinBox);
    spikeSaveLayout->addStretch(1);

    QLabel *disableSuggestion = new QLabel(tr("To minimize the disk space required for data files, "
                                   "disable all unused channels."), this);

    QLabel *linkMessage = new QLabel(tr("For detailed information on file formats, see "
                                   "<b>") + fileSuffix.toUpper() + tr(" data file formats</b>, "
                                   "available at <br> <i>www.intantech.com/downloads</i>"), this);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(traditionalBox);
    mainLayout->addWidget(oneFilePerSignalTypeBox);
    mainLayout->addWidget(oneFilePerChannelBox);
    mainLayout->addWidget(saveWidebandAmplifierWaveformsCheckBox);
    mainLayout->addLayout(lowpassSaveLayout);
    mainLayout->addWidget(saveHighpassAmplifierWaveformsCheckBox);
    mainLayout->addLayout(spikeSaveLayout);
    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        mainLayout->addWidget(saveDCAmplifierWaveformsCheckBox);
    }
    mainLayout->addWidget(disableSuggestion);
    mainLayout->addWidget(linkMessage);
    mainLayout->addWidget(buttonBox);

    QWidget *mainWidget = new QWidget(this);
    mainWidget->setLayout(mainLayout);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(mainWidget);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QVBoxLayout *scrollLayout = new QVBoxLayout;
    scrollLayout->addWidget(scrollArea);

    setLayout(scrollLayout);

    // Set dialog initial size to 5% larger than scrollArea's sizeHint - should avoid scroll bars for default size.
    int initialWidth = round(mainWidget->sizeHint().width() * 1.05);
    int initialHeight = round(mainWidget->sizeHint().height() * 1.05);
    resize(initialWidth, initialHeight);

    updateFromState();
}

void SetFileFormatDialog::updateFromState()
{
    if (state->getFileFormatEnum() == FileFormatIntan) {
        fileFormatIntanButton->setChecked(true);
    } else if (state->getFileFormatEnum() == FileFormatFilePerSignalType) {
        fileFormatNeuroScopeButton->setChecked(true);
    } else if (state->getFileFormatEnum() == FileFormatFilePerChannel) {
        fileFormatOpenEphysButton->setChecked(true);
    }

    saveAuxInWithAmpCheckBox->setChecked(state->saveAuxInWithAmpWaveforms->getValue());
    saveWidebandAmplifierWaveformsCheckBox->setChecked(state->saveWidebandAmplifierWaveforms->getValue());
    saveLowpassAmplifierWaveformsCheckBox->setChecked(state->saveLowpassAmplifierWaveforms->getValue());
    saveHighpassAmplifierWaveformsCheckBox->setChecked(state->saveHighpassAmplifierWaveforms->getValue());
    saveSpikeDataCheckBox->setChecked(state->saveSpikeData->getValue());
    saveSpikeSnapshotsCheckBox->setChecked(state->saveSpikeSnapshots->getValue());

    spikeSnapshotPreDetectSpinBox->setValue(state->spikeSnapshotPreDetect->getValue());
    spikeSnapshotPostDetectSpinBox->setValue(state->spikeSnapshotPostDetect->getValue());

    lowpassWaveformDownsampleRateComboBox->setCurrentIndex(state->lowpassWaveformDownsampleRate->getIndex());

    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        saveDCAmplifierWaveformsCheckBox->setChecked(state->saveDCAmplifierWaveforms->getValue());
    }
    recordTimeSpinBox->setValue(state->newSaveFilePeriodMinutes->getValue());
    updateSaveLowpass();
    updateSaveSpikes();
    updateLowpassSampleRate();
    updateOldFileFormat();
}

QString SetFileFormatDialog::getFileFormat() const
{
    return state->fileFormat->getValue(buttonGroup->checkedId());
}

bool SetFileFormatDialog::getSaveAuxInWithAmps() const
{
    return saveAuxInWithAmpCheckBox->isChecked();
}

bool SetFileFormatDialog::getSaveWidebandAmps() const
{
    return saveWidebandAmplifierWaveformsCheckBox->isChecked();
}

bool SetFileFormatDialog::getSaveLowpassAmps() const
{
    return saveLowpassAmplifierWaveformsCheckBox->isChecked();
}

bool SetFileFormatDialog::getSaveHighpassAmps() const
{
    return saveHighpassAmplifierWaveformsCheckBox->isChecked();
}

bool SetFileFormatDialog::getSaveDCAmps() const
{
    return saveDCAmplifierWaveformsCheckBox->isChecked();
}

bool SetFileFormatDialog::getSaveSpikeData() const
{
    return saveSpikeDataCheckBox->isChecked();
}

bool SetFileFormatDialog::getSaveSpikeSnapshots() const
{
    return saveSpikeSnapshotsCheckBox->isChecked();
}

int SetFileFormatDialog::getSnapshotPreDetect() const
{
    return spikeSnapshotPreDetectSpinBox->value();
}

int SetFileFormatDialog::getSnapshotPostDetect() const
{
    return spikeSnapshotPostDetectSpinBox->value();
}

int SetFileFormatDialog::getRecordTime() const
{
    return recordTimeSpinBox->value();
}

int SetFileFormatDialog::getLowpassDownsampleFactorIndex() const
{
    return lowpassWaveformDownsampleRateComboBox->currentIndex();
}

void SetFileFormatDialog::updateLowpassSampleRate()
{
    lowpassSampleRateLabel->setText("");
    double downsampleFactor =
            state->lowpassWaveformDownsampleRate->getNumericValue(lowpassWaveformDownsampleRateComboBox->currentIndex());
    if (downsampleFactor == 1.0) return;
    double lowpassSampleRate = state->sampleRate->getNumericValue() / downsampleFactor;
    if (lowpassSampleRate < 1000.0) {
        lowpassSampleRateLabel->setText("to " + QString::number(lowpassSampleRate, 'f', 1) + " S/s");
    } else if (lowpassSampleRate < 10000.0) {
        lowpassSampleRateLabel->setText("to " + QString::number(lowpassSampleRate / 1000.0, 'f', 2) + " kS/s");
    } else {
        lowpassSampleRateLabel->setText("to " + QString::number(lowpassSampleRate / 1000.0, 'f', 1) + " kS/s");
    }
}

void SetFileFormatDialog::updateSaveLowpass()
{
    lowpassWaveformDownsampleRateComboBox->setEnabled(saveLowpassAmplifierWaveformsCheckBox->isChecked());
    lowpassSampleRateLabel->setEnabled(saveLowpassAmplifierWaveformsCheckBox->isChecked());
    downsampleLabel->setEnabled(saveLowpassAmplifierWaveformsCheckBox->isChecked());
}

void SetFileFormatDialog::updateSaveSpikes()
{
    saveSpikeSnapshotsCheckBox->setEnabled(saveSpikeDataCheckBox->isChecked());
    updateSaveSnapshots();
}

void SetFileFormatDialog::updateSaveSnapshots()
{
    bool enable = saveSpikeDataCheckBox->isChecked() && saveSpikeSnapshotsCheckBox->isChecked();
    fromLabel->setEnabled(enable);
    spikeSnapshotPreDetectSpinBox->setEnabled(enable);
    toLabel->setEnabled(enable);
    spikeSnapshotPostDetectSpinBox->setEnabled(enable);
}

void SetFileFormatDialog::updateOldFileFormat()
{
    // Traditional Intan format does not support saving lowpass, highpass, or spike data.
    bool oldFileFormat = (buttonGroup->checkedButton() == fileFormatIntanButton);

    saveWidebandAmplifierWaveformsCheckBox->setEnabled(!oldFileFormat);
    saveLowpassAmplifierWaveformsCheckBox->setEnabled(!oldFileFormat);

    lowpassWaveformDownsampleRateComboBox->setEnabled(!oldFileFormat && saveLowpassAmplifierWaveformsCheckBox->isChecked());
    lowpassSampleRateLabel->setEnabled(!oldFileFormat && saveLowpassAmplifierWaveformsCheckBox->isChecked());
    downsampleLabel->setEnabled(!oldFileFormat && saveLowpassAmplifierWaveformsCheckBox->isChecked());

    saveHighpassAmplifierWaveformsCheckBox->setEnabled(!oldFileFormat);
    saveSpikeDataCheckBox->setEnabled(!oldFileFormat);

    saveSpikeSnapshotsCheckBox->setEnabled(!oldFileFormat && saveSpikeDataCheckBox->isChecked());

    bool enable = saveSpikeDataCheckBox->isChecked() && saveSpikeSnapshotsCheckBox->isChecked();
    fromLabel->setEnabled(!oldFileFormat && enable);
    spikeSnapshotPreDetectSpinBox->setEnabled(!oldFileFormat && enable);
    toLabel->setEnabled(!oldFileFormat && enable);
    spikeSnapshotPostDetectSpinBox->setEnabled(!oldFileFormat && enable);
}

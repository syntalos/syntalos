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
#include <QSettings>
#include "signalsources.h"
#include "psthdialog.h"

PSTHDialog::PSTHDialog(SystemState* state_, QWidget *parent) :
    QDialog(parent),
    state(state_)
{
    connect(state, SIGNAL(stateChanged()), this, SLOT(updateFromState()));

    channelName = new QLabel("", this);

    lockScopeCheckbox = new QCheckBox(tr("Lock Plot to Selected"), this);
    connect(lockScopeCheckbox, SIGNAL(clicked(bool)), this, SLOT(toggleLock()));

    setToSelectedButton = new QPushButton(tr("Set to Selected"), this);
    connect(setToSelectedButton, SIGNAL(clicked()), this, SLOT(setToSelected()));

    preTriggerSpanComboBox = new QComboBox(this);
    state->tSpanPreTriggerPSTH->setupComboBox(preTriggerSpanComboBox);
    connect(preTriggerSpanComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setPreTriggerSpan(int)));

    postTriggerSpanComboBox = new QComboBox(this);
    state->tSpanPostTriggerPSTH->setupComboBox(postTriggerSpanComboBox);
    connect(postTriggerSpanComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setPostTriggerSpan(int)));

    binSizeComboBox = new QComboBox(this);
    state->binSizePSTH->setupComboBox(binSizeComboBox);
    connect(binSizeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setBinSize(int)));

    maxNumTrialsComboBox = new QComboBox(this);
    state->maxNumTrialsPSTH->setupComboBox(maxNumTrialsComboBox);
    connect(maxNumTrialsComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setMaxNumTrials(int)));

    clearLastTrialPushButton = new QPushButton(tr("Clear Last Trial"), this);
    connect(clearLastTrialPushButton, SIGNAL(clicked(bool)), this, SLOT(clearLastTrial()));

    clearAllTrialsPushButton = new QPushButton(tr("Clear All Trials"), this);
    connect(clearAllTrialsPushButton, SIGNAL(clicked(bool)), this, SLOT(clearAllTrials()));

    digitalTriggerComboBox = new QComboBox(this);
    state->digitalTriggerPSTH->setupComboBox(digitalTriggerComboBox);
    connect(digitalTriggerComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setDigitalTrigger(int)));

    triggerPolarityComboBox = new QComboBox(this);
    state->triggerPolarityPSTH->setupComboBox(triggerPolarityComboBox);
    connect(triggerPolarityComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setTriggerPolarity(int)));

    configSaveButton = new QPushButton(tr("Config"), this);
    connect(configSaveButton, SIGNAL(clicked()), this, SLOT(configSave()));

    saveButton = new QPushButton(tr("Save Data"), this);
    connect(saveButton, SIGNAL(clicked()), this, SLOT(saveData()));

    psthPlot = new PSTHPlot(state, this);

    QHBoxLayout *channelRow = new QHBoxLayout;
    channelRow->addWidget(channelName);
    channelRow->addStretch(1);

    QHBoxLayout *lockRow = new QHBoxLayout;
    lockRow->addWidget(lockScopeCheckbox);

    QHBoxLayout *setRow = new QHBoxLayout;
    setRow->addWidget(setToSelectedButton);

    QVBoxLayout *channelColumn = new QVBoxLayout;
    channelColumn->addLayout(channelRow);
    channelColumn->addLayout(lockRow);
    channelColumn->addLayout(setRow);
    QGroupBox *channelGroup = new QGroupBox(tr("Channel"), this);
    channelGroup->setLayout(channelColumn);

    QHBoxLayout *digitalChannelRow = new QHBoxLayout;
    digitalChannelRow->addWidget(new QLabel(tr("Stimulus Marker"), this));
    digitalChannelRow->addWidget(digitalTriggerComboBox);
    digitalChannelRow->addStretch(1);

    QHBoxLayout *triggerPolarityRow = new QHBoxLayout;
    triggerPolarityRow->addWidget(new QLabel(tr("Trigger Polarity"), this));
    triggerPolarityRow->addWidget(triggerPolarityComboBox);
    triggerPolarityRow->addStretch(1);

    QVBoxLayout *triggerSettingsColumn = new QVBoxLayout;
    triggerSettingsColumn->addLayout(digitalChannelRow);
    triggerSettingsColumn->addLayout(triggerPolarityRow);
    QGroupBox *triggerSettingsGroup = new QGroupBox(tr("Trigger Settings"), this);
    triggerSettingsGroup->setLayout(triggerSettingsColumn);

    QHBoxLayout *preTriggerSpanRow = new QHBoxLayout;
    preTriggerSpanRow->addWidget(new QLabel(tr("Pre-Trigger Span"), this));
    preTriggerSpanRow->addWidget(preTriggerSpanComboBox);
    preTriggerSpanRow->addStretch(1);

    QHBoxLayout *postTriggerSpanRow = new QHBoxLayout;
    postTriggerSpanRow->addWidget(new QLabel(tr("Post-Trigger Span"), this));
    postTriggerSpanRow->addWidget(postTriggerSpanComboBox);
    postTriggerSpanRow->addStretch(1);

    QHBoxLayout *binSizeRow = new QHBoxLayout;
    binSizeRow->addWidget(new QLabel(tr("Bin Size"), this));
    binSizeRow->addWidget(binSizeComboBox);
    binSizeRow->addStretch(1);

    QVBoxLayout *timeScaleColumn = new QVBoxLayout;
    timeScaleColumn->addLayout(preTriggerSpanRow);
    timeScaleColumn->addLayout(postTriggerSpanRow);
    timeScaleColumn->addLayout(binSizeRow);
    QGroupBox *timeScaleGroup = new QGroupBox(tr("Time Scale"), this);
    timeScaleGroup->setLayout(timeScaleColumn);

    QHBoxLayout *maxNumTrialsRow = new QHBoxLayout;
    maxNumTrialsRow->addWidget(new QLabel(tr("Max Number of Trials"), this));
    maxNumTrialsRow->addWidget(maxNumTrialsComboBox);
    maxNumTrialsRow->addStretch(1);

    QVBoxLayout *trialsColumn = new QVBoxLayout;
    trialsColumn->addLayout(maxNumTrialsRow);
    trialsColumn->addWidget(clearLastTrialPushButton);
    trialsColumn->addWidget(clearAllTrialsPushButton);
    QGroupBox *trialsGroup = new QGroupBox(tr("Trials"), this);
    trialsGroup->setLayout(trialsColumn);

    QHBoxLayout *saveLayout = new QHBoxLayout;
    saveLayout->addWidget(configSaveButton);
    saveLayout->addWidget(saveButton);

    QVBoxLayout *leftColumn = new QVBoxLayout;
    leftColumn->addWidget(channelGroup);
    leftColumn->addWidget(triggerSettingsGroup);
    leftColumn->addWidget(trialsGroup);
    leftColumn->addWidget(timeScaleGroup);
    leftColumn->addStretch(1);
    leftColumn->addLayout(saveLayout);

    QVBoxLayout *rightColumn = new QVBoxLayout;
    rightColumn->addWidget(psthPlot);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addLayout(leftColumn);
    mainLayout->addLayout(rightColumn);
    mainLayout->setStretch(0, 0);
    mainLayout->setStretch(1, 1);

    setLayout(mainLayout);

    state->updateForChangeHeadstages();

    if (state->running) updateForRun();
    else updateForStop();
}

PSTHDialog::~PSTHDialog()
{
    delete psthPlot;
}

void PSTHDialog::updateFromState()
{
    psthPlot->updateFromState();

    // Check if single channel selection has changed at all.
    QString channelNativeName = state->signalSources->singleSelectedAmplifierChannelName();
    if (channelNativeName != state->psthChannel->getValue()) {
        if (!channelNativeName.isEmpty()) {
            setToSelectedButton->setEnabled(true);
            if (lockScopeCheckbox->isChecked()) {
                changeCurrentChannel(channelNativeName);
            }
        } else {
            setToSelectedButton->setEnabled(false);
        }
    }

    // Update channel name label.
    if (channelName->text() != state->psthChannel->getValue()) {
        channelName->setText(state->signalSources->getNativeAndCustomNames(state->psthChannel->getValue()));
    }
    if (channelName->text().isEmpty()) {
        channelName->setText(tr("N/A"));
    }

    // Update PSTH plot if channel has changed.
    if (psthPlot->getWaveform() != state->psthChannel->getValue())
        psthPlot->setWaveform(state->psthChannel->getValue().toStdString());

    if (preTriggerSpanComboBox->currentIndex() != state->tSpanPreTriggerPSTH->getIndex())
        preTriggerSpanComboBox->setCurrentIndex(state->tSpanPreTriggerPSTH->getIndex());

    if (postTriggerSpanComboBox->currentIndex() != state->tSpanPostTriggerPSTH->getIndex())
        postTriggerSpanComboBox->setCurrentIndex(state->tSpanPostTriggerPSTH->getIndex());

    if (binSizeComboBox->currentIndex() != state->binSizePSTH->getIndex())
        binSizeComboBox->setCurrentIndex(state->binSizePSTH->getIndex());

    if (maxNumTrialsComboBox->currentIndex() != state->maxNumTrialsPSTH->getIndex())
        maxNumTrialsComboBox->setCurrentIndex(state->maxNumTrialsPSTH->getIndex());

    if (digitalTriggerComboBox->currentIndex() != state->digitalTriggerPSTH->getIndex())
        digitalTriggerComboBox->setCurrentIndex(state->digitalTriggerPSTH->getIndex());

    if (triggerPolarityComboBox->currentIndex() != state->triggerPolarityPSTH->getIndex())
        triggerPolarityComboBox->setCurrentIndex(state->triggerPolarityPSTH->getIndex());

    updateTitle();
}

void PSTHDialog::activate()
{
    updateFromState();
    psthPlot->resetPSTH();
    show();
    raise();
    activateWindow();
}

void PSTHDialog::updatePSTH(WaveformFifo *waveformFifo, int numSamples)
{
    if (isHidden()) return;
    psthPlot->updateWaveforms(waveformFifo, numSamples);
}

void PSTHDialog::setToSelected()
{
    QString channelNativeName = state->signalSources->singleSelectedAmplifierChannelName();
    if (channelNativeName.isEmpty()) {
        qDebug() << "Error: Somehow this was triggered without a single channel being selected";
        return;
    }
    changeCurrentChannel(channelNativeName);
    psthPlot->setFocus();
}

void PSTHDialog::updateForRun()
{
    configSaveButton->setEnabled(false);
    saveButton->setEnabled(false);
}

void PSTHDialog::updateForLoad()
{
    updateForRun();
}

void PSTHDialog::updateForStop()
{
    configSaveButton->setEnabled(true);
    bool anySaveFilesSelected =
            state->saveCsvFilePSTH->getValue() ||
            state->saveMatFilePSTH->getValue() ||
            state->savePngFilePSTH->getValue();
    saveButton->setEnabled(anySaveFilesSelected);
}

void PSTHDialog::updateForChangeHeadstages()
{
    psthPlot->setWaveform(state->psthChannel->getValue().toStdString());
}

void PSTHDialog::configSave()
{
    PSTHSaveConfigDialog configDialog(state, this);

    if (configDialog.exec()) {
        state->saveCsvFilePSTH->setValue(configDialog.csvFileCheckBox->isChecked());
        state->saveMatFilePSTH->setValue(configDialog.matFileCheckBox->isChecked());
        state->savePngFilePSTH->setValue(configDialog.pngFileCheckBox->isChecked());
    }
    psthPlot->setFocus();
}

void PSTHDialog::saveData()
{
    QSettings settings;
    QString defaultDirectory = settings.value("saveDirectory", ".").toString();
    QString saveFileName = QFileDialog::getSaveFileName(this, tr("Save Data As"), defaultDirectory);

    if (!saveFileName.isEmpty()) {
        QFileInfo saveFileInfo(saveFileName);
        settings.setValue("saveDirectory", saveFileInfo.absolutePath());
        QString fileName = saveFileInfo.path() + "/" + saveFileInfo.completeBaseName();

        if (state->saveMatFilePSTH->getValue()) {
            psthPlot->saveMatFile(fileName + ".mat");
        }

        if (state->saveCsvFilePSTH->getValue()) {
            psthPlot->saveCsvFile(fileName + ".csv");
        }

        if (state->savePngFilePSTH->getValue()) {
            psthPlot->savePngFile(fileName + ".png");
        }
    }
    psthPlot->setFocus();
}

void PSTHDialog::updateTitle()
{
    setWindowTitle(tr("Per-Stimulus Time Histogram") + " (" + state->psthChannel->getValue() + ")");
}


PSTHSaveConfigDialog::PSTHSaveConfigDialog(SystemState* state_, QWidget *parent) :
    QDialog(parent),
    state(state_)
{
    QLabel* label1 = new QLabel(tr("Select the file types to be created when data is saved.  "
                                   "Multiple file types may be selected."), this);

    matFileCheckBox = new QCheckBox(tr("MATLAB MAT-File Data File (can be read into Python using scipy.io.loadmat)"), this);
    csvFileCheckBox = new QCheckBox(tr("CSV Text File (can be read into spreadsheet software)"), this);
    pngFileCheckBox = new QCheckBox(tr("PNG Screen Capture"), this);

    matFileCheckBox->setChecked(state->saveMatFilePSTH->getValue());
    csvFileCheckBox->setChecked(state->saveCsvFilePSTH->getValue());
    pngFileCheckBox->setChecked(state->savePngFilePSTH->getValue());

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QHBoxLayout *buttonBoxRow = new QHBoxLayout;
    buttonBoxRow->addStretch();
    buttonBoxRow->addWidget(buttonBox);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(label1);
    mainLayout->addWidget(matFileCheckBox);
    mainLayout->addWidget(csvFileCheckBox);
    mainLayout->addWidget(pngFileCheckBox);
    mainLayout->addLayout(buttonBoxRow);
    setLayout(mainLayout);

    setWindowTitle(tr("PSTH Save Data Configuration"));
}

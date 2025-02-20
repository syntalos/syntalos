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
#include "isidialog.h"

ISIDialog::ISIDialog(SystemState *state_, QWidget *parent) :
    QDialog(parent),
    state(state_)
{
    connect(state, SIGNAL(stateChanged()), this, SLOT(updateFromState()));

    channelName = new QLabel("", this);

    lockScopeCheckbox = new QCheckBox(tr("Lock Plot to Selected"), this);
    connect(lockScopeCheckbox, SIGNAL(clicked(bool)), this, SLOT(toggleLock()));

    setToSelectedButton = new QPushButton(tr("Set to Selected"), this);
    connect(setToSelectedButton, SIGNAL(clicked()), this, SLOT(setToSelected()));

    timeSpanComboBox = new QComboBox(this);
    state->tSpanISI->setupComboBox(timeSpanComboBox);
    connect(timeSpanComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setTimeSpan(int)));

    binSizeComboBox = new QComboBox(this);
    state->binSizeISI->setupComboBox(binSizeComboBox);
    connect(binSizeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setBinSize(int)));

    yAxisButtonGroup = new QButtonGroup(this);
    yAxisLinearRadioButton = new QRadioButton(tr("Linear Scale"), this);
    yAxisLogRadioButton = new QRadioButton(tr("Log Scale"), this);
    yAxisButtonGroup->addButton(yAxisLinearRadioButton, 0);
    yAxisButtonGroup->addButton(yAxisLogRadioButton, 1);

    QVBoxLayout* yAxisColumn = new QVBoxLayout;
    yAxisColumn->addWidget(yAxisLinearRadioButton);
    yAxisColumn->addWidget(yAxisLogRadioButton);
    yAxisLinearRadioButton->setChecked(true);
    QGroupBox* yAxisGroup = new QGroupBox(tr("Y Axis"), this);
    yAxisGroup->setLayout(yAxisColumn);
    connect(yAxisButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(changeYAxisMode(int)));

    clearISIPushButton = new QPushButton(tr("Clear ISI Plot"), this);
    connect(clearISIPushButton, SIGNAL(clicked(bool)), this, SLOT(clearISI()));

    configSaveButton = new QPushButton(tr("Config"), this);
    connect(configSaveButton, SIGNAL(clicked()), this, SLOT(configSave()));

    saveButton = new QPushButton(tr("Save Data"), this);
    connect(saveButton, SIGNAL(clicked()), this, SLOT(saveData()));

    isiPlot = new ISIPlot(state, this);

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

    QHBoxLayout *timeSpanRow = new QHBoxLayout;
    timeSpanRow->addWidget(new QLabel(tr("Time Span"), this));
    timeSpanRow->addWidget(timeSpanComboBox);
    timeSpanRow->addStretch(1);

    QHBoxLayout *binSizeRow = new QHBoxLayout;
    binSizeRow->addWidget(new QLabel(tr("Bin Size"), this));
    binSizeRow->addWidget(binSizeComboBox);
    binSizeRow->addStretch(1);

    QVBoxLayout *timeScaleColumn = new QVBoxLayout;
    timeScaleColumn->addLayout(timeSpanRow);
    timeScaleColumn->addLayout(binSizeRow);
    QGroupBox *timeScaleGroup = new QGroupBox(tr("Time Scale"), this);
    timeScaleGroup->setLayout(timeScaleColumn);

    QHBoxLayout *saveLayout = new QHBoxLayout;
    saveLayout->addWidget(configSaveButton);
    saveLayout->addWidget(saveButton);

    QVBoxLayout *leftColumn = new QVBoxLayout;
    leftColumn->addWidget(channelGroup);
    leftColumn->addWidget(timeScaleGroup);
    leftColumn->addWidget(yAxisGroup);
    leftColumn->addWidget(clearISIPushButton);
    leftColumn->addStretch(1);
    leftColumn->addLayout(saveLayout);

    QVBoxLayout *rightColumn = new QVBoxLayout;
    rightColumn->addWidget(isiPlot);

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

ISIDialog::~ISIDialog()
{
    delete isiPlot;
}

void ISIDialog::updateFromState()
{
    isiPlot->updateFromState();

    // Check if single channel selection has changed at all.
    QString channelNativeName = state->signalSources->singleSelectedAmplifierChannelName();
    if (channelNativeName != state->isiChannel->getValue()) {
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
    if (channelName->text() != state->isiChannel->getValue()) {
        channelName->setText(state->signalSources->getNativeAndCustomNames(state->isiChannel->getValue()));
    }
    if (channelName->text().isEmpty()) {
        channelName->setText(tr("N/A"));
    }

    // Update ISI plot if channel has changed.
    if (isiPlot->getWaveform() != state->isiChannel->getValue())
        isiPlot->setWaveform(state->isiChannel->getValue().toStdString());

    if (timeSpanComboBox->currentIndex() != state->tSpanISI->getIndex())
        timeSpanComboBox->setCurrentIndex(state->tSpanISI->getIndex());

    if (binSizeComboBox->currentIndex() != state->binSizeISI->getIndex())
        binSizeComboBox->setCurrentIndex(state->binSizeISI->getIndex());

    updateTitle();
}

void ISIDialog::activate()
{
    updateFromState();
    isiPlot->resetISI();
    show();
    raise();
    activateWindow();
}

void ISIDialog::updateISI(WaveformFifo *waveformFifo, int numSamples)
{
    if (isHidden()) return;
    isiPlot->updateWaveforms(waveformFifo, numSamples);
}

void ISIDialog::setToSelected()
{
    QString channelNativeName = state->signalSources->singleSelectedAmplifierChannelName();
    if (channelNativeName.isEmpty()) {
        qDebug() << "Error: Somehow this was triggered without a single channel being selected";
        return;
    }
    changeCurrentChannel(channelNativeName);
    isiPlot->setFocus();
}

void ISIDialog::updateForRun()
{
    configSaveButton->setEnabled(false);
    saveButton->setEnabled(false);
}

void ISIDialog::updateForLoad()
{
    updateForRun();
}

void ISIDialog::updateForStop()
{
    configSaveButton->setEnabled(true);
    bool anySaveFilesSelected =
            state->saveCsvFileISI->getValue() ||
            state->saveMatFileISI->getValue() ||
            state->savePngFileISI->getValue();
    saveButton->setEnabled(anySaveFilesSelected);
}

void ISIDialog::updateForChangeHeadstages()
{
    isiPlot->setWaveform(state->isiChannel->getValue().toStdString());
}

void ISIDialog::configSave()
{
    ISISaveConfigDialog configDialog(state, this);

    if (configDialog.exec()) {
        state->saveCsvFileISI->setValue(configDialog.csvFileCheckBox->isChecked());
        state->saveMatFileISI->setValue(configDialog.matFileCheckBox->isChecked());
        state->savePngFileISI->setValue(configDialog.pngFileCheckBox->isChecked());
    }
    isiPlot->setFocus();
}

void ISIDialog::saveData()
{
    QSettings settings;
    QString defaultDirectory = settings.value("saveDirectory", ".").toString();
    QString saveFileName = QFileDialog::getSaveFileName(this, tr("Save Data As"), defaultDirectory);

    if (!saveFileName.isEmpty()) {
        QFileInfo saveFileInfo(saveFileName);
        settings.setValue("saveDirectory", saveFileInfo.absolutePath());
        QString fileName = saveFileInfo.path() + "/" + saveFileInfo.completeBaseName();

        if (state->saveMatFileISI->getValue()) {
            isiPlot->saveMatFile(fileName + ".mat");
        }

        if (state->saveCsvFileISI->getValue()) {
            isiPlot->saveCsvFile(fileName + ".csv");
        }

        if (state->savePngFileISI->getValue()) {
            isiPlot->savePngFile(fileName + ".png");
        }
    }
    isiPlot->setFocus();
}

void ISIDialog::updateTitle()
{
    setWindowTitle(tr("Inter-Spike Interval Histogram") + " (" + state->isiChannel->getValue() + ")");
}


ISISaveConfigDialog::ISISaveConfigDialog(SystemState* state_, QWidget *parent) :
    QDialog(parent),
    state(state_)
{
    QLabel* label1 = new QLabel(tr("Select the file types to be created when data is saved.  "
                                   "Multiple file types may be selected."), this);

    matFileCheckBox = new QCheckBox(tr("MATLAB MAT-File Data File (can be read into Python using scipy.io.loadmat)"), this);
    csvFileCheckBox = new QCheckBox(tr("CSV Text File (can be read into spreadsheet software)"), this);
    pngFileCheckBox = new QCheckBox(tr("PNG Screen Capture"), this);

    matFileCheckBox->setChecked(state->saveMatFileISI->getValue());
    csvFileCheckBox->setChecked(state->saveCsvFileISI->getValue());
    pngFileCheckBox->setChecked(state->savePngFileISI->getValue());

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

    setWindowTitle(tr("ISI Save Data Configuration"));
}

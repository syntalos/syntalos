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
#include <QSettings>
#include "signalsources.h"
#include "spectrogramdialog.h"

SpectrogramDialog::SpectrogramDialog(SystemState* state_, QWidget *parent) :
    QDialog(parent),
    state(state_)
{
    setWindowTitle(tr("Spectrogram"));
    connect(state, SIGNAL(stateChanged()), this, SLOT(updateFromState()));

    channelName = new QLabel("", this);

    lockScopeCheckbox = new QCheckBox(tr("Lock Plot to Selected"), this);
    connect(lockScopeCheckbox, SIGNAL(clicked(bool)), this, SLOT(toggleLock()));

    setToSelectedButton = new QPushButton(tr("Set to Selected"), this);
    connect(setToSelectedButton, SIGNAL(clicked()), this, SLOT(setToSelected()));

    nFftSlider = new QSlider(Qt::Horizontal, this);
    nFftSlider->setRange(0, state->fftSizeSpectrogram->numberOfItems() - 1);
    nFftSlider->setSliderPosition(state->fftSizeSpectrogram->getIndex());
    nFftSlider->setTickPosition(QSlider::TicksBelow);
    nFftSlider->setPageStep(1);
    connect(nFftSlider, SIGNAL(valueChanged(int)), this, SLOT(setNumFftPoints(int)));

    deltaTimeLabel = new QLabel(EnDashSymbol + tr(" ms"), this);
    deltaFreqLabel = new QLabel(EnDashSymbol + tr(" Hz"), this);

    fMaxSpinBox = new QSpinBox(this);
    fMaxSpinBox->setSingleStep(10);
    fMaxSpinBox->setSuffix(" Hz");
    state->fMaxSpectrogram->setupSpinBox(fMaxSpinBox);
    connect(fMaxSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setFMax(int)));

    fMinSpinBox = new QSpinBox(this);
    fMinSpinBox->setSingleStep(10);
    fMinSpinBox->setSuffix(" Hz");
    state->fMinSpectrogram->setupSpinBox(fMinSpinBox);
    connect(fMinSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setFMin(int)));

    fMarkerLabel = new QLabel(tr("Marker"), this);
    fMarkerSpinBox = new QSpinBox(this);
    fMarkerSpinBox->setSingleStep(10);
    fMarkerSpinBox->setSuffix(" Hz");
    state->fMarkerSpectrogram->setupSpinBox(fMarkerSpinBox);
    connect(fMarkerSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setFMarker(int)));

    fMarkerShowCheckBox = new QCheckBox(tr("Show Marker"), this);
    connect(fMarkerShowCheckBox, SIGNAL(clicked(bool)), this, SLOT(toggleShowFMarker(bool)));

    fMarkerHarmonicsLabel1 = new QLabel(tr("Show"), this);
    fMarkerHarmonicsLabel2 = new QLabel(tr("Harmonics"), this);
    fMarkerHarmonicsSpinBox = new QSpinBox(this);
    state->numHarmonicsSpectrogram->setupSpinBox(fMarkerHarmonicsSpinBox);
    connect(fMarkerHarmonicsSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setNumHarmonics(int)));

    timeScaleLabel = new QLabel(tr("Time Scale"), this);
    timeScaleComboBox = new QComboBox(this);
    state->tScaleSpectrogram->setupComboBox(timeScaleComboBox);
    connect(timeScaleComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setTimeScale(int)));

    digitalDisplayComboBox = new QComboBox(this);
    state->digitalDisplaySpectrogram->setupComboBox(digitalDisplayComboBox);
    connect(digitalDisplayComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setDigitalDisplay(int)));

    configSaveButton = new QPushButton(tr("Config"), this);
    connect(configSaveButton, SIGNAL(clicked()), this, SLOT(configSave()));

    saveButton = new QPushButton(tr("Save Data"), this);
    connect(saveButton, SIGNAL(clicked()), this, SLOT(saveData()));

    specPlot = new SpectrogramPlot(state, this);

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

    spectrogramRadioButton = new QRadioButton(tr("Spectrogram"), this);
    spectrumRadioButton = new QRadioButton(tr("Spectrum"), this);

    displayModeButtonGroup = new QButtonGroup(this);
    displayModeButtonGroup->addButton(spectrogramRadioButton, 0);
    displayModeButtonGroup->addButton(spectrumRadioButton, 1);

    QVBoxLayout* modeColumn = new QVBoxLayout;
    modeColumn->addWidget(spectrogramRadioButton);
    modeColumn->addWidget(spectrumRadioButton);
    spectrogramRadioButton->setChecked(true);
    QGroupBox* modeGroup = new QGroupBox(tr("Display Mode"), this);
    modeGroup->setLayout(modeColumn);
    connect(displayModeButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(changeDisplayMode(int)));

    QHBoxLayout *deltaTimeFreqRow = new QHBoxLayout;
    deltaTimeFreqRow->addWidget(new QLabel(DeltaSymbol + tr("t:"), this));
    deltaTimeFreqRow->addWidget(deltaTimeLabel);
    deltaTimeFreqRow->addStretch();
    deltaTimeFreqRow->addWidget(new QLabel(DeltaSymbol + tr("f:"), this));
    deltaTimeFreqRow->addWidget(deltaFreqLabel);

    QHBoxLayout *fMaxRow = new QHBoxLayout;
    fMaxRow->addWidget(new QLabel(tr("Max Freq"), this));
    fMaxRow->addWidget(fMaxSpinBox);
    fMaxRow->addStretch(1);

    QHBoxLayout *fMinRow = new QHBoxLayout;
    fMinRow->addWidget(new QLabel(tr("Min Freq"), this));
    fMinRow->addWidget(fMinSpinBox);
    fMinRow->addStretch(1);

    QHBoxLayout *fMarkerRow = new QHBoxLayout;
    fMarkerRow->addWidget(fMarkerLabel);
    fMarkerRow->addWidget(fMarkerSpinBox);
    fMarkerRow->addStretch(1);

    QHBoxLayout *fMarkerHarmonicsRow = new QHBoxLayout;
    fMarkerHarmonicsRow->addWidget(fMarkerHarmonicsLabel1);
    fMarkerHarmonicsRow->addWidget(fMarkerHarmonicsSpinBox);
    fMarkerHarmonicsRow->addWidget(fMarkerHarmonicsLabel2);
    fMarkerHarmonicsRow->addStretch(1);

    QHBoxLayout *timeScaleRow = new QHBoxLayout;
    timeScaleRow->addWidget(timeScaleLabel);
    timeScaleRow->addWidget(timeScaleComboBox);
    timeScaleRow->addStretch(1);

    QVBoxLayout *timeFreqLayout = new QVBoxLayout;
    timeFreqLayout->addLayout(deltaTimeFreqRow);
    timeFreqLayout->addWidget(nFftSlider);
    QGroupBox *timeFreqGroup = new QGroupBox(tr("Time/Frequency Resolution"), this);
    timeFreqGroup->setLayout(timeFreqLayout);

    QVBoxLayout *freqScaleLayout = new QVBoxLayout;
    freqScaleLayout->addLayout(fMaxRow);
    freqScaleLayout->addLayout(fMinRow);
    freqScaleLayout->addWidget(fMarkerShowCheckBox);
    freqScaleLayout->addLayout(fMarkerRow);
    freqScaleLayout->addLayout(fMarkerHarmonicsRow);
    QGroupBox *freqScaleGroup = new QGroupBox(tr("Frequency Scale"), this);
    freqScaleGroup->setLayout(freqScaleLayout);

    QHBoxLayout *digitalDisplayLayout = new QHBoxLayout;
    digitalDisplayLayout->addWidget(digitalDisplayComboBox);
    digitalDisplayLayout->addStretch(1);
    QGroupBox *digitalDisplayGroup = new QGroupBox(tr("Digital Signal Display"), this);
    digitalDisplayGroup->setLayout(digitalDisplayLayout);

    QHBoxLayout *saveLayout = new QHBoxLayout;
    saveLayout->addWidget(configSaveButton);
    saveLayout->addWidget(saveButton);

    QVBoxLayout *leftColumn = new QVBoxLayout;
    leftColumn->addWidget(channelGroup);
    leftColumn->addWidget(modeGroup);
    leftColumn->addWidget(timeFreqGroup);
    leftColumn->addWidget(freqScaleGroup);
    leftColumn->addLayout(timeScaleRow);
    leftColumn->addWidget(digitalDisplayGroup);
    leftColumn->addStretch(1);
    leftColumn->addLayout(saveLayout);

    QVBoxLayout *rightColumn = new QVBoxLayout;
    rightColumn->addWidget(specPlot);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addLayout(leftColumn);
    mainLayout->addLayout(rightColumn);
    mainLayout->setStretch(0, 0);
    mainLayout->setStretch(1, 1);

    setLayout(mainLayout);

    updateDeltaTimeFreqLabels();

    state->updateForChangeHeadstages();

    if (state->running) updateForRun();
    else updateForStop();
}

SpectrogramDialog::~SpectrogramDialog()
{
    delete specPlot;
}

void SpectrogramDialog::updateFromState()
{
    specPlot->updateFromState();

    // Check if single channel selection has changed at all.
    QString channelNativeName = state->signalSources->singleSelectedAmplifierChannelName();
    if (channelNativeName != state->spectrogramChannel->getValue()) {
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
    if (channelName->text() != state->spectrogramChannel->getValue()) {
        channelName->setText(state->signalSources->getNativeAndCustomNames(state->spectrogramChannel->getValue()));
    }
    if (channelName->text().isEmpty()) {
        channelName->setText(tr("N/A"));
    }

    // Update spectrogram plot if channel has changed.
    if (specPlot->getWaveform() != state->spectrogramChannel->getValue())
        specPlot->setWaveform(state->spectrogramChannel->getValue().toStdString());

    if (nFftSlider->value() != state->fftSizeSpectrogram->getIndex())
        nFftSlider->setSliderPosition(state->fftSizeSpectrogram->getIndex());

    if (fMinSpinBox->value() != state->fMinSpectrogram->getValue())
        fMinSpinBox->setValue(state->fMinSpectrogram->getValue());

    if (fMaxSpinBox->value() != state->fMaxSpectrogram->getValue())
        fMaxSpinBox->setValue(state->fMaxSpectrogram->getValue());

    if (fMarkerSpinBox->value() != state->fMarkerSpectrogram->getValue())
        fMarkerSpinBox->setValue(state->fMarkerSpectrogram->getValue());

    if (timeScaleComboBox->currentIndex() != state->tScaleSpectrogram->getIndex())
        timeScaleComboBox->setCurrentIndex(state->tScaleSpectrogram->getIndex());

    if (displayModeButtonGroup->checkedId() != state->displayModeSpectrogram->getIndex()) {
        if (state->displayModeSpectrogram->getIndex() == 0) spectrogramRadioButton->setChecked(true);
        else spectrumRadioButton->setChecked(true);
    }

    if (fMarkerShowCheckBox->isChecked() != state->showFMarkerSpectrogram->getValue())
        fMarkerShowCheckBox->setChecked(state->showFMarkerSpectrogram->getValue());

    if (fMarkerHarmonicsSpinBox->value() != state->numHarmonicsSpectrogram->getValue())
        fMarkerHarmonicsSpinBox->setValue(state->numHarmonicsSpectrogram->getValue());

    if (digitalDisplayComboBox->currentIndex() != state->digitalDisplaySpectrogram->getIndex())
        digitalDisplayComboBox->setCurrentIndex(state->digitalDisplaySpectrogram->getIndex());

    updateDeltaTimeFreqLabels();
}

void SpectrogramDialog::activate()
{
    updateFromState();
    specPlot->resetSpectrogram();
    show();
    raise();
    activateWindow();
}

void SpectrogramDialog::updateSpectrogram(WaveformFifo *waveformFifo, int numSamples)
{
    if (this->isHidden()) return;
    specPlot->updateWaveforms(waveformFifo, numSamples);
}

void SpectrogramDialog::changeDisplayMode(int index)
{
    bool timeScaleOn = (index == 0);
    timeScaleLabel->setEnabled(timeScaleOn);
    timeScaleComboBox->setEnabled(timeScaleOn);
    digitalDisplayComboBox->setEnabled(timeScaleOn);
    state->displayModeSpectrogram->setIndex(index);
}

void SpectrogramDialog::setFMin(int fMin)
{
    if (fMaxSpinBox->value() - fMin < FSpanMin) {
        fMin = fMaxSpinBox->value() - FSpanMin;
        fMinSpinBox->setValue(fMin);
    } else {
        state->fMinSpectrogram->setValue(fMin);
    }
}

void SpectrogramDialog::setFMax(int fMax)
{
    if (fMax - fMinSpinBox->value() < FSpanMin) {
        fMax = fMinSpinBox->value() + FSpanMin;
        fMaxSpinBox->setValue(fMax);
    } else {
        state->fMaxSpectrogram->setValue(fMax);
    }
}

void SpectrogramDialog::toggleShowFMarker(bool enabled)
{
    fMarkerSpinBox->setEnabled(enabled);
    fMarkerLabel->setEnabled(enabled);
    fMarkerHarmonicsSpinBox->setEnabled(enabled);
    fMarkerHarmonicsLabel1->setEnabled(enabled);
    fMarkerHarmonicsLabel2->setEnabled(enabled);
    state->showFMarkerSpectrogram->setValue(enabled);
}

void SpectrogramDialog::setToSelected()
{
    QString channelNativeName = state->signalSources->singleSelectedAmplifierChannelName();
    if (channelNativeName.isEmpty()) {
        qDebug() << "Error: Somehow this was triggered without a single channel being selected";
        return;
    }
    changeCurrentChannel(channelNativeName);
    specPlot->setFocus();
}

void SpectrogramDialog::updateDeltaTimeFreqLabels()
{
    double deltaTimeMsec = specPlot->getDeltaTimeMsec();
    double deltaFreqHz = specPlot->getDeltaFreqHz();

    int digitsAfterDecimalTime = deltaTimeMsec >= 100.0 ? 0 : 1;
    int digitsAfterDecimalFreq = deltaFreqHz >= 10.0 ? 1 : 2;
    if (deltaFreqHz >= 100.0) digitsAfterDecimalFreq = 0;

    deltaTimeLabel->setText(QString::number(deltaTimeMsec, 'f', digitsAfterDecimalTime) + tr(" ms"));
    deltaFreqLabel->setText(QString::number(deltaFreqHz, 'f', digitsAfterDecimalFreq) + tr(" Hz"));
}

void SpectrogramDialog::updateForRun()
{
    configSaveButton->setEnabled(false);
    saveButton->setEnabled(false);
}

void SpectrogramDialog::updateForLoad()
{
    updateForRun();
}

void SpectrogramDialog::updateForStop()
{
    configSaveButton->setEnabled(true);
    bool anySaveFilesSelected =
            state->saveCsvFileSpectrogram->getValue() ||
            state->saveMatFileSpectrogram->getValue() ||
            state->savePngFileSpectrogram->getValue();
    saveButton->setEnabled(anySaveFilesSelected);
}

void SpectrogramDialog::updateForChangeHeadstages()
{
    specPlot->setWaveform(state->spectrogramChannel->getValue().toStdString());
}

void SpectrogramDialog::configSave()
{
    SpectrogramSaveConfigDialog configDialog(state, this);

    if (configDialog.exec()) {
        state->saveCsvFileSpectrogram->setValue(configDialog.csvFileCheckBox->isChecked());
        state->saveMatFileSpectrogram->setValue(configDialog.matFileCheckBox->isChecked());
        state->savePngFileSpectrogram->setValue(configDialog.pngFileCheckBox->isChecked());
    }
    specPlot->setFocus();
}

void SpectrogramDialog::saveData()
{
    QSettings settings;
    QString defaultDirectory = settings.value("saveDirectory", ".").toString();
    QString saveFileName = QFileDialog::getSaveFileName(this, tr("Save Data As"), defaultDirectory);

    if (!saveFileName.isEmpty()) {
        QFileInfo saveFileInfo(saveFileName);
        settings.setValue("saveDirectory", saveFileInfo.absolutePath());
        QString fileName = saveFileInfo.path() + "/" + saveFileInfo.completeBaseName();

        if (state->saveMatFileSpectrogram->getValue()) {
            specPlot->saveMatFile(fileName + ".mat");
        }

        if (state->saveCsvFileSpectrogram->getValue()) {
            specPlot->saveCsvFile(fileName + ".csv");
        }

        if (state->savePngFileSpectrogram->getValue()) {
            specPlot->savePngFile(fileName + ".png");
        }
    }
    specPlot->setFocus();
}


SpectrogramSaveConfigDialog::SpectrogramSaveConfigDialog(SystemState* state_, QWidget *parent) :
    QDialog(parent),
    state(state_)
{
    QLabel* label1 = new QLabel(tr("Select the file types to be created when data is saved.  "
                                   "Multiple file types may be selected."), this);

    matFileCheckBox = new QCheckBox(tr("MATLAB MAT-File Data File (can be read into Python using scipy.io.loadmat)"), this);
    csvFileCheckBox = new QCheckBox(tr("CSV Text File (can be read into spreadsheet software)"), this);
    pngFileCheckBox = new QCheckBox(tr("PNG Screen Capture"), this);

    matFileCheckBox->setChecked(state->saveMatFileSpectrogram->getValue());
    csvFileCheckBox->setChecked(state->saveCsvFileSpectrogram->getValue());
    pngFileCheckBox->setChecked(state->savePngFileSpectrogram->getValue());

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

    setWindowTitle(tr("Spectrogram Save Data Configuration"));
}

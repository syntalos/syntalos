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
#include "spikesortingdialog.h"

SpikeSortingDialog:: SpikeSortingDialog(SystemState* state_, ControllerInterface* controllerInterface_, QWidget *parent) :
    QDialog(parent),
    state(state_),
    controllerInterface(controllerInterface_)
{
    setAcceptDrops(true);
    connect(state, SIGNAL(stateChanged()), this, SLOT(updateFromState()));

    channelName = new QLabel("", this);

    lockScopeCheckbox = new QCheckBox(tr("Lock Plot to Selected"), this);
    connect(lockScopeCheckbox, SIGNAL(clicked()), this, SLOT(toggleLock()));

    setToSelectedButton = new QPushButton(tr("Set to Selected"), this);
    connect(setToSelectedButton, SIGNAL(clicked()), this, SLOT(setToSelected()));

    thresholdSpinBox = new QSpinBox(this);
    thresholdSpinBox->setSingleStep(5);
    thresholdSpinBox->setSuffix(" " + MicroVoltsSymbol);

    voltageScaleComboBox = new QComboBox(this);
    state->yScaleSpikeScope->setupComboBox(voltageScaleComboBox);
    connect(voltageScaleComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setVoltageScale(int)));

    timeScaleComboBox = new QComboBox(this);
    state->tScaleSpikeScope->setupComboBox(timeScaleComboBox);
    connect(timeScaleComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setTimeScale(int)));

    showSpikesComboBox = new QComboBox(this);
    state->numSpikesDisplayed->setupComboBox(showSpikesComboBox);
    connect(showSpikesComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setNumSpikesDisplayed(int)));

    clearScopeButton = new QPushButton(tr("Clear Scope"), this);
    connect(clearScopeButton, SIGNAL(clicked(bool)), this, SLOT(clearScope()));

    takeSnapshotButton = new QPushButton(tr("Take Snapshot"), this);
    connect(takeSnapshotButton, SIGNAL(clicked(bool)), this, SLOT(takeSnapshot()));

    clearSnapshotButton = new QPushButton(tr("Clear Snapshot"), this);
    connect(clearSnapshotButton, SIGNAL(clicked(bool)), this, SLOT(clearSnapshot()));

    QGroupBox *artifactSuppressionBox = new QGroupBox(tr("Artifact Suppression"), this);

    suppressionCheckBox = new QCheckBox(tr("Enable Suppression"), this);
    connect(suppressionCheckBox, SIGNAL(clicked(bool)), this, SLOT(toggleSuppressionEnabled(bool)));

    artifactsShownCheckBox = new QCheckBox(tr("Show Artifacts"), this);
    connect(artifactsShownCheckBox, SIGNAL(clicked(bool)), this, SLOT(toggleArtifactsShown(bool)));

    suppressionThresholdLabel1 = new QLabel(tr("Artifact Threshold"), this);
    suppressionThresholdSpinBox = new QSpinBox(this);
    suppressionThresholdSpinBox->setSingleStep(10);
    suppressionThresholdSpinBox->setSuffix(" " + MicroVoltsSymbol);
    state->suppressionThreshold->setupSpinBox(suppressionThresholdSpinBox);
    connect(suppressionThresholdSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setSuppressionThreshold()));

    QVBoxLayout *artifactSuppressionLayout = new QVBoxLayout;
    QHBoxLayout *suppressionEnabledRow = new QHBoxLayout;
    suppressionEnabledRow->addWidget(suppressionCheckBox);
    QHBoxLayout *artifactsShownRow = new QHBoxLayout;
    artifactsShownRow->addWidget(artifactsShownCheckBox);
    QHBoxLayout *suppressionThresholdRow = new QHBoxLayout;
    suppressionThresholdRow->addWidget(suppressionThresholdLabel1);
    suppressionThresholdRow->addWidget(suppressionThresholdSpinBox);
    suppressionThresholdRow->addStretch(1);
    artifactSuppressionLayout->addLayout(suppressionEnabledRow);
    artifactSuppressionLayout->addLayout(artifactsShownRow);
    artifactSuppressionLayout->addLayout(suppressionThresholdRow);

    artifactSuppressionBox->setLayout(artifactSuppressionLayout);

    // Currently, deactivated any user hoop selection.
    //QGroupBox *hoopSpikeSortingBox = new QGroupBox("Hoop Spike Sorting", this);
    //QCheckBox *useHoopsCheckBox = new QCheckBox("Hoop Sorting Enabled", this);

//    QVBoxLayout *hoopSpikeSortingLayout = new QVBoxLayout;
//    hoopSpikeSortingLayout->addWidget(useHoopsCheckBox);
//    hoopSpikeSortingBox->setLayout(hoopSpikeSortingLayout);

//    unitTabWidget = new QTabWidget;

//    for (int unit = 0; unit < 4; unit++) {
//        QWidget *page = new QWidget(this);
//        QVBoxLayout *pageLayout = new QVBoxLayout;
//        for (int hoop = 0; hoop < 4; hoop++) {
//            QGroupBox *hoopGroupBox = new QGroupBox(this);

//            hoopEnabledCheckBoxes.append(new QCheckBox("Hoop " + QString::number(hoop + 1), this));

//            QHBoxLayout *rowA = new QHBoxLayout;
//            QLabel *labelA = new QLabel("A: ");
//            rowA->addWidget(labelA);

//            tALabels.append(new QLabel("tA"));
//            rowA->addWidget(tALabels[unit * 4 + hoop]);

//            vALabels.append(new QLabel("vA"));
//            rowA->addWidget(vALabels[unit * 4 + hoop]);

//            QHBoxLayout *rowB = new QHBoxLayout;
//            QLabel *labelB = new QLabel("B: ");
//            rowB->addWidget(labelB);

//            tBLabels.append(new QLabel("tB"));
//            rowB->addWidget(tBLabels[unit * 4 + hoop]);

//            vBLabels.append(new QLabel("vB"));
//            rowB->addWidget(vBLabels[unit * 4 + hoop]);

//            QVBoxLayout *hoopLayout = new QVBoxLayout;
//            hoopLayout->addWidget(hoopEnabledCheckBoxes[unit * 4 + hoop]);
//            hoopLayout->addLayout(rowA);
//            hoopLayout->addLayout(rowB);

//            hoopGroupBox->setLayout(hoopLayout);
//            pageLayout->addWidget(hoopGroupBox);
//        }
//        page->setLayout(pageLayout);
//        unitTabWidget->addTab(page, "Unit " + QString::number(unit + 1));
//    }

    loadSpikeSortingParametersButton = new QPushButton(tr("Load Detection Parameters"), this);
    connect(loadSpikeSortingParametersButton, SIGNAL(clicked(bool)), this, SLOT(loadSpikeSortingParameters()));

    saveSpikeSortingParametersButton = new QPushButton(tr("Save Detection Parameters"), this);
    connect(saveSpikeSortingParametersButton, SIGNAL(clicked(bool)), this, SLOT(saveSpikeSortingParameters()));

    spikePlot = new SpikePlot(state, this);

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

    QHBoxLayout *thresholdRow = new QHBoxLayout;
    thresholdRow->addWidget(new QLabel(tr("Detection Threshold"), this));
    thresholdRow->addWidget(thresholdSpinBox);
    thresholdRow->addStretch(1);

    QVBoxLayout *detectionSettingsColumn = new QVBoxLayout;
    detectionSettingsColumn->addLayout(thresholdRow);
    QGroupBox *detectionSettingsGroup = new QGroupBox(tr("Spike Detection Settings"), this);
    detectionSettingsGroup->setLayout(detectionSettingsColumn);

    QHBoxLayout *voltageScaleRow = new QHBoxLayout;
    voltageScaleRow->addWidget(new QLabel(tr("Voltage Scale"), this));
    voltageScaleRow->addWidget(voltageScaleComboBox);
    voltageScaleRow->addStretch(1);

    QHBoxLayout *timeScaleRow = new QHBoxLayout;
    timeScaleRow->addWidget(new QLabel(tr("Time Scale"), this));
    timeScaleRow->addWidget(timeScaleComboBox);
    timeScaleRow->addStretch(1);

    QHBoxLayout *showSpikesRow = new QHBoxLayout;
    showSpikesRow->addWidget(new QLabel(tr("Show"), this));
    showSpikesRow->addWidget(showSpikesComboBox);
    showSpikesRow->addWidget(new QLabel(tr("spikes"), this));
    showSpikesRow->addStretch(1);

    QHBoxLayout *snapshotRow = new QHBoxLayout;
    snapshotRow->addWidget(takeSnapshotButton);
    snapshotRow->addWidget(clearSnapshotButton);

    QVBoxLayout *displaySettingsColumn = new QVBoxLayout;
    displaySettingsColumn->addLayout(voltageScaleRow);
    displaySettingsColumn->addLayout(timeScaleRow);
    displaySettingsColumn->addLayout(showSpikesRow);
    displaySettingsColumn->addWidget(clearScopeButton);
    displaySettingsColumn->addLayout(snapshotRow);
    QGroupBox *displaySettingsGroup = new QGroupBox(tr("Display Settings"), this);
    displaySettingsGroup->setLayout(displaySettingsColumn);

    QVBoxLayout *leftColumn = new QVBoxLayout;
    leftColumn->addWidget(channelGroup);
    leftColumn->addWidget(displaySettingsGroup);
    leftColumn->addWidget(detectionSettingsGroup);
    leftColumn->addWidget(artifactSuppressionBox);
//    leftColumn->addWidget(hoopSpikeSortingBox);
//    leftColumn->addWidget(unitTabWidget);
    leftColumn->addStretch();
    leftColumn->addWidget(loadSpikeSortingParametersButton);
    leftColumn->addWidget(saveSpikeSortingParametersButton);

    QVBoxLayout *rightColumn = new QVBoxLayout;
    rightColumn->addWidget(spikePlot);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addLayout(leftColumn);
    mainLayout->addLayout(rightColumn);
    mainLayout->setStretch(0, 0);
    mainLayout->setStretch(1, 1);

    setLayout(mainLayout);

    spikeSettingsInterface = new XMLInterface(state, controllerInterface, XMLIncludeSpikeSortingParameters);

    state->updateForChangeHeadstages();

    state->signalSources->channelByName(state->spikeScopeChannel->getValue())->setupSpikeThresholdSpinBox(thresholdSpinBox);
    connect(thresholdSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setVoltageThreshold(int)));

    if (state->running) updateForRun();
    else updateForStop();
}

SpikeSortingDialog::~SpikeSortingDialog()
{
    delete spikeSettingsInterface;
    delete spikePlot;
}

void SpikeSortingDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        // Only accept events with a single local file url
        if (event->mimeData()->urls().size() == 1) {
            if (event->mimeData()->urls().at(0).isLocalFile())
                event->accept();
        }
    }
}

void SpikeSortingDialog::dropEvent(QDropEvent *event)
{
    QSettings settings;
    QString filename = event->mimeData()->urls().at(0).toLocalFile();

    QFileInfo fileInfo(filename);
    settings.setValue("spikeSortingDirectory", fileInfo.absolutePath());

    QString errorMessage;
    bool loadSuccess = spikeSettingsInterface->loadFile(filename, errorMessage);

    if (!loadSuccess) {
        QMessageBox::critical(this, tr("Error: Loading from XML"), errorMessage);
        return;
    } else if (!errorMessage.isEmpty()) {
        QMessageBox::warning(this, tr("Warning: Loading from XML"), errorMessage);
    }
}

void SpikeSortingDialog::updateFromState()
{
    // Check if single channel selection has changed at all.
    QString channelNativeName = state->signalSources->singleSelectedAmplifierChannelName();
    if (channelNativeName != state->spikeScopeChannel->getValue()) {
        if (!channelNativeName.isEmpty()) {
            setToSelectedButton->setEnabled(true);
            if (lockScopeCheckbox->isChecked()) {
                changeCurrentChannel(channelNativeName);
            }
        } else {
            setToSelectedButton->setEnabled(false); // (a) Disable setToSelectedButton.
        }
    }

    // Update channel name label.
    if (channelName->text() != state->spikeScopeChannel->getValue()) {
        channelName->setText(state->signalSources->getNativeAndCustomNames(state->spikeScopeChannel->getValue()));
    }
    if (channelName->text().isEmpty()) {
        channelName->setText(tr("N/A"));
    }

    // Update spike plot if channel has changed.
    if (spikePlot->getWaveform() != state->spikeScopeChannel->getValue()) {
        spikePlot->setWaveform(state->spikeScopeChannel->getValue().toStdString());
    }

    // Update spike threshold if spikeScopeChannel exists.
    QString channelName = state->spikeScopeChannel->getValue();

    if (channelName != "N/A") {
        if (thresholdSpinBox->value() != state->signalSources->channelByName(channelName)->getSpikeThreshold()) {
            thresholdSpinBox->setValue(state->signalSources->channelByName(channelName)->getSpikeThreshold());
        }
    }

    if (voltageScaleComboBox->currentIndex() != state->yScaleSpikeScope->getIndex())
        voltageScaleComboBox->setCurrentIndex(state->yScaleSpikeScope->getIndex());

    if (timeScaleComboBox->currentIndex() != state->tScaleSpikeScope->getIndex())
        timeScaleComboBox->setCurrentIndex(state->tScaleSpikeScope->getIndex());

    if (showSpikesComboBox->currentIndex() != state->numSpikesDisplayed->getIndex())
        showSpikesComboBox->setCurrentIndex(state->numSpikesDisplayed->getIndex());

    if (suppressionCheckBox->isChecked() != state->suppressionEnabled->getValue())
        suppressionCheckBox->setChecked(state->suppressionEnabled->getValue());

    if (artifactsShownCheckBox->isChecked() != state->artifactsShown->getValue())
        artifactsShownCheckBox->setChecked(state->artifactsShown->getValue());

    if (suppressionThresholdSpinBox->value() != state->suppressionThreshold->getValue())
        suppressionThresholdSpinBox->setValue(state->suppressionThreshold->getValue());

    updateTitle();
}

void SpikeSortingDialog::updateForRun()
{
    loadSpikeSortingParametersButton->setEnabled(false);
    saveSpikeSortingParametersButton->setEnabled(false);
}

void SpikeSortingDialog::updateForLoad()
{
    updateForRun();
}

void SpikeSortingDialog::updateForStop()
{
    loadSpikeSortingParametersButton->setEnabled(true);
    saveSpikeSortingParametersButton->setEnabled(true);
}

void SpikeSortingDialog::updateForChangeHeadstages()
{
    spikePlot->setWaveform(state->spikeScopeChannel->getValue().toStdString());
}

void SpikeSortingDialog::activate()
{
    updateFromState();
    show();
    raise();
    activateWindow();
}

void SpikeSortingDialog::updateSpikeScope(WaveformFifo *waveformFifo, int numSamples)
{
    if (isHidden()) return;
    spikePlot->updateWaveforms(waveformFifo, numSamples);
}

void SpikeSortingDialog::loadSpikeSortingParameters()
{
    QSettings settings;
    QString defaultDirectory = settings.value("spikeSortingDirectory", ".").toString();
    QString filename = QFileDialog::getOpenFileName(this, "Select Parameter File", defaultDirectory, "XML files (*.xml)");

    // If user clicked 'cancel', then abort.
    if (filename.isEmpty()) return;

    QFileInfo fileInfo(filename);
    settings.setValue("spikeSortingDirectory", fileInfo.absolutePath());

    QString errorMessage;
    bool loadSuccess = spikeSettingsInterface->loadFile(filename, errorMessage);

    if (!loadSuccess) {
        QMessageBox::critical(this, tr("Error: Loading from XML"), errorMessage);
        return;
    } else if (!errorMessage.isEmpty()) {
        QMessageBox::warning(this, tr("Warning: Loading from XML"), errorMessage);
    }
}

void SpikeSortingDialog::saveSpikeSortingParameters()
{
    QSettings settings;
    QString defaultDirectory = settings.value("spikeSortingDirectory", ".").toString();
    QString filename = QFileDialog::getSaveFileName(this, "Save Spike Sorting Parameters As", defaultDirectory, "XML files (*.xml)");

    // If user clicked 'cancel', then abort.
    if (filename.isEmpty()) return;

    QFileInfo fileInfo(filename);
    settings.setValue("spikeSortingDirectory", fileInfo.absolutePath());

    if (!spikeSettingsInterface->saveFile(filename)) qDebug() << "Failure writing XML Spike Sorting Parameters";
}

void SpikeSortingDialog::setToSelected()
{
    QString channelNativeName = state->signalSources->singleSelectedAmplifierChannelName();
    if (channelNativeName.isEmpty()) {
        qDebug() << "Error: Somehow this was triggered without a single channel being selected";
        return;
    }
    changeCurrentChannel(channelNativeName);
    spikePlot->setFocus();
}

void SpikeSortingDialog::toggleSuppressionEnabled(bool enabled)
{
    artifactsShownCheckBox->setEnabled(enabled);
    suppressionThresholdLabel1->setEnabled(enabled);
    suppressionThresholdSpinBox->setEnabled(enabled);
    state->suppressionEnabled->setValue(enabled);
}

void SpikeSortingDialog::updateTitle()
{
    setWindowTitle(tr("Spike Scope") + " (" + state->spikeScopeChannel->getValue() + ")");
}

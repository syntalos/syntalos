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

#include "stimparamdialog.h"
#include "anoutdialog.h"
#include "digoutdialog.h"
#include "controlpanelbandwidthtab.h"
#include "controlpanelimpedancetab.h"
#include "controlpanelaudioanalogtab.h"
#include "controlpanelconfiguretab.h"
#include "controlpaneltriggertab.h"
#include "controlwindow.h"
#include "controlpanel.h"

ColorWidget::ColorWidget(QWidget *parent) :
    QWidget(parent)
{
    checkerboardIcon = QIcon(":/images/checkerboard.png");

    QPixmap pixmap(16, 16);
    pixmap.fill(QColor(204, 204, 204));
    blankIcon = QIcon(pixmap);

    chooseColorToolButton = new QToolButton(this);
    chooseColorToolButton->setIcon(blankIcon);
    connect(chooseColorToolButton, SIGNAL(clicked()), this, SIGNAL(clicked()));

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(chooseColorToolButton);
    setLayout(mainLayout);
}

void ColorWidget::setColor(QColor color)
{
    QPixmap pixmap(16, 16);
    pixmap.fill(QColor(color));
    chooseColorToolButton->setIcon(QIcon(pixmap));
}


ControlPanel::ControlPanel(ControllerInterface* controllerInterface_, SystemState* state_, CommandParser* parser_,
                           ControlWindow *parent) :
    QWidget(parent),
    state(state_),
    parser(parser_),
    controlWindow(parent),
    controllerInterface(controllerInterface_),
    filterDisplaySelector(nullptr),
    hideControlPanelButton(nullptr),
    topLabel(nullptr),
    tabWidget(nullptr),
    bandwidthTab(nullptr),
    impedanceTab(nullptr),
    audioAnalogTab(nullptr),
    configureTab(nullptr),
    triggerTab(nullptr),
    stimParamDialog(nullptr),
    anOutDialog(nullptr),
    digOutDialog(nullptr),
    selectionNameLabel(nullptr),
    selectionImpedanceLabel(nullptr),
    selectionReferenceLabel(nullptr),
    selectionStimTriggerLabel(nullptr),
    colorAttribute(nullptr),
    enableCheckBox(nullptr),
    renameButton(nullptr),
    setRefButton(nullptr),
    setStimButton(nullptr),
    wideSlider(nullptr),
    lowSlider(nullptr),
    highSlider(nullptr),
    variableSlider(nullptr),
    analogSlider(nullptr),
    wideLabel(nullptr),
    lowLabel(nullptr),
    highLabel(nullptr),
    variableLabel(nullptr),
    analogLabel(nullptr),
    clipWaveformsCheckBox(nullptr),
    timeScaleComboBox(nullptr)
{
    state->writeToLog("Beginning of ControlPanel ctor");
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);

    state->writeToLog("About to create topLabel");
    topLabel = new QLabel(tr("Selection Properties"), this);
    state->writeToLog("Created topLabel. About to create FilterDisplaySelector");

    filterDisplaySelector = new FilterDisplaySelector(state, this);
    state->writeToLog("Created FilterDisplaySelector");

    hideControlPanelButton = new QToolButton(this);
    hideControlPanelButton->setIcon(QIcon(":/images/hideicon.png"));
    hideControlPanelButton->setToolTip(tr("Hide Control Panel"));
    connect(hideControlPanelButton, SIGNAL(clicked()), controlWindow, SLOT(hideControlPanel()));
    state->writeToLog("Created hideControlPanelButton");

    bandwidthTab = new ControlPanelBandwidthTab(controllerInterface, state, this);
    state->writeToLog("Created bandwidthTab");
    impedanceTab = new ControlPanelImpedanceTab(controllerInterface, state, parser, this);
    state->writeToLog("Created impedanceTab");
    audioAnalogTab = new ControlPanelAudioAnalogTab(controllerInterface, state, this);
    state->writeToLog("Created audioAnalogTab");
    configureTab = new ControlPanelConfigureTab(controllerInterface, state, parser, this);
    state->writeToLog("Created configureTab");
    triggerTab = new ControlPanelTriggerTab(controllerInterface, state, this);
    state->writeToLog("Created triggerTab");

    tabWidget = new QTabWidget(this);
    tabWidget->addTab(bandwidthTab, tr("BW"));
    tabWidget->addTab(impedanceTab, tr("Impedance"));
    tabWidget->addTab(audioAnalogTab, tr("Audio/Analog"));
    tabWidget->addTab(configureTab, tr("Config"));
    tabWidget->addTab(triggerTab, tr("Trigger"));
    state->writeToLog("Created tabWidget");

    QHBoxLayout *topRow = new QHBoxLayout;
    topRow->addWidget(hideControlPanelButton);
    topRow->addWidget(topLabel);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(topRow);
    mainLayout->addLayout(createSelectionLayout());
    mainLayout->addLayout(createSelectionToolsLayout());
    mainLayout->addWidget(filterDisplaySelector);
    mainLayout->addLayout(createDisplayLayout());
    mainLayout->addWidget(tabWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QWidget *mainWidget = new QWidget(this);
    mainWidget->setLayout(mainLayout);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(mainWidget);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QVBoxLayout *scrollLayout = new QVBoxLayout;
    scrollLayout->addWidget(scrollArea);

    setLayout(scrollLayout);

    YScaleUsed yScaleUsed;
    updateSlidersEnabled(yScaleUsed);
    state->writeToLog("Created layout. About to call updateFromState()");

    updateFromState();
    state->writeToLog("Completed updateFromState(). End of ControlPanel ctor");
}

ControlPanel::~ControlPanel()
{
    if (stimParamDialog) delete stimParamDialog;
    if (anOutDialog) delete anOutDialog;
    if (digOutDialog) delete digOutDialog;
}

void ControlPanel::updateForRun()
{
    configureTab->updateForRun();
}

void ControlPanel::updateForLoad()
{
    setEnabled(false);
}

void ControlPanel::updateForStop()
{
    setEnabled(true);

    configureTab->updateForStop();
}

void ControlPanel::updateSlidersEnabled(YScaleUsed yScaleUsed)
{
    wideSlider->setEnabled(yScaleUsed.widepassYScaleUsed);
    wideLabel->setEnabled(yScaleUsed.widepassYScaleUsed);
    lowSlider->setEnabled(yScaleUsed.lowpassYScaleUsed);
    lowLabel->setEnabled(yScaleUsed.lowpassYScaleUsed);
    highSlider->setEnabled(yScaleUsed.highpassYScaleUsed);
    highLabel->setEnabled(yScaleUsed.highpassYScaleUsed);
    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        variableSlider->setEnabled(yScaleUsed.dcYScaleUsed);
        variableLabel->setEnabled(yScaleUsed.dcYScaleUsed);
    } else {
        variableSlider->setEnabled(yScaleUsed.auxInputYScaleUsed);
        variableLabel->setEnabled(yScaleUsed.auxInputYScaleUsed);
    }
    analogSlider->setEnabled(yScaleUsed.analogIOYScaleUsed);
    analogLabel->setEnabled(yScaleUsed.analogIOYScaleUsed);
}

YScaleUsed ControlPanel::slidersEnabled() const
{
    YScaleUsed yScaleUsed;
    yScaleUsed.widepassYScaleUsed = wideSlider->isEnabled();
    yScaleUsed.lowpassYScaleUsed = lowSlider->isEnabled();
    yScaleUsed.highpassYScaleUsed = highSlider->isEnabled();
    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        yScaleUsed.dcYScaleUsed = variableSlider->isEnabled();
    } else {
        yScaleUsed.auxInputYScaleUsed = variableSlider->isEnabled();
    }
    yScaleUsed.analogIOYScaleUsed = analogSlider->isEnabled();
    return yScaleUsed;
}

QHBoxLayout* ControlPanel::createSelectionLayout()
{
    enableCheckBox = new QCheckBox(tr("Enable"), this);
    connect(enableCheckBox, SIGNAL(clicked()), this, SLOT(enableChannelsSlot()));

    colorAttribute = new ColorWidget(this);
    connect(colorAttribute, SIGNAL(clicked()), this, SLOT(promptColorChange()));

    selectionNameLabel = new QLabel(tr("no selection"), this);
    selectionImpedanceLabel = new QLabel(tr("no selection"), this);
    selectionReferenceLabel = new QLabel(tr("no selection"), this);
    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        selectionStimTriggerLabel = new QLabel(tr("no selection"), this);
    }

    renameButton  = new QPushButton(tr("Rename"), this);
    renameButton->setFixedWidth(renameButton->fontMetrics().horizontalAdvance(renameButton->text()) + 14);
    connect(renameButton, SIGNAL(clicked()), controlWindow, SLOT(renameChannel()));

    setRefButton  = new QPushButton(tr("Set Ref"), this);
    setRefButton->setFixedWidth(setRefButton->fontMetrics().horizontalAdvance(setRefButton->text()) + 14);
    connect(setRefButton, SIGNAL(clicked()), controlWindow, SLOT(setReference()));

    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        setStimButton = new QPushButton(tr("Set Stim"), this);
        setStimButton->setFixedWidth(setStimButton->fontMetrics().horizontalAdvance(setStimButton->text()) + 14);
        connect(setStimButton, SIGNAL(clicked()), this, SLOT(openStimParametersDialog()));
    }

    QGridLayout* selectionGrid = new QGridLayout;
    selectionGrid->addWidget(new QLabel(tr("Name:"), this), 0, 0, Qt::AlignRight);
    selectionGrid->addWidget(selectionNameLabel, 0, 1, 1, 3);
    selectionGrid->addWidget(new QLabel(tr("Reference:"), this), 1, 0, Qt::AlignRight);
    selectionGrid->addWidget(selectionReferenceLabel, 1, 1, 1, 3);
    selectionGrid->addWidget(new QLabel(tr("Impedance:"), this), 2, 0, Qt::AlignRight);
    selectionGrid->addWidget(selectionImpedanceLabel, 2, 1);
    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        selectionGrid->addWidget(new QLabel(tr("Stim Trigger:"), this), 2, 2, Qt::AlignRight);
        selectionGrid->addWidget(selectionStimTriggerLabel, 2, 3);
    }
    selectionGrid->setColumnMinimumWidth(1, 90);

    QHBoxLayout *selectionLayout = new QHBoxLayout;
    selectionLayout->addLayout(selectionGrid);
    selectionLayout->addStretch(1);
    return selectionLayout;
}

QHBoxLayout* ControlPanel::createSelectionToolsLayout()
{
    QHBoxLayout *selectionToolsLayout = new QHBoxLayout;
    selectionToolsLayout->addWidget(enableCheckBox);
    selectionToolsLayout->addWidget(new QLabel(tr("Color"), this));
    selectionToolsLayout->addWidget(colorAttribute);
    selectionToolsLayout->addWidget(renameButton);
    selectionToolsLayout->addWidget(setRefButton);
    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        selectionToolsLayout->addWidget(setStimButton);
    }
    selectionToolsLayout->addStretch(1);
    return selectionToolsLayout;
}

QHBoxLayout* ControlPanel::createDisplayLayout()
{
    timeScaleComboBox = new QComboBox(this);
    state->tScale->setupComboBox(timeScaleComboBox);
    connect(timeScaleComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(changeTimeScale(int)));

    clipWaveformsCheckBox = new QCheckBox(tr("Clip Waves"), this);
    connect(clipWaveformsCheckBox, SIGNAL(stateChanged(int)), this, SLOT(clipWaveforms(int)));

    QVBoxLayout *timeScaleColumn = new QVBoxLayout;
    timeScaleColumn->addWidget(clipWaveformsCheckBox);
    timeScaleColumn->addStretch();
    timeScaleColumn->addWidget(new QLabel(tr("Time Scale"), this));
    timeScaleColumn->addWidget(timeScaleComboBox);

    const int SliderHeight = 50;

    wideSlider = new QSlider(Qt::Vertical, this);
    wideSlider->setFixedHeight(SliderHeight);
    wideSlider->setRange(0, state->yScaleWide->numberOfItems() - 1);
    wideSlider->setInvertedAppearance(true);
    wideSlider->setInvertedControls(true);
    wideSlider->setPageStep(1);
    wideSlider->setValue(state->yScaleWide->getIndex());
    wideSlider->setTickPosition(QSlider::TicksRight);
//    wideSlider->setStyleSheet("width: 16px");
    connect(wideSlider, SIGNAL(valueChanged(int)), this, SLOT(changeWideScale(int)));

    lowSlider = new QSlider(Qt::Vertical, this);
    lowSlider->setFixedHeight(SliderHeight);
    lowSlider->setRange(0, state->yScaleLow->numberOfItems() - 1);
    lowSlider->setInvertedAppearance(true);
    lowSlider->setInvertedControls(true);
    lowSlider->setPageStep(1);
    lowSlider->setValue(state->yScaleLow->getIndex());
    lowSlider->setTickPosition(QSlider::TicksRight);
    connect(lowSlider, SIGNAL(valueChanged(int)), this, SLOT(changeLowScale(int)));

    highSlider = new QSlider(Qt::Vertical, this);
    highSlider->setFixedHeight(SliderHeight);
    highSlider->setRange(0, state->yScaleHigh->numberOfItems() - 1);
    highSlider->setInvertedAppearance(true);
    highSlider->setInvertedControls(true);
    highSlider->setPageStep(1);
    highSlider->setValue(state->yScaleHigh->getIndex());
    highSlider->setTickPosition(QSlider::TicksRight);
    connect(highSlider, SIGNAL(valueChanged(int)), this, SLOT(changeHighScale(int)));

    variableSlider = new QSlider(Qt::Vertical, this);
    variableSlider->setFixedHeight(SliderHeight);
    variableSlider->setValue(state->yScaleAux->getIndex());
    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        variableSlider->setRange(0, state->yScaleDC->numberOfItems() - 1);
        variableSlider->setValue(state->yScaleDC->getIndex());
        connect(variableSlider, SIGNAL(valueChanged(int)), this, SLOT(changeDCSScale(int)));
    } else {
        variableSlider->setRange(0, state->yScaleAux->numberOfItems() - 1);
        variableSlider->setValue(state->yScaleAux->getIndex());
        connect(variableSlider, SIGNAL(valueChanged(int)), this, SLOT(changeAuxScale(int)));
    }
    variableSlider->setInvertedAppearance(true);
    variableSlider->setInvertedControls(true);
    variableSlider->setPageStep(1);
    variableSlider->setTickPosition(QSlider::TicksRight);

    analogSlider = new QSlider(Qt::Vertical, this);
    analogSlider->setFixedHeight(SliderHeight);
    analogSlider->setRange(0, state->yScaleAnalog->numberOfItems() - 1);
    analogSlider->setInvertedAppearance(true);
    analogSlider->setInvertedControls(true);
    analogSlider->setPageStep(1);
    analogSlider->setValue(state->yScaleAnalog->getIndex());
    analogSlider->setTickPosition(QSlider::TicksRight);
    connect(analogSlider, SIGNAL(valueChanged(int)), this, SLOT(changeAnaScale(int)));

    wideLabel = new QLabel(tr("WIDE"), this);
    lowLabel = new QLabel(tr("LOW"), this);
    highLabel = new QLabel(tr("HIGH"), this);
    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        variableLabel = new QLabel(tr("DC"), this);
    } else {
        variableLabel = new QLabel(tr("AUX"), this);
    }
    analogLabel = new QLabel(tr("ANALOG"), this);

    QVBoxLayout *wideColumn = new QVBoxLayout;
    wideColumn->addWidget(wideSlider);
    wideColumn->setAlignment(wideSlider, Qt::AlignHCenter);
    wideColumn->addWidget(wideLabel);
    wideColumn->setAlignment(wideLabel, Qt::AlignHCenter);

    QVBoxLayout *lowColumn = new QVBoxLayout;
    lowColumn->addWidget(lowSlider);
    lowColumn->setAlignment(lowSlider, Qt::AlignHCenter);
    lowColumn->addWidget(lowLabel);
    lowColumn->setAlignment(lowLabel, Qt::AlignHCenter);

    QVBoxLayout *highColumn = new QVBoxLayout;
    highColumn->addWidget(highSlider);
    highColumn->setAlignment(highSlider, Qt::AlignHCenter);
    highColumn->addWidget(highLabel);
    highColumn->setAlignment(highLabel, Qt::AlignHCenter);

    QVBoxLayout *variableColumn = new QVBoxLayout;
    variableColumn->addWidget(variableSlider);
    variableColumn->setAlignment(variableSlider, Qt::AlignHCenter);
    variableColumn->addWidget(variableLabel);
    variableColumn->setAlignment(variableLabel, Qt::AlignHCenter);

    QVBoxLayout *analogColumn = new QVBoxLayout;
    analogColumn->addWidget(analogSlider);
    analogColumn->setAlignment(analogSlider, Qt::AlignHCenter);
    analogColumn->addWidget(analogLabel);
    analogColumn->setAlignment(analogLabel, Qt::AlignHCenter);

    QGridLayout *displayGrid = new QGridLayout;
    displayGrid->setHorizontalSpacing(0);
    displayGrid->addLayout(wideColumn, 0, 0);
    displayGrid->addLayout(lowColumn, 0, 1);
    displayGrid->addLayout(highColumn, 0, 2);
    displayGrid->addLayout(variableColumn, 0, 3);
    displayGrid->addLayout(analogColumn, 0, 4);

    const int ColumnMinWidth = analogLabel->fontMetrics().horizontalAdvance(analogLabel->text()) + 5;
    displayGrid->setColumnMinimumWidth(0, ColumnMinWidth);
    displayGrid->setColumnMinimumWidth(1, ColumnMinWidth);
    displayGrid->setColumnMinimumWidth(2, ColumnMinWidth);
    displayGrid->setColumnMinimumWidth(3, ColumnMinWidth);
    displayGrid->setColumnMinimumWidth(4, ColumnMinWidth);

    QHBoxLayout* displayLayout = new QHBoxLayout;
    displayLayout->addLayout(displayGrid);
    displayLayout->addStretch(1);
    displayLayout->addLayout(timeScaleColumn);
    displayLayout->addStretch(1);
    return displayLayout;
}

void ControlPanel::promptColorChange()
{
    // Identify all currently selected channels.
    state->signalSources->getSelectedSignals(selectedSignals);

    if (!selectedSignals.empty()) {
        // Get QColor from modal dialog from user.
        QColor result = QColorDialog::getColor(selectedSignals.at(0)->getColor(), this);

        // If this color is valid (user didn't cancel), set the color for all currently selected channels.
        if (result.isValid()) {
            state->signalSources->undoManager->pushStateToUndoStack();
            for (int i = 0; i < selectedSignals.size(); i++) {
                selectedSignals[i]->setColor(result);
                state->forceUpdate();
            }
        }
    }
}

void ControlPanel::clipWaveforms(int checked)
{
    state->clipWaveforms->setValue(checked != 0);
}

void ControlPanel::changeTimeScale(int index)
{
    state->tScale->setIndex(index);
}

void ControlPanel::changeWideScale(int index)
{
    state->yScaleWide->setIndex(index);
}

void ControlPanel::changeLowScale(int index)
{
    state->yScaleLow->setIndex(index);
}

void ControlPanel::changeHighScale(int index)
{
    state->yScaleHigh->setIndex(index);
}

void ControlPanel::changeAuxScale(int index)
{
    state->yScaleAux->setIndex(index);
}

void ControlPanel::changeAnaScale(int index)
{
    state->yScaleAnalog->setIndex(index);
}

void ControlPanel::changeDCSScale(int index)
{
    state->yScaleDC->setIndex(index);
}

void ControlPanel::openStimParametersDialog()
{
    // This slot should only be called when a single channel is selected AND board is not running.
    Channel* selectedChannel = state->signalSources->selectedChannel();
    SignalType type = selectedChannel->getSignalType();

    if (type == AmplifierSignal) {
        if (stimParamDialog) {
            disconnect(this, nullptr, stimParamDialog, nullptr);
            delete stimParamDialog;
            stimParamDialog = nullptr;
        }
        stimParamDialog = new StimParamDialog(state, selectedChannel, this);
        connect(qApp, SIGNAL(focusChanged(QWidget*, QWidget*)), stimParamDialog, SLOT(notifyFocusChanged(QWidget*, QWidget*)));

        if (stimParamDialog->exec() == QDialog::Accepted) {
            state->stimParamsHaveChanged = true;
            controllerInterface->uploadStimParameters(selectedChannel);
        }
    } else if (type == BoardDigitalOutSignal) {
        if (digOutDialog) {
            disconnect(this, nullptr, digOutDialog, nullptr);
            delete digOutDialog;
            digOutDialog = nullptr;
        }
        digOutDialog = new DigOutDialog(state, selectedChannel, this);
        connect(qApp, SIGNAL(focusChanged(QWidget*, QWidget*)), digOutDialog, SLOT(notifyFocusChanged(QWidget*, QWidget*)));

        if (digOutDialog->exec() == QDialog::Accepted){
            state->stimParamsHaveChanged = true;
            controllerInterface->uploadStimParameters(selectedChannel);
        }
    } else if (type == BoardDacSignal) {
        if (anOutDialog) {
            disconnect(this, nullptr, anOutDialog, nullptr);
            delete anOutDialog;
            anOutDialog = nullptr;
        }
        anOutDialog = new AnOutDialog(state, selectedChannel, this);
        connect(qApp, SIGNAL(focusChanged(QWidget*, QWidget*)), anOutDialog, SLOT(notifyFocusChanged(QWidget*, QWidget*)));

        if (anOutDialog->exec() == QDialog::Accepted) {
            state->stimParamsHaveChanged = true;
            controllerInterface->uploadStimParameters(selectedChannel);
        }
    }
    state->forceUpdate();
}

void ControlPanel::updateFromState()
{
    state->writeToLog("Beginning of updateFromState()");
    state->signalSources->getSelectedSignals(selectedSignals);
    state->writeToLog("Got selected signals");

    filterDisplaySelector->updateFromState();
    state->writeToLog("Completed filterDisplaySelector->updateFromState()");
    updateSelectionName();
    state->writeToLog("Completed updateSelectionName()");
    updateElectrodeImpedance();
    state->writeToLog("Completed updateElectrodeImpedance()");
    updateReferenceChannel();
    state->writeToLog("Completed updateReferenceChannel()");
    updateColor();
    state->writeToLog("Completed updateColor()");
    updateEnableCheckBox();
    state->writeToLog("Completed updateEnableCheckBox()");
    updateYScales();
    state->writeToLog("Completed updateYScales()");

    clipWaveformsCheckBox->setChecked(state->clipWaveforms->getValue());
    timeScaleComboBox->setCurrentIndex(state->tScale->getIndex());

    state->writeToLog("About to do tabs' updateFromStates");
    bandwidthTab->updateFromState();
    state->writeToLog("Completed bandwidthTab->updateFromState()");
    impedanceTab->updateFromState();
    state->writeToLog("Completed impedanceTab->updateFromState()");
    audioAnalogTab->updateFromState();
    state->writeToLog("Completed audioAnalogTab->updateFromState()");
    configureTab->updateFromState();
    state->writeToLog("Completed configureTab->updateFromState()");
    triggerTab->updateFromState();
    state->writeToLog("Completed triggerTab->updateFromState()");

    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        updateStimTrigger();
        updateStimParamDialogButton();
    }
    state->writeToLog("End of ControlPanel::updateFromState()");
}

void ControlPanel::updateYScales()
{
    wideSlider->setValue(state->yScaleWide->getIndex());
    lowSlider->setValue(state->yScaleLow->getIndex());
    highSlider->setValue(state->yScaleHigh->getIndex());

    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        variableSlider->setValue(state->yScaleDC->getIndex());
    } else {
        variableSlider->setValue(state->yScaleAux->getIndex());
    }

    analogSlider->setValue(state->yScaleAnalog->getIndex());
}

void ControlPanel::updateStimParamDialogButton()
{
    // If a single channel with stim capability is selected and board is not currently running, then enable the button.
    bool hasStimCapability = false;
    if (state->signalSources->numChannelsSelected() == 1) {
        Channel* channel = state->signalSources->selectedChannel();
        hasStimCapability = channel->isStimCapable();
    }

    setStimButton->setEnabled(hasStimCapability && !(state->running || state->sweeping));
}

void ControlPanel::updateSelectionName()
{
    if (selectedSignals.isEmpty()) {
        selectionNameLabel->setText(tr("no selection"));
        renameButton->setEnabled(false);
    } else if (selectedSignals.size() == 1) {
        QString name = selectedSignals.at(0)->getNativeAndCustomNames();
        selectionNameLabel->setText(name);
        renameButton->setEnabled(true);
    } else {
        bool allSameGroup = true;
        int groupID = selectedSignals.at(0)->getGroupID();
        if (groupID != 0) {
            for (int i = 1; i < selectedSignals.size(); ++i) {
                if (selectedSignals.at(i)->getGroupID() != groupID) {
                    allSameGroup = false;
                    break;
                }
            }
        } else {
            allSameGroup = false;
        }
        if (allSameGroup) {
            QString name = selectedSignals.at(0)->getNativeAndCustomNames();
            selectionNameLabel->setText(name + tr(" GROUP"));
        } else {
            selectionNameLabel->setText("");
        }
        renameButton->setEnabled(false);
    }
    if (state->running || state->sweeping) renameButton->setEnabled(false);
}

void ControlPanel::updateElectrodeImpedance()
{
    QString impedanceText;
    if (selectedSignals.isEmpty()) {
        impedanceText = tr("no selection");
    } else if (selectedSignals.at(0)->getSignalType() != AmplifierSignal) {
        impedanceText = tr("n/a");
    } else if (!selectedSignals.at(0)->isImpedanceValid()) {
        impedanceText = tr("not measured");
    } else if (selectedSignals.size() == 1) {
        impedanceText = selectedSignals.at(0)->getImpedanceMagnitudeString() + " " +
                selectedSignals.at(0)->getImpedancePhaseString();
    }
    selectionImpedanceLabel->setText(impedanceText);
}

void ControlPanel::updateReferenceChannel()
{   
    if (selectedSignals.isEmpty()) {
        selectionReferenceLabel->setText(tr("no selection"));
        setRefButton->setEnabled(false);
        return;
    }

    if (selectedSignals.at(0)->getSignalType() != AmplifierSignal) {
        selectionReferenceLabel->setText(tr("n/a"));
        setRefButton->setEnabled(false);
        return;
    }

    bool sameReference = true;
    for (int i = 0; i < selectedSignals.size() - 1; i++) {
        if (selectedSignals.at(i)->getReference() != selectedSignals.at(i + 1)->getReference()) {
            sameReference = false;
            break;
        }
    }
    if (sameReference) {
        QString refText = selectedSignals.at(0)->getReference();
        if (refText.length() > 36) refText = refText.left(36) + "...";
        selectionReferenceLabel->setText(refText);
    } else {
        selectionReferenceLabel->setText("");
    }
    setRefButton->setEnabled(!(state->running || state->sweeping) && state->signalSources->onlyAmplifierChannelsSelected());
}

void ControlPanel::updateStimTrigger()
{
    if (selectedSignals.isEmpty()) {
        selectionStimTriggerLabel->setText(tr("no selection"));
        setStimButton->setEnabled(false);
        return;
    }

    if (!selectedSignals.at(0)->isStimCapable()) {
        selectionStimTriggerLabel->setText(tr("n/a"));
        setStimButton->setEnabled(false);
        return;
    }

    bool sameStimTrigger = true;
    for (int i = 0; i < selectedSignals.size() - 1; i++) {
        if (selectedSignals.at(i)->getStimTrigger() != selectedSignals.at(i + 1)->getStimTrigger() ||
                !selectedSignals.at(i)->isStimEnabled() || !selectedSignals.at(i + 1)->isStimEnabled()) {
            sameStimTrigger = false;
            break;
        }
    }
    if (sameStimTrigger) {
        if (selectedSignals.at(0)->isStimEnabled()) {
            selectionStimTriggerLabel->setText(selectedSignals.at(0)->getStimTrigger());
        } else {
            selectionStimTriggerLabel->setText("");
        }
    } else {
        selectionStimTriggerLabel->setText("");
    }
    if (selectedSignals.size() == 1) {
        setStimButton->setEnabled(!(state->running || state->sweeping));
    }
}

void ControlPanel::updateColor()
{
    if (selectedSignals.isEmpty()) {
        colorAttribute->setBlank();
        colorAttribute->setEnabled(false);
        return;
    }

    bool sameColor = true;
    for (int i = 0; i < selectedSignals.size() - 1; i++) {
        if (selectedSignals.at(i)->getColor() != selectedSignals.at(i + 1)->getColor()) {
            sameColor = false;
            break;
        }
    }
    if (sameColor) {
        colorAttribute->setColor(selectedSignals.at(0)->getColor());
        colorAttribute->setEnabled(!(state->running || state->sweeping));
    } else {
        colorAttribute->setCheckerboard();
        colorAttribute->setEnabled(!(state->running || state->sweeping));
    }
}

void ControlPanel::updateEnableCheckBox()
{
    int enableState = state->signalSources->enableStateOfSelectedChannels();
    enableCheckBox->setEnabled(enableState >= 0);
    if (enableState >= 0) {
        Qt::CheckState checkState = (Qt::CheckState) enableState;
        enableCheckBox->setCheckState(checkState);
        if (checkState == Qt::Unchecked || checkState == Qt::Checked) {
            enableCheckBox->setTristate(false);
        }
    }
}

void ControlPanel::enableChannelsSlot()
{
    bool enable = enableCheckBox->checkState() == Qt::Checked;
    controlWindow->enableSelectedChannels(enable);
}

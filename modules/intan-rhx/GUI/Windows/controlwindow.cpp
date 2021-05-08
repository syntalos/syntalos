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

#include <QSettings>
#include "setthresholdsdialog.h"
#include "autocolordialog.h"
#include "autogroupdialog.h"
#include "playbackfilepositiondialog.h"
#include "ampsettledialog.h"
#include "chargerecoverydialog.h"
#include "renamechanneldialog.h"
#include "referenceselectdialog.h"
#include "controlwindow.h"

using namespace std;

ControlWindow::ControlWindow(SystemState* state_, CommandParser* parser_, ControllerInterface* controllerInterface_) :
    QMainWindow(nullptr), // Since the parent isn't a QMainWindow but a QDialog, just pass a nullptr
    state(state_),
    controllerInterface(controllerInterface_),
    parser(parser_),
    showControlPanelButton(nullptr),
    probeMapWindow(nullptr),
    controlButtons(nullptr),
    keyboardShortcutDialog(nullptr),
    triggerRecordDialog(nullptr),
    fileFormatDialog(nullptr),
    isiDialog(nullptr),
    psthDialog(nullptr),
    spectrogramDialog(nullptr),
    spikeSortingDialog(nullptr),
    fileMenu(nullptr),
    displayMenu(nullptr),
    channelMenu(nullptr),
    stimulationMenu(nullptr),
    impedanceMenu(nullptr),
    toolsMenu(nullptr),
    helpMenu(nullptr),
    runAction(nullptr),
    stopAction(nullptr),
    selectFilenameAction(nullptr),
    chooseFileFormatAction(nullptr),
    recordAction(nullptr),
    triggeredRecordAction(nullptr),
    rewindAction(nullptr),
    fastForwardAction(nullptr),
    fastPlaybackAction(nullptr),
    jumpToStartAction(nullptr),
    jumpBack1SecAction(nullptr),
    jumpBack10SecAction(nullptr),
    jumpAction(nullptr),
    zoomInTScaleAction(nullptr),
    zoomOutTScaleAction(nullptr),
    toggleRollSweepAction(nullptr),
    zoomInYScaleAction(nullptr),
    zoomOutYScaleAction(nullptr),
    displayLabelGroup(nullptr),
    displayLabelCustomNameAction(nullptr),
    displayLabelNativeNameAction(nullptr),
    displayLabelImpedanceMagnitudeAction(nullptr),
    displayLabelImpedancePhaseAction(nullptr),
    displayLabelReferenceAction(nullptr),
    displayCycleAction(nullptr),
    changeBackgroundColorAction(nullptr),
    showAuxInputsAction(nullptr),
    showSupplyVoltagesAction(nullptr),
    loadSettingsAction(nullptr),
    defaultSettingsAction(nullptr),
    saveSettingsAction(nullptr),
    loadStimSettingsAction(nullptr),
    saveStimSettingsAction(nullptr),
    exitAction(nullptr),
    undoAction(nullptr),
    redoAction(nullptr),
    groupAction(nullptr),
    ungroupAction(nullptr),
    renameChannelAction(nullptr),
    setReferenceAction(nullptr),
    setSpikeThresholdsAction(nullptr),
    autoColorChannelsAction(nullptr),
    autoGroupChannelsAction(nullptr),
    ungroupAllChannelsAction(nullptr),
    restoreOrderChannelsAction(nullptr),
    alphabetizeChannelsAction(nullptr),
    copyStimParametersAction(nullptr),
    pasteStimParametersAction(nullptr),
    ampSettleSettingsAction(nullptr),
    chargeRecoverySettingsAction(nullptr),
    keyboardHelpAction(nullptr),
    enableLoggingAction(nullptr),
    intanWebsiteAction(nullptr),
    aboutAction(nullptr),
    remoteControlAction(nullptr),
    isiAction(nullptr),
    probeMapAction(nullptr),
    spectrogramAction(nullptr),
    psthAction(nullptr),
    performanceAction(nullptr),
    spikeSortingAction(nullptr),
    timeLabel(nullptr),
    topStatusLabel(nullptr),
    statusBarLabel(nullptr),
    statusBars(nullptr),
    controlPanel(nullptr),
    multiColumnDisplay(nullptr),
    tcpDialog(nullptr),
    tcpDisplay(nullptr),
    showHideRow(nullptr),
    showHideStretch(nullptr),
    stimParametersInterface(nullptr),
    stimClipboard(nullptr),
    currentlyRunning(false),
    currentlyRecording(false),
    fastPlaybackMode(false),
    hwFifoNearlyFull(0),
    mainCpuLoad(0.0)

{
    state->writeToLog("Entered ControlWindow ctor");
    connect(state, SIGNAL(headstagesChanged()), this, SLOT(updateForChangeHeadstages()));
    state->writeToLog("Connected updateForChangeHeadstages");

    state->writeToLog("About to create showControlPanelButton");
    showControlPanelButton = new QToolButton(this);
    showControlPanelButton->setIcon(QIcon(":/images/showicon.png"));
    showControlPanelButton->setToolTip(tr("Show Control Panel"));
    connect(showControlPanelButton, SIGNAL(clicked()), this, SLOT(showControlPanel()));

    state->writeToLog("About to call createActions()");
    createActions();
    state->writeToLog("Completed createActions()");
    state->writeToLog("About to call createMenus()");
    createMenus();
    state->writeToLog("Completed createMenus()");
    state->writeToLog("About to call createStatusBar()");
    createStatusBar();
    state->writeToLog("Completed createStatusBar()");
    connect(this, SIGNAL(setStatusBar(QString)), this, SLOT(updateStatusBar(QString)));
    connect(this, SIGNAL(setTimeLabel(QString)), this, SLOT(updateTimeLabel(QString)));

    state->writeToLog("About to create controlButtons");
    controlButtons = new QToolBar(this);
    if (state->playback->getValue()) {
        controlButtons->addAction(jumpToStartAction);
        controlButtons->addAction(jumpBack10SecAction);
        controlButtons->addAction(jumpBack1SecAction);
    } else {
        controlButtons->addAction(rewindAction);
        controlButtons->addAction(fastForwardAction);
    }
    controlButtons->addAction(stopAction);
    runAction->setVisible(false);
    if (state->playback->getValue()) {
        controlButtons->addAction(fastPlaybackAction);
        controlButtons->addAction(jumpAction);
    }
    selectFilenameAction->setVisible(false);
    recordAction->setVisible(false);
    triggeredRecordAction->setVisible(false);
    controlButtons->addAction(chooseFileFormatAction);

    controlButtons->addWidget(new QLabel("   "));

    state->writeToLog("About to create timeLabel");
    timeLabel = new QLabel("<b>00:00:00</b>", this);
    int fontSize = 14;
    if (state->highDPIScaleFactor > 1) fontSize = 21;
    state->writeToLog("Font size: " + QString::number(fontSize));
    QFont timeFont("Courier", fontSize);
    timeLabel->setFont(timeFont);
    int margin = 6;
    timeLabel->setContentsMargins(margin, 0, margin, 0);
    QHBoxLayout* timeLayout = new QHBoxLayout;
    timeLayout->addWidget(timeLabel);
    timeLayout->setContentsMargins(0, 0, 0, 0);
    QFrame* timeBox = new QFrame(this);
    timeBox->setFrameStyle(QFrame::Box | QFrame::Plain);
    timeBox->setLayout(timeLayout);
    controlButtons->addWidget(timeBox);

    state->writeToLog("About to create statusBars");
    statusBars = new StatusBars(this);
    controlButtons->addWidget(new QLabel("   "));
    controlButtons->addWidget(statusBars);
    controlButtons->addWidget(new QLabel("   "));

    string sampleRateString =
            AbstractRHXController::getSampleRateString(AbstractRHXController::nearestSampleRate(state->sampleRate->getNumericValue()));
    QLabel *sampleRateLabel = new QLabel(QString::fromStdString(sampleRateString) + " " + tr("sample rate"), this);
    sampleRateLabel->setContentsMargins(margin, 0, margin, 0);
    controlButtons->addWidget(sampleRateLabel);

    state->writeToLog("About to create topStatusLabel");
    topStatusLabel = new QLabel("", this);
    topStatusLabel->setContentsMargins(margin, 0, margin, 0);
    controlButtons->addWidget(topStatusLabel);

    this->addToolBar(Qt::TopToolBarArea, controlButtons);

    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        stimParametersInterface = new XMLInterface(state, controllerInterface, XMLIncludeStimParameters);
        stimClipboard = new StimParametersClipboard(state, controllerInterface);

    }

    state->writeToLog("About to create MultiColumnDisplay");
    multiColumnDisplay = new MultiColumnDisplay(controllerInterface, state, this);
    state->writeToLog("Created MultiColumnDisplay");
    controllerInterface->setDisplay(multiColumnDisplay);

    state->writeToLog("About to create ControlPanel");
    controlPanel = new ControlPanel(controllerInterface, state, parser, this);
    state->writeToLog("Created ControlPanel");
    controlPanel->hide();

    controllerInterface->setControlPanel(controlPanel);

    state->writeToLog("About to set layouts");

    QVBoxLayout *controlPanelCol = new QVBoxLayout;
    controlPanelCol->addWidget(showControlPanelButton);
    controlPanelCol->addWidget(controlPanel);
    controlPanelCol->setAlignment(Qt::AlignTop);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addLayout(controlPanelCol);
    mainLayout->setAlignment(controlPanelCol, Qt::AlignTop);
    mainLayout->addWidget(multiColumnDisplay);

    QWidget *central = new QWidget(this);
    central->setLayout(mainLayout);

    setCentralWidget(central);

    connect(state, SIGNAL(stateChanged()), this, SLOT(updateFromState()));  // Wait until ControlPanel is set up before making
                                                                            // this connection.
    QString title = tr("Intan Technologies ");
    if (state->getControllerTypeEnum() == ControllerRecordUSB2) {
        title += tr("USB Interface Board");
    } else if (state->getControllerTypeEnum() == ControllerRecordUSB3) {
        title += tr("Recording Controller");
    } else if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        title += tr("Stimulation/Recording Controller");
    }

    if (state->synthetic->getValue()) {
        title += tr(" - Demonstration Mode");
    } else if (state->playback->getValue()) {
        title += tr(" - Playback Mode: ") + controllerInterface->playbackFileName();
    }
    setWindowTitle(title);
    state->writeToLog("Finished setting window title");

    setStatusBarReady();
    state->writeToLog("Finished setting status bar");
    resize(1080, 870);

    state->writeToLog("Finished resizing");

    state->writeToLog("About to updateMenus()");
    updateMenus();
    state->writeToLog("Finished updateMenus(). End of ctor");
}

ControlWindow::~ControlWindow()
{
    if (probeMapWindow) {
        probeMapWindow->close();
        delete probeMapWindow;
    }
    if (isiDialog) {
        isiDialog->close();
        delete isiDialog;
    }
    if (psthDialog) {
        psthDialog->close();
        delete psthDialog;
    }
    if (spectrogramDialog) {
        spectrogramDialog->close();
        delete spectrogramDialog;
    }
    if (spikeSortingDialog) {
        spikeSortingDialog->close();
        delete spikeSortingDialog;
    }
    if (triggerRecordDialog) {
        triggerRecordDialog->close();
        delete triggerRecordDialog;
    }
    if (fileFormatDialog) {
        fileFormatDialog->close();
        delete fileFormatDialog;
    }
    if (tcpDialog) {
        tcpDialog->close();
        delete tcpDialog;
    }
    if (stimParametersInterface) {
        delete stimParametersInterface;
    }
}

void ControlWindow::createActions()
{
    // Menu actions (at the top of the window)

    // File menu
    loadSettingsAction = new QAction(tr("Load Settings"), this);
    loadSettingsAction->setShortcut(tr("Ctrl+O"));
    connect(loadSettingsAction, SIGNAL(triggered()), this, SLOT(loadSettingsSlot()));

    defaultSettingsAction = new QAction(tr("Set Default Settings"), this);
    connect(defaultSettingsAction, SIGNAL(triggered()), this, SLOT(defaultSettingsSlot()));

    saveSettingsAction = new QAction(tr("Save Settings"), this);
    saveSettingsAction->setShortcut(tr("Ctrl+S"));
    connect(saveSettingsAction, SIGNAL(triggered()), this, SLOT(saveSettingsSlot()));

    exitAction = new QAction(tr("Exit"), this);
    exitAction->setShortcut(tr("Ctrl+Q"));
    connect(exitAction, SIGNAL(triggered()), this, SLOT(close()));

    // Display menu
    zoomInTScaleAction = new QAction(tr("Zoom In Time Scale"), this);
    zoomInTScaleAction->setShortcuts({ tr("<"), tr(",") });
    connect(zoomInTScaleAction, SIGNAL(triggered()), this, SLOT(zoomInTScale()));

    zoomOutTScaleAction = new QAction(tr("Zoom Out Time Scale"), this);
    zoomOutTScaleAction->setShortcuts({ tr(">"), tr(".") });
    connect(zoomOutTScaleAction, SIGNAL(triggered()), this, SLOT(zoomOutTScale()));

    toggleRollSweepAction = new QAction(tr("Toggle Roll/Sweep"), this);
    toggleRollSweepAction->setShortcuts({ tr("/"), tr("?") });
    connect(toggleRollSweepAction, SIGNAL(triggered()), this, SLOT(toggleRollSweep()));

    zoomInYScaleAction = new QAction(tr("Zoom In Voltage Scale"), this);
    zoomInYScaleAction->setShortcuts({ tr("+"), tr("=") });
    connect(zoomInYScaleAction, SIGNAL(triggered()), this, SLOT(zoomInYScale()));

    zoomOutYScaleAction = new QAction(tr("Zoom Out Voltage Scale"), this);
    zoomOutYScaleAction->setShortcuts({ tr("-"), tr("_") });
    connect(zoomOutYScaleAction, SIGNAL(triggered()), this, SLOT(zoomOutYScale()));

    displayLabelCustomNameAction = new QAction(tr("Display Custom Channel Name"), this);
    displayLabelCustomNameAction->setShortcut(tr("Ctrl+1"));
    displayLabelCustomNameAction->setCheckable(true);
    connect(displayLabelCustomNameAction, SIGNAL(triggered()), this, SLOT(setLabelCustomName()));

    displayLabelNativeNameAction = new QAction(tr("Display Native Channel Name"), this);
    displayLabelNativeNameAction->setShortcut(tr("Ctrl+2"));
    displayLabelNativeNameAction->setCheckable(true);
    connect(displayLabelNativeNameAction, SIGNAL(triggered()), this, SLOT(setLabelNativeName()));

    displayLabelImpedanceMagnitudeAction = new QAction(tr("Display Impedance Magnitude"), this);
    displayLabelImpedanceMagnitudeAction->setShortcut(tr("Ctrl+3"));
    displayLabelImpedanceMagnitudeAction->setCheckable(true);
    connect(displayLabelImpedanceMagnitudeAction, SIGNAL(triggered()), this, SLOT(setLabelImpedanceMagnitude()));

    displayLabelImpedancePhaseAction = new QAction(tr("Display Impedance Phase"), this);
    displayLabelImpedancePhaseAction->setShortcut(tr("Ctrl+4"));
    displayLabelImpedancePhaseAction->setCheckable(true);
    connect(displayLabelImpedancePhaseAction, SIGNAL(triggered()), this, SLOT(setLabelImpedancePhase()));

    displayLabelReferenceAction = new QAction(tr("Display Reference"), this);
    displayLabelReferenceAction->setShortcut(tr("Ctrl+5"));
    displayLabelReferenceAction->setCheckable(true);
    connect(displayLabelReferenceAction, SIGNAL(triggered()), this, SLOT(setLabelReference()));

    displayCycleAction = new QAction(tr("Cycle Display Text"), this);
    displayCycleAction->setShortcut(tr("Ctrl+T"));
    connect(displayCycleAction, SIGNAL(triggered()), this, SLOT(cycleLabelText()));

    changeBackgroundColorAction = new QAction(tr("Change Background Color"), this);
    connect(changeBackgroundColorAction, SIGNAL(triggered()), this, SLOT(changeBackgroundColor()));

    if (state->getControllerTypeEnum() != ControllerStimRecordUSB2) {
        showAuxInputsAction = new QAction(tr("Show Aux Inputs"), this);
        showAuxInputsAction->setCheckable(true);
        showAuxInputsAction->setChecked(state->showAuxInputs->getValue());
        connect(showAuxInputsAction, SIGNAL(triggered()), this, SLOT(setShowAuxInputs()));

        showSupplyVoltagesAction = new QAction(tr("Show Supply Voltages"), this);
        showSupplyVoltagesAction->setCheckable(true);
        showAuxInputsAction->setChecked(state->showSupplyVoltages->getValue());
        connect(showSupplyVoltagesAction, SIGNAL(triggered()), this, SLOT(setShowSupplyVoltages()));
    }

    displayLabelGroup = new QActionGroup(this);
    displayLabelGroup->addAction(displayLabelCustomNameAction);
    displayLabelGroup->addAction(displayLabelNativeNameAction);
    displayLabelGroup->addAction(displayLabelImpedanceMagnitudeAction);
    displayLabelGroup->addAction(displayLabelImpedancePhaseAction);
    displayLabelGroup->addAction(displayLabelReferenceAction);
    displayLabelCustomNameAction->setChecked(true);

    // Channel menu
    undoAction = new QAction(tr("Undo"), this);
    undoAction->setShortcut(tr("Ctrl+Z"));
    connect(undoAction, SIGNAL(triggered()), this, SLOT(undoSlot()));

    redoAction = new QAction(tr("Redo"), this);
    redoAction->setShortcut(tr("Ctrl+Y"));
    connect(redoAction, SIGNAL(triggered()), this, SLOT(redoSlot()));

    groupAction = new QAction(tr("Group"), this);
    groupAction->setShortcut(tr("Ctrl+G"));
    connect(groupAction, SIGNAL(triggered()), this, SLOT(group()));

    ungroupAction = new QAction(tr("Ungroup"), this);
    ungroupAction->setShortcut(tr("Ctrl+Shift+G"));
    connect(ungroupAction, SIGNAL(triggered()), this, SLOT(ungroup()));

    renameChannelAction = new QAction(tr("Rename Channel"), this);
    renameChannelAction->setShortcut(tr("Ctrl+R"));
    connect(renameChannelAction, SIGNAL(triggered()), this, SLOT(renameChannel()));

    setReferenceAction = new QAction(tr("Set Reference"), this);
    connect(setReferenceAction, SIGNAL(triggered()), this, SLOT(setReference()));

    setSpikeThresholdsAction = new QAction(tr("Set Spike Detection Thresholds"), this);
    connect(setSpikeThresholdsAction, SIGNAL(triggered()), this, SLOT(setSpikeThresholds()));

    autoColorChannelsAction = new QAction(tr("Color Amplifier Channels"), this);
    connect(autoColorChannelsAction, SIGNAL(triggered()), this, SLOT(autoColorChannels()));

    autoGroupChannelsAction = new QAction(tr("Group Amplifier Channels"), this);
    connect(autoGroupChannelsAction, SIGNAL(triggered()), this, SLOT(autoGroupChannels()));

    ungroupAllChannelsAction = new QAction(tr("Ungroup All Channels"), this);
    connect(ungroupAllChannelsAction, SIGNAL(triggered()), this, SLOT(ungroupAllChannels()));

    restoreOrderChannelsAction = new QAction(tr("Restore Original Order"), this);
    connect(restoreOrderChannelsAction, SIGNAL(triggered()), this, SLOT(restoreOrderChannels()));

    alphabetizeChannelsAction = new QAction(tr("Alphabetize Channels"), this);
    connect(alphabetizeChannelsAction, SIGNAL(triggered()), this, SLOT(alphabetizeChannels()));

    // Stimulation menu
    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        loadStimSettingsAction = new QAction(tr("Load Stimulation Settings"), this);
        connect(loadStimSettingsAction, SIGNAL(triggered()), this, SLOT(loadStimSettingsSlot()));

        saveStimSettingsAction = new QAction(tr("Save Stimulation Settings"), this);
        connect(saveStimSettingsAction, SIGNAL(triggered()), this, SLOT(saveStimSettingsSlot()));

        copyStimParametersAction = new QAction(tr("Copy Stimulation Parameters"), this);
        copyStimParametersAction->setShortcut(QKeySequence::Copy);
        connect(copyStimParametersAction, SIGNAL(triggered()), this, SLOT(copyStimParametersSlot()));

        pasteStimParametersAction = new QAction(tr("Paste Stimulation Parameters"), this);
        pasteStimParametersAction->setShortcut(QKeySequence::Paste);
        connect(pasteStimParametersAction, SIGNAL(triggered()), this, SLOT(pasteStimParametersSlot()));

        ampSettleSettingsAction = new QAction(tr("Amplifier Settle Settings"), this);
        connect(ampSettleSettingsAction, SIGNAL(triggered()), this, SLOT(ampSettleSettingsSlot()));

        chargeRecoverySettingsAction = new QAction(tr("Charge Recovery Settings"), this);
        connect(chargeRecoverySettingsAction, SIGNAL(triggered()), this, SLOT(chargeRecoverySettingsSlot()));
    }

    // Network menu
    remoteControlAction = new QAction(tr("Remote TCP Control"), this);
    connect(remoteControlAction, SIGNAL(triggered()), this, SLOT(remoteControl()));

    // Help menu
    keyboardHelpAction = new QAction(tr("Keyboard Shortcuts..."), this);
    keyboardHelpAction->setShortcut(tr("F12"));
    connect(keyboardHelpAction, SIGNAL(triggered()), this, SLOT(keyboardShortcutsHelp()));

    enableLoggingAction = new QAction(tr("Generate Log File for Debug"), this);
    enableLoggingAction->setCheckable(true);
    enableLoggingAction->setChecked(state->logErrors);
    connect(enableLoggingAction, SIGNAL(toggled(bool)), this, SLOT(enableLoggingSlot(bool)));

    intanWebsiteAction = new QAction(tr("Visit Intan Website..."), this);
    connect(intanWebsiteAction, SIGNAL(triggered()), this, SLOT(openIntanWebsite()));

    aboutAction = new QAction(tr("About Intan RHX Software..."), this);
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(about()));

    // Tools menu
    isiAction = new QAction(tr("ISI"), this);
    connect(isiAction, SIGNAL(triggered()), this, SLOT(isi()));

    probeMapAction = new QAction(tr("Probe Map"), this);
    connect(probeMapAction, SIGNAL(triggered()), this, SLOT(probeMap()));

    performanceAction = new QAction(tr("Performance Optimization"), this);
    connect(performanceAction, SIGNAL(triggered()), this, SLOT(performance()));

    psthAction = new QAction(tr("PSTH"), this);
    connect(psthAction, SIGNAL(triggered()), this, SLOT(psth()));

    spectrogramAction = new QAction(tr("Spectrogram"), this);
    connect(spectrogramAction, SIGNAL(triggered()), this, SLOT(spectrogram()));

    spikeSortingAction = new QAction(tr("Spike Scope"), this);
    connect(spikeSortingAction, SIGNAL(triggered()), this, SLOT(spikeSorting()));

    // Toolbar actions (within the toolbar, represented by an icon)
    rewindAction = new QAction(QIcon(":/images/rewindicon.png"), tr("Rewind"), this);
    rewindAction->setEnabled(false);
    connect(rewindAction, SIGNAL(triggered()), this, SLOT(rewindSlot()));

    fastForwardAction = new QAction(QIcon(":/images/fficon.png"), tr("Fast Forward"), this);
    fastForwardAction->setEnabled(false);
    connect(fastForwardAction, SIGNAL(triggered()), this, SLOT(fastForwardSlot()));

    runAction = new QAction(QIcon(":/images/runicon.png"), tr("Run"), this);
    connect(runAction, SIGNAL(triggered()), this, SLOT(runControllerSlot()));

    fastPlaybackAction = new QAction(QIcon(":/images/fficon.png"), tr("Fast Forward"), this);
    connect(fastPlaybackAction, SIGNAL(triggered()), this, SLOT(fastPlaybackSlot()));

    jumpToStartAction = new QAction(QIcon(":/images/tostarticon.png"), tr("Jump to Start"), this);
    connect(jumpToStartAction, SIGNAL(triggered()), this, SLOT(jumpToStartSlot()));

    jumpBack1SecAction = new QAction(QIcon(":/images/jumpback1s.png"), tr("Jump Back 1s"), this);
    connect(jumpBack1SecAction, SIGNAL(triggered()), this, SLOT(jumpBack1SecSlot()));

    jumpBack10SecAction = new QAction(QIcon(":/images/jumpback10s.png"), tr("Jump Back 10s"), this);
    connect(jumpBack10SecAction, SIGNAL(triggered()), this, SLOT(jumpBack10SecSlot()));

    jumpAction = new QAction(QIcon(":/images/jumptoicon.png"), tr("Jump to Position"), this);
    connect(jumpAction, SIGNAL(triggered()), this, SLOT(jumpToPositionSlot()));

    recordAction = new QAction(QIcon(":/images/recordicon.png"), tr("Record (no valid filename)"), this);
    recordAction->setEnabled(state->filename->isValid());
    connect(recordAction, SIGNAL(triggered()), this, SLOT(recordControllerSlot()));

    stopAction = new QAction(QIcon(":/images/stopicon.png"), tr("Stop"), this);
    stopAction->setEnabled(false);
    connect(stopAction, SIGNAL(triggered()), this, SLOT(stopControllerSlot()));

    selectFilenameAction = new QAction(QIcon(":/images/selectfilenameicon.png"), tr("Select Filename"), this);
    connect(selectFilenameAction, SIGNAL(triggered()), this, SLOT(selectBaseFilenameSlot()));

    chooseFileFormatAction = new QAction(QIcon(":/images/choosefileformaticon.png"), tr("Choose File Format"), this);
    connect(chooseFileFormatAction, SIGNAL(triggered()), this, SLOT(chooseFileFormatDialog()));

    triggeredRecordAction = new QAction(QIcon(":/images/triggeredrecordicon.png"), tr("Triggered Recording (no valid filename)"), this);
    triggeredRecordAction->setEnabled(state->filename->isValid());
    connect(triggeredRecordAction, SIGNAL(triggered()), this, SLOT(triggeredRecordControllerSlot()));
}

void ControlWindow::createMenus()
{
    // File menu
#if 0
    fileMenu = menuBar()->addMenu(tr("File"));
    fileMenu->addAction(loadSettingsAction);
    fileMenu->addAction(saveSettingsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(defaultSettingsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);
#endif

    // Display menu
    displayMenu = menuBar()->addMenu(tr("Display"));
    displayMenu->addAction(zoomInTScaleAction);
    displayMenu->addAction(zoomOutTScaleAction);
    displayMenu->addAction(toggleRollSweepAction);
    displayMenu->addSeparator();
    displayMenu->addAction(zoomInYScaleAction);
    displayMenu->addAction(zoomOutYScaleAction);
    displayMenu->addSeparator()->setText(tr("Labels"));
    displayMenu->addAction(displayLabelCustomNameAction);
    displayMenu->addAction(displayLabelNativeNameAction);
    displayMenu->addAction(displayLabelImpedanceMagnitudeAction);
    displayMenu->addAction(displayLabelImpedancePhaseAction);
    displayMenu->addAction(displayLabelReferenceAction);
    displayMenu->addAction(displayCycleAction);
    displayMenu->addSeparator();
    displayMenu->addAction(changeBackgroundColorAction);
    if (state->getControllerTypeEnum() != ControllerStimRecordUSB2) {
        displayMenu->addSeparator();
        displayMenu->addAction(showAuxInputsAction);
        displayMenu->addAction(showSupplyVoltagesAction);
    }

    // Channels menu
    channelMenu = menuBar()->addMenu(tr("Channels"));
    channelMenu->addAction(undoAction);
    channelMenu->addAction(redoAction);
    channelMenu->addSeparator();
    channelMenu->addAction(groupAction);
    channelMenu->addAction(ungroupAction);
    channelMenu->addSeparator();
    channelMenu->addAction(autoColorChannelsAction);
    channelMenu->addAction(autoGroupChannelsAction);
    channelMenu->addAction(ungroupAllChannelsAction);
    channelMenu->addSeparator();
    channelMenu->addAction(renameChannelAction);
    channelMenu->addAction(setReferenceAction);
    channelMenu->addAction(alphabetizeChannelsAction);
    channelMenu->addAction(restoreOrderChannelsAction);
    channelMenu->addSeparator();
    channelMenu->addAction(setSpikeThresholdsAction);

    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        // Stimulation menu
        stimulationMenu = menuBar()->addMenu(tr("Stimulation"));
        stimulationMenu->addAction(loadStimSettingsAction);
        stimulationMenu->addAction(saveStimSettingsAction);
        stimulationMenu->addSeparator();
        stimulationMenu->addAction(copyStimParametersAction);
        stimulationMenu->addAction(pasteStimParametersAction);
        stimulationMenu->addSeparator();
        stimulationMenu->addAction(ampSettleSettingsAction);
        stimulationMenu->addAction(chargeRecoverySettingsAction);
    }

    // Tools menu
    toolsMenu = menuBar()->addMenu(tr("Tools"));
    toolsMenu->addAction(spikeSortingAction);
    toolsMenu->addAction(isiAction);
    toolsMenu->addAction(psthAction);
    toolsMenu->addAction(spectrogramAction);
    toolsMenu->addAction(probeMapAction);

    // Performance menu
    toolsMenu = menuBar()->addMenu(tr("Performance"));
    toolsMenu->addAction(performanceAction);

    menuBar()->addSeparator();

    // Help
    helpMenu = menuBar()->addMenu(tr("Help"));
    helpMenu->addAction(keyboardHelpAction);
    helpMenu->addSeparator();
    helpMenu->addAction(enableLoggingAction);
    helpMenu->addSeparator();
    helpMenu->addAction(intanWebsiteAction);
    helpMenu->addAction(aboutAction);
}

void ControlWindow::createStatusBar()
{
    statusBarLabel = new QLabel(tr(""));
    statusBar()->addWidget(statusBarLabel, 1);
    statusBar()->setSizeGripEnabled(false); // Fixed window size
}

void ControlWindow::updateFromState()
{
    // Update menus.
    updateMenus();

    // Update trigger record dialog settings.
    if (triggerRecordDialog) {
        triggerRecordDialog->updateFromState();
    }

    // Update saved data file format dialog settings.
    if (fileFormatDialog) {
        fileFormatDialog->updateFromState();
    }

    // Update widgets to reflect filename's status.
    if (!state->running && !state->recording) {
        updateForFilename(state->filename->isValid());
    }

    // Update which widgets are disabled due to run mode.
    if (state->running != currentlyRunning || state->recording != currentlyRecording) {
        if (state->running || state->recording) updateForRun();
        else updateForStop();
        currentlyRunning = state->running;
        currentlyRecording = state->recording;
    }

    // Update TCP data output enabled window action.
    if (tcpDialog) {
        tcpDisplay->updateFromState();
    }

    // Update channel control panel.
    if (controlPanel)
        controlPanel->updateFromState();

    // Update probe map.
    if (probeMapWindow) {
        probeMapWindow->updateFromState();
    }
}

void ControlWindow::updateForChangeHeadstages()
{
    if (isiDialog) {
        isiDialog->updateForChangeHeadstages();
    }

    if (psthDialog) {
        psthDialog->updateForChangeHeadstages();
    }

    if (spectrogramDialog) {
        spectrogramDialog->updateForChangeHeadstages();
    }

    if (spikeSortingDialog) {
        spikeSortingDialog->updateForChangeHeadstages();
    }
}

void ControlWindow::updateForFilename(bool valid)
{
    recordAction->setEnabled(valid);
    triggeredRecordAction->setEnabled(valid);
    if (valid) {
        recordAction->setToolTip(tr("Record to ") + state->filename->getFullFilename());
        triggeredRecordAction->setToolTip(tr("Triggered Record to ") + state->filename->getFullFilename());
    } else {
        recordAction->setToolTip(tr("Record (no valid filename)"));
        triggeredRecordAction->setToolTip(tr("Triggered Recording (no valid filename)"));
    }
}

void ControlWindow::updateForRun()
{
    // Enable/disable various GUI buttons.
    runAction->setEnabled(fastPlaybackMode);
    fastForwardAction->setEnabled(false);
    rewindAction->setEnabled(false);
    //!stopAction->setEnabled(true);

    recordAction->setEnabled(false);
    if (state->filename->isValid())
        recordAction->setToolTip(tr("Record (must not be running)"));
    else
        recordAction->setToolTip(tr("Record (no valid filename)"));

    triggeredRecordAction->setEnabled(false);
    if (state->filename->isValid())
        triggeredRecordAction->setToolTip(tr("Triggered Recording (must not be running)"));
    else
        triggeredRecordAction->setToolTip(tr("Triggered Recording (no valid filename)"));

    selectFilenameAction->setEnabled(false);
    chooseFileFormatAction->setEnabled(false);

    if (state->playback->getValue()) {
        fastPlaybackAction->setEnabled(!fastPlaybackMode);
        jumpToStartAction->setEnabled(false);
        jumpBack1SecAction->setEnabled(false);
        jumpBack10SecAction->setEnabled(false);
        jumpAction->setEnabled(false);
    }

    loadSettingsAction->setEnabled(false);
    defaultSettingsAction->setEnabled(false);
    saveSettingsAction->setEnabled(false);

    changeBackgroundColorAction->setEnabled(false);

    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        loadStimSettingsAction->setEnabled(false);
        saveStimSettingsAction->setEnabled(false);
        ampSettleSettingsAction->setEnabled(false);
        chargeRecoverySettingsAction->setEnabled(false);
    }

    performanceAction->setEnabled(false);
    if (isiDialog) isiDialog->updateForRun();
    if (psthDialog) psthDialog->updateForRun();
    if (spectrogramDialog) spectrogramDialog->updateForRun();
    if (spikeSortingDialog) spikeSortingDialog->updateForRun();
    if (probeMapWindow) probeMapWindow->updateForRun();

    if (!state->recording && !state->triggerSet) {
        setStatusBarRunning();
    }

    controlPanel->updateForRun();
    multiColumnDisplay->updateForRun();
}

void ControlWindow::updateForLoad()
{
    // Disable all GUI buttons to prevent user changes during a load.
    runAction->setEnabled(false);
    fastForwardAction->setEnabled(false);
    rewindAction->setEnabled(false);
    stopAction->setEnabled(false);

    recordAction->setEnabled(false);
    triggeredRecordAction->setEnabled(false);
    selectFilenameAction->setEnabled(false);
    chooseFileFormatAction->setEnabled(false);

    if (state->playback->getValue()) {
        fastPlaybackAction->setEnabled(false);
        jumpToStartAction->setEnabled(false);
        jumpBack1SecAction->setEnabled(false);
        jumpBack10SecAction->setEnabled(false);
        jumpAction->setEnabled(false);
    }

    loadSettingsAction->setEnabled(false);
    defaultSettingsAction->setEnabled(false);
    saveSettingsAction->setEnabled(false);

    changeBackgroundColorAction->setEnabled(false);

    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        loadStimSettingsAction->setEnabled(false);
        saveStimSettingsAction->setEnabled(false);
        ampSettleSettingsAction->setEnabled(false);
        chargeRecoverySettingsAction->setEnabled(false);
    }

    performanceAction->setEnabled(false);
    if (isiDialog) isiDialog->updateForLoad();
    if (psthDialog) psthDialog->updateForLoad();
    if (spectrogramDialog) spectrogramDialog->updateForLoad();
    if (spikeSortingDialog) spikeSortingDialog->updateForLoad();
    if (probeMapWindow) probeMapWindow->updateForLoad();

    setStatusBarLoading();

    controlPanel->updateForLoad();
    multiColumnDisplay->updateForLoad();
}

void ControlWindow::updateForStop()
{
    setStatusBarReady();

    // Enable/disable various GUI buttons.
    runAction->setEnabled(true);
    fastForwardAction->setEnabled(controllerInterface->fastForwardPossible());
    rewindAction->setEnabled(controllerInterface->rewindPossible());
    stopAction->setEnabled(false);

    recordAction->setEnabled(state->filename->isValid());
    if (state->filename->isValid()) {
        recordAction->setToolTip(tr("Record to ") + state->filename->getFullFilename());
    } else {
        recordAction->setToolTip(tr("Record (no valid filename)"));
    }

    triggeredRecordAction->setEnabled(state->filename->isValid());
    if (state->filename->isValid()) {
        triggeredRecordAction->setToolTip(tr("Triggered Recording to ") + state->filename->getFullFilename());
    } else {
        triggeredRecordAction->setToolTip(tr("Triggered Recording (no valid filename)"));
    }

    selectFilenameAction->setEnabled(true);
    chooseFileFormatAction->setEnabled(true);

    if (state->playback->getValue()) {
        fastPlaybackAction->setEnabled(true);
        jumpToStartAction->setEnabled(true);
        jumpBack1SecAction->setEnabled(true);
        jumpBack10SecAction->setEnabled(true);
        jumpAction->setEnabled(true);
    }

    loadSettingsAction->setEnabled(true);
    defaultSettingsAction->setEnabled(true);
    saveSettingsAction->setEnabled(true);

    changeBackgroundColorAction->setEnabled(true);

    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        loadStimSettingsAction->setEnabled(true);
        saveStimSettingsAction->setEnabled(true);
        ampSettleSettingsAction->setEnabled(true);
        chargeRecoverySettingsAction->setEnabled(true);
    }

    performanceAction->setEnabled(true);
    if (isiDialog) isiDialog->updateForStop();
    if (psthDialog) psthDialog->updateForStop();
    if (spectrogramDialog) spectrogramDialog->updateForStop();
    if (spikeSortingDialog) spikeSortingDialog->updateForStop();
    if (probeMapWindow) probeMapWindow->updateForStop();

    controlPanel->updateForStop();
    multiColumnDisplay->updateForStop();
}

void ControlWindow::stopAndReportAnyErrors()
{
    updateForStop();
    if (!queuedErrorMessage.isEmpty()) {
        QMessageBox::critical(this, tr("Error"), queuedErrorMessage);
        queuedErrorMessage.clear();
    }
}

void ControlWindow::setStatusBarReady()
{
    if (state->synthetic->getValue()) {
        emit setStatusBar(tr("Ready to run with synthesized data."));
    } else if (state->playback->getValue()) {
        emit setStatusBar(tr("Ready to play back data file."));  // This is displayed when software starts, only.
        emit setStatusBarReadyPlayback();
    } else {
        emit setStatusBar(tr("Ready."));
    }
}

void ControlWindow::setStatusBarRunning()
{
    if (state->synthetic->getValue()) {
        emit setStatusBar(tr("Running with synthesized data."));
    } else if (state->playback->getValue()) {
        emit setStatusBar(tr("Data playback running."));
    } else {
        emit setStatusBar(tr("Running."));
    }
    emit setTimeLabel("00:00:00");
}

void ControlWindow::setStatusBarLoading()
{
    emit setStatusBar(tr("Loading..."));
}

void ControlWindow::showControlPanel()
{
    showControlPanelButton->hide();
    controlPanel->show();
}

void ControlWindow::hideControlPanel()
{
    showControlPanelButton->show();
    controlPanel->hide();
}

// If this is the first time the probe map window has been activated, create a probe map window. Activate the probe map.
void ControlWindow::probeMap()
{
    if (!probeMapWindow) {
        probeMapWindow = new ProbeMapWindow(state, controllerInterface, this);
        // Add any 'connect' statements here.
    }

    // If window was successfully created, show it.
    probeMapWindow->show();
    probeMapWindow->raise();
    probeMapWindow->activateWindow();
}

// If this is the first time the spectrogram window has been activated, create a spectrogram window. Activate the spectrogram.
void ControlWindow::spectrogram()
{
    if (!spectrogramDialog) {
        spectrogramDialog = new SpectrogramDialog(state, this);
        controllerInterface->setSpectrogramDialog(spectrogramDialog);
    }
    spectrogramDialog->activate();
}

// If this is the first time the PSTH window has been activated, create a PSTH window. Activate the PSTH tool.
void ControlWindow::psth()
{
    if (!psthDialog) {
        psthDialog = new PSTHDialog(state, this);
        controllerInterface->setPSTHDialog(psthDialog);
    }
    psthDialog->activate();
}

void ControlWindow::isi()
{
    if (!isiDialog) {
        isiDialog = new ISIDialog(state, this);
        controllerInterface->setISIDialog(isiDialog);
    }
    isiDialog->activate();
}

void ControlWindow::keyboardShortcutsHelp()
{
    if (!keyboardShortcutDialog) {
        keyboardShortcutDialog = new KeyboardShortcutDialog(this);
    }
    keyboardShortcutDialog->show();
    keyboardShortcutDialog->raise();
    keyboardShortcutDialog->activateWindow();
}

void ControlWindow::enableLoggingSlot(bool enable)
{
    // If true, inform the user how the log file works and query user for location log file should be written to.
    if (enable) {
        QMessageBox::StandardButton result;
        result = QMessageBox::information(nullptr, tr("Select Log File Name and Directory"),
                                       tr("Please select a name and directory for the log file (for Intan debugging purposes "
                                          "only).  This log file will be overwritten every time the software starts.  The file "
                                          "name and location will be retained across software sessions, and logging will persist "
                                          "until disabled through the Help menu."), QMessageBox::Ok | QMessageBox::Cancel);
        if (result == QMessageBox::Ok) {
            QSettings settings;
            QString defaultDirectory = settings.value("logFileDirectory", ".").toString();
            QString logFileName = QFileDialog::getSaveFileName(this, tr("Save Log File As"), defaultDirectory, "", nullptr,
                                                                QFileDialog::ShowDirsOnly);
            if (!logFileName.isEmpty()) {
                QFileInfo logFileInfo(logFileName);
                QString fileName = logFileInfo.path() + "/" + logFileInfo.completeBaseName() + ".txt";
                settings.setValue("logFileName", fileName);
                settings.setValue("logFileDirectory", logFileInfo.absolutePath());
            } else {
                enableLoggingAction->setChecked(false);
                return;
            }
        } else {
            enableLoggingAction->setChecked(false);
            return;
        }
    }

    // Toggle logging on/off.
    state->enableLogging(enable);
}

void ControlWindow::openIntanWebsite()
{
    QDesktopServices::openUrl(QUrl("Http://www.intantech.com", QUrl::TolerantMode));
}

void ControlWindow::about()
{
    QMessageBox::about(this, tr("About Intan RHX Software"),
                       tr("<h2>Intan RHX Data Acquisition Software</h2>"
                       "<p>Version ") + SoftwareVersion +
                       tr("<p>Copyright ") + CopyrightSymbol + " " + ApplicationCopyrightYear + tr(" Intan Technologies"
                       "<p>This application controls the "
                       "RHD USB Interface Board, RHD Recording Controllers, "
                       "and RHS Stimulation/Recording Controller "
                       "from Intan Technologies.  The C++/Qt source code "
                       "for this application is freely available from Intan Technologies. "
                       "For more information visit <i>www.intantech.com</i>."
                       "<p>This program is free software: you can redistribute it and/or modify "
                       "it under the terms of the GNU General Public License as published "
                       "by the Free Software Foundation, either version 3 of the License, or "
                       "(at your option) any later version."
                       "<p>This program is distributed in the hope that it will be useful, "
                       "but WITHOUT ANY WARRANTY; without even the implied warranty of "
                       "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
                       "GNU General Public License for more details."));
}

void ControlWindow::performance()
{
    PerformanceOptimizationDialog performanceDialog(state, this);
    if (performanceDialog.exec()) {
        // Get updated settings
        changeUsedXPUIndex(performanceDialog.XPUSelectionComboBox->currentIndex());
    }
}

void ControlWindow::spikeSorting()
{
    if (!spikeSortingDialog) {
        spikeSortingDialog = new SpikeSortingDialog(state, controllerInterface, this);
        controllerInterface->setSpikeSortingDialog(spikeSortingDialog);
    }
    spikeSortingDialog->activate();
}

void ControlWindow::chooseFileFormatDialog()
{
    if (!fileFormatDialog) {
        fileFormatDialog = new SetFileFormatDialog(state, this);
    } else {
        fileFormatDialog->updateFromState();
    }
    fileFormatDialog->show();
    fileFormatDialog->raise();
    fileFormatDialog->activateWindow();

    if (fileFormatDialog->exec()) {
        // Store current dialog values before sending update commands, so that GUI updates don't clear any changes.
        QString fileFormat = fileFormatDialog->getFileFormat();
        bool saveAuxInWithAmpWaveforms = fileFormatDialog->getSaveAuxInWithAmps();
        bool saveWidebandAmplifierWaveforms = fileFormatDialog->getSaveWidebandAmps();
        bool saveLowpassAmplifierWaveforms = fileFormatDialog->getSaveLowpassAmps();
        bool saveHighpassAmplifierWaveforms = fileFormatDialog->getSaveHighpassAmps();
        bool saveSpikeData = fileFormatDialog->getSaveSpikeData();
        bool saveSpikeSnapshots = fileFormatDialog->getSaveSpikeSnapshots();
        int spikeSnapshotPreDetect = fileFormatDialog->getSnapshotPreDetect();
        int spikeSnapshotPostDetect = fileFormatDialog->getSnapshotPostDetect();
        bool saveDCAmplifierWaveforms = false;
        if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
            saveDCAmplifierWaveforms = fileFormatDialog->getSaveDCAmps();
        }
        int newSaveFilePeriodMinutes = fileFormatDialog->getRecordTime();
        int lowpassDownsampleFactorIndex = fileFormatDialog->getLowpassDownsampleFactorIndex();

        state->holdUpdate();

        state->fileFormat->setValue(fileFormat);
        state->saveAuxInWithAmpWaveforms->setValue(saveAuxInWithAmpWaveforms);
        state->saveWidebandAmplifierWaveforms->setValue(saveWidebandAmplifierWaveforms);
        state->saveLowpassAmplifierWaveforms->setValue(saveLowpassAmplifierWaveforms);
        state->saveHighpassAmplifierWaveforms->setValue(saveHighpassAmplifierWaveforms);
        state->saveSpikeData->setValue(saveSpikeData);
        state->saveSpikeSnapshots->setValue(saveSpikeSnapshots);
        state->spikeSnapshotPreDetect->setValue(spikeSnapshotPreDetect);
        state->spikeSnapshotPostDetect->setValue(spikeSnapshotPostDetect);
        if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
            state->saveDCAmplifierWaveforms->setValue(saveDCAmplifierWaveforms);
        }
        state->newSaveFilePeriodMinutes->setValue(newSaveFilePeriodMinutes);
        state->lowpassWaveformDownsampleRate->setIndex(lowpassDownsampleFactorIndex);

        state->releaseUpdate();
    }
}

void ControlWindow::selectBaseFilenameSlot()
{
    QString newFilename;
    QString suffix = (state->getControllerTypeEnum() == ControllerStimRecordUSB2 ? ".rhs" : ".rhd");

    QSettings settings;
    QString defaultDirectory = settings.value("saveDirectory", ".").toString();

    switch (state->getFileFormatEnum()) {
    case FileFormatIntan:
        newFilename = QFileDialog::getSaveFileName(this, tr("Select Base Filename"), defaultDirectory, tr("Intan Data Files (*") + suffix + ")");
        break;

    case FileFormatFilePerSignalType:
        newFilename = QFileDialog::getSaveFileName(this, tr("Select Base Filename"), defaultDirectory, tr("Intan Data Files (*") + suffix + ")");
        break;

    case FileFormatFilePerChannel:
        newFilename = QFileDialog::getSaveFileName(this, tr("Select Base Filename"), defaultDirectory, tr("Intan Data Files (*") + suffix + ")");
        break;
    }

    if (!newFilename.isEmpty()) {
        QFileInfo newFileInfo(newFilename);
        state->filename->setBaseFilename(newFileInfo.baseName());
        state->filename->setPath(newFileInfo.absolutePath());
        settings.setValue("saveDirectory", newFileInfo.absolutePath());
    }
}

void ControlWindow::runControllerSlot()
{
    if (fastPlaybackMode) {
        fastPlaybackMode = false;
        fastPlaybackAction->setEnabled(true);
        runAction->setEnabled(false);
        emit setDataFileReaderSpeed(1.0);
    }
    if (!state->running) {
        emit sendSetCommand("RunMode", "Run");
    }
}

void ControlWindow::initiateSweep(double speed)
{
    rewindAction->setEnabled(false);
    fastForwardAction->setEnabled(false);
    runAction->setEnabled(false);
    stopAction->setEnabled(true);
    state->sweeping = true;
    state->forceUpdate();
    controllerInterface->sweepDisplay(speed);
    state->forceUpdate();
}

void ControlWindow::fastPlaybackSlot()
{
    fastPlaybackMode = true;
    emit setDataFileReaderSpeed(5.0);
    if (!state->running) {
        emit sendSetCommand("RunMode", "Run");
    } else {
        fastPlaybackAction->setEnabled(false);
        runAction->setEnabled(true);
    }
}

void ControlWindow::recordControllerSlot()
{
    emit sendSetCommand("RunMode", "Record");
    stopAction->setEnabled(false);
}

// Wait for user-defined trigger to start recording data from USB interface board to disk.
void ControlWindow::triggeredRecordControllerSlot()
{
    if (!triggerRecordDialog) {
        triggerRecordDialog = new TriggerRecordDialog(state, this);
    } else {
        triggerRecordDialog->updateFromState();
    }
    triggerRecordDialog->show();
    triggerRecordDialog->raise();
    triggerRecordDialog->activateWindow();

    if (triggerRecordDialog->exec()) {
        // Store current dialog values before sending update commands, so that GUI updates don't clear any changes.
        QString triggerSource = triggerRecordDialog->getTriggerInput();
        QString triggerPolarity = triggerRecordDialog->getTriggerPolarity();
        QString triggerSave = triggerRecordDialog->getTriggerSave();
        QString preTriggerBufferSeconds = triggerRecordDialog->getRecordBuffer();
        QString postTriggerBufferSeconds = triggerRecordDialog->getPostTriggerBufferSeconds();

        state->triggerSource->setValue(triggerSource);
        state->triggerPolarity->setValue(triggerPolarity);
        state->saveTriggerSource->setValue(triggerSave);
        state->preTriggerBuffer->setValue(preTriggerBufferSeconds);
        state->postTriggerBuffer->setValue(postTriggerBufferSeconds);

        // Automatically enable channel used for recording trigger if user has selected this option.
        if (state->saveTriggerSource->getValue()) {
            Channel* triggerChannel = state->signalSources->channelByName(state->triggerSource->getValue());
            if (triggerChannel) triggerChannel->setEnabled(true);
        }

        emit sendSetCommand("RunMode", "Trigger");
    } else {
        delete triggerRecordDialog;
        triggerRecordDialog = nullptr;
    }
}

void ControlWindow::stopControllerSlot()
{
    state->sweeping = false;
    emit sendSetCommand("RunMode", "Stop");
}

void ControlWindow::jumpToStartSlot()
{
    emit jumpToStart();
    controllerInterface->resetWaveformFifo();
    multiColumnDisplay->reset();
}

void ControlWindow::jumpBack1SecSlot()
{
    emit jumpRelative(-1.0);
    controllerInterface->resetWaveformFifo();
    multiColumnDisplay->reset();
}

void ControlWindow::jumpBack10SecSlot()
{
    emit jumpRelative(-10.0);
    controllerInterface->resetWaveformFifo();
    multiColumnDisplay->reset();
}

void ControlWindow::jumpToPositionSlot()
{
    PlaybackFilePositionDialog jumpToPositionDialog(controllerInterface->currentTimePlaybackFile(),
                                                    controllerInterface->startTimePlaybackFile(),
                                                    controllerInterface->endTimePlaybackFile(),
                                                    state->runAfterJumpToPosition->getValue(), this);
    if (jumpToPositionDialog.exec()) {
        emit jumpToPosition(jumpToPositionDialog.getTime());
        controllerInterface->resetWaveformFifo();
        multiColumnDisplay->reset();

        bool runImmediately = jumpToPositionDialog.runImmediately();
        state->runAfterJumpToPosition->setValue(runImmediately);
        if (runImmediately) {
            emit sendSetCommand("RunMode", "Run");
        }
    }
}

void ControlWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_F1:
        if (!(event->isAutoRepeat())) {
            controllerInterface->setManualStimTrigger(0, true);
        }
        break;
    case Qt::Key_F2:
        if (!(event->isAutoRepeat())) {
            controllerInterface->setManualStimTrigger(1, true);
        }
        break;
    case Qt::Key_F3:
        if (!(event->isAutoRepeat())) {
            controllerInterface->setManualStimTrigger(2, true);
        }
        break;
    case Qt::Key_F4:
        if (!(event->isAutoRepeat())) {
            controllerInterface->setManualStimTrigger(3, true);
        }
        break;
    case Qt::Key_F5:
        if (!(event->isAutoRepeat())) {
            controllerInterface->setManualStimTrigger(4, true);
        }
        break;
    case Qt::Key_F6:
        if (!(event->isAutoRepeat())) {
            controllerInterface->setManualStimTrigger(5, true);
        }
        break;
    case Qt::Key_F7:
        if (!(event->isAutoRepeat())) {
            controllerInterface->setManualStimTrigger(6, true);
        }
        break;
    case Qt::Key_F8:
        if (!(event->isAutoRepeat())) {
            controllerInterface->setManualStimTrigger(7, true);
        }
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

void ControlWindow::keyReleaseEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_F1:
        if (!(event->isAutoRepeat())) {
            controllerInterface->setManualStimTrigger(0, false);
        }
        break;
    case Qt::Key_F2:
        if (!(event->isAutoRepeat())) {
            controllerInterface->setManualStimTrigger(1, false);
        }
        break;
    case Qt::Key_F3:
        if (!(event->isAutoRepeat())) {
            controllerInterface->setManualStimTrigger(2, false);
        }
        break;
    case Qt::Key_F4:
        if (!(event->isAutoRepeat())) {
            controllerInterface->setManualStimTrigger(3, false);
        }
        break;
    case Qt::Key_F5:
        if (!(event->isAutoRepeat())) {
            controllerInterface->setManualStimTrigger(4, false);
        }
        break;
    case Qt::Key_F6:
        if (!(event->isAutoRepeat())) {
            controllerInterface->setManualStimTrigger(5, false);
        }
        break;
    case Qt::Key_F7:
        if (!(event->isAutoRepeat())) {
            controllerInterface->setManualStimTrigger(6, false);
        }
        break;
    case Qt::Key_F8:
        if (!(event->isAutoRepeat())) {
            controllerInterface->setManualStimTrigger(7, false);
        }
        break;
    default:
        QWidget::keyReleaseEvent(event);
    }
}

void ControlWindow::closeEvent(QCloseEvent *event)
{
    // Perform any clean-up here before application closes.
    if (state->running)
        emit sendSetCommand("RunMode", "Stop");

    event->accept();
}

bool ControlWindow::loadSettingsFile(QString filename)
{
    QString errorMessage;
    bool loadSuccess = state->loadGlobalSettings(filename, errorMessage);

    if (!loadSuccess) {
        QMessageBox::critical(this, tr("Error: Could not load settings file ") + filename, errorMessage);
    } else if (!errorMessage.isEmpty()) {
        QMessageBox::critical(this, tr("Warning: Problem loading settings file ") + filename, errorMessage);
    }
    controllerInterface->updateChipCommandLists(false);  // Update amplifier bandwidth settings.
    restoreDisplaySettings();
    return loadSuccess;
}

void ControlWindow::restoreDisplaySettings()
{
    // Update display settings from displaySettings string: state of multi-column display, scroll bars, pinned
    // waveforms, etc.
    multiColumnDisplay->restoreFromDisplaySettingsString(state->displaySettings->getValueString());
}

void ControlWindow::setSaveFilenameTemplate(const QString &fname)
{
    QFileInfo newFileInfo(fname);
    state->filename->setBaseFilename(newFileInfo.baseName());
    state->filename->setPath(newFileInfo.absolutePath());
}

QByteArray ControlWindow::globalSettingsAsByteArray()
{
    state->displaySettings->setValue(multiColumnDisplay->getDisplaySettingsString());
    return state->globalSettingsAsByteArray();
}

bool ControlWindow::globalSettingsFromByteArray(const QByteArray &settings)
{
    QString errorMessage;
    const auto ret = state->globalSettingsFromByteArray(settings, errorMessage);

    if (!ret) {
        QMessageBox::critical(this, QStringLiteral("Error: Could not load settings"), errorMessage);
    } else if (!errorMessage.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("Warning: Problem loading settings"), errorMessage);
    }
    controllerInterface->updateChipCommandLists(false);  // Update amplifier bandwidth settings.
    restoreDisplaySettings();

    return ret;
}

void ControlWindow::loadSettingsSlot()
{
    loadSettings();
}

void ControlWindow::defaultSettingsSlot()
{
    QString controllerString = ControllerTypeString[(int)state->getControllerTypeEnum()];
    QMessageBox::information(this, tr("Set Default Settings File"),
                             tr("Please select a default settings file.  This file will load automatically every time "
                                "the software starts with an ") + controllerString +
                             tr(".  You can cancel this action by unchecking the appropriate box when the software starts."));

    QString filename = loadSettings();

    if (!filename.isEmpty()) {
        QSettings settings;
        settings.beginGroup(ControllerTypeSettingsGroup[(int)state->getControllerTypeEnum()]);
        settings.setValue("loadDefaultSettingsFile", true);
        settings.setValue("defaultSettingsFile", filename);
        settings.endGroup();
    }
}

QString ControlWindow::loadSettings()
{
    if (stimParamWarning()) return QString("");

    updateForLoad();

    QSettings settings;
    settings.beginGroup(ControllerTypeSettingsGroup[(int)state->getControllerTypeEnum()]);
    QString defaultDirectory = settings.value("settingsDirectory", ".").toString();

    QString filename = QFileDialog::getOpenFileName(this, "Select Settings File", defaultDirectory, "XML files (*.xml)");

    // If user clicked 'cancel', then abort.
    if (filename.isEmpty()) {
        updateForStop();
        settings.endGroup();
        return QString("");
    }

    bool loadSuccess = loadSettingsFile(filename);

    if (loadSuccess) {
        QFileInfo fileInfo(filename);
        settings.setValue("settingsDirectory", fileInfo.absolutePath());
        settings.endGroup();
    }
    updateForStop();
    return filename;
}

void ControlWindow::saveSettingsSlot()
{
    QSettings settings;
    settings.beginGroup(ControllerTypeSettingsGroup[(int)state->getControllerTypeEnum()]);
    QString defaultDirectory = settings.value("settingsDirectory", ".").toString();

    QString filename = QFileDialog::getSaveFileName(this, "Save Settings As", defaultDirectory, "XML files (*.xml)");

    // If user clicked 'cancel', then abort.
    if (filename.isEmpty()) {
        settings.endGroup();
        return;
    }

    QFileInfo fileInfo(filename);
    settings.setValue("settingsDirectory", fileInfo.absolutePath());
    settings.endGroup();

    // Generate display settings string to record state of multi-column display, scroll bars, pinned waveforms, etc.
    state->displaySettings->setValue(multiColumnDisplay->getDisplaySettingsString());

    if (!state->saveGlobalSettings(filename)) qDebug() << "Failure writing XML Global Settings";
}

// Load stimulation settings from *.xml file.
void ControlWindow::loadStimSettingsSlot()
{
    if (stimParamWarning()) return;

    QSettings settings;
    settings.beginGroup(ControllerTypeSettingsGroup[(int)state->getControllerTypeEnum()]);
    QString defaultDirectory = settings.value("stimSettingsDirectory", ".").toString();

    QString filename = QFileDialog::getOpenFileName(this, "Select Settings File", defaultDirectory, "XML files (*.xml)");

    // If user clicked 'cancel', then abort.
    if (filename.isEmpty()) {
        settings.endGroup();
        return;
    }

    QFileInfo fileInfo(filename);
    settings.setValue("stimSettingsDirectory", fileInfo.absolutePath());
    settings.endGroup();

    QString errorMessage;
    bool loadSuccess = false;
    loadSuccess = stimParametersInterface->loadFile(filename, errorMessage);

    if (!loadSuccess) {
        QMessageBox::critical(this, tr("Error: Loading from XML"), errorMessage + " Trying to parse this XML as a legacy file...");
    } else  {
        if (!errorMessage.isEmpty()) {
            QMessageBox::warning(this, tr("Warning: Loading from XML"), errorMessage);
        }
        return;
    }

    errorMessage = "";
    loadSuccess = stimParametersInterface->loadFile(filename, errorMessage, true); // Try parsing as with stimLegacy=true

    if (!loadSuccess) {
        QMessageBox::critical(this, tr("Error: Loading from XML"), errorMessage);
    } else {
        if (!errorMessage.isEmpty()) {
            QMessageBox::warning(this, tr("Warning: Loading from XML"), errorMessage);
        }
    }
}

// Save stimulation settings to *.xml file.
void ControlWindow::saveStimSettingsSlot()
{
    QSettings settings;
    settings.beginGroup(ControllerTypeSettingsGroup[(int)state->getControllerTypeEnum()]);
    QString defaultDirectory = settings.value("stimSettingsDirectory", ".").toString();

    QString filename = QFileDialog::getSaveFileName(this, "Save Settings As", defaultDirectory, "XML files (*.xml)");

    // If user clicked 'cancel', then abort.
    if (filename.isEmpty()) {
        settings.endGroup();
        return;
    }

    QFileInfo fileInfo(filename);
    settings.setValue("stimSettingsDirectory", fileInfo.absolutePath());
    settings.endGroup();

    if (stimParametersInterface->saveFile(filename)) qDebug() << "Success writing Stimulation Parameters";
    else qDebug() << "Failure writing Stimulation Parameters";
}

void ControlWindow::undoSlot()
{
    state->signalSources->undoManager->undo();
    state->forceUpdate();
}

void ControlWindow::redoSlot()
{
    state->signalSources->undoManager->redo();
    state->forceUpdate();
}

void ControlWindow::copyStimParametersSlot()
{
    stimClipboard->copy(state->signalSources->selectedChannel());
    state->forceUpdate();
}

void ControlWindow::pasteStimParametersSlot()
{
    QList<Channel*> selectedChannels;
    state->signalSources->getSelectedSignals(selectedChannels);
    stimClipboard->paste(selectedChannels);
}

// Launch amp settle settings dialog and apply new settings.
void ControlWindow::ampSettleSettingsSlot()
{
    AmpSettleDialog ampSettleDialog(state->desiredLowerSettleBandwidth->getValue(), state->useFastSettle->getValue(), state->headstageGlobalSettle->getValue(), this);
    if (ampSettleDialog.exec()) {
        // Get updated settings.
        state->desiredLowerSettleBandwidth->setValueWithLimits(ampSettleDialog.lowFreqSettleLineEdit->text().toDouble());
        state->useFastSettle->setValue(ampSettleDialog.ampSettleModeComboBox->currentIndex() == 1);
        state->headstageGlobalSettle->setValue(ampSettleDialog.settleHeadstageCheckBox->isChecked());

        controllerInterface->uploadAmpSettleSettings();
    }
}

// Launch charge recovery settings dialog and apply new settings.
void ControlWindow::chargeRecoverySettingsSlot()
{
    ChargeRecoveryDialog chargeRecoveryDialog(state->chargeRecoveryMode->getValue(), state->getChargeRecoveryCurrentLimitEnum(), state->chargeRecoveryTargetVoltage->getValue(), this);
    if (chargeRecoveryDialog.exec()) {
        // Update settings.
        state->chargeRecoveryMode->setValue(chargeRecoveryDialog.chargeRecoveryModeComboBox->currentIndex() == 1);
        state->chargeRecoveryCurrentLimit->setIndex((int) chargeRecoveryDialog.getCurrentLimit());
        state->chargeRecoveryTargetVoltage->setValueWithLimits(chargeRecoveryDialog.targetVoltageLineEdit->text().toDouble());

        controllerInterface->uploadChargeRecoverySettings();
    }
}

void ControlWindow::toggleRollSweep()
{
    if (state->triggerModeDisplay->getValue() && !state->rollMode->getValue()) return;  // Don't allow roll mode when triggering.
    state->rollMode->toggleValue();
}

void ControlWindow::changeUsedXPUIndex(int index)
{
    emit sendSetCommand("UsedXPUIndex", QString::number(index));
}

void ControlWindow::changeAudioVolume(int volume)
{
    state->audioVolume->setValue(volume);
}

void ControlWindow::changeAudioThreshold(int threshold)
{
    state->audioThreshold->setValue(threshold);
}

void ControlWindow::remoteControl()
{
    if (!tcpDialog) {
        tcpDialog = new QDialog(this);
        tcpDisplay = new TCPDisplay(state, this);
        QVBoxLayout *tcpLayout = new QVBoxLayout;
        tcpLayout->addWidget(tcpDisplay);
        tcpDialog->setLayout(tcpLayout);
        tcpDialog->setWindowTitle(tr("Remote TCP Control"));
        connect(tcpDisplay, SIGNAL(sendSetCommand(QString, QString)), parser, SLOT(setCommandSlot(QString, QString)));
        connect(tcpDisplay, SIGNAL(sendGetCommand(QString)), parser, SLOT(getCommandSlot(QString)));
        connect(tcpDisplay, SIGNAL(sendExecuteCommand(QString)), parser, SLOT(executeCommandSlot(QString)));
        connect(tcpDisplay, SIGNAL(sendExecuteCommandWithParameter(QString,QString)), parser, SLOT(executeCommandWithParameterSlot(QString,QString)));
        connect(tcpDisplay, SIGNAL(sendNoteCommand(QString)), parser, SLOT(noteCommandSlot(QString)));
        connect(parser, SIGNAL(TCPReturnSignal(QString)), tcpDisplay, SLOT(TCPReturn(QString)));
        connect(parser, SIGNAL(TCPErrorSignal(QString)), tcpDisplay, SLOT(TCPError(QString)));
    }
    tcpDialog->show();
    tcpDialog->raise();
    tcpDialog->activateWindow();
}

void ControlWindow::updateMenus()
{
    if (state->running || state->sweeping) {
        undoAction->setEnabled(false);
        redoAction->setEnabled(false);
        groupAction->setEnabled(false);
        ungroupAction->setEnabled(false);
        ungroupAllChannelsAction->setEnabled(false);
        renameChannelAction->setEnabled(false);
        setReferenceAction->setEnabled(false);
        autoColorChannelsAction->setEnabled(false);
        autoGroupChannelsAction->setEnabled(false);
        restoreOrderChannelsAction->setEnabled(false);
        alphabetizeChannelsAction->setEnabled(false);
        setSpikeThresholdsAction->setEnabled(false);
    } else {
        undoAction->setEnabled(state->signalSources->undoManager->canUndo());
        redoAction->setEnabled(state->signalSources->undoManager->canRedo());
        groupAction->setEnabled(groupIsPossible());
        ungroupAction->setEnabled(ungroupIsPossible());
        ungroupAllChannelsAction->setEnabled(state->signalSources->anyChannelsGrouped());
        renameChannelAction->setEnabled(state->signalSources->numChannelsSelected() == 1);
        setReferenceAction->setEnabled(state->signalSources->onlyAmplifierChannelsSelected());
        autoColorChannelsAction->setEnabled(true);
        autoGroupChannelsAction->setEnabled(true);
        restoreOrderChannelsAction->setEnabled(true);
        alphabetizeChannelsAction->setEnabled(true);
        setSpikeThresholdsAction->setEnabled(true);
    }

    bool enableTools = state->headstagePresent->getValue();

    probeMapAction->setEnabled(enableTools);
    isiAction->setEnabled(enableTools);
    psthAction->setEnabled(enableTools);
    spectrogramAction->setEnabled(enableTools);
    spikeSortingAction->setEnabled(enableTools);

    if (!enableTools) {
        if (probeMapWindow) {
            probeMapWindow->close();
            delete probeMapWindow;
            probeMapWindow = nullptr;
        }
        if (isiDialog) {
            isiDialog->close();
            delete isiDialog;
            isiDialog = nullptr;
            controllerInterface->setISIDialog(isiDialog);
        }
        if (psthDialog) {
            psthDialog->close();
            delete psthDialog;
            psthDialog = nullptr;
            controllerInterface->setPSTHDialog(psthDialog);
        }
        if (spectrogramDialog) {
            spectrogramDialog->close();
            delete spectrogramDialog;
            spectrogramDialog = nullptr;
            controllerInterface->setSpectrogramDialog(spectrogramDialog);
        }
        if (spikeSortingDialog) {
            spikeSortingDialog->close();
            delete spikeSortingDialog;
            spikeSortingDialog = nullptr;
            controllerInterface->setSpikeSortingDialog(spikeSortingDialog);
        }
    }

    // Only allow copying of stim parameters if a single channel is selected, and it must be headstage, analog out, or digital out
    if (state->controllerType->getIndex() == ControllerStimRecordUSB2) {
        bool enableCopy = false;
        if (state->signalSources->numChannelsSelected() == 1) {
            Channel* selectedChannel = state->signalSources->selectedChannel();
            if (selectedChannel->getSignalType() == AmplifierSignal || selectedChannel->getSignalType() == BoardDacSignal || selectedChannel->getSignalType() == BoardDigitalOutSignal) {
                enableCopy = true;
            }
        }
        copyStimParametersAction->setEnabled(enableCopy);
    }

    // Only allow pasting of stim parameters if board is stopped, at least one channel is selected, the clipboard must contain a set of parameters, and
    // the clipboard's type must match all of the selected channels' type
    if (state->controllerType->getIndex() == ControllerStimRecordUSB2) {
        bool enablePaste = false;

        // If board is running, don't allow pasting. If board isn't running, check other conditions.
        if (!state->running) {
            if (state->signalSources->numChannelsSelected() >= 1 && stimClipboard->isFull()) {
                // All selected channels must have the same type, and also the same type of the clipboard
                enablePaste = true;
                SignalType firstSignalType = state->signalSources->selectedChannel()->getSignalType();
                QList<Channel*> selectedChannels;
                state->signalSources->getSelectedSignals(selectedChannels);
                for (QList<Channel*>::const_iterator i = selectedChannels.begin(); i != selectedChannels.end(); ++i) {
                    if ((*i)->getSignalType() != firstSignalType || (*i)->getSignalType() != stimClipboard->getSignalType()) {
                        enablePaste = false;
                        break;
                    }
                }
            }
        }
        pasteStimParametersAction->setEnabled(enablePaste);
    }

    if (state->controllerType->getIndex() != ControllerStimRecordUSB2) {
        showAuxInputsAction->setChecked(state->showAuxInputs->getValue());
        showSupplyVoltagesAction->setChecked(state->showSupplyVoltages->getValue());
    }
}

void ControlWindow::adjustYScale(int delta)
{
    state->holdUpdate();

    YScaleUsed yScaleUsed = controlPanel->slidersEnabled();
    if (yScaleUsed.widepassYScaleUsed) state->yScaleWide->shiftIndex(delta);
    if (yScaleUsed.lowpassYScaleUsed) state->yScaleLow->shiftIndex(delta);
    if (yScaleUsed.highpassYScaleUsed) state->yScaleHigh->shiftIndex(delta);
    if (yScaleUsed.dcYScaleUsed) state->yScaleDC->shiftIndex(delta);
    if (yScaleUsed.auxInputYScaleUsed) state->yScaleAux->shiftIndex(delta);
    if (yScaleUsed.analogIOYScaleUsed) state->yScaleAnalog->shiftIndex(delta);

    state->releaseUpdate();
}

void ControlWindow::cycleLabelText()
{
    int index = state->displayLabelText->getIndex();
    if (++index == state->displayLabelText->numberOfItems()) index = 0;
    QString value = state->displayLabelText->getValue(index);
    state->displayLabelText->setValue(value);
    if (value == "CustomName") {
        displayLabelCustomNameAction->setChecked(true);
    } else if (value == "NativeName") {
        displayLabelNativeNameAction->setChecked(true);
    } else if (value == "ImpedanceMagnitude") {
        displayLabelImpedanceMagnitudeAction->setChecked(true);
    } else if (value == "ImpedancePhase") {
        displayLabelImpedancePhaseAction->setChecked(true);
    } else if (value == "Reference") {
        displayLabelReferenceAction->setChecked(true);
    }
}

void ControlWindow::changeBackgroundColor()
{
    // Get QColor from modal dialog from user.
    QColor result = QColorDialog::getColor(QColor(state->backgroundColor->getValueString()), this);

    // If this color is valid (user didn't cancel), set the color for all currently selected channels.
    if (result.isValid()) {
        state->backgroundColor->setValue(result.name());
    }
}

void ControlWindow::setSpikeThresholds()
{
    SetThresholdsDialog setThresholdsDialog(state->absoluteThresholdsEnabled->getValue(), state->absoluteThreshold->getValue(),
                                            state->rmsMultipleThreshold->getValue(), state->negativeRelativeThreshold->getValue(),
                                            this);
    if (setThresholdsDialog.exec()) {
        state->absoluteThresholdsEnabled->setValue(setThresholdsDialog.absoluteThreshold());
        state->absoluteThreshold->setValue(setThresholdsDialog.threshold());
        state->rmsMultipleThreshold->setValueWithLimits(abs(setThresholdsDialog.rmsMultiple()));
        state->negativeRelativeThreshold->setValue(setThresholdsDialog.rmsMultiple() < 0);

        controllerInterface->setAllSpikeDetectionThresholds();
    }
}

void ControlWindow::autoColorChannels()
{
    AutoColorDialog autoColorDialog(this);
    if (autoColorDialog.exec()) {
        state->signalSources->undoManager->pushStateToUndoStack();
        state->signalSources->autoColorAmplifierChannels(autoColorDialog.numColors(), autoColorDialog.numChannelsSameColor());
        state->forceUpdate();
    }
}

void ControlWindow::autoGroupChannels()
{
    AutoGroupDialog autoGroupDialog(this);
    if (autoGroupDialog.exec()) {
        state->signalSources->undoManager->pushStateToUndoStack();
        state->signalSources->autoGroupAmplifierChannels(autoGroupDialog.groupSize());
        state->forceUpdate();
    }
}

void ControlWindow::ungroupAllChannels()
{
    if (QMessageBox::question(this, tr("Ungroup All Channels"), tr("Ungroup all grouped amplifier channels?")) ==
            QMessageBox::Yes) {
        state->signalSources->undoManager->pushStateToUndoStack();
        state->signalSources->ungroupAllChannels();
        state->forceUpdate();
    }
}

void ControlWindow::restoreOrderChannels()
{
    state->signalSources->undoManager->pushStateToUndoStack();
    state->signalSources->setOriginalChannelOrder();
    state->forceUpdate();
}

void ControlWindow::alphabetizeChannels()
{
    state->signalSources->undoManager->pushStateToUndoStack();
    state->signalSources->setAlphabeticalChannelOrder();
    state->forceUpdate();
}

void ControlWindow::renameChannel()
{
    if (state->signalSources->numChannelsSelected() != 1) return;  // Single channel must be selected.

    Channel* selectedChannel = state->signalSources->selectedChannel();
    if (!selectedChannel) return;
    QString nativeName = selectedChannel->getNativeName();
    QString oldCustomName = selectedChannel->getCustomName();

    RenameChannelDialog renameChannelDialog(nativeName, oldCustomName, this);
    if (renameChannelDialog.exec()) {
        QString newCustomName = renameChannelDialog.name();
        if (state->signalSources->isCustomNamePresent(newCustomName)) {
            QMessageBox::warning(this, tr("Warning: Duplicate Names Not Allowed"), tr("The name ") + newCustomName +
                                                                               tr(" is already used by another channel."));
        } else {
            state->signalSources->undoManager->pushStateToUndoStack();
            selectedChannel->setCustomName(newCustomName);
            state->forceUpdate();
        }
    }
}

void ControlWindow::setReference()
{
    QList<Channel*> selectedSignals;
    state->signalSources->getSelectedSignals(selectedSignals);
    QString oldReference = "Hardware";
    if (selectedSignals.size() == 1) {
        oldReference = selectedSignals[0]->getReference();
    } else {
        bool sameReference = true;
        for (int i = 0; i < selectedSignals.size() - 1; i++) {
            if (selectedSignals.at(i)->getReference() != selectedSignals.at(i + 1)->getReference()) {
                sameReference = false;
                break;
            }
        }
        if (sameReference) {
            oldReference = selectedSignals[0]->getReference();
        }
    }

    ReferenceSelectDialog referenceSelectDialog(oldReference, state->signalSources, this);
    if (referenceSelectDialog.exec()) {
        QString newReference = referenceSelectDialog.referenceString();
        if (newReference != oldReference) {
            state->signalSources->undoManager->pushStateToUndoStack();
            state->signalSources->setSelectedChannelReferences(newReference);
            state->forceUpdate();
        }
    }
}

// Return true if user has canceled load. Returns false is user hasn't canceled, or if this warning doesn't apply.
bool ControlWindow::stimParamWarning()
{
    bool cancel = false;
    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2 && state->stimParamsHaveChanged) {
        QMessageBox::StandardButton ret = QMessageBox::warning(this, tr("Warning: Stimulation Parameters Overwrite"),
                                      tr("Stimulation parameters have been changed by the user.  "
                                         "Loading a settings file with new stimulation parameters will "
                                         "overwrite these changes.  Continue?"),
                                      QMessageBox::Yes | QMessageBox::No);
        if (ret == QMessageBox::Yes) {
            cancel = false;
            state->stimParamsHaveChanged = false;
        }
        else if (ret == QMessageBox::No)
            cancel = true;
    }
    return cancel;
}

void ControlWindow::updateHardwareFifoStatus(double percentFull)
{
    if (percentFull > 95.0) {
        // We must see the FIFO >95% full three times in a row to eliminate the possiblity of a USB glitch
        // causing recording to stop.
        if (++hwFifoNearlyFull > 2) {
            stopControllerSlot();
            queueErrorMessage(tr("<b>USB Buffer Overrun Error</b>"
                                 "<p>Recording was stopped because the USB buffer on the Intan controller "
                                 "reached maximum capacity.  This happens when the host computer "
                                 "cannot keep up with the data streaming over the USB cable."
                                 "<p>Try running with a lower sample rate or closing other applications "
                                 "to reduce CPU load."));
        }
    } else {
        hwFifoNearlyFull = 0;
    }

    double swBuffer = controllerInterface->swBufferPercentFull();
    if (swBuffer > 98.0) {
        stopControllerSlot();
        queueErrorMessage(tr("<b>Software Buffer Overrun Error</b>"
                             "<p>Recording was stopped because the software waveform buffer "
                             "reached maximum capacity.  This happens when the host computer "
                             "cannot process the waveform data quickly enough."
                             "<p>Try running with a lower sample rate or closing other applications "
                             "to reduce CPU load."));
    }

    double cpuLoad = max(mainCpuLoad, controllerInterface->latestWaveformProcessorCpuLoad());

    if (statusBars) statusBars->updateBars(percentFull, swBuffer, cpuLoad);
}

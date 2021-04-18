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

#ifndef CONTROLWINDOW_H
#define CONTROLWINDOW_H

#include <iostream>
#include <QtWidgets>

#include "rhxglobals.h"
#include "rhxcontroller.h"
#include "rhxregisters.h"
#include "probemapwindow.h"
#include "controllerinterface.h"
#include "systemstate.h"
#include "keyboardshortcutdialog.h"
#include "setfileformatdialog.h"
#include "triggerrecorddialog.h"
#include "controlpanel.h"
#include "multicolumndisplay.h"
#include "tcpdisplay.h"
#include "commandparser.h"
#include "performanceoptimizationdialog.h"
#include "isidialog.h"
#include "psthdialog.h"
#include "spectrogramdialog.h"
#include "spikesortingdialog.h"
#include "xmlinterface.h"
#include "stimparametersclipboard.h"
#include "statusbars.h"

class QPushButton;

class ControlWindow : public QMainWindow
{
    Q_OBJECT

public:
    ControlWindow(SystemState* state_, CommandParser* parser_, ControllerInterface* controllerInterface_);
    ~ControlWindow();

    bool groupIsPossible() const { return multiColumnDisplay->groupIsPossible(); }
    bool ungroupIsPossible() const { return multiColumnDisplay->ungroupIsPossible(); }

    void enableSelectedChannels(bool enable) { multiColumnDisplay->enableSelectedChannels(enable); }
    bool loadSettingsFile(QString filename);
    void restoreDisplaySettings();

signals:
    void sendSetCommand(QString parameter, QString value);
    void sendGetCommand(QString parameter);
    void sendExecuteCommand(QString action);
    void sendExecuteCommandWithParameter(QString action, QString parameter);
    void sendNoteCommand(QString noteString);
    void setTimeLabel(QString text);
    void setStatusBar(QString text);
    void setStatusBarReadyPlayback();
    void jumpToStart();
    void jumpToPosition(QString target);
    void jumpRelative(double jumpInSeconds);
    void setDataFileReaderSpeed(double playbackSpeed);

public slots:
    void updateFromState();
    void updateForChangeHeadstages();
    void updateTimeLabel(QString text) { timeLabel->setText("<b>" + text + "</b>"); }
    void updateTopStatusLabel(QString text) { topStatusLabel->setText(text); }
    void updateStatusBar(QString text) { statusBarLabel->setText(text); }
    void queueErrorMessage(QString errorMessage) { queuedErrorMessage = errorMessage; }
    void stopAndReportAnyErrors();
    void group() { multiColumnDisplay->group(); }
    void ungroup() { multiColumnDisplay->ungroup(); }
    void renameChannel();
    void setReference();
    void updateHardwareFifoStatus(double percentFull);
    void updateMainCpuLoad(double percent) { mainCpuLoad = percent; }
    void hideControlPanel();

private slots:
    void showControlPanel();

    void probeMap();

    void performance();
    void psth();
    void spectrogram();
    void spikeSorting();

    void isi();

    void keyboardShortcutsHelp();
    void enableLoggingSlot(bool enable);
    void openIntanWebsite();
    void about();

    void chooseFileFormatDialog();
    void selectBaseFilenameSlot();

    void runControllerSlot();
    void recordControllerSlot();
    void triggeredRecordControllerSlot();
    void stopControllerSlot();

    void rewindSlot() { initiateSweep(-2.5); }
    void fastForwardSlot() { initiateSweep(2.5); }

    void fastPlaybackSlot();
    void jumpToStartSlot();
    void jumpBack1SecSlot();
    void jumpBack10SecSlot();
    void jumpToPositionSlot();

    void loadSettingsSlot();
    void defaultSettingsSlot();
    void saveSettingsSlot();
    void loadStimSettingsSlot();
    void saveStimSettingsSlot();
    void undoSlot();
    void redoSlot();
    void copyStimParametersSlot();
    void pasteStimParametersSlot();
    void ampSettleSettingsSlot();
    void chargeRecoverySettingsSlot();

    void zoomInTScale() { state->tScale->decrementIndex(); }
    void zoomOutTScale() { state->tScale->incrementIndex(); }
    void toggleRollSweep();
    void zoomInYScale() { adjustYScale(-1); }
    void zoomOutYScale() { adjustYScale(1); }

    void setLabelCustomName() { state->displayLabelText->setValue("CustomName"); }
    void setLabelNativeName() { state->displayLabelText->setValue("NativeName"); }
    void setLabelImpedanceMagnitude() { state->displayLabelText->setValue("ImpedanceMagnitude"); }
    void setLabelImpedancePhase() { state->displayLabelText->setValue("ImpedancePhase"); }
    void setLabelReference() { state->displayLabelText->setValue("Reference"); }
    void cycleLabelText();
    void changeBackgroundColor();
    void setShowAuxInputs() { state->showAuxInputs->setValue(showAuxInputsAction->isChecked()); }
    void setShowSupplyVoltages() { state->showSupplyVoltages->setValue(showSupplyVoltagesAction->isChecked()); }

    void setSpikeThresholds();
    void autoColorChannels();
    void autoGroupChannels();
    void ungroupAllChannels();
    void restoreOrderChannels();
    void alphabetizeChannels();

    void changeUsedXPUIndex(int index);

    void changeAudioVolume(int volume);
    void changeAudioThreshold(int threshold);

    void remoteControl(); // Create a non-modal dialog window governing TCP communication.

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    SystemState* state;
    ControllerInterface* controllerInterface;
    CommandParser* parser;

    QToolButton *showControlPanelButton;

    ProbeMapWindow *probeMapWindow;

    QToolBar *controlButtons;

    KeyboardShortcutDialog *keyboardShortcutDialog;
    TriggerRecordDialog *triggerRecordDialog;
    SetFileFormatDialog *fileFormatDialog;
    ISIDialog *isiDialog;
    PSTHDialog *psthDialog;
    SpectrogramDialog *spectrogramDialog;
    SpikeSortingDialog *spikeSortingDialog;

    QMenu *fileMenu;
    QMenu *displayMenu;
    QMenu *channelMenu;
    QMenu *stimulationMenu;
    QMenu *tcpMenu;
    QMenu *impedanceMenu;
    QMenu *toolsMenu;
    QMenu *helpMenu;

    QAction *runAction;
    QAction *stopAction;
    QAction *selectFilenameAction;
    QAction *chooseFileFormatAction;
    QAction *recordAction;
    QAction *triggeredRecordAction;
    QAction *rewindAction;
    QAction *fastForwardAction;
    QAction *fastPlaybackAction;
    QAction *jumpToStartAction;
    QAction *jumpBack1SecAction;
    QAction *jumpBack10SecAction;
    QAction *jumpAction;

    QAction *zoomInTScaleAction;
    QAction *zoomOutTScaleAction;
    QAction *toggleRollSweepAction;
    QAction *zoomInYScaleAction;
    QAction *zoomOutYScaleAction;

    QActionGroup *displayLabelGroup;
    QAction *displayLabelCustomNameAction;
    QAction *displayLabelNativeNameAction;
    QAction *displayLabelImpedanceMagnitudeAction;
    QAction *displayLabelImpedancePhaseAction;
    QAction *displayLabelReferenceAction;
    QAction *displayCycleAction;
    QAction *changeBackgroundColorAction;
    QAction *showAuxInputsAction;
    QAction *showSupplyVoltagesAction;

    QAction *loadSettingsAction;
    QAction *defaultSettingsAction;
    QAction *saveSettingsAction;
    QAction *loadStimSettingsAction;
    QAction *saveStimSettingsAction;
    QAction *exitAction;

    QAction *undoAction;
    QAction *redoAction;
    QAction *groupAction;
    QAction *ungroupAction;
    QAction *renameChannelAction;
    QAction *setReferenceAction;
    QAction *setSpikeThresholdsAction;
    QAction *autoColorChannelsAction;
    QAction *autoGroupChannelsAction;
    QAction *ungroupAllChannelsAction;
    QAction *restoreOrderChannelsAction;
    QAction *alphabetizeChannelsAction;

    QAction *copyStimParametersAction;
    QAction *pasteStimParametersAction;
    QAction *ampSettleSettingsAction;
    QAction *chargeRecoverySettingsAction;

    QAction *keyboardHelpAction;
    QAction *enableLoggingAction;
    QAction *intanWebsiteAction;
    QAction *aboutAction;

    QAction *remoteControlAction;

    QAction *isiAction;
    QAction *probeMapAction;
    QAction *spectrogramAction;
    QAction *psthAction;

    QAction *performanceAction;

    QAction *spikeSortingAction;

    QLabel *timeLabel;
    QLabel *topStatusLabel;
    QLabel *statusBarLabel;
    QString queuedErrorMessage;

    StatusBars* statusBars;

    ControlPanel *controlPanel;

    MultiColumnDisplay *multiColumnDisplay;

    QDialog *tcpDialog;

    TCPDisplay *tcpDisplay;

    QHBoxLayout *showHideRow;

    QSpacerItem *showHideStretch;

    XMLInterface* stimParametersInterface;

    StimParametersClipboard *stimClipboard;

    bool currentlyRunning;
    bool currentlyRecording;
    bool fastPlaybackMode;

    int hwFifoNearlyFull;
    double mainCpuLoad;

    QString loadSettings();

    void updateForFilename(bool valid);
    void updateLiveNotesDialog();
    void updateForRun();
    void updateForLoad();
    void updateForStop();

    void createActions();
    void createMenus();
    void updateMenus();

    void createStatusBar();
    void setStatusBarReady();
    void setStatusBarRunning();
    void setStatusBarLoading();

    void initiateSweep(double speed);

    void adjustYScale(int delta);

    bool stimParamWarning();
};

#endif // CONTROLWINDOW_H

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

#include <QProgressDialog>
#include <QElapsedTimer>
#include "cabledelaydialog.h"
#include "auxdigoutconfigdialog.h"
#include "controlpanelconfiguretab.h"

ControlPanelConfigureTab::ControlPanelConfigureTab(ControllerInterface* controllerInterface_, SystemState* state_,
                                                   CommandParser* parser_, QWidget *parent) :
    QWidget(parent),
    fastSettleCheckBox(nullptr),
    state(state_),
    parser(parser_),
    controllerInterface(controllerInterface_),
    scanButton(nullptr),
    setCableDelayButton(nullptr),
    digOutButton(nullptr),
    externalFastSettleCheckBox(nullptr),
    externalFastSettleSpinBox(nullptr),
    note1LineEdit(nullptr),
    note2LineEdit(nullptr),
    note3LineEdit(nullptr),
    liveNotesLineEdit(nullptr),
    liveNotesButton(nullptr),
    lastLiveNoteLabel(nullptr),
    fastSettleEnabledOld(false),
    externalFastSettleEnabledOld(false),
    externalFastSettleChannelOld(1)
{
    const int NumPorts = state->numSPIPorts;
    spiPort.resize(NumPorts, nullptr);
    for (int i = 0; i < (int) spiPort.size(); ++i) {
        spiPort[i] = state->signalSources->portGroupByIndex(i);
    }
    manualDelayEnabledOld.resize(8, false);
    manualDelayOld.resize(8, 1);
    auxDigOutEnabledOld.resize(8, false);
    auxDigOutChannelOld.resize(8, 0);

    QVBoxLayout *configLayout = new QVBoxLayout;
    scanButton = new QPushButton(tr("Rescan Ports"), this);
    setCableDelayButton = new QPushButton(tr("Manual"), this);

    QHBoxLayout *scanLayout = new QHBoxLayout;
    scanLayout->addWidget(scanButton);
    scanLayout->addWidget(setCableDelayButton);
    if (!state->testMode->getValue()) {
        scanLayout->addStretch();
    }

    QGroupBox *scanGroupBox = new QGroupBox(tr("Connected Amplifiers"), this);
    scanGroupBox->setLayout(scanLayout);

    QHBoxLayout *configTopLayout1 = new QHBoxLayout;
    configTopLayout1->addWidget(scanGroupBox);

    QGroupBox *notesGroupBox = nullptr;
    QGroupBox *liveNotesGroupBox = nullptr;
    if (!state->testMode->getValue()) {
        note1LineEdit = new QLineEdit(this);
        note2LineEdit = new QLineEdit(this);
        note3LineEdit = new QLineEdit(this);
        note1LineEdit->setMaxLength(255);   // Note: default maxlength of a QLineEdit is 32767
        note2LineEdit->setMaxLength(255);
        note3LineEdit->setMaxLength(255);

        connect(note1LineEdit, SIGNAL(editingFinished()), this, SLOT(setNotes()));
        connect(note2LineEdit, SIGNAL(editingFinished()), this, SLOT(setNotes()));
        connect(note3LineEdit, SIGNAL(editingFinished()), this, SLOT(setNotes()));

        QHBoxLayout *note1Layout = new QHBoxLayout;
        note1Layout->addWidget(new QLabel(tr("Note 1:"), this));
        note1Layout->addWidget(note1LineEdit);
        QHBoxLayout *note2Layout = new QHBoxLayout;
        note2Layout->addWidget(new QLabel(tr("Note 2:"), this));
        note2Layout->addWidget(note2LineEdit);
        QHBoxLayout *note3Layout = new QHBoxLayout;
        note3Layout->addWidget(new QLabel(tr("Note 3:"), this));
        note3Layout->addWidget(note3LineEdit);

        QVBoxLayout *notesLayout = new QVBoxLayout;
        notesLayout->addWidget(new QLabel(tr("The following text will be appended to saved data files"), this));
        notesLayout->addLayout(note1Layout);
        notesLayout->addLayout(note2Layout);
        notesLayout->addLayout(note3Layout);
        notesLayout->addStretch(1);

        notesGroupBox = new QGroupBox(tr("Notes"), this);
        notesGroupBox->setLayout(notesLayout);

        QVBoxLayout *liveNotesLayout = new QVBoxLayout;
        liveNotesLayout->addWidget(new QLabel(tr("The following text will be appended to the live notes file"), this));
        liveNotesLineEdit = new QLineEdit(this);
        liveNotesLineEdit->setMaxLength(1024);
        liveNotesLayout->addWidget(liveNotesLineEdit);
        QHBoxLayout *liveNotesButtonLayout = new QHBoxLayout;
        liveNotesButton = new QPushButton(tr("Add Live Note"), this);
        lastLiveNoteLabel = new QLabel("", this);
        lastLiveNoteLabel->setFixedWidth(200);
        liveNotesButtonLayout->addWidget(liveNotesButton);
        liveNotesButtonLayout->addWidget(lastLiveNoteLabel);
        liveNotesButtonLayout->addStretch(1);

        liveNotesLayout->addLayout(liveNotesButtonLayout);
        liveNotesLayout->addStretch(1);

        liveNotesGroupBox = new QGroupBox(tr("Live Notes"), this);
        liveNotesGroupBox->setLayout(liveNotesLayout);

        connect(liveNotesButton, SIGNAL(clicked()), this, SLOT(addLiveNote()));
        connect(liveNotesLineEdit, SIGNAL(returnPressed()), this, SLOT(addLiveNote()));
    }

    QGroupBox *fastSettleGroupBox;
    if (state->getControllerTypeEnum() != ControllerStimRecord) {
        if (!state->testMode->getValue()) {
            digOutButton = new QPushButton(tr("Configure"), this);
            QHBoxLayout *digOutLayout = new QHBoxLayout;
            digOutLayout->addWidget(digOutButton);
            digOutLayout->addStretch(1);

            QGroupBox *digOutGroupBox = new QGroupBox(tr("Auxout Pins"), this);
            digOutGroupBox->setLayout(digOutLayout);

            configTopLayout1->addWidget(digOutGroupBox);

            connect(digOutButton, SIGNAL(clicked()), this, SLOT(configDigOutControl()));
        }

        fastSettleCheckBox = new QCheckBox(tr("Manual"), this);

        externalFastSettleCheckBox = new QCheckBox(tr("Realtime Control"), this);
        externalFastSettleCheckBox->setEnabled(!(state->playback->getValue()));

        externalFastSettleSpinBox = new QSpinBox(this);
        externalFastSettleSpinBox->setPrefix(tr("DIGITAL IN "));
        externalFastSettleSpinBox->setRange(1, 16);
        externalFastSettleSpinBox->setSingleStep(1);
        externalFastSettleSpinBox->setValue(1);
        externalFastSettleSpinBox->setEnabled(!(state->playback->getValue()));

        connect(fastSettleCheckBox, SIGNAL(clicked(bool)), this, SLOT(enableFastSettle(bool)));
        connect(externalFastSettleCheckBox, SIGNAL(clicked(bool)), this, SLOT(enableExternalFastSettle(bool)));
        connect(externalFastSettleSpinBox, SIGNAL(valueChanged(int)), this, SLOT(setExternalFastSettleChannel(int)));

        QHBoxLayout *fastSettleLayout = new QHBoxLayout;
        fastSettleLayout->addWidget(fastSettleCheckBox);
        if (!state->testMode->getValue()) {
            fastSettleLayout->addStretch(1);
        }
        fastSettleLayout->addWidget(externalFastSettleCheckBox);
        fastSettleLayout->addWidget(externalFastSettleSpinBox);

        fastSettleGroupBox = new QGroupBox(tr("Amplifier Fast Settle (Blanking)"), this);
        fastSettleGroupBox->setLayout(fastSettleLayout);
    }
    configLayout->addLayout(configTopLayout1);
    if (state->getControllerTypeEnum() != ControllerStimRecord) {
        configLayout->addWidget(fastSettleGroupBox);
    }
    if (!state->testMode->getValue()) {
        configLayout->addWidget(notesGroupBox);
        configLayout->addWidget(liveNotesGroupBox);
        configLayout->addStretch(1);
    }
    setLayout(configLayout);

    connect(this, SIGNAL(sendExecuteCommand(QString)), parser, SLOT(executeCommandSlot(QString)));
    connect(this, SIGNAL(sendNoteCommand(QString)), parser, SLOT(noteCommandSlot(QString)));
    connect(parser, SIGNAL(sendLiveNote(QString)), this, SLOT(displayLastLiveNote(QString)));
    connect(scanButton, SIGNAL(clicked()), this, SLOT(rescanPorts()));
    connect(setCableDelayButton, SIGNAL(clicked()), this, SLOT(manualCableDelayControl()));
}

void ControlPanelConfigureTab::updateFromState()
{
    bool nonPlayback = !(state->playback->getValue());
    scanButton->setEnabled(!state->running && nonPlayback);
    setCableDelayButton->setEnabled(!state->running && nonPlayback);

    if (state->getControllerTypeEnum() != ControllerStimRecord) {
        if (!state->testMode->getValue()) {
            digOutButton->setEnabled(!state->running && nonPlayback);
        }
        fastSettleCheckBox->setEnabled(!externalFastSettleCheckBox->isChecked() && nonPlayback);
        externalFastSettleCheckBox->setEnabled(!fastSettleCheckBox->isChecked() && nonPlayback);
        externalFastSettleSpinBox->setEnabled(!fastSettleCheckBox->isChecked() && nonPlayback);
    }

    bool fastSettleEnabled = state->manualFastSettleEnabled->getValue();
    if (fastSettleEnabled != fastSettleEnabledOld) {
        fastSettleEnabledOld = fastSettleEnabled;
        fastSettleCheckBox->setChecked(fastSettleEnabled);
        controllerInterface->enableFastSettle(fastSettleEnabled);
    }

    bool externalFastSettleEnabled = state->externalFastSettleEnabled->getValue();
    if (externalFastSettleEnabled != externalFastSettleEnabledOld) {
        externalFastSettleEnabledOld = externalFastSettleEnabled;
        externalFastSettleCheckBox->setChecked(externalFastSettleEnabled);
        controllerInterface->enableExternalFastSettle(externalFastSettleEnabled);
    }

    int externalFastSettleChannel = state->externalFastSettleChannel->getValue();
    if (externalFastSettleChannel != externalFastSettleChannelOld) {
        externalFastSettleChannelOld = externalFastSettleChannel;
        externalFastSettleSpinBox->setValue(externalFastSettleChannel);
        controllerInterface->setExternalFastSettleChannel(externalFastSettleChannel);
    }

    bool manualDelayChanged = false;
    for (int port = 0; port < (int) spiPort.size(); ++port) {
        if (manualDelayEnabledOld[port] != spiPort[port]->manualDelayEnabled->getValue()) {
            manualDelayEnabledOld[port] = spiPort[port]->manualDelayEnabled->getValue();
            manualDelayChanged = true;
        }
        if (manualDelayOld[port] != spiPort[port]->manualDelay->getValue()) {
            manualDelayOld[port] = spiPort[port]->manualDelay->getValue();
            manualDelayChanged = true;
        }
    }
    if (manualDelayChanged) {
        controllerInterface->setManualCableDelays();
    }

    bool digOutChanged = false;
    for (int port = 0; port < (int) spiPort.size(); ++port) {
        if (auxDigOutEnabledOld[port] != spiPort[port]->auxDigOutEnabled->getValue()) {
            auxDigOutEnabledOld[port] = spiPort[port]->auxDigOutEnabled->getValue();
            digOutChanged = true;
        }
        if (auxDigOutChannelOld[port] != spiPort[port]->auxDigOutChannel->getValue()) {
            auxDigOutChannelOld[port] = spiPort[port]->auxDigOutChannel->getValue();
            digOutChanged = true;
        }
    }
    if (digOutChanged) {
        controllerInterface->enableExternalDigOut(PortA, spiPort[0]->auxDigOutEnabled->getValue());
        controllerInterface->enableExternalDigOut(PortB, spiPort[1]->auxDigOutEnabled->getValue());
        controllerInterface->enableExternalDigOut(PortC, spiPort[2]->auxDigOutEnabled->getValue());
        controllerInterface->enableExternalDigOut(PortD, spiPort[3]->auxDigOutEnabled->getValue());
        if (state->numSPIPorts == 8) {
            controllerInterface->enableExternalDigOut(PortE, spiPort[4]->auxDigOutEnabled->getValue());
            controllerInterface->enableExternalDigOut(PortF, spiPort[5]->auxDigOutEnabled->getValue());
            controllerInterface->enableExternalDigOut(PortG, spiPort[6]->auxDigOutEnabled->getValue());
            controllerInterface->enableExternalDigOut(PortH, spiPort[7]->auxDigOutEnabled->getValue());
        }

        controllerInterface->setExternalDigOutChannel(PortA, spiPort[0]->auxDigOutChannel->getValue());
        controllerInterface->setExternalDigOutChannel(PortB, spiPort[1]->auxDigOutChannel->getValue());
        controllerInterface->setExternalDigOutChannel(PortC, spiPort[2]->auxDigOutChannel->getValue());
        controllerInterface->setExternalDigOutChannel(PortD, spiPort[3]->auxDigOutChannel->getValue());
        if (state->numSPIPorts == 8) {
            controllerInterface->setExternalDigOutChannel(PortE, spiPort[4]->auxDigOutChannel->getValue());
            controllerInterface->setExternalDigOutChannel(PortF, spiPort[5]->auxDigOutChannel->getValue());
            controllerInterface->setExternalDigOutChannel(PortG, spiPort[6]->auxDigOutChannel->getValue());
            controllerInterface->setExternalDigOutChannel(PortH, spiPort[7]->auxDigOutChannel->getValue());
        }
    }

    if (!state->testMode->getValue()) {
        if (state->recording) {
            liveNotesLineEdit->setEnabled(true);
            liveNotesButton->setEnabled(true);
        } else {
            liveNotesLineEdit->setEnabled(false);
            liveNotesButton->setEnabled(false);
            lastLiveNoteLabel->clear();
        }

        note1LineEdit->setText(state->note1->getValueString());
        note2LineEdit->setText(state->note2->getValueString());
        note3LineEdit->setText(state->note3->getValueString());
    }
}

void ControlPanelConfigureTab::updateForRun()
{
    enableNotes(false);
    setNotes();
}

void ControlPanelConfigureTab::updateForLoad()
{
    updateForRun();
}

void ControlPanelConfigureTab::updateForStop()
{
    enableNotes(true);
}

void ControlPanelConfigureTab::rescanPorts(bool usePreviousDelay, int selectedPort)
{
    // Create a dummy progress bar to show that the rescan has been executed.  (Not necessary, but good user feedback.)
    int maxProgress = 25;
    QProgressDialog progress(QObject::tr("Scanning Ports"), QString(), 0, maxProgress);
    progress.setWindowTitle(QObject::tr("Progress"));
    progress.setMinimumDuration(0);
    progress.setModal(true);

    state->usePreviousDelay->setValue(usePreviousDelay);
    state->previousDelaySelectedPort->setValue(selectedPort);

    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < maxProgress / 2; ++i) {
        progress.setValue(i);
        while (timer.nsecsElapsed() < qint64(50000000)) {
            qApp->processEvents();
        }
        timer.restart();
    }

    emit sendExecuteCommand("RescanPorts");

    for (int i = maxProgress / 2; i <= maxProgress; ++i) {
        progress.setValue(i);
        while (timer.nsecsElapsed() < qint64(50000000)) {
            qApp->processEvents();
        }
        timer.restart();
    }
}

void ControlPanelConfigureTab::manualCableDelayControl()
{
    const int NumPorts = (int) spiPort.size();
    std::vector<int> currentDelays(NumPorts, 0);
    controllerInterface->getCableDelay(currentDelays);
    std::vector<bool> manualDelayEnabled(NumPorts, false);
    for (int i = 0; i < (int) manualDelayEnabled.size(); ++i) {
        manualDelayEnabled[i] = spiPort[i]->manualDelayEnabled->getValue();
    }

    CableDelayDialog manualCableDelayDialog(manualDelayEnabled, currentDelays, NumPorts, this);
    if (manualCableDelayDialog.exec()) {
        spiPort[0]->manualDelayEnabled->setValue(manualCableDelayDialog.manualPortACheckBox->isChecked());
        spiPort[1]->manualDelayEnabled->setValue(manualCableDelayDialog.manualPortBCheckBox->isChecked());
        spiPort[2]->manualDelayEnabled->setValue(manualCableDelayDialog.manualPortCCheckBox->isChecked());
        spiPort[3]->manualDelayEnabled->setValue(manualCableDelayDialog.manualPortDCheckBox->isChecked());
        if (NumPorts == 8) {
            spiPort[4]->manualDelayEnabled->setValue(manualCableDelayDialog.manualPortECheckBox->isChecked());
            spiPort[5]->manualDelayEnabled->setValue(manualCableDelayDialog.manualPortFCheckBox->isChecked());
            spiPort[6]->manualDelayEnabled->setValue(manualCableDelayDialog.manualPortGCheckBox->isChecked());
            spiPort[7]->manualDelayEnabled->setValue(manualCableDelayDialog.manualPortHCheckBox->isChecked());
        }

        if (spiPort[0]->manualDelayEnabled->getValue()) {
            spiPort[0]->manualDelay->setValue(manualCableDelayDialog.delayPortASpinBox->value());
            controllerInterface->setCableDelay(PortA, spiPort[0]->manualDelay->getValue());
        }
        if (spiPort[1]->manualDelayEnabled->getValue()) {
            spiPort[1]->manualDelay->setValue(manualCableDelayDialog.delayPortBSpinBox->value());
            controllerInterface->setCableDelay(PortA, spiPort[1]->manualDelay->getValue());
        }
        if (spiPort[2]->manualDelayEnabled->getValue()) {
            spiPort[2]->manualDelay->setValue(manualCableDelayDialog.delayPortCSpinBox->value());
            controllerInterface->setCableDelay(PortA, spiPort[2]->manualDelay->getValue());
        }
        if (spiPort[3]->manualDelayEnabled->getValue()) {
            spiPort[3]->manualDelay->setValue(manualCableDelayDialog.delayPortDSpinBox->value());
            controllerInterface->setCableDelay(PortA, spiPort[3]->manualDelay->getValue());
        }
        if (NumPorts == 8) {
            if (spiPort[4]->manualDelayEnabled->getValue()) {
                spiPort[4]->manualDelay->setValue(manualCableDelayDialog.delayPortESpinBox->value());
                controllerInterface->setCableDelay(PortA, spiPort[4]->manualDelay->getValue());
            }
            if (spiPort[5]->manualDelayEnabled->getValue()) {
                spiPort[5]->manualDelay->setValue(manualCableDelayDialog.delayPortFSpinBox->value());
                controllerInterface->setCableDelay(PortA, spiPort[5]->manualDelay->getValue());
            }
            if (spiPort[6]->manualDelayEnabled->getValue()) {
                spiPort[6]->manualDelay->setValue(manualCableDelayDialog.delayPortGSpinBox->value());
                controllerInterface->setCableDelay(PortA, spiPort[6]->manualDelay->getValue());
            }
            if (spiPort[7]->manualDelayEnabled->getValue()) {
                spiPort[7]->manualDelay->setValue(manualCableDelayDialog.delayPortHSpinBox->value());
                controllerInterface->setCableDelay(PortA, spiPort[7]->manualDelay->getValue());
            }
        }
        // Commands to controller are send in updateFromState()
    }
}

void ControlPanelConfigureTab::configDigOutControl()
{
    const int NumPorts = state->numSPIPorts;
    std::vector<SignalGroup*> spiPort(NumPorts, nullptr);
    for (int i = 0; i < (int) spiPort.size(); ++i) {
        spiPort[i] = state->signalSources->portGroupByIndex(i);
    }
    std::vector<bool> auxDigOutEnabled(NumPorts, false);
    for (int i = 0; i < (int) auxDigOutEnabled.size(); ++i) {
        auxDigOutEnabled[i] = spiPort[i]->auxDigOutEnabled->getValue();
    }
    std::vector<int> auxDigOutChannel(NumPorts, 0);
    for (int i = 0; i < (int) auxDigOutEnabled.size(); ++i) {
        auxDigOutChannel[i] = spiPort[i]->auxDigOutChannel->getValue();
    }
    AuxDigOutConfigDialog auxDigOutConfigDialog(auxDigOutEnabled, auxDigOutChannel, NumPorts, this);
    if (auxDigOutConfigDialog.exec()) {
        for (int i = 0; i < NumPorts; ++i) {
            spiPort[i]->auxDigOutEnabled->setValue(auxDigOutConfigDialog.enabled(i));
            spiPort[i]->auxDigOutChannel->setValue(auxDigOutConfigDialog.channel(i));
        }
        controllerInterface->enableExternalDigOut(PortA, auxDigOutConfigDialog.enabled(0));
        controllerInterface->enableExternalDigOut(PortB, auxDigOutConfigDialog.enabled(1));
        controllerInterface->enableExternalDigOut(PortC, auxDigOutConfigDialog.enabled(2));
        controllerInterface->enableExternalDigOut(PortD, auxDigOutConfigDialog.enabled(3));
        if (NumPorts == 8) {
            controllerInterface->enableExternalDigOut(PortE, auxDigOutConfigDialog.enabled(4));
            controllerInterface->enableExternalDigOut(PortF, auxDigOutConfigDialog.enabled(5));
            controllerInterface->enableExternalDigOut(PortG, auxDigOutConfigDialog.enabled(6));
            controllerInterface->enableExternalDigOut(PortH, auxDigOutConfigDialog.enabled(7));
        }
        controllerInterface->setExternalDigOutChannel(PortA, auxDigOutConfigDialog.channel(0));
        controllerInterface->setExternalDigOutChannel(PortB, auxDigOutConfigDialog.channel(1));
        controllerInterface->setExternalDigOutChannel(PortC, auxDigOutConfigDialog.channel(2));
        controllerInterface->setExternalDigOutChannel(PortD, auxDigOutConfigDialog.channel(3));
        if (NumPorts == 8) {
            controllerInterface->setExternalDigOutChannel(PortE, auxDigOutConfigDialog.channel(4));
            controllerInterface->setExternalDigOutChannel(PortF, auxDigOutConfigDialog.channel(5));
            controllerInterface->setExternalDigOutChannel(PortG, auxDigOutConfigDialog.channel(6));
            controllerInterface->setExternalDigOutChannel(PortH, auxDigOutConfigDialog.channel(7));
        }
    }
}

void ControlPanelConfigureTab::enableFastSettle(bool enable)
{
    state->manualFastSettleEnabled->setValue(enable);
}

void ControlPanelConfigureTab::enableExternalFastSettle(bool enable)
{
    state->externalFastSettleEnabled->setValue(enable);
}

void ControlPanelConfigureTab::setExternalFastSettleChannel(int channel)
{
    state->externalFastSettleChannel->setValue(channel);
}

void ControlPanelConfigureTab::addLiveNote()
{
    if (state->testMode->getValue()) return;
    emit sendNoteCommand(liveNotesLineEdit->text());
    liveNotesLineEdit->clear();
}

void ControlPanelConfigureTab::setNotes()
{
    if (state->testMode->getValue()) return;
    state->note1->setValue(note1LineEdit->text());
    state->note2->setValue(note2LineEdit->text());
    state->note3->setValue(note3LineEdit->text());
}

void ControlPanelConfigureTab::enableNotes(bool enabled)
{
    if (state->testMode->getValue()) return;
    note1LineEdit->setEnabled(enabled);
    note2LineEdit->setEnabled(enabled);
    note3LineEdit->setEnabled(enabled);
}

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

#ifndef CONTROLPANELCONFIGURETAB_H
#define CONTROLPANELCONFIGURETAB_H

#include <QtWidgets>
#include "controllerinterface.h"
#include "systemstate.h"
#include "commandparser.h"

class ControlPanelConfigureTab : public QWidget
{
    Q_OBJECT
public:
    explicit ControlPanelConfigureTab(ControllerInterface* controllerInterface_, SystemState* state_, CommandParser* parser_,
                                      QWidget *parent = nullptr);
    void updateFromState();
    void updateForRun();
    void updateForLoad();
    void updateForStop();

signals:
    void sendExecuteCommand(QString);
    void sendNoteCommand(QString);

private slots:
    void rescanPorts();
    void manualCableDelayControl();
    void configDigOutControl();
    void enableFastSettle(bool enable);
    void enableExternalFastSettle(bool enable);
    void setExternalFastSettleChannel(int channel);
    void addLiveNote();
    void displayLastLiveNote(QString note) { lastLiveNoteLabel->setText("Last note: " + note); }
    void setNotes();

private:
    void enableNotes(bool enabled);

    SystemState* state;
    CommandParser* parser;
    ControllerInterface* controllerInterface;

    QPushButton *scanButton;
    QPushButton *setCableDelayButton;
    QPushButton *digOutButton;

    QCheckBox *fastSettleCheckBox;
    QCheckBox *externalFastSettleCheckBox;
    QSpinBox *externalFastSettleSpinBox;

    QLineEdit *note1LineEdit;
    QLineEdit *note2LineEdit;
    QLineEdit *note3LineEdit;

    QLineEdit *liveNotesLineEdit;
    QPushButton *liveNotesButton;
    QLabel *lastLiveNoteLabel;

    vector<SignalGroup*> spiPort;
    vector<bool> manualDelayEnabledOld;
    vector<int> manualDelayOld;
    bool fastSettleEnabledOld;
    bool externalFastSettleEnabledOld;
    int externalFastSettleChannelOld;
};

#endif // CONTROLPANELCONFIGURETAB_H

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

#ifndef TCPDISPLAY_H
#define TCPDISPLAY_H

#include <QtWidgets>
#include <QtNetwork>

#include "tcpcommunicator.h"
#include "systemstate.h"
#include "signalsources.h"

class QLineEdit;
class QTextEdit;
class QSpinBox;
class QPushButton;
class QLabel;
class QCheckBox;
class QTableWidget;

class TCPDisplay : public QWidget
{
    Q_OBJECT
public:
    explicit TCPDisplay(SystemState* state_, QWidget *parent = nullptr);
    void updateFromState();

private:
    QLineEdit *waveformOutputHostLineEdit;
    QSpinBox *waveformOutputPortSpinBox;

    QLineEdit *spikeOutputHostLineEdit;
    QSpinBox *spikeOutputPortSpinBox;

    QLineEdit *commandsHostLineEdit;
    QSpinBox *commandsPortSpinBox;
    QPushButton *commandsConnectButton;
    QPushButton *commandsDisconnectButton;
    QLabel *commandsStatus;

    QTextEdit *commandTextEdit;
    QPushButton *clearCommandsButton;

    QTextEdit *errorTextEdit;
    QPushButton *clearErrorsButton;

    QPushButton *waveformOutputConnectButton;
    QPushButton *spikeOutputConnectButton;

    QPushButton *waveformOutputDisconnectButton;
    QPushButton *spikeOutputDisconnectButton;

    QTableWidget *presentChannelsTable;

    QPushButton *addChannelButton;
    QPushButton *removeChannelButton;

    QPushButton *addAllChannelsButton;
    QPushButton *removeAllChannelsButton;

    QLabel *filterSelectLabel;
    QComboBox *filterSelectComboBox;

    QTableWidget *channelsToStreamTable;

    QLabel *dataOutputStatus;

    QLabel *dataRateStatus;

    SystemState *state;
    SignalSources *signalSources;

    void parseCommands(const QString& commands);
    void addChannel(const QString& channelName);
    void removeChannel(const QString& channelName);
    void updateTables();
    void updatePresentChannelsTable();
    void updateChannelsToStreamTable();

private slots:
    void updateCommandWidgets();

    void waveformOutputHostEdited();
    void waveformOutputPortChanged();

    void spikeOutputHostEdited();
    void spikeOutputPortChanged();

    void clearCommands();
    void clearErrors();

    void selectPresentChannels();
    void selectChannelsToStream();
    void addChannels();
    void removeChannels();
    void addAllChannels();
    void removeAllChannels();

    void updateDataOutputWidgets();

signals:
    void sendSetCommand(QString parameter, QString value);
    void sendGetCommand(QString parameter);
    void sendExecuteCommand(QString action);
    void sendExecuteCommandWithParameter(QString action, QString parameter);
    void sendNoteCommand(QString note);
    void establishWaveformConnection();
    void establishSpikeConnection();

public slots:
    void processNewCommandConnection();
    void processNewWaveformOutputConnection();
    void processNewSpikeOutputConnection();
    void commandsConnect();
    void waveformOutputConnect();
    void spikeOutputConnect();
    void commandsDisconnect();
    void waveformOutputDisconnect();
    void spikeOutputDisconnect();
    void readClientCommand();
    void TCPReturn(QString result);
    void TCPError(QString errorString);
    void TCPWarning(QString warningString);
};

#endif // TCPDISPLAY_H

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

#ifndef COMMANDPARSER_H
#define COMMANDPARSER_H

#include <QObject>
#include <QDialog>
#include "signalsources.h"
#include "impedancereader.h"
#include "controllerinterface.h"

class ControlWindow;

class CommandParser : public QObject
{
    Q_OBJECT
public:
    explicit CommandParser(SystemState *state_, ControllerInterface *controllerInterface_, QObject *parent = nullptr);
    ControlWindow *controlWindow;

signals:
    void updateGUIFromState();
    void TCPReturnSignal(QString result);
    void TCPErrorSignal(QString error);
    void TCPWarningSignal(QString warning);
    void sendLiveNote(QString note);
    void stimTriggerOn(QString keyName);
    void stimTriggerOff(QString keyName);
    void stimTriggerPulse(QString keyName);
    void connectTCPWaveformDataOutput();
    void connectTCPSpikeDataOutput();
    void disconnectTCPWaveformDataOutput();
    void disconnectTCPSpikeDataOutput();

public slots:
    void setCommandSlot(QString parameter, QString value);
    void getCommandSlot(QString parameter);
    void executeCommandSlot(QString action);
    void executeCommandWithParameterSlot(QString action, QString parameter);
    void noteCommandSlot(QString note);
    void TCPErrorSlot(QString errorMessage);

private:
    ControllerInterface *controllerInterface;
    SystemState *state;

    void errorTCPReadOnly(const QString& parameter)
        { emit TCPErrorSignal(parameter + " cannot be changed through software after startup."); }
    void errorTCPNonStim(const QString& parameter)
        { emit TCPErrorSignal(parameter + " does not apply to non-stimulation controllers"); }
    void errorTCPRunning(const QString& parameter)
        { emit TCPErrorSignal(parameter + " cannot be set while the board is running"); }

    void returnTCP(const QString& parameter, const QString& value)
        { emit TCPReturnSignal("Return: " + parameter + " " + value); }
    void errorTCP(const QString& parameter, const QString& validValues)
        { emit TCPErrorSignal("Invalid value for " + parameter + " command. Acceptable: " + validValues); }

    // Return a pointer to a Channel given a channel name in 'parameter' - if 'parameter' doesn't fit any channel return a null pointer
    Channel* parseChannelNameDot(const QString& parameter, QString& returnedParameter);

    // Return a pointer to a SignalGroup given a name in 'parameter' - if 'parameter' doesn't fit any signal group return a null pointer
    SignalGroup* parsePortNameDot(const QString& parameter, QString& returnedParameter);

    void setStateItemCommand(StateSingleItem* item, const QString& value);
    void getStateItemCommand(StateSingleItem* item);

    void setStateFilenameItemCommand(StateFilenameItem* item, const QString& pathOrBase, const QString& value);
    void getStateFilenameItemCommand(StateFilenameItem* item, const QString& pathOrBase);

    void setAvailableXPUListCommand(const QString&);
    void getAvailableXPUListCommand();

    void setUsedXPUIndexCommand(const QString& value);
    void getUsedXPUIndexCommand();

    void setRunModeCommand(const QString& value);
    void getRunModeCommand();

    void setTCPWaveformDataOutputHostCommand(const QString& value);
    void getTCPWaveformDataOutputHostCommand();

    void setTCPSpikeDataOutputHostCommand(const QString& value);
    void getTCPSpikeDataOutputHostCommand();

    void setTCPWaveformDataOutputPortCommand(const QString& value);
    void getTCPWaveformDataOutputPortCommand();

    void setTCPSpikeDataOutputPortCommand(const QString& value);
    void getTCPSpikeDataOutputPortCommand();

    void setTCPWaveformDataConnectionStatusCommand(const QString&);
    void getTCPWaveformDataConnectionStatusCommand();

    void setTCPSpikeDataConnectionStatusCommand(const QString&);
    void getTCPSpikeDataConnectionStatusCommand();

    void getCurrentTimestampCommand();
    void getCurrentTimeSecondsCommand();

    void measureImpedanceCommand();
    void saveImpedanceCommand();
    void rescanPortsCommand();
    void connectTCPWaveformDataOutputCommand();
    void connectTCPSpikeDataOutputCommand();
    void disconnectTCPWaveformDataOutputCommand();
    void disconnectTCPSpikeDataOutputCommand();
    void clearAllDataOutputsCommand();

    void uploadAmpSettleSettingsCommand();
    void uploadChargeRecoverySettingsCommand();
    void uploadStimParametersCommand();
    void uploadStimParametersCommand(QString channelName);
    void uploadBandwidthSettingsCommand();

    void setSpikeDetectionThresholdsCommand();

    void loadSettingsFileCommand(QString fileName);
    void saveSettingsFileCommand(QString fileName);
    void loadStimulationSettingsFileCommand(QString fileName);
    void saveStimulationSettingsFileCommand(QString fileName);

    bool isDependencyRelated(QString parameter) const;
    QString validateStimParams(StimParameters *stimParams) const;
};

#endif // COMMANDPARSER_H

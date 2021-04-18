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

#include "commandparser.h"

CommandParser::CommandParser(SystemState* state_, ControllerInterface *controllerInterface_, QObject *parent) :
    QObject(parent),
    controllerInterface(controllerInterface_),
    state(state_)
{
    // These connections allow for interactions with communicators that may live in another thread
    connect(this, SIGNAL(connectTCPWaveformDataOutput()), state->tcpWaveformDataCommunicator, SLOT(attemptNewConnection()));
    connect(this, SIGNAL(connectTCPSpikeDataOutput()), state->tcpSpikeDataCommunicator, SLOT(attemptNewConnection()));

    connect(this, SIGNAL(disconnectTCPWaveformDataOutput()), state->tcpWaveformDataCommunicator, SLOT(returnToDisconnected()));
    connect(this, SIGNAL(disconnectTCPSpikeDataOutput()), state->tcpSpikeDataCommunicator, SLOT(returnToDisconnected()));
}

// Return a pointer to a Channel given a channel name in parameter.  If parameter doesn't fit any channel,
// return a null pointer.
Channel* CommandParser::parseChannelNameDot(const QString& parameter, QString &returnedParameter)
{
    // If parameter doesn't contain a period, just return a null pointer.
    int periodIndex = parameter.indexOf(QChar('.'));
    if (periodIndex == -1) {
        return nullptr;
    } else {
        // If parameter does contain a period, extract the channel name and attempt to identify that channel.
        // If no channel is found by that name, just return a null pointer.
        QStringList stringList = parameter.split('.');
        QString channelName = stringList.at(0).toUpper();
        returnedParameter = stringList.at(1);
        return state->signalSources->channelByName(channelName);
    }
}

// Return a pointer to a SignalGroup given a name in 'parameter' - if 'parameter' doesn't fit any
// signal group, return a null pointer.
SignalGroup* CommandParser::parsePortNameDot(const QString &parameter, QString &returnedParameter)
{
    // If parameter doesn't contain a period at position 1, just return a null pointer.
    int periodIndex = parameter.indexOf(QChar('.'));
    if (periodIndex != 1) {
        return nullptr;
    } else {
        // If parameter does contain a period, extract the port name and attempt to identify that port.
        // If no port is found by that name, just return a null pointer.
        QString portPrefix = parameter.left(1);
        returnedParameter = parameter.right(parameter.length() - 2);
        return state->signalSources->groupByName("Port " + portPrefix);
    }
}

void CommandParser::getStateItemCommand(StateSingleItem* item)
{
    returnTCP(item->getParameterName(), item->getValueString());
}

void CommandParser::setStateItemCommand(StateSingleItem* item, const QString& value)
{
    if (!item->setValue(value)) {
        cerr << "CommandParser::setStateItemCommand: invalid value for " << item->getParameterName().toStdString() << '\n';
        errorTCP(item->getParameterName(), item->getValidValues());
        return;
    }
}

void CommandParser::setStateFilenameItemCommand(StateFilenameItem *item, const QString& pathOrBase, const QString& value)
{
    if (pathOrBase.toLower() == item->getPathParameterName().toLower()) {
        item->setPath(value);
    } else if (pathOrBase.toLower() == item->getBaseFilenameParameterName().toLower()) {
        item->setBaseFilename(value);
    }
}

void CommandParser::getStateFilenameItemCommand(StateFilenameItem* item, const QString& pathOrBase)
{
    if (pathOrBase.toLower() == item->getPathParameterName().toLower()) {
        returnTCP(item->getParameterName() + "." + pathOrBase, item->getPath());
    } else if (pathOrBase.toLower() == item->getBaseFilenameParameterName().toLower()) {
        returnTCP(item->getParameterName() + "." + pathOrBase, item->getBaseFilename());
    }
}

// Check the input QString 'parameter' to try to find a valid 'Get' command. If one is found, it is called.
// If not, an error message is returned.
void CommandParser::getCommandSlot(QString parameter)
{
    QString parameterLower = parameter.toLower();

    // Check in Global-level list for filename items
    QString pathOrBase;
    StateFilenameItem* filenameItem = state->locateStateFilenameItem(state->stateFilenameItems, parameterLower, pathOrBase); // Can be filename.path or filename.basefilename
    if (filenameItem) {
        cout << ">> " << (filenameItem->getParameterName().toLower() + "." + pathOrBase).toStdString() << '\n';
        getStateFilenameItemCommand(filenameItem, pathOrBase);
        return;
    }

    // Parse first for channel names before the first period.
    StateSingleItem* item;
    QString returnedParameter;
    Channel *channel = parseChannelNameDot(parameterLower, returnedParameter);
    if (channel) {
        item = state->locateStateSingleItem(channel->channelItems, returnedParameter);
        if (item) {
            getStateItemCommand(item);
            return;
        }
    }

    // Parse next for port names before the first period.
    SignalGroup *port = parsePortNameDot(parameterLower, returnedParameter);
    if (port) {
        item = state->locateStateSingleItem(port->portItems, returnedParameter);
        if (item) {
            getStateItemCommand(item);
            return;
        }
    }

    // Try this parameter at the Global level
    item = state->locateStateSingleItem(state->globalItems, parameterLower);
    if (item) {
        cout << ">> " << item->getParameterName().toLower().toStdString() << '\n';
        getStateItemCommand(item);
        return;
    }

    // All of these variables are unique in that there's currently no StateItem that accurately represents the variable, so treat them individually.
    if (parameterLower == "availablexpulist")
        getAvailableXPUListCommand();
    else if (parameterLower == "usedxpuindex")
        getUsedXPUIndexCommand();
    else if (parameterLower == "runmode")
        getRunModeCommand();
    else if (parameterLower == "tcpwaveformdataoutputhost")
        getTCPWaveformDataOutputHostCommand();
    else if (parameterLower == "tcpspikedataoutputhost")
        getTCPSpikeDataOutputHostCommand();
    else if (parameterLower == "tcpwaveformdataoutputport")
        getTCPWaveformDataOutputPortCommand();
    else if (parameterLower == "tcpspikedataoutputport")
        getTCPSpikeDataOutputPortCommand();
    else if (parameterLower == "tcpwaveformdataoutputconnectionstatus")
        getTCPWaveformDataConnectionStatusCommand();
    else if (parameterLower == "tcpspikedataoutputconnectionstatus")
        getTCPSpikeDataConnectionStatusCommand();

    // If parameter doesn't match an acceptable command, return an error.
   else emit TCPErrorSignal("Unrecognized parameter");
}

// Check the input QString 'parameter' to try to find a valid 'Set' command. If one is found, it is called
// using the input QString 'value'. If not, an error message is returned.
void CommandParser::setCommandSlot(QString parameter, QString value)
{
    QString parameterLower = parameter.toLower();
    QString valueLower = value.toLower();

    // Check in Global-level list for filename items
    QString pathOrBase;
    StateFilenameItem* filenameItem = state->locateStateFilenameItem(state->stateFilenameItems, parameterLower, pathOrBase); // Can be filename.path or filename.basefilename
    if (filenameItem) {
        if (filenameItem->isRestricted()) {
            emit TCPErrorSignal(filenameItem->getRestrictErrorMessage());
            return;
        }
        setStateFilenameItemCommand(filenameItem, pathOrBase, value);
        return;
    }

    StateSingleItem* item;
    QString returnedParameter;

    // Parse first for channel names before the first period.
    Channel* channel = parseChannelNameDot(parameterLower, returnedParameter);
    if (channel) {
        item = state->locateStateSingleItem(channel->channelItems, returnedParameter);
        if (item) {
            if (item->isRestricted()) {
                emit TCPErrorSignal(item->getRestrictErrorMessage());
                return;
            }
            setStateItemCommand(item, valueLower);
            return;
        }
    }

    // Parse next for port names before the first period.
    SignalGroup *port = parsePortNameDot(parameterLower, returnedParameter);
    if (port) {
        item = state->locateStateSingleItem(port->portItems, returnedParameter);
        if (item) {
            if (item->isRestricted()) {
                emit TCPErrorSignal(item->getRestrictErrorMessage());
                return;
            }
            setStateItemCommand(item, valueLower);
            return;
        }
    }


    // Try this parameter at the Global level
    item = state->locateStateSingleItem(state->globalItems, parameterLower);
    if (item) {
        if (item->isRestricted()) {
            emit TCPErrorSignal(item->getRestrictErrorMessage());
            return;
        }
        setStateItemCommand(item, valueLower);
        return;
    }

    // All of these variables are unique in that there's currently no StateItem that accurately represents the variable, so treat them individually.
    if (parameterLower == "availablexpulist")
        setAvailableXPUListCommand(valueLower);
    else if (parameterLower == "usedxpuindex")
        setUsedXPUIndexCommand(valueLower);
    else if (parameterLower == "runmode")
        setRunModeCommand(valueLower);
    else if (parameterLower == "tcpwaveformdataoutputhost")
        setTCPWaveformDataOutputHostCommand(valueLower);
    else if (parameterLower == "tcpspikedataoutputhost")
        setTCPSpikeDataOutputHostCommand(valueLower);
    else if (parameterLower == "tcpwaveformdataoutputport")
        setTCPWaveformDataOutputPortCommand(valueLower);
    else if (parameterLower == "tcpspikedataoutputport")
        setTCPSpikeDataOutputPortCommand(valueLower);
    else if (parameterLower == "tcpwaveformdataoutputconnectionstatus")
        setTCPWaveformDataConnectionStatusCommand(valueLower);
    else if (parameterLower == "tcpspikedataoutputconnectionstatus")
        setTCPSpikeDataConnectionStatusCommand(valueLower);
    // If parameter doesn't match an acceptable command, return an error.
    else emit TCPErrorSignal("Unrecognized parameter");
}

void CommandParser::executeCommandSlot(QString action)
{
    QString actionLower = action.toLower();

    if (actionLower == "measureimpedance") {
        if (!state->running) {
            measureImpedanceCommand();
        } else {
            emit TCPErrorSignal("MeasureImpedance cannot be executed while the board is running");
        }
    } else if (actionLower == "saveimpedance") {
        saveImpedanceCommand();
    } else if (actionLower == "rescanports") {
        if (!state->running) {
            rescanPortsCommand();
        } else {
            emit TCPErrorSignal("RescanPorts cannot be executed while the board is running");
        }
    } else if (actionLower == "connecttcpwaveformdataoutput") {
        connectTCPWaveformDataOutputCommand();
    } else if (actionLower == "connecttcpspikedataoutput") {
        connectTCPSpikeDataOutputCommand();
    } else if (actionLower == "disconnecttcpwaveformdataoutput") {
        disconnectTCPWaveformDataOutputCommand();
    } else if (actionLower == "disconnecttcpspikedataoutput") {
        disconnectTCPSpikeDataOutputCommand();
    } else if (actionLower == "clearalldataoutputs") {
        clearAllDataOutputsCommand();
    } else if (actionLower == "uploadampsettlesettings") {
        if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
            if (!state->running) {
                uploadAmpSettleSettingsCommand();
            } else {
                emit TCPErrorSignal("UploadAmpSettleSettings cannot be executed while the board is running");
            }
        }
    } else if (actionLower == "uploadchargerecoverysettings") {
        if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
            if (!state->running) {
                uploadChargeRecoverySettingsCommand();
            } else {
                emit TCPErrorSignal("UploadChargeRecoverySettings cannot be executed while the board is running");
            }
        }
    } else if (actionLower == "uploadstimparameters") {
        if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
            if (!state->running) {
                uploadStimParametersCommand();
            } else {
                emit TCPErrorSignal("UploadStimParameters cannot be executed while the board is running");
            }
        }
    } else if (actionLower == "uploadbandwidthsettings") {
        if (!state->running) {
            uploadBandwidthSettingsCommand();
        } else {
            emit TCPErrorSignal("UploadBandwidthSettings cannot be executed while the board is running");
        }
    } else if (actionLower == "setspikedetectionthresholds") {
        if (!state->running) {
            setSpikeDetectionThresholdsCommand();
        } else {
            emit TCPErrorSignal("SetSpikeDetectionThresholds cannot be executed while the board is running");
        }
    }

    else {
        emit TCPErrorSignal("Unrecognized action");
    }
}

void CommandParser::executeCommandWithParameterSlot(QString action, QString parameter)
{
    QString actionLower = action.toLower();
    QString parameterLower = parameter.toLower();

    if (actionLower == "manualstimtriggeron") {
        controllerInterface->manualStimTriggerOn(parameterLower);
    } else if (actionLower == "manualstimtriggeroff") {
        controllerInterface->manualStimTriggerOff(parameterLower);
    } else if (actionLower == "manualstimtriggerpulse") {
        controllerInterface->manualStimTriggerPulse(parameterLower);
    } else if (actionLower == "uploadstimparameters") {
        if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
            uploadStimParametersCommand(parameterLower);
        }
    }

    else {
        emit TCPErrorSignal("Unrecognized action");
    }
}

void CommandParser::noteCommandSlot(QString note)
{
    if (!state->recording) {
        emit TCPErrorSignal("LiveNotes cannot be added unless the board is recording");
        return;
    }
    emit sEndOfLineiveNote(note);
}

void CommandParser::TCPErrorSlot(QString errorMessage)
{
    emit TCPErrorSignal(errorMessage);
}

void CommandParser::setAvailableXPUListCommand(const QString& /* value */)
{
    emit TCPErrorSignal("Available GPUs cannot be changed through this command: This only reports the currently connected and available hardware");
}

void CommandParser::getAvailableXPUListCommand()
{
    QString XPUList;
    XPUList.append(QString::number(0) + ":" + state->cpuInfo.name + "...");
    for (int gpu = 0; gpu < state->gpuList.size(); gpu++) {
        XPUList.append(QString::number(gpu + 1) + ":" + state->gpuList[gpu].name + "...");
    }
    emit TCPReturnSignal("Return: AvailableXPUListCommand " + XPUList);
}

void CommandParser::setUsedXPUIndexCommand(const QString& value)
{
    if (state->running) {
        emit TCPErrorSignal("UsedXPUIndex cannot be set while board is running");
        return;
    }

    if (value.toInt() < 0 || value.toInt() > state->gpuList.size()) {
        emit TCPErrorSignal("Invalid value for UsedXPUIndex command");
        return;
    }
    state->cpuInfo.used = false;
    for (int gpu = 0; gpu < state->gpuList.size(); gpu++) {
        state->gpuList[gpu].used = false;
    }
    if (value.toInt() == 0)
        state->cpuInfo.used = true;
    else
        state->gpuList[value.toInt() - 1].used = true;
    state->forceUpdate();
}

void CommandParser::getUsedXPUIndexCommand()
{
    int usedXPUIndex = state->usedXPUIndex();
    if (usedXPUIndex != -1)
        emit TCPReturnSignal("Return: UsedXPUIndex " + QString::number(usedXPUIndex));
    else
        emit TCPReturnSignal("Return: UsedXPUIndex not found");
}

void CommandParser::setRunModeCommand(const QString& value)
{
    if (value == "run") {
        if (state->running) {
            emit TCPErrorSignal("Board must be stopped in order to start running");
            return;
        }
        state->running = true;
        state->sweeping = false;
        emit updateGUIFromState();
        state->forceUpdate();
        controllerInterface->runController();
    } else if (value == "record") {
        if (state->running) {
            emit TCPErrorSignal("Board must be stopped in order to start recording");
            return;
        }

        // Check to make sure that both BaseFilename and Path are valid
        if (!state->filename->isValid()) {
            emit TCPErrorSignal("Filename.BaseFilename and Filename.Path must both be specified before recording can occur");
            return;
        }

        state->recording = true;
        state->triggerSet = false;
        state->triggered = false;
        state->running = true;
        state->sweeping = false;
        emit updateGUIFromState();
        state->forceUpdate();
        controllerInterface->runController();
    } else if (value == "trigger") {
        if (state->running) {
            emit TCPErrorSignal("Board must be stopped in order to start trigger");
            return;
        }

        // Check to make sure that both BaseFilename and Path are valid
        if (!state->filename->isValid()) {
            emit TCPErrorSignal("Filename.BaseFilename and Filename.Path must both be specified before triggered recording can occur");
            return;
        }

        state->recording = false;
        state->triggerSet = true;
        state->triggered = false;
        state->running = true;
        state->sweeping = false;
        emit updateGUIFromState();
        state->forceUpdate();
        controllerInterface->runController();
    } else if (value == "stop") {
        if (!state->running) {
            emit TCPErrorSignal("Board must be running in order to stop");
            return;
        }
        state->recording = false;
        state->triggerSet = false;
        state->triggered = false;
        state->running = false;
        state->sweeping = false;
        emit updateGUIFromState();
        state->forceUpdate();
    } else
        emit TCPErrorSignal("Invalid value for SetRunMode command");
    return;
}

void CommandParser::getRunModeCommand()
{
    if (state->recording)
        emit TCPReturnSignal("Return: RunMode Record");
    else if (state->triggerSet)
        emit TCPReturnSignal("Return: RunMode Trigger");
    else if (state->running)
        emit TCPReturnSignal("Return: RunMode Run");
    else
        emit TCPReturnSignal("Return: RunMode Stop");
}

void CommandParser::setTCPWaveformDataOutputHostCommand(const QString &value)
{
    if (state->running) {
        emit TCPErrorSignal("TCPWaveformDataOutputHost cannot be set while controller is running.");
        return;
    }
    state->tcpWaveformDataCommunicator->address = value;
}

void CommandParser::getTCPWaveformDataOutputHostCommand()
{
    if (!state->tcpWaveformDataCommunicator->address.isEmpty())
        emit TCPReturnSignal("Return: TCPWaveformDataOutputHost " + state->tcpWaveformDataCommunicator->address);
    else
        emit TCPReturnSignal("Return: Empty TCPWaveformDataOutputHost");
}


void CommandParser::setTCPSpikeDataOutputHostCommand(const QString &value)
{
    if (state->running) {
        emit TCPErrorSignal("TCPSpikeDataOutputHost cannot be set while controller is running.");
        return;
    }
    state->tcpSpikeDataCommunicator->address = value;
}

void CommandParser::getTCPSpikeDataOutputHostCommand()
{
    if (!state->tcpSpikeDataCommunicator->address.isEmpty())
        emit TCPReturnSignal("Return: TCPSpikeDataOutputHost " + state->tcpSpikeDataCommunicator->address);
    else
        emit TCPReturnSignal("Return: Empty TCPSpikeDataOutputHost");
}

void CommandParser::setTCPWaveformDataOutputPortCommand(const QString &value)
{
    if (state->running) {
        emit TCPErrorSignal("TCPWaveformDataOutputPort cannot be set while controller is running.");
        return;
    }
    int port = value.toInt();
    if (port >= 0 && port <= 9999)
        state->tcpWaveformDataCommunicator->port = port;
    else
        emit TCPErrorSignal("Invalid value for TCPWaveformDataOutputPort command");
}

void CommandParser::setTCPSpikeDataOutputPortCommand(const QString &value)
{
    if (state->running) {
        emit TCPErrorSignal("TCPSpikeDataOutputPort cannot be set while controller is running.");
        return;
    }
    int port = value.toInt();
    if (port >= 0 && port <= 9999)
        state->tcpSpikeDataCommunicator->port = port;
    else
        emit TCPErrorSignal("Invalid value for TPCSpikeDataOutputPort command");
}

void CommandParser::getTCPWaveformDataOutputPortCommand()
{
    emit TCPReturnSignal("Return: TCPWaveformDataOutputPort " + QString::number(state->tcpWaveformDataCommunicator->port));
}

void CommandParser::getTCPSpikeDataOutputPortCommand()
{
    emit TCPReturnSignal("Return: TCPSpikeDataOutputPort " + QString::number(state->tcpSpikeDataCommunicator->port));
}

void CommandParser::setTCPWaveformDataConnectionStatusCommand(const QString & /* value */)
{
    emit TCPErrorSignal("Connection status cannot be changed through this command. Execute ConnectTCPWaveformDataOutput or DisconnectTCPWaveformDataOutput");
}

void CommandParser::setTCPSpikeDataConnectionStatusCommand(const QString & /* value */)
{
    emit TCPErrorSignal("Connection status cannot be changed through this command. Execute ConnectTCPSpikeDataOutput or DisconnectTCPSpikeDataOutput");
}

void CommandParser::getTCPWaveformDataConnectionStatusCommand()
{
    if (state->tcpWaveformDataCommunicator->status == TCPCommunicator::Connected)
        emit TCPReturnSignal("Return: TCPWaveformDataOutputConnectionStatus Connected");
    else if (state->tcpWaveformDataCommunicator->status == TCPCommunicator::Pending)
        emit TCPReturnSignal("Return: TCPWaveformDataOutputConnectionStatus Pending");
    else if (state->tcpWaveformDataCommunicator->status == TCPCommunicator::Disconnected)
        emit TCPReturnSignal("Return: TCPWaveformDataOutputConnectionStatus Disconnected");
}

void CommandParser::getTCPSpikeDataConnectionStatusCommand()
{
    if (state->tcpSpikeDataCommunicator->status == TCPCommunicator::Connected)
        emit TCPReturnSignal("Return: TCPSpikeDataOutputConnectionStatus Connected");
    else if (state->tcpSpikeDataCommunicator->status == TCPCommunicator::Pending)
        emit TCPReturnSignal("Return: TCPSpikeDataOutputConnectionStatus Pending");
    else if (state->tcpSpikeDataCommunicator->status == TCPCommunicator::Disconnected)
        emit TCPReturnSignal("Return: TCPSpikeDataOutputConnectionStatus Disconnected");
}

void CommandParser::measureImpedanceCommand()
{
    controllerInterface->measureImpedances();
}

void CommandParser::saveImpedanceCommand()
{
    controllerInterface->saveImpedances();
}

void CommandParser::rescanPortsCommand()
{
    state->signalSources->undoManager->clearUndoStack();
    controllerInterface->rescanPorts(true);
}

void CommandParser::connectTCPWaveformDataOutputCommand()
{
    // Since tcpWaveformDataCommunicator might be in TCPDataOutputThread, instead of calling the slot directly, emit a signal that calls the slot
    emit connectTCPWaveformDataOutput();
}

void CommandParser::connectTCPSpikeDataOutputCommand()
{
    // Since tcpSpikeDataCommunicator might be in TCPDataOutputThread, instead of calling the slot directly, emit a signal that calls the slot
    emit connectTCPSpikeDataOutput();
}

void CommandParser::disconnectTCPWaveformDataOutputCommand()
{
    // Since tcpWaveformDataCommunicator might be in TCPDataOutputThread, instead of calling the slot directly, emit a signal that calls the slot
    emit disconnectTCPWaveformDataOutput();
}

void CommandParser::disconnectTCPSpikeDataOutputCommand()
{
    // Since tcpSpikeDataCommunicator might be in TCPDataOutputThread, instead of calling the slot directly, emit a signal that calls the slot
    emit disconnectTCPSpikeDataOutput();
}

void CommandParser::clearAllDataOutputsCommand()
{
    state->signalSources->clearTCPDataOutput();
}

void CommandParser::uploadAmpSettleSettingsCommand()
{
    controllerInterface->uploadAmpSettleSettings();
}

void CommandParser::uploadChargeRecoverySettingsCommand()
{
    controllerInterface->uploadChargeRecoverySettings();
}

void CommandParser::uploadBandwidthSettingsCommand()
{
    controllerInterface->uploadBandwidthSettings();
}

void CommandParser::uploadStimParametersCommand()
{
    controllerInterface->uploadStimParameters();
}

void CommandParser::uploadStimParametersCommand(QString channelName)
{
    Channel *channel = state->signalSources->channelByName(channelName.toUpper());
    if (channel)
        controllerInterface->uploadStimParameters(channel);
}

void CommandParser::setSpikeDetectionThresholdsCommand()
{
    controllerInterface->setAllSpikeDetectionThresholds();
}

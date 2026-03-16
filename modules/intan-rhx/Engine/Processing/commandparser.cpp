//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.5.0
//
//  Copyright (c) 2020-2026 Intan Technologies
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
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <https://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

#include "commandparser.h"
#include "controlwindow.h"

CommandParser::CommandParser(SystemState* state_, ControllerInterface *controllerInterface_, QObject *parent) :
    QObject(parent),
    controlWindow(nullptr),
    controllerInterface(controllerInterface_),
    state(state_)
{
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
        std::cerr << "CommandParser::setStateItemCommand: invalid value for " << item->getParameterName().toStdString() << '\n';
        errorTCP(item->getParameterName(), item->getValidValues());
        return;
    }
}

void CommandParser::setStateFilenameItemCommand(StateFilenameItem *item, const QString& pathOrBaseOrTimestamp, const QString& value)
{
    if (pathOrBaseOrTimestamp.toLower() == item->getPathParameterName().toLower()) {
        item->setPath(value);
    } else if (pathOrBaseOrTimestamp.toLower() == item->getBaseFilenameParameterName().toLower()) {
        item->setBaseFilename(value);
    } else if (pathOrBaseOrTimestamp.toLower() == item->getTimestampParameterName().toLower()) {
        std::cerr << "CommandParser::setStateItemCommand: invalid value for " << item->getParameterName().toStdString() << '\n';
        errorTCP(item->getParameterName(), item->getValidValues());
    }
}

void CommandParser::getStateFilenameItemCommand(StateFilenameItem* item, const QString& pathOrBaseOrTimestamp)
{
    if (pathOrBaseOrTimestamp.toLower() == item->getPathParameterName().toLower()) {
        returnTCP(item->getParameterName() + "." + pathOrBaseOrTimestamp, item->getPath());
    } else if (pathOrBaseOrTimestamp.toLower() == item->getBaseFilenameParameterName().toLower()) {
        returnTCP(item->getParameterName() + "." + pathOrBaseOrTimestamp, item->getBaseFilename());
    } else if (pathOrBaseOrTimestamp.toLower() == item->getTimestampParameterName().toLower()) {
        returnTCP(item->getParameterName() + "." + pathOrBaseOrTimestamp, item->getTimestamp());
    }
}

void CommandParser::setStateTCPCommunicatorItemCommand(StateTCPCommunicatorItem *item, const QString &hostOrPortOrStatus, const QString &value)
{
    if (hostOrPortOrStatus.toLower() == item->getHostParameterName().toLower()) {
        if (item->communicator->status != Disconnected) {
            const QString warning = "Warning: Changes to host will not take effect until socket reconnects; disconnect then connect to switch to new host.";
            emit TCPWarningSignal(warning);
        }
        item->setHost(value);
    } else if (hostOrPortOrStatus.toLower() == item->getPortParameterName().toLower()) {
        if (item->communicator->status != Disconnected) {
            const QString warning = "Warning: Changes to port will not take effect until socket reconnects; disconnect then connect to switch to new port.";
            emit TCPWarningSignal(warning);
        }
        item->setPort(value);
    } else if (hostOrPortOrStatus.toLower() == item->getStatusParameterName().toLower()) {
        item->setStatus(value);
    }
}

void CommandParser::getStateTCPCommunicatorItemCommand(StateTCPCommunicatorItem *item, const QString &hostOrPortOrStatus)
{
    if (hostOrPortOrStatus.toLower() == item->getHostParameterName().toLower()) {
        returnTCP(item->getParameterName() + "." + hostOrPortOrStatus, item->getHost());
    } else if (hostOrPortOrStatus.toLower() == item->getPortParameterName().toLower()) {
        returnTCP(item->getParameterName() + "." + hostOrPortOrStatus, item->getPort());
    } else if (hostOrPortOrStatus.toLower() == item->getStatusParameterName().toLower()) {
        returnTCP(item->getParameterName() + "." + hostOrPortOrStatus, item->getStatus());
    }
}

// Check the input QString 'parameter' to try to find a valid 'Get' command. If one is found, it is called.
// If not, an error message is returned.
void CommandParser::getCommandSlot(QString parameter)
{
    QString parameterLower = parameter.toLower();

    // Check in Global-level list for filename items
    QString pathOrBaseOrTimestamp;
    StateFilenameItem* filenameItem = state->locateStateFilenameItem(state->stateFilenameItems, parameterLower, pathOrBaseOrTimestamp); // Can be filename.path, filename.basefilename or filename.activefiletimestamp
    if (filenameItem) {
        std::cout << ">> " << (filenameItem->getParameterName().toLower() + "." + pathOrBaseOrTimestamp).toStdString() << '\n';
        getStateFilenameItemCommand(filenameItem, pathOrBaseOrTimestamp);
        return;
    }

    // Check in Global-level list for tcpcommunicator items
    QString hostOrPortOrStatus;
    StateTCPCommunicatorItem* tcpCommunicatorItem = state->locateStateTCPCommunicatorItem(state->stateTCPCommunicatorItems, parameterLower, hostOrPortOrStatus); // Can be <tcpsocket>.host, <tcpsocket>.port, or <tcpsocket>.status
    if (tcpCommunicatorItem) {
        std::cout << ">> " << (tcpCommunicatorItem->getParameterName().toLower() + "." + hostOrPortOrStatus).toStdString() << '\n';
        getStateTCPCommunicatorItemCommand(tcpCommunicatorItem, hostOrPortOrStatus);
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
        std::cout << ">> " << item->getParameterName().toLower().toStdString() << '\n';
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
    else if (parameterLower == "currenttimestamp")
        getCurrentTimestampCommand();
    else if (parameterLower == "currenttimeseconds")
        getCurrentTimeSecondsCommand();

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
    QString pathOrBaseOrTimestamp;
    StateFilenameItem* filenameItem = state->locateStateFilenameItem(state->stateFilenameItems, parameterLower, pathOrBaseOrTimestamp); // Can be filename.path, filename.basefilename, or filename.activefiletimestamp
    if (filenameItem) {
        if (filenameItem->isRestricted()) {
            emit TCPErrorSignal(filenameItem->getRestrictErrorMessage());
            return;
        }
        setStateFilenameItemCommand(filenameItem, pathOrBaseOrTimestamp, value);
        return;
    }

    // Check in Global-level list for tcpcommunicator items
    QString hostOrPortOrStatus;
    StateTCPCommunicatorItem* tcpCommunicatorItem = state->locateStateTCPCommunicatorItem(state->stateTCPCommunicatorItems, parameterLower, hostOrPortOrStatus); // Can be <tcpsocket>.host, <tcpsocket>.port, or <tcpsocket>.status
    if (tcpCommunicatorItem) {
        if (tcpCommunicatorItem->isRestricted()) {
            emit TCPErrorSignal(tcpCommunicatorItem->getRestrictErrorMessage());
            return;
        }
        setStateTCPCommunicatorItemCommand(tcpCommunicatorItem, hostOrPortOrStatus, value);
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

            // Check if this is a Stim Parameter, and if it is, check validity and potentially emit a TCPWarningSignal
            if (!isDependencyRelated(item->getParameterName())) return;

            QString warningMessage = channel->stimParameters->validate();
            if (warningMessage != "") {
                emit TCPWarningSignal("Warning: " + warningMessage);
            }
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
    } else if (actionLower == "clearalldataoutputs") {
        clearAllDataOutputsCommand();
    } else if (actionLower == "uploadampsettlesettings") {
        if (state->getControllerTypeEnum() == ControllerStimRecord) {
            if (!state->running) {
                uploadAmpSettleSettingsCommand();
            } else {
                emit TCPErrorSignal("UploadAmpSettleSettings cannot be executed while the board is running");
            }
        }
    } else if (actionLower == "uploadchargerecoverysettings") {
        if (state->getControllerTypeEnum() == ControllerStimRecord) {
            if (!state->running) {
                uploadChargeRecoverySettingsCommand();
            } else {
                emit TCPErrorSignal("UploadChargeRecoverySettings cannot be executed while the board is running");
            }
        }
    } else if (actionLower == "uploadstimparameters") {
        if (state->getControllerTypeEnum() == ControllerStimRecord) {
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
        if (state->getControllerTypeEnum() == ControllerStimRecord) {
            uploadStimParametersCommand(parameterLower);
        }
    } else if (actionLower == "loadsettingsfile") {
        if (!state->running) {
            loadSettingsFileCommand(parameterLower);
        } else {
            emit TCPErrorSignal("LoadSettingsFile cannot be executed while the board is running");
        }
    } else if (actionLower == "savesettingsfile") {
        if (!state->running) {
            saveSettingsFileCommand(parameterLower);
        } else {
            emit TCPErrorSignal("SaveSettingsFile cannot be executed while the board is running");
        }
    } else if (actionLower == "loadstimulationsettingsfile") {
        if (!state->running) {
            loadStimulationSettingsFileCommand(parameterLower);
        } else {
            emit TCPErrorSignal("LoadStimulationSettingsFile cannot be executed while the board is running");
        }
    } else if (actionLower == "savestimulationsettingsfile") {
        if (!state->running) {
            saveStimulationSettingsFileCommand(parameterLower);
        } else {
            emit TCPErrorSignal("SaveStimulationSettingsFile cannot be executed while the board is running");
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
    emit sendLiveNote(note);
}

void CommandParser::TCPErrorSlot(QString errorMessage)
{
    emit TCPErrorSignal(errorMessage);
}

void CommandParser::TCPWarningSlot(QString warningMessage)
{
    emit TCPWarningSignal(warningMessage);
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
        controllerInterface->controllerRunStart();
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
        controllerInterface->controllerRunStart();
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
        controllerInterface->controllerRunStart();
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
        controllerInterface->controllerRunFinalize();
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

void CommandParser::getCurrentTimestampCommand()
{
    if (state->running) {
        emit TCPReturnSignal("Return: " + QString::number(state->getLastTimestamp()));
    } else {
        emit TCPReturnSignal("Return: -1");
    }
}

void CommandParser::getCurrentTimeSecondsCommand()
{
    if (state->running) {
        emit TCPReturnSignal("Return: " + QString::number((double) state->getLastTimestamp() / state->sampleRate->getNumericValue()));
    } else {
        emit TCPReturnSignal("Return: -1");
    }
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

void CommandParser::loadSettingsFileCommand(QString fileName)
{
    controlWindow->updateForLoad();

    QString errorMessage;
    bool loadSuccess = state->loadGlobalSettings(fileName, errorMessage);
    if (!loadSuccess) {
        emit TCPErrorSignal(errorMessage);
    } else if (!errorMessage.isEmpty()) {
        emit TCPErrorSignal(errorMessage);
    }
    controllerInterface->updateChipCommandLists(false); // Update amplifier bandwidth settings
    controlWindow->restoreDisplaySettings();

    if (loadSuccess) {
        QFileInfo fileInfo(fileName);
        QSettings settings;
        settings.beginGroup(ControllerTypeSettingsGroup[(int) state->getControllerTypeEnum()]);
        settings.setValue("settingsDirectory", fileInfo.absolutePath());
        settings.endGroup();
    }

    controlWindow->updateForStop();
}

void CommandParser::saveSettingsFileCommand(QString fileName)
{
    QFileInfo fileInfo(fileName);
    QSettings settings;
    settings.setValue("settingsDirectory", fileInfo.absolutePath());

    // Generate display settings string to record state of multi-column display, scroll bars, pinned waveforms, etc.
    state->displaySettings->setValue(controlWindow->getDisplaySettingsString());

    if (!state->saveGlobalSettings(fileName)) {
        emit TCPErrorSignal("Failure writing XML Global Settings");
    }
}

void CommandParser::loadStimulationSettingsFileCommand(QString fileName)
{    
    controlWindow->updateForLoad();

    QFileInfo fileInfo(fileName);
    QString errorMessage;
    bool loadSuccess = false;
    loadSuccess = controlWindow->stimParametersInterface->loadFile(fileName, errorMessage, false, false, true); // Parse with stimOnly, so StimParameters are all that are loaded
    if (loadSuccess) {
        QSettings settings;
        settings.beginGroup(ControllerTypeSettingsGroup[(int)state->getControllerTypeEnum()]);
        settings.setValue("stimSettingsDirectory", fileInfo.absolutePath());
        settings.endGroup();
        controlWindow->updateForStop();
        return;
    }

    errorMessage = "";
    loadSuccess = controlWindow->stimParametersInterface->loadFile(fileName, errorMessage, true, false, true); // Try parsing as with stimLegacy=true

    if (!loadSuccess) {
        emit TCPErrorSignal("Error: Loading from XML: " + errorMessage);
    } else {
        if (!errorMessage.isEmpty()) {
            emit TCPErrorSignal("Warning: Loading from XML: " + errorMessage);
        }
        QSettings settings;
        settings.beginGroup(ControllerTypeSettingsGroup[(int)state->getControllerTypeEnum()]);
        settings.setValue("stimSettingsDirectory", fileInfo.absolutePath());
        settings.endGroup();
        controlWindow->updateForStop();
        return;
    }
}

void CommandParser::saveStimulationSettingsFileCommand(QString fileName)
{
    QFileInfo fileInfo(fileName);
    QSettings settings;
    settings.beginGroup(ControllerTypeSettingsGroup[(int)state->getControllerTypeEnum()]);
    settings.setValue("stimSettingsDirectory", fileInfo.absolutePath());
    settings.endGroup();

    if (!controlWindow->stimParametersInterface->saveFile(fileName)) {
        emit TCPErrorSignal("Failure writing Stimulation Parameters");
    }
}

bool CommandParser::isDependencyRelated(QString parameter) const
{
    if (parameter == "PostTriggerDelayMicroseconds" ||
            parameter == "PreStimAmpSettleMicroseconds" ||
            parameter == "PostStimAmpSettleMicroseconds" ||
            parameter == "PostStimChargeRecovOffMicroseconds" ||
            parameter == "PostStimChargeRecovOnMicroseconds" ||
            parameter == "RefractoryPeriodMicroseconds" ||
            parameter == "PulseTrainPeriodMicroseconds" ||
            parameter == "FirstPhaseDurationMicroseconds" ||
            parameter == "SecondPhaseDurationMicroseconds" ||
            parameter == "InterphaseDelayMicroseconds" ||
            parameter == "Shape") {
        return true;
    }
    return false;
}

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

#include <QApplication>
#include <QtGlobal>
#include <QElapsedTimer>
#include <iostream>
#include "controlpanel.h"
#include "impedancereader.h"
#include "controllerinterface.h"

using namespace std;

ControllerInterface::ControllerInterface(SystemState* state_, AbstractRHXController* rhxController_, const QString& boardSerialNumber,
                                         DataFileReader* dataFileReader_, QObject* parent) :
    QObject(parent),
    state(state_),
    rhxController(rhxController_),
    dataFileReader(dataFileReader_),
    tcpDataOutputThread(nullptr),
    xpuController(nullptr),
    usbStreamFifo(nullptr),
    usbDataThread(nullptr),
    waveformFifo(nullptr),
    waveformProcessorThread(nullptr),
    display(nullptr),
    controlPanel(nullptr),
    isiDialog(nullptr),
    psthDialog(nullptr),
    spectrogramDialog(nullptr),
    spikeSortingDialog(nullptr),
    audioThread(nullptr),
    saveToDiskThread(nullptr)
{
    state->writeToLog("Entered ControllerInterface ctor");
    connect(state, SIGNAL(stateChanged()), this, SLOT(updateFromState()));
    state->writeToLog("Connected updateFromState");
    openController(boardSerialNumber);
    state->writeToLog("Completed openController");

    const int NumSeconds = 10;  // Size of RAM buffer, in seconds.
    int fifoBufferSize = NumSeconds * rhxController->getSampleRate() * BytesPerWord *
            (RHXDataBlock::dataBlockSizeInWords(state->getControllerTypeEnum(),
                                                RHXController::maxNumDataStreams(state->getControllerTypeEnum())) / RHXDataBlock::samplesPerDataBlock(state->getControllerTypeEnum()));

    int usbBufferSize = MaxNumBlocksToRead * RHXDataBlock::dataBlockSizeInWords(state->getControllerTypeEnum(),
                                                                                    rhxController->maxNumDataStreams());

    state->writeToLog("Calculated buffer sizes");

    double memoryRequired = 0.0;

    usbStreamFifo = new DataStreamFifo(fifoBufferSize, usbBufferSize);
    if (!usbStreamFifo->memoryWasAllocated(memoryRequired)) {
        outOfMemoryError(memoryRequired);
    }

    state->writeToLog("Created DataStreamFifo");

    hardwareFifoPercentFull = 0.0;
    waveformProcessorCpuLoad = 0.0;

    usbDataThread = new USBDataThread(rhxController, usbStreamFifo, this);
    if (!usbDataThread->memoryWasAllocated(memoryRequired)) {
        outOfMemoryError(memoryRequired);
    }

    state->writeToLog("Created USBDataThread");
    usbDataThread->setNumUsbBlocksToRead(RHXDataBlock::blocksFor30Hz(state->getSampleRateEnum()));
    connect(usbDataThread, SIGNAL(finished()), usbDataThread, SLOT(deleteLater()));
    connect(usbDataThread, SIGNAL(hardwareFifoReport(double)), this, SLOT(updateHardwareFifo(double)));
    state->writeToLog("Set num usb blocks to read, and connected signals");

    initializeController();
    state->writeToLog("Completed initializeController()");

    xpuController = new XPUController(state);
    state->writeToLog("Created XPUController");

    rescanPorts();
    state->writeToLog("Completed rescanPorts()");

    rhxController->enableDacHighpassFilter(false);
    rhxController->setDacHighpassFilter(250.0);

    state->writeToLog("About to run diagnostic");
    xpuController->runDiagnostic();
    state->writeToLog("Finished run diagnostic");

    double waveformMemoryInSeconds = 30.0;  // TODO: Eventually increase this to 45 or 60 if there is sufficient RAM?
    double waveformExtraBufferInSeconds = 15.0;
    double sampleRate = state->sampleRate->getNumericValue();
    double samplesPerDataBlock = (double) RHXDataBlock::samplesPerDataBlock(state->getControllerTypeEnum());
    int waveformFifoMemoryDataBlocks = ceil(waveformMemoryInSeconds * sampleRate / samplesPerDataBlock);
    int waveformFifoBufferDataBlocks = ceil((waveformMemoryInSeconds + waveformExtraBufferInSeconds) * sampleRate / samplesPerDataBlock);
    waveformFifo = new WaveformFifo(state->signalSources, waveformFifoBufferDataBlocks, waveformFifoMemoryDataBlocks, 1);
    if (!waveformFifo->memoryWasAllocated(memoryRequired)) {
        outOfMemoryError(memoryRequired);
    }
    state->writeToLog("Created waveformFifo");

    waveformProcessorThread = new WaveformProcessorThread(state, rhxController->getNumEnabledDataStreams(), rhxController->getSampleRate(), usbStreamFifo, waveformFifo, xpuController, this);
    connect(waveformProcessorThread, SIGNAL(finished()), waveformProcessorThread, SLOT(deleteLater()));
    connect(waveformProcessorThread, SIGNAL(cpuLoadPercent(double)), this, SLOT(updateWaveformProcessorCpuLoad(double)));
    state->writeToLog("Created waveformProcessorThread");

    saveToDiskThread = new SaveToDiskThread(waveformFifo, state, this);
    connect(saveToDiskThread, SIGNAL(finished()), saveToDiskThread, SLOT(deleteLater()));
    if (dataFileReader) {
        // Establish connections so that stimulation amplitudes read from playback file can be re-saved.
        connect(dataFileReader, SIGNAL(setPosStimAmplitude(int, int, int)),
                saveToDiskThread, SLOT(setPosStimAmplitude(int, int, int)));
        connect(dataFileReader, SIGNAL(setNegStimAmplitude(int, int, int)),
                saveToDiskThread, SLOT(setNegStimAmplitude(int, int, int)));
    }
    state->writeToLog("Created saveToDiskThread");

    currentSweepPosition = 0;
    audioEnabled = false;
    tcpDataOutputEnabled = false;

    cpuLoadHistory.resize(20, 0.0);
}

ControllerInterface::~ControllerInterface()
{
    if (state->running) {
        state->running = false;
    }

    saveToDiskThread->close();
    saveToDiskThread->wait();
    delete saveToDiskThread;

    waveformProcessorThread->close();
    waveformProcessorThread->wait();
    delete waveformProcessorThread;

    usbDataThread->close();
    usbDataThread->wait();
    delete usbDataThread;

    if (audioThread) {
        audioThread->close();
        audioThread->wait();
        delete audioThread;
    }

    if (tcpDataOutputThread) {
        tcpDataOutputThread->closeExternal();
        tcpDataOutputThread->wait();
        delete tcpDataOutputThread;
    }

    delete usbStreamFifo;
    delete waveformFifo;
    delete xpuController;
}

void ControllerInterface::outOfMemoryError(double memRequiredGB)
{
    QMessageBox::critical(nullptr, tr("Out of Memory Error"), tr("Software was unable to allocate ") +
                          QString::number(memRequiredGB, 'f', 1) +
                          tr(" GB of memory.  Try running with fewer amplifier channels or a lower sample rate, "
                             "or use a computer with more RAM."));
    exit(EXIT_FAILURE);
}

void ControllerInterface::updateFromState()
{
    // Check if audio enabled has changed.
    if (state->audioEnabled->getValue() != audioEnabled)
        toggleAudioThread(state->audioEnabled->getValue());

    if (!tcpDataOutputEnabled && state->running && state->getTCPDataOutputChannels().length() > 0) {
        runTCPDataOutputThread();
    }
}

void ControllerInterface::toggleAudioThread(bool enabled)
{
    if (enabled) {
        audioEnabled = true;
        audioThread = new AudioThread(state, waveformFifo, rhxController->getSampleRate());
        connect(audioThread, SIGNAL(finished()), audioThread, SLOT(deleteLater()));
        connect(audioThread, SIGNAL(newChannel(QString)), this, SLOT(updateCurrentAudioChannel(QString)));

        // This starts the thread running, ideally on its own CPU core.
        audioThread->start();
        audioThread->setPriority(QThread::HighestPriority);

        // This activates the thread so it can do useful activity.
        audioThread->startRunning();
    } else {
        audioEnabled = false;
        if (audioThread) {
            audioThread->close();
            audioThread->wait();
            delete audioThread;
            audioThread = nullptr;
        }
    }
}

void ControllerInterface::updateCurrentAudioChannel(QString name)
{
    currentAudioChannel = name;
    state->forceUpdate();
}

void ControllerInterface::runTCPDataOutputThread()
{
        tcpDataOutputEnabled = true;
        if (!tcpDataOutputThread) {
            tcpDataOutputThread = new TCPDataOutputThread(waveformFifo, rhxController->getSampleRate(), state, this);
        }

        state->tcpWaveformDataCommunicator->moveToThread(tcpDataOutputThread);
        state->tcpSpikeDataCommunicator->moveToThread(tcpDataOutputThread);

        connect(tcpDataOutputThread, SIGNAL(finished()), tcpDataOutputThread, SLOT(deleteLater()));

        // This starts the thread running, ideally on its own CPU core.
        tcpDataOutputThread->start();
        tcpDataOutputThread->setPriority(QThread::HighestPriority);

        // This activates the thread so it can do useful activity.
        tcpDataOutputThread->startRunning();
}

void ControllerInterface::rescanPorts(bool updateDisplay)
{
    int numDataStreams = 0;
    if (rhxController->isPlayback()) {
        rhxController->enableDataStream(0, false);
        for (int stream = 0; stream < dataFileReader->numDataStreams(); ++stream) {
            rhxController->enableDataStream(stream, true);
            ++numDataStreams;
        }
        addPlaybackHeadstageChannels();
    } else {
        vector<int> portIndex, commandStream, numChannelsOnPort;
        numDataStreams = scanPorts(state->chipType, portIndex, commandStream, numChannelsOnPort);
        addAmplifierChannels(state->chipType, portIndex, commandStream, numChannelsOnPort);
        setManualCableDelays();
    }

    state->signalSources->updateChannelMap();
    if (rhxController->isPlayback()) {
        enablePlaybackChannels();  // Enable only the signals that are present in playback file.
    }

    state->signalSources->autoColorAmplifierChannels(32, 1);
    xpuController->updateNumStreams(numDataStreams);

    if (updateDisplay) {
        display->updatePortSelectionBoxes();
    }

    state->headstagePresent->setValue(state->signalSources->numAmplifierChannels() > 0);
    state->updateForChangeHeadstages();

    if (waveformFifo) {
        waveformFifo->updateForRescan();
    }

    if (display) {
        display->updateForRescan();
    }
}

// Returns number of data streams used.
int ControllerInterface::scanPorts(vector<ChipType> &chipType, vector<int> &portIndex, vector<int> &commandStream,
                                    vector<int> &numChannelsOnPort)
{
    // Scan SPI Ports.
    int warningCode = rhxController->findConnectedChips(chipType, portIndex, commandStream, numChannelsOnPort);
    if (warningCode == -1) {
        QMessageBox::warning(nullptr, tr("Capacity of RHD USB Interface Exceeded"),
                             tr("This RHD USB interface board can support only 256 amplifier channels."
                                "<p>More than 256 total amplifier channels are currently connected."
                                "<p>Amplifier chips exceeding this limit will not appear in the GUI."));
    } else if (warningCode == -2) {
        QMessageBox::warning(nullptr, tr("Capacity of RHD USB Interface Exceeded"),
                             tr("This RHD USB interface board can support only 256 amplifier channels."
                                "<p>More than 256 total amplifier channels are currently connected.  (Each RHD2216 "
                                "chip counts as 32 channels.)"
                                "<p>Amplifier chips exceeding this limit will not appear in the GUI."));
    }

    int numDataStreams = 0;
    for (int i = 0; i < (int) portIndex.size(); ++i) {
        if (portIndex[i] != -1) ++numDataStreams;
    }

    // Turn on appropriate LEDs for Ports A-H.
    int ledArray[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    if (rhxController->getType() == ControllerStimRecordUSB2) {
        for (int port = 0; port < 4; port++) {
            if (numChannelsOnPort[port] > 0) ledArray[2 * port] = 1;
        }
    } else if (rhxController->getType() == ControllerRecordUSB3) {
        for (int port = 0; port < 8; port++) {
            if (numChannelsOnPort[port] > 0) ledArray[port] = 1;
        }
    }
    rhxController->setSpiLedDisplay(ledArray);
    return numDataStreams;
}

void ControllerInterface::addAmplifierChannels(const vector<ChipType> &chipType, const vector<int> &portIndex,
                                               const vector<int> &commandStream, const vector<int> &numChannelsOnPort)
{
    state->signalSources->undoManager->clearUndoStack();

    int numDataStreams = (int) chipType.size();

    for (int port = 0; port < state->numSPIPorts; port++) {
        SignalGroup* group = state->signalSources->portGroupByIndex(port);
        if (numChannelsOnPort[port] == 0) {
            group->removeAllChannels();
            group->setEnabled(false);
        } else if (group->numChannels(AmplifierSignal) != numChannelsOnPort[port]) {
            // If number of channels on port has changed...
            group->removeAllChannels();  // ...clear existing channels...
            group->setEnabled(true);
            // ...and create new ones.
            int channel = 0;
            // Create amplifier channels for each chip.
            for (int stream = 0; stream < numDataStreams; stream++) {
                if (portIndex[stream] == port) {
                    if (chipType[stream] == RHD2216Chip ||
                        chipType[stream] == RHS2116Chip) {
                        for (int i = 0; i < 16; i++) {
                            group->addAmplifierChannel(channel, stream, commandStream[stream], i);
                            channel++;
                        }
                    } else if (chipType[stream] == RHD2132Chip ||
                               chipType[stream] == RHD2164Chip ||
                               chipType[stream] == RHD2164MISOBChip) {
                        for (int i = 0; i < 32; i++) {
                            group->addAmplifierChannel(channel, stream, commandStream[stream], i);
                            channel++;
                        }
                    }
                }
            }
            //  Now create auxiliary input channels and supply voltage channels for each chip.
            int auxName = 1;
            int vddName = 1;
            for (int stream = 0; stream < numDataStreams; stream++) {
                if (portIndex[stream] == port) {
                    if (chipType[stream] == RHD2216Chip ||
                        chipType[stream] == RHD2132Chip ||
                        chipType[stream] == RHD2164Chip) {
                        group->addAuxInputChannel(channel++, stream, 0, auxName++);
                        group->addAuxInputChannel(channel++, stream, 1, auxName++);
                        group->addAuxInputChannel(channel++, stream, 2, auxName++);
                        group->addSupplyVoltageChannel(channel++, stream, vddName++);
                    }
                }
            }
        } else {    // If number of channels on port has not changed, don't create new channels (since this
                    // would clear all user-defined channel names.  But we must update the data stream indices
                    // on the port.
            int channel = 0;
            // Update stream indices for amplifier channels.
            for (int stream = 0; stream < numDataStreams; stream++) {
                if (portIndex[stream] == port) {
                    if (chipType[stream] == RHD2216Chip ||
                        chipType[stream] == RHS2116Chip) {
                        for (int i = channel; i < channel + 16; i++) {
                            Channel* channel = group->channelByIndex(i);
                            channel->setBoardStream(stream);
                            channel->setCommandStream(commandStream[stream]);
                        }
                        channel += 16;
                    } else if (chipType[stream] == RHD2132Chip ||
                               chipType[stream] == RHD2164Chip ||
                               chipType[stream] == RHD2164MISOBChip) {
                        for (int i = channel; i < channel + 32; i++) {
                            Channel* channel = group->channelByIndex(i);
                            channel->setBoardStream(stream);
                            channel->setCommandStream(commandStream[stream]);
                        }
                        channel += 32;
                    }
                }
            }
            // Update stream indices for auxiliary channels and supply voltage channels.
            for (int stream = 0; stream < numDataStreams; ++stream) {
                if (portIndex[stream] == port) {
                    if (chipType[stream] == RHD2216Chip ||
                        chipType[stream] == RHD2132Chip ||
                        chipType[stream] == RHD2164Chip) {
                        group->channelByIndex(channel++)->setBoardStream(stream);
                        group->channelByIndex(channel++)->setBoardStream(stream);
                        group->channelByIndex(channel++)->setBoardStream(stream);
                        group->channelByIndex(channel++)->setBoardStream(stream);
                   }
                }
            }
        }
    }
}

void ControllerInterface::enablePlaybackChannels()
{
    if (!dataFileReader) return;
    const IntanHeaderInfo* fileInfo = dataFileReader->getHeaderInfo();

    for (int i = 0; i < state->signalSources->numGroups(); ++i) {
        SignalGroup* group = state->signalSources->groupByIndex(i);
        QString groupPrefix = group->getPrefix();
        int index = fileInfo->groupIndex(groupPrefix);
        if (index == -1) {
            cerr << "ControllerInterface::enablePlaybackChannels: Could not find group with prefix " <<
                    groupPrefix.toStdString() << '\n';
        } else {
            const HeaderFileGroup& fileGroup = fileInfo->groups[index];
            for (int i = 0; i < fileGroup.numChannels(); ++i) {
                const HeaderFileChannel& fileChannel = fileGroup.channels[i];
                Channel* channel = state->signalSources->channelByName(fileChannel.nativeChannelName);
                if (channel) {
                    channel->setEnabled(fileChannel.enabled);
                } else {
                    cerr << "ControllerInterface::enablePlaybackChannels: Could not find channel " <<
                            fileChannel.nativeChannelName.toStdString() << '\n';
                }
            }
        }
    }
}

void ControllerInterface::addPlaybackHeadstageChannels()
{
    if (!dataFileReader) return;
    const IntanHeaderInfo* fileInfo = dataFileReader->getHeaderInfo();

    // Set bandwidth parameters from playback file.
    state->holdUpdate();
    state->desiredDspCutoffFreq->setValueWithLimits(fileInfo->desiredDspCutoffFreq);
    state->desiredLowerBandwidth->setValueWithLimits(fileInfo->desiredLowerBandwidth);
    state->desiredUpperBandwidth->setValueWithLimits(fileInfo->desiredUpperBandwidth);
    state->dspEnabled->setValue(fileInfo->dspEnabled);
    state->actualDspCutoffFreq->setValueWithLimits(fileInfo->actualDspCutoffFreq);
    state->actualLowerBandwidth->setValueWithLimits(fileInfo->actualLowerBandwidth);
    state->actualUpperBandwidth->setValueWithLimits(fileInfo->actualUpperBandwidth);
    state->releaseUpdate();

    for (int port = 0; port < fileInfo->numSPIPorts; ++port) {
        SignalGroup* group = state->signalSources->portGroupByIndex(port);
        QString portPrefix = QString(QChar('A' + port));
        int index = fileInfo->groupIndex(portPrefix);
        if (index == -1) {
            group->removeAllChannels();
            group->setEnabled(false);
        } else {
            const HeaderFileGroup& fileGroup = fileInfo->groups[index];
            for (int i = 0; i < fileGroup.numChannels(); ++i) {
                const HeaderFileChannel& fileChannel = fileGroup.channels[i];
                if (fileChannel.signalType == AmplifierSignal) {
                    group->addAmplifierChannel(fileChannel.nativeOrder, fileChannel.boardStream,
                                               fileChannel.commandStream, fileChannel.chipChannel);
//                    cout << "Playback configuration: Adding " << portPrefix.toStdString() << "-" <<
//                            QString("%1").arg(fileChannel.channelNumber(), 3, 10, QChar('0')).toStdString() << EndOfLine;
                } else if (fileChannel.signalType == AuxInputSignal) {
                    group->addAuxInputChannel(fileChannel.nativeOrder, fileChannel.boardStream,
                                              fileChannel.chipChannel, fileChannel.endingNumber(1));
//                    cout << "Playback configuration: Adding " << portPrefix.toStdString() << "-AUX" <<
//                            fileChannel.endingNumber(1) << EndOfLine;
                } else if (fileChannel.signalType == SupplyVoltageSignal) {
                    group->addSupplyVoltageChannel(fileChannel.nativeOrder, fileChannel.boardStream,
                                                   fileChannel.endingNumber(1));
//                    cout << "Playback configuration: Adding " << portPrefix.toStdString() << "-VDD" <<
//                            fileChannel.endingNumber(1) << EndOfLine;
                }
            }
        }
    }
}

void ControllerInterface::setManualCableDelays()
{
    SignalSources* signalSources = state->signalSources;
    for (int port = 0; port < signalSources->numPortGroups(); ++port) {
        if (signalSources->portGroupByIndex(port)->manualDelayEnabled->getValue()) {
            rhxController->setCableDelay((BoardPort)port, signalSources->portGroupByIndex(port)->manualDelay->getValue());
        }
    }
}

void ControllerInterface::openController(const QString& boardSerialNumber)
{
    rhxController->open(boardSerialNumber.toStdString());

    // Upload FPGA bit file.
    QString bitfilename;
    if (state->getControllerTypeEnum() == ControllerRecordUSB2) {
        bitfilename = ConfigFileRHDBoard;
    } else if (state->getControllerTypeEnum() == ControllerRecordUSB3) {
        bitfilename = ConfigFileRHDController;
    } else {
        bitfilename = ConfigFileRHSController;
    }
    if (!rhxController->uploadFPGABitfile(QString(QCoreApplication::applicationDirPath() + "/" + bitfilename).toStdString())) {
        QMessageBox::critical(nullptr, tr("Configuration File Error: Software Aborting"),
                              tr("Cannot upload configuration file: ") + bitfilename +
                              tr(".  Make sure file is in the same directory as the executable file."));
        exit(EXIT_FAILURE);
    }

    rhxController->resetBoard();
}

// Initialize a controller connected to a USB port.
void ControllerInterface::initializeController()
{
    rhxController->initialize();

    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        rhxController->enableDcAmpConvert(true);
        rhxController->setExtraStates(0);
    }
    rhxController->setSampleRate(state->getSampleRateEnum());

    // Upload all SPI command sequences.
    updateChipCommandLists(true);

    if (state->getControllerTypeEnum() != ControllerStimRecordUSB2) {
        // Select RAM Bank 0 for AuxCmd3 initially, so the ADC is calibrated.
        rhxController->selectAuxCommandBankAllPorts(RHXController::AuxCmd3, 0);
    }

    // Since our longest command sequence is N commands, we run the SPI interface for N samples.
    rhxController->setMaxTimeStep(RHXDataBlock::samplesPerDataBlock(state->getControllerTypeEnum()));
    rhxController->setContinuousRunMode(false);

    // Start SPI interface.
    rhxController->run();

    // Wait for the N-sample run to complete.
    while (rhxController->isRunning()) {
        qApp->processEvents();
    }

    // Read the resulting single data block from the USB interface.
    RHXDataBlock dataBlock(state->getControllerTypeEnum(), rhxController->getNumEnabledDataStreams());
    if (!state->synthetic->getValue() && !state->playback->getValue()) rhxController->readDataBlock(&dataBlock);

    if (state->getControllerTypeEnum() != ControllerStimRecordUSB2) {
        // Now that ADC calibration has been performed, we switch to the command sequence that does not execute
        // ADC calibration.
        rhxController->selectAuxCommandBankAllPorts(RHXController::AuxCmd3, state->manualFastSettleEnabled->getValue() ? 2 : 1);
    }

    // Set default configuration for all eight DACs on controller.
    int dacManualStream = (state->getControllerTypeEnum() == ControllerRecordUSB3) ? 32 : 8;
    for (int i = 0; i < 8; i++) {
        rhxController->enableDac(i, false);
        rhxController->selectDacDataStream(i, dacManualStream); // Initially point DACs to DacManual1 input
        rhxController->selectDacDataChannel(i, 0);
        setDacThreshold(i, 0);
    }
    rhxController->setDacManual(32768);
    rhxController->setDacGain(0);
    rhxController->setAudioNoiseSuppress(0);

    // Set default SPI cable delay values.
    rhxController->setCableDelay(PortA, 1);
    rhxController->setCableDelay(PortB, 1);
    rhxController->setCableDelay(PortC, 1);
    rhxController->setCableDelay(PortD, 1);
    if (state->numSPIPorts > 4) {
        rhxController->setCableDelay(PortE, 1);
        rhxController->setCableDelay(PortF, 1);
        rhxController->setCableDelay(PortG, 1);
        rhxController->setCableDelay(PortH, 1);
    }
}

// Create SPI command lists and upload to auxiliary command slots.
void ControllerInterface::updateChipCommandLists(bool updateStimParams)
{
    RHXRegisters chipRegisters(state->getControllerTypeEnum(), rhxController->getSampleRate(), state->getStimStepSizeEnum());

    chipRegisters.setDigOutLow(RHXRegisters::DigOut::DigOut1); // Take auxiliary output out of HiZ mode.
    chipRegisters.setDigOutLow(RHXRegisters::DigOut::DigOut2); // Take auxiliary output out of HiZ mode.
    chipRegisters.setDigOutLow(RHXRegisters::DigOut::DigOutOD); // Take auxiliary output out of HiZ mode.

    vector<unsigned int> commandList;
    int numCommands = RHXDataBlock::samplesPerDataBlock(state->getControllerTypeEnum());
    int commandSequenceLength;

    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        // Create a command list for the AuxCmd1 slot.  This command sequence programs most of the RAM registers
        // on the RHS2116 chip.
        commandSequenceLength = chipRegisters.createCommandListRHSRegisterConfig(commandList, updateStimParams);
        rhxController->uploadCommandList(commandList, RHXController::AuxCmd1, 0); // RHS - bank doesn't matter
        rhxController->selectAuxCommandLength(RHXController::AuxCmd1, 0, commandSequenceLength - 1);

        // Next, fill the other three command slots with dummy commands
        chipRegisters.createCommandListDummy(commandList, 8192, chipRegisters.createRHXCommand(RHXRegisters::RHXCommandRegRead, 255));
        rhxController->uploadCommandList(commandList, RHXController::AuxCmd2, 0); // RHS - bank doesn't matter
        chipRegisters.createCommandListDummy(commandList, 8192, chipRegisters.createRHXCommand(RHXRegisters::RHXCommandRegRead, 254));
        rhxController->uploadCommandList(commandList, RHXController::AuxCmd3, 0); // RHS - bank doesn't matter
        chipRegisters.createCommandListDummy(commandList, 8192, chipRegisters.createRHXCommand(RHXRegisters::RHXCommandRegRead, 253));
        rhxController->uploadCommandList(commandList, RHXController::AuxCmd4, 0);
    } else {
        // Create a command list for the AuxCmd1 slot.  This command sequence will continuously
        // update Register 3, which controls the auxiliary digital output pin on each chip.
        // This permits real-time control of the digital output pin on chips on each SPI port.
        commandSequenceLength = chipRegisters.createCommandListRHDUpdateDigOut(commandList, numCommands);
        rhxController->uploadCommandList(commandList, RHXController::AuxCmd1, 0);
        rhxController->selectAuxCommandLength(RHXController::AuxCmd1, 0, commandSequenceLength - 1);
        rhxController->selectAuxCommandBankAllPorts(RHXController::AuxCmd1, 0);

        // Next, we'll create a command list for the AuxCmd2 slot.  This command sequence
        // will sample the temperature sensor and other auxiliary ADC inputs.
        commandSequenceLength = chipRegisters.createCommandListRHDSampleAuxIns(commandList, numCommands);
        rhxController->uploadCommandList(commandList, RHXController::AuxCmd2, 0);
        rhxController->selectAuxCommandLength(RHXController::AuxCmd2, 0, commandSequenceLength - 1);
        rhxController->selectAuxCommandBankAllPorts(RHXController::AuxCmd2, 0);
    }

    // Set amplifier bandwidth parameters.
    state->holdUpdate();
    state->actualDspCutoffFreq->setValueWithLimits(chipRegisters.setDspCutoffFreq(state->desiredDspCutoffFreq->getValue()));
    state->actualLowerBandwidth->setValueWithLimits(chipRegisters.setLowerBandwidth(state->desiredLowerBandwidth->getValue(), 0));
    state->actualLowerSettleBandwidth->setValueWithLimits(chipRegisters.setLowerBandwidth(state->desiredLowerSettleBandwidth->getValue(), 1));
    state->actualUpperBandwidth->setValueWithLimits(chipRegisters.setUpperBandwidth(state->desiredUpperBandwidth->getValue()));
    chipRegisters.enableDsp(state->dspEnabled->getValue());
    state->releaseUpdate();

    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        commandSequenceLength = chipRegisters.createCommandListRHSRegisterConfig(commandList, updateStimParams);
        // Upload version with no ADC calibration to AuxCmd1 RAM Bank.
        rhxController->uploadCommandList(commandList, RHXController::AuxCmd1, 0); // RHS - bank doesn't matter
        rhxController->selectAuxCommandLength(RHXController::AuxCmd1, 0, commandSequenceLength - 1);

        // Run system once for changes to take effect.
        rhxController->setContinuousRunMode(false);
        rhxController->setMaxTimeStep(RHXDataBlock::samplesPerDataBlock(rhxController->getType()));

        // Start SPI interface.
        rhxController->run();

        // Wait for the 128-sample run to complete.
        while (rhxController->isRunning()) {
            qApp->processEvents();
        }

        rhxController->flush();
    } else {
        // For the AuxCmd3 slot, we will create three command sequences.  All sequences
        // will configure and read back the RHD2000 chip registers, but one sequence will
        // also run ADC calibration.  Another sequence will enable amplifier 'fast settle'.

        commandSequenceLength = chipRegisters.createCommandListRHDRegisterConfig(commandList, true, numCommands);
        // Upload version with ADC calibration to AuxCmd3 RAM Bank 0.
        rhxController->uploadCommandList(commandList, RHXController::AuxCmd3, 0);
        rhxController->selectAuxCommandLength(RHXController::AuxCmd3, 0, commandSequenceLength - 1);

        commandSequenceLength = chipRegisters.createCommandListRHDRegisterConfig(commandList, false, numCommands);
        // Upload version with no ADC calibration to AuxCmd3 RAM Bank 1.
        rhxController->uploadCommandList(commandList, RHXController::AuxCmd3, 1);
        rhxController->selectAuxCommandLength(RHXController::AuxCmd3, 0, commandSequenceLength - 1);

        chipRegisters.setFastSettle(true);
        commandSequenceLength = chipRegisters.createCommandListRHDRegisterConfig(commandList, false, numCommands);
        // Upload version with fast settle enabled to AuxCmd3 RAM Bank 2.
        rhxController->uploadCommandList(commandList, RHXController::AuxCmd3, 2);
        rhxController->selectAuxCommandLength(RHXController::AuxCmd3, 0, commandSequenceLength - 1);
        chipRegisters.setFastSettle(false);

        rhxController->selectAuxCommandBankAllPorts(RHXController::AuxCmd3, state->manualFastSettleEnabled->getValue() ? 2 : 1);
    }

    setDacHighpassFilterEnabled(state->analogOutHighpassFilterEnabled->getValue());
    setDacHighpassFilterFrequency(state->analogOutHighpassFilterFrequency->getValue());
}

void ControllerInterface::runController()
{
    if (state->uploadInProgress->getValue()) {
        sendTCPError("Error - To avoid data corruption, controller cannot start running until previously started upload function completes");
        return;
    }

    usbDataThread->start();
    waveformProcessorThread->start();
    saveToDiskThread->start();

    usbDataThread->startRunning();
    waveformProcessorThread->startRunning(rhxController->getNumEnabledDataStreams());

    saveToDiskThread->startRunning();

    if (audioThread) audioThread->startRunning();
    if (tcpDataOutputThread) tcpDataOutputThread->startRunning();

    int numSamples = display->getSamplesPerRefresh();  // 1000 at 20 kHz; 1500 at 30 kHz

    uint32_t* timeStamps = new uint32_t [display->getMaxSamplesPerRefresh()];
    int lastTimeStamp = -1;
    int currentTimeStamp = 0;

    QElapsedTimer loopTimer, workTimer, reportTimer;
//    QElapsedTimer plotTimer;

    fill(cpuLoadHistory.begin(), cpuLoadHistory.end(), 0.0);

    loopTimer.start();
    workTimer.start();
    reportTimer.start();

    currentSweepPosition = 0;
    waveformFifo->resetBuffer();  // Clear any memory in waveform FIFO from previous running.
    display->reset();

    int triggerWaitNotify = 0;
    YScaleUsed yScaleUsed;
    while (state->running) {
        workTimer.restart();
        if (state->running && waveformFifo->requestReadNewData(WaveformFifo::ReaderDisplay, numSamples)) {
            waveformFifo->copyTimeStamps(WaveformFifo::ReaderDisplay, timeStamps, 0, numSamples);

            // Main thread plots data:
//            plotTimer.start();

            if (!state->triggerModeDisplay->getValue()) {
                // Normal (non-triggered) display
                yScaleUsed = display->loadWaveformData(waveformFifo);
                emit setTopStatusLabel("");
            } else {
                // Triggered display
                int numSamplesDisplayed = display->getSamplesPerFullRefresh();
                if (waveformFifo->numWordsInMemory(WaveformFifo::ReaderDisplay) > numSamplesDisplayed + numSamples) {
                    int memoryPosition = -round((1.0 - state->triggerPositionDisplay->getNumericValue()) * numSamplesDisplayed);

                    QString triggerChannelName = state->triggerSourceDisplay->getValueString();
                    bool useAnalogTrigger = triggerChannelName.left(1).toUpper() == "A";

                    uint16_t triggerMask = 0x01u;
                    if (!useAnalogTrigger) triggerMask = 0x01u << (int)state->triggerSourceDisplay->getNumericValue();

                    uint16_t* digitalInWaveform = waveformFifo->getDigitalWaveformPointer("DIGITAL-IN-WORD");
                    float* analogInWaveform = nullptr;
                    float logicThreshold = 0.0F;
                    if (useAnalogTrigger) {  // Get thresholded analog signal as digital signal
                        analogInWaveform = waveformFifo->getAnalogWaveformPointer(triggerChannelName.toStdString());
                        logicThreshold = (float)state->triggerAnalogVoltageThreshold->getValue();
                    }

                    bool risingEdge = state->triggerPolarityDisplay->getValue() == "Rising";

                    bool triggerFound = false;
                    int t = memoryPosition - numSamples - 1;
                    bool prevTriggerValue;
                    if (useAnalogTrigger) {
                        prevTriggerValue =
                                waveformFifo->getAnalogDataAsDigital(WaveformFifo::ReaderDisplay, analogInWaveform, t, logicThreshold) &
                                triggerMask;
                    } else {
                        prevTriggerValue = waveformFifo->getDigitalData(WaveformFifo::ReaderDisplay, digitalInWaveform, t) &
                                triggerMask;
                    }
                    for (++t; t <= memoryPosition; ++t) {
                        bool triggerValue;
                        if (useAnalogTrigger) {
                            triggerValue =
                                    waveformFifo->getAnalogDataAsDigital(WaveformFifo::ReaderDisplay, analogInWaveform, t, logicThreshold) &
                                    triggerMask;
                        } else {
                            triggerValue = waveformFifo->getDigitalData(WaveformFifo::ReaderDisplay, digitalInWaveform, t) &
                                    triggerMask;
                        }
                        if (risingEdge) {
                            if (!prevTriggerValue && triggerValue) {
                                triggerFound = true;
                                break;
                            }
                        } else {
                            if (prevTriggerValue && !triggerValue) {
                                triggerFound = true;
                                break;
                            }
                        }
                        prevTriggerValue = triggerValue;
                    }
                    if (triggerFound) {
                        int startTime = t - round((state->triggerPositionDisplay->getNumericValue()) * numSamplesDisplayed);
                        yScaleUsed = display->loadWaveformDataFromMemory(waveformFifo, startTime, true);
                        emit setTopStatusLabel("");
                        triggerWaitNotify = 0;
                    } else {
                        if (triggerWaitNotify++ > 20) {
                            emit setTopStatusLabel(tr("Waiting for trigger..."));
                            triggerWaitNotify = 20;
                        }
                    }
                }
            }

            if (controlPanel) controlPanel->updateSlidersEnabled(yScaleUsed);

            if (isiDialog) isiDialog->updateISI(waveformFifo, numSamples);
            if (psthDialog) psthDialog->updatePSTH(waveformFifo, numSamples);
            if (spectrogramDialog) spectrogramDialog->updateSpectrogram(waveformFifo, numSamples);
            if (spikeSortingDialog) spikeSortingDialog->updateSpikeScope(waveformFifo, numSamples);

            waveformFifo->freeOldData(WaveformFifo::ReaderDisplay);

//            double plotTime = (double) plotTimer.nsecsElapsed();

            if (!audioThread) {
                if (waveformFifo->requestReadNewData(WaveformFifo::ReaderAudio, numSamples)) {
                    waveformFifo->freeOldData(WaveformFifo::ReaderAudio);
                }
            }

            if (!tcpDataOutputThread) {
                if (waveformFifo->requestReadNewData(WaveformFifo::ReaderTCP, numSamples)) {
                    waveformFifo->freeOldData(WaveformFifo::ReaderTCP);
                }
            }

            for (int i = 0; i < numSamples; ++i) {
                currentTimeStamp = (int) timeStamps[i];
                if (currentTimeStamp - lastTimeStamp != 1 && lastTimeStamp != -1) {
                    cout << "Timestamp discontinuity: " << lastTimeStamp << " " << currentTimeStamp << '\n';
                }
                lastTimeStamp = currentTimeStamp;
            }

            double workTime = (double) workTimer.nsecsElapsed();
            double loopTime = (double) loopTimer.nsecsElapsed();
            workTimer.restart();
            loopTimer.restart();
            if (reportTimer.elapsed() >= 2000) {
                double cpuUsage = 100.0 * workTime / loopTime;

                // Calculate running average of CPU usage to smooth out fluctuations.
                for (int i = 1; i < (int) cpuLoadHistory.size(); ++i) {
                    cpuLoadHistory[i - 1] = cpuLoadHistory[i];
                }
                cpuLoadHistory[cpuLoadHistory.size() - 1] = cpuUsage;
                double total = 0.0;
                for (int i = 0; i < (int) cpuLoadHistory.size(); ++i) {
                    total += cpuLoadHistory[i];
                }
                double averageCpuLoad = total / (double)(cpuLoadHistory.size());

                emit cpuLoadPercent(averageCpuLoad);

//                cout << "        Controller Interface (Main Thread) CPU usage: " << (int) cpuUsage << "%" << EndOfLine;
//                cout << "Plot time = " << plotTime / 1.0e6 << " ms" << EndOfLine;
//                cout << "Work time = " << workTime / 1.0e6 << " ms" << EndOfLine;
//                cout << "Loop time = " << loopTime / 1.0e6 << " ms" << EndOfLine;
                reportTimer.restart();
            }
            qApp->processEvents();
        }

        qApp->processEvents();
        numSamples = display->getSamplesPerRefresh();
    }

    if (audioThread) {
        audioThread->stopRunning();
        while (audioThread->isActive()) {
            qApp->processEvents();
        }
    }

    if (tcpDataOutputThread) {
        tcpDataOutputThread->stopRunning();
        while (tcpDataOutputThread->isActive()) {
            qApp->processEvents();
        }
        tcpDataOutputEnabled = false;
    }

    saveToDiskThread->stopRunning();
    while (saveToDiskThread->isActive()) {
        qApp->processEvents();
    }

    waveformProcessorThread->stopRunning();
    while(waveformProcessorThread->isActive()) {
        qApp->processEvents();
    }

    usbDataThread->stopRunning();
    while (usbDataThread->isActive()) {  // Important: Must wait for usbDataThread to fully stop before we reset usbStreamFifo buffer!
        qApp->processEvents();  // Stay responsive to GUI events during this loop.
    }

    waveformFifo->pauseBuffer();

    usbStreamFifo->resetBuffer();

    delete [] timeStamps;
    fill(cpuLoadHistory.begin(), cpuLoadHistory.end(), 0.0);
    emit cpuLoadPercent(0.0);
    emit haveStopped();
}

void ControllerInterface::runControllerSilently(double nSeconds, QProgressDialog* progress)
{
    qint64 runTimeNsecs = nSeconds * 1e9;
    qint64 progressTickNsecs = 0.1 * 1e9;
    int progressStep = 1;

    if (progress) {
        progress->setWindowTitle(QObject::tr("Progress"));
        progress->setModal(true);
        progress->setMaximum(nSeconds * 10);
        progress->show();
    }

    int numSamples = 1000;

    usbDataThread->start();
    waveformProcessorThread->start();

    usbDataThread->startRunning();
    waveformProcessorThread->startRunning(rhxController->getNumEnabledDataStreams());

    waveformFifo->resetBuffer();  // Clear any memory in waveform FIFO from previous running.

    QElapsedTimer mainTimer, tickTimer;
    mainTimer.start();
    tickTimer.start();

    while (mainTimer.nsecsElapsed() < runTimeNsecs) {
        if (waveformFifo->requestReadNewData(WaveformFifo::ReaderDisplay, numSamples)) {
            waveformFifo->freeOldData(WaveformFifo::ReaderDisplay);

            if (waveformFifo->requestReadNewData(WaveformFifo::ReaderDisk, numSamples)) {
                waveformFifo->freeOldData(WaveformFifo::ReaderDisk);
            }

            if (waveformFifo->requestReadNewData(WaveformFifo::ReaderAudio, numSamples)) {
                waveformFifo->freeOldData(WaveformFifo::ReaderAudio);
            }

            if (waveformFifo->requestReadNewData(WaveformFifo::ReaderTCP, numSamples)) {
                waveformFifo->freeOldData(WaveformFifo::ReaderTCP);
            }

            qApp->processEvents();
        }

        qApp->processEvents();
        numSamples = display->getSamplesPerRefresh();

        if (tickTimer.nsecsElapsed() >= progressTickNsecs) {
            tickTimer.restart();
            if (progress) {
                progress->setValue(progressStep++);
            }
        }
    }

    waveformProcessorThread->stopRunning();
    while(waveformProcessorThread->isActive()) {
        qApp->processEvents();
    }

    usbDataThread->stopRunning();
    while (usbDataThread->isActive()) {  // Important: Must wait for usbDataThread to fully stop before we reset usbStreamFifo buffer!
        qApp->processEvents();  // Stay responsive to GUI events during this loop.
    }

    waveformFifo->pauseBuffer();
    usbStreamFifo->resetBuffer();

    if (progress) {
        progress->hide();
    }
}

float ControllerInterface::measureRmsLevel(string waveName, double timeSec) const
{
    int numSamples = round(state->sampleRate->getNumericValue() * timeSec);
    float* waveform = new float [numSamples];
    GpuWaveformAddress gpuWaveformAddress = waveformFifo->getGpuWaveformAddress(waveName);
    waveformFifo->copyGpuAmplifierData(WaveformFifo::ReaderDisplay, waveform, gpuWaveformAddress, -numSamples, numSamples);

    // Calculate RMS value of waveform.
    float sumOfSquares = 0.0;
    for (int i = 0; i < numSamples; ++i) {
        sumOfSquares += waveform[i] * waveform[i];
    }
    float rmsLevel = sqrt(sumOfSquares / (float)numSamples);

    delete [] waveform;

    return rmsLevel;
}

void ControllerInterface::setAllSpikeDetectionThresholds()
{
    if (state->absoluteThresholdsEnabled->getValue()) {
        double threshold = state->absoluteThreshold->getValue();
        vector<string> waveNameList = state->signalSources->amplifierChannelsNameList();
        for (int i = 0; i < (int) waveNameList.size(); ++i) {
            Channel* channel = state->signalSources->channelByName(waveNameList[i]);
            if (channel) {
                if (channel->isEnabled()) {
                    channel->setSpikeThreshold(round(threshold));
                }
            }
        }
    } else {
        double rmsMultiple = state->rmsMultipleThreshold->getValue();
        if (state->negativeRelativeThreshold->getValue()) rmsMultiple *= -1;
        double numSecondsToMeasure = 3.0;

        QProgressDialog* progress = new QProgressDialog(QObject::tr("Measuring Noise Floor to Calculate Thresholds"), QString(), 0, 1);
        runControllerSilently(numSecondsToMeasure + 1.0, progress);  // Add one second at beginning so we ignore starting transients.
        delete progress;

        vector<string> waveNameList = state->signalSources->amplifierChannelsNameList();
        for (int i = 0; i < (int) waveNameList.size(); ++i) {
            string waveName = waveNameList[i] + "|HIGH";  // Measure RMS levels of highpass filtered signal for spike threshold calculation.
            float rmsLevel = measureRmsLevel(waveName, numSecondsToMeasure);
            Channel* channel = state->signalSources->channelByName(waveNameList[i]);
            if (channel) {
                if (channel->isEnabled()) {
                    channel->setSpikeThreshold(round(rmsMultiple * rmsLevel));
                }
            }
        }
    }
}

// Negative values of speed rewind into waveform FIFO memory; positive values fast forward, at the specified multiple of realtime.
void ControllerInterface::sweepDisplay(double speed)
{
    int numSamples = display->getSamplesPerRefresh();
    double nanosecondsPerRefresh = 1.0e9 * (double)numSamples / state->sampleRate->getNumericValue();
    double speedUpFactor = fabs(speed);
    int64_t nanosecondsPerLoop = round(nanosecondsPerRefresh / speedUpFactor);
    int numSamplesInMemory = waveformFifo->numWordsInMemory(WaveformFifo::ReaderDisplay);
    QElapsedTimer timer;
    timer.start();

    int currentTimeStamp = 0;
    int startTime = currentSweepPosition;

    YScaleUsed yScaleUsed;
    while (state->sweeping) {
        if (timer.nsecsElapsed() > nanosecondsPerLoop) {
            timer.start();
            if (startTime >= -numSamplesInMemory && startTime <= 0) {
                yScaleUsed = display->loadWaveformDataFromMemory(waveformFifo, startTime);
                if (controlPanel) controlPanel->updateSlidersEnabled(yScaleUsed);
                QTime sweepTime(0, 0);
                int timeStamp = currentTimeStamp + startTime;
                int totalSweepTimeSeconds = round((double)timeStamp / state->sampleRate->getNumericValue());
                QString timeString;
                if (timeStamp < 0) {
                    timeString = "-" + sweepTime.addSecs(-totalSweepTimeSeconds).toString("HH:mm:ss");
                    if (timeString == "-00:00:00") timeString = "00:00:00";
                } else {
                    timeString = sweepTime.addSecs(totalSweepTimeSeconds).toString("HH:mm:ss");
                }
                emit setTimeLabel(timeString);
            } else {
                state->sweeping = false;
            }
            if (speed < 0) {
                startTime -= numSamples;
            } else {
                startTime += numSamples;
            }
        }
        numSamples = display->getSamplesPerRefresh();
        qApp->processEvents();
    }

    startTime = qBound(-numSamplesInMemory, startTime, 0);
    currentSweepPosition = startTime;

    emit haveStopped();
}

void ControllerInterface::resetWaveformFifo()
{
    waveformFifo->resetBuffer();
}

void ControllerInterface::setDacGain(int dacGainIndex)
{
    rhxController->setDacGain(dacGainIndex);
}

void ControllerInterface::setAudioNoiseSuppress(int noiseSuppressIndex)
{
    rhxController->setAudioNoiseSuppress(noiseSuppressIndex);
}

QString ControllerInterface::playbackFileName() const
{
    QString fileName;
    if (state->playback->getValue()) {
        fileName = dataFileReader->currentFileName();
    }
    return fileName;
}

QString ControllerInterface::currentTimePlaybackFile() const
{
    QString timeString;
    if (state->playback->getValue()) {
        timeString = dataFileReader->filePositionString();
    }
    return timeString;
}

QString ControllerInterface::startTimePlaybackFile() const
{
    QString timeString;
    if (state->playback->getValue()) {
        timeString = dataFileReader->startPositionString();
    }
    return timeString;
}

QString ControllerInterface::endTimePlaybackFile() const
{
    QString timeString;
    if (state->playback->getValue()) {
        timeString = dataFileReader->endPositionString();
    }
    return timeString;
}

void ControllerInterface::setStimSequenceParameters(Channel* ampChannel)
{
    if (rhxController->isSynthetic() || rhxController->isPlayback()) return;

    const int Never = 65535;

    StimParameters* parameters = ampChannel->stimParameters;
    int stream = ampChannel->getCommandStream();
    int channel = ampChannel->getChipChannel();
    double timestep = 1.0e6 / state->sampleRate->getNumericValue();  // time step in microseconds
    double currentstep = RHXRegisters::stimStepSizeToDouble(state->getStimStepSizeEnum()) * 1.0e6;  // current step in microamps

    int numOfPulses = ((PulseOrTrain) parameters->pulseOrTrain->getIndex() == SinglePulse) ?
                1 : parameters->numberOfStimPulses->getValue();

    rhxController->configureStimTrigger(stream, channel, parameters->triggerSource->getIndex(),
                                        parameters->enabled->getValue(),
                                        ((TriggerEdgeOrLevel) parameters->triggerEdgeOrLevel->getIndex() == TriggerEdge),
                                        ((TriggerHighOrLow) parameters->triggerHighOrLow->getIndex() == TriggerLow));
    rhxController->configureStimPulses(stream, channel, numOfPulses, (StimShape)(parameters->stimShape->getIndex()),
                                       ((StimPolarity) parameters->stimPolarity->getIndex() == NegativeFirst));

    int preStimAmpSettle = round(parameters->preStimAmpSettle->getValue() / timestep);
    int postStimAmpSettle = round(parameters->postStimAmpSettle->getValue() / timestep);
    int postTriggerDelay = round(parameters->postTriggerDelay->getValue() / timestep);
    int firstPhaseDuration = round(parameters->firstPhaseDuration->getValue() / timestep);
    int secondPhaseDuration = round(parameters->secondPhaseDuration->getValue() / timestep);
    int interphaseDelay = round(parameters->interphaseDelay->getValue() / timestep);
    int refractoryPeriod = round(parameters->refractoryPeriod->getValue() / timestep);
    int postStimChargeRecovOn = round(parameters->postStimChargeRecovOn->getValue() / timestep);
    int postStimChargeRecovOff = round(parameters->postStimChargeRecovOff->getValue() / timestep );
    int pulseTrainPeriod = round(parameters->pulseTrainPeriod->getValue() / timestep);

    int eventStartStim;
    int eventStimPhase2;
    int eventStimPhase3;
    int eventEndStim;
    int eventEnd;
    int eventRepeatStim;
    int eventAmpSettleOn;
    int eventAmpSettleOff;
    int eventAmpSettleOnRepeat;
    int eventAmpSettleOffRepeat;
    int eventChargeRecovOn;
    int eventChargeRecovOff;

    switch ((StimShape) parameters->stimShape->getIndex()) {
    case Biphasic:
        eventStartStim = postTriggerDelay;
        eventStimPhase2 = eventStartStim + firstPhaseDuration;
        eventStimPhase3 = Never;
        eventEndStim = eventStimPhase2 + secondPhaseDuration;
        eventEnd = eventEndStim + refractoryPeriod;
        break;
    case BiphasicWithInterphaseDelay:
        eventStartStim = postTriggerDelay;
        eventStimPhase2 = eventStartStim + firstPhaseDuration;
        eventStimPhase3 = eventStimPhase2 + interphaseDelay;
        eventEndStim = eventStimPhase3 + secondPhaseDuration;
        eventEnd = eventEndStim + refractoryPeriod;
        break;
    case Triphasic:
        eventStartStim = postTriggerDelay;
        eventStimPhase2 = eventStartStim + firstPhaseDuration;
        eventStimPhase3 = eventStimPhase2 + secondPhaseDuration;
        eventEndStim = eventStimPhase3 + firstPhaseDuration;
        eventEnd = eventEndStim + refractoryPeriod;
        break;
    case Monophasic:
        // Monophasic doesn't apply to StimParameters for amp channels.
        cerr << "Attempted to set amp channel's StimShape to Monophasic";
        return;
    }

    if ((PulseOrTrain) parameters->pulseOrTrain->getIndex() == PulseTrain) {
        eventRepeatStim = eventStartStim + pulseTrainPeriod;
    } else {
        eventRepeatStim = Never;
    }

    if (parameters->enableAmpSettle->getValue()) {
        eventAmpSettleOn = eventStartStim - preStimAmpSettle;
        eventAmpSettleOff = eventEndStim + postStimAmpSettle;
        if (parameters->maintainAmpSettle->getValue()) {
            eventAmpSettleOnRepeat = Never;
            eventAmpSettleOffRepeat = Never;
        } else {
            eventAmpSettleOnRepeat = eventRepeatStim - preStimAmpSettle;
            eventAmpSettleOffRepeat = postStimAmpSettle;
        }
    } else {
        eventAmpSettleOn = Never;
        eventAmpSettleOff = 0;
        eventAmpSettleOnRepeat = Never;
        eventAmpSettleOffRepeat = Never;
    }

    if (parameters->enableChargeRecovery->getValue()) {
        eventChargeRecovOn = eventEndStim + postStimChargeRecovOn;
        eventChargeRecovOff = eventEndStim + postStimChargeRecovOff;
    } else {
        eventChargeRecovOn = Never;
        eventChargeRecovOff = 0;
    }

    rhxController->programStimReg(stream, channel, AbstractRHXController::EventAmpSettleOn, eventAmpSettleOn);
    rhxController->programStimReg(stream, channel, AbstractRHXController::EventStartStim, eventStartStim);
    rhxController->programStimReg(stream, channel, AbstractRHXController::EventStimPhase2, eventStimPhase2);
    rhxController->programStimReg(stream, channel, AbstractRHXController::EventStimPhase3, eventStimPhase3);
    rhxController->programStimReg(stream, channel, AbstractRHXController::EventEndStim, eventEndStim);
    rhxController->programStimReg(stream, channel, AbstractRHXController::EventRepeatStim, eventRepeatStim);
    rhxController->programStimReg(stream, channel, AbstractRHXController::EventAmpSettleOff, eventAmpSettleOff);
    rhxController->programStimReg(stream, channel, AbstractRHXController::EventChargeRecovOn, eventChargeRecovOn);
    rhxController->programStimReg(stream, channel, AbstractRHXController::EventChargeRecovOff, eventChargeRecovOff);
    rhxController->programStimReg(stream, channel, AbstractRHXController::EventAmpSettleOnRepeat, eventAmpSettleOnRepeat);
    rhxController->programStimReg(stream, channel, AbstractRHXController::EventAmpSettleOffRepeat, eventAmpSettleOffRepeat);
    rhxController->programStimReg(stream, channel, AbstractRHXController::EventEnd, eventEnd);

    rhxController->enableAuxCommandsOnOneStream(stream);

    RHXRegisters chipRegisters(rhxController->getType(), rhxController->getSampleRate(), state->getStimStepSizeEnum());
    int commandSequenceLength;
    vector<unsigned int> commandList;

    int firstPhaseAmplitude = round(parameters->firstPhaseAmplitude->getValue() / currentstep);
    int secondPhaseAmplitude = round(parameters->secondPhaseAmplitude->getValue() / currentstep);
    int posMag = ((StimPolarity) parameters->stimPolarity->getIndex() == PositiveFirst) ?
                firstPhaseAmplitude : secondPhaseAmplitude;
    int negMag = ((StimPolarity) parameters->stimPolarity->getIndex() == NegativeFirst) ?
                firstPhaseAmplitude : secondPhaseAmplitude;

    commandSequenceLength = chipRegisters.createCommandListSetStimMagnitudes(commandList, channel, posMag, 0, negMag, 0);
    rhxController->uploadCommandList(commandList, AbstractRHXController::AuxCmd1, 0);  // RHS - bank doesn't matter
    rhxController->selectAuxCommandLength(AbstractRHXController::AuxCmd1, 0, commandSequenceLength - 1);

    chipRegisters.createCommandListDummy(commandList, 8192, chipRegisters.createRHXCommand(RHXRegisters::RHXCommandRegRead, 255));
    rhxController->uploadCommandList(commandList, AbstractRHXController::AuxCmd2, 0);  // RHS - bank doesn't matter
    rhxController->uploadCommandList(commandList, AbstractRHXController::AuxCmd3, 0);  // RHS - bank doesn't matter
    rhxController->uploadCommandList(commandList, AbstractRHXController::AuxCmd4, 0);  // RHS - bank doesn't matter

    rhxController->setMaxTimeStep(commandSequenceLength);
    rhxController->setContinuousRunMode(false);
    rhxController->setStimCmdMode(false);
    rhxController->enableAuxCommandsOnOneStream(stream);

    rhxController->run();
    while (rhxController->isRunning() ) {
        qApp->processEvents();
    }

    commandSequenceLength = chipRegisters.createCommandListRHSRegisterRead(commandList);
    rhxController->uploadCommandList(commandList, AbstractRHXController::AuxCmd1, 0);  // RHS - bank doesn't matter
    rhxController->selectAuxCommandLength(AbstractRHXController::AuxCmd1, 0, commandSequenceLength - 1);
    rhxController->run();
    while (rhxController->isRunning() ) {
        qApp->processEvents();
    }

    RHXDataBlock dataBlock(rhxController->getType(), rhxController->getNumEnabledDataStreams());
    rhxController->readDataBlock(&dataBlock);
    rhxController->readDataBlock(&dataBlock);

    commandSequenceLength = chipRegisters.createCommandListRHSRegisterConfig(commandList, true);
    rhxController->uploadCommandList(commandList, AbstractRHXController::AuxCmd1, 0);  // RHS - bank doesn't matter
    rhxController->selectAuxCommandLength(AbstractRHXController::AuxCmd1, 0, commandSequenceLength - 1);

    rhxController->enableAuxCommandsOnAllStreams();
}

void ControllerInterface::setAnalogOutSequenceParameters(Channel* anOutChannel)
{
    if (rhxController->isSynthetic() || rhxController->isPlayback()) return;

    const int Never = 65535;

    StimParameters* parameters = anOutChannel->stimParameters;
    int channel = anOutChannel->getNativeChannelNumber();
    int stream = 8 + channel;
    double timestep = 1.0e6 / state->sampleRate->getNumericValue();  // time step in microseconds

    int numOfPulses = ((PulseOrTrain) parameters->pulseOrTrain->getIndex() == SinglePulse) ?
                1 : parameters->numberOfStimPulses->getValue();

    rhxController->configureStimTrigger(stream, 0, parameters->triggerSource->getIndex(),
                                        parameters->enabled->getValue(),
                                        ((TriggerEdgeOrLevel) parameters->triggerEdgeOrLevel->getIndex() == TriggerEdge),
                                        ((TriggerHighOrLow) parameters->triggerHighOrLow->getIndex() == TriggerLow));
    rhxController->configureStimPulses(stream, 0, numOfPulses, (StimShape)(parameters->stimShape->getIndex()),
                                       ((StimPolarity) parameters->stimPolarity->getIndex() == NegativeFirst));

    int postTriggerDelay = round(parameters->postTriggerDelay->getValue() / timestep);
    int firstPhaseDuration = round(parameters->firstPhaseDuration->getValue() / timestep);
    int secondPhaseDuration = round(parameters->secondPhaseDuration->getValue() / timestep);
    int interphaseDelay = round(parameters->interphaseDelay->getValue() / timestep);
    int refractoryPeriod = round(parameters->refractoryPeriod->getValue() / timestep);
    int pulseTrainPeriod = round(parameters->pulseTrainPeriod->getValue() / timestep);

    int eventStartStim;
    int eventStimPhase2;
    int eventStimPhase3;
    int eventEndStim;
    int eventEnd;
    int eventRepeatStim;

    switch ((StimShape) parameters->stimShape->getIndex()) {
    case Biphasic:
        eventStartStim = postTriggerDelay;
        eventStimPhase2 = eventStartStim + firstPhaseDuration;
        eventStimPhase3 = Never;
        eventEndStim = eventStimPhase2 + secondPhaseDuration;
        eventEnd = eventEndStim + refractoryPeriod;
        break;
    case BiphasicWithInterphaseDelay:
        eventStartStim = postTriggerDelay;
        eventStimPhase2 = eventStartStim + firstPhaseDuration;
        eventStimPhase3 = eventStimPhase2 + interphaseDelay;
        eventEndStim = eventStimPhase3 + secondPhaseDuration;
        eventEnd = eventEndStim + refractoryPeriod;
        break;
    case Triphasic:
        eventStartStim = postTriggerDelay;
        eventStimPhase2 = eventStartStim + firstPhaseDuration;
        eventStimPhase3 = eventStimPhase2 + secondPhaseDuration;
        eventEndStim = eventStimPhase3 + firstPhaseDuration;
        eventEnd = eventEndStim + refractoryPeriod;
        break;
    case Monophasic:
        eventStartStim = postTriggerDelay;
        eventStimPhase2 = Never;
        eventStimPhase3 = Never;
        eventEndStim = eventStartStim + firstPhaseDuration;
        eventEnd = eventEndStim + refractoryPeriod;
        break;
    }

    if ((PulseOrTrain) parameters->pulseOrTrain->getIndex() == PulseTrain) {
        eventRepeatStim = eventStartStim + pulseTrainPeriod;
    } else {
        eventRepeatStim = Never;
    }

    rhxController->programStimReg(stream, 0, AbstractRHXController::EventStartStim, eventStartStim);
    rhxController->programStimReg(stream, 0, AbstractRHXController::EventStimPhase2, eventStimPhase2);
    rhxController->programStimReg(stream, 0, AbstractRHXController::EventStimPhase3, eventStimPhase3);
    rhxController->programStimReg(stream, 0, AbstractRHXController::EventEndStim, eventEndStim);
    rhxController->programStimReg(stream, 0, AbstractRHXController::EventRepeatStim, eventRepeatStim);
    rhxController->programStimReg(stream, 0, AbstractRHXController::EventEnd, eventEnd);

    int dacBaseline, dacPositive, dacNegative;
    const double dacLsb = (2 * 10.24) / 65536;
    const int dacMid = 32768;
    dacBaseline = dacMid + (int)(parameters->baselineVoltage->getValue() / dacLsb);

    if ((StimShape) parameters->stimShape->getIndex() == Monophasic) {
        if ((StimPolarity) parameters->stimPolarity->getIndex() == NegativeFirst) {
            dacPositive = dacBaseline;
            dacNegative = dacBaseline + (int)(-1.0 * parameters->firstPhaseAmplitude->getValue() / dacLsb);
        } else {
            dacPositive = dacBaseline + (int)(parameters->firstPhaseAmplitude->getValue() / dacLsb);
            dacNegative = dacBaseline;
        }
    } else {
        dacPositive = dacBaseline + (int)(((StimPolarity) parameters->stimPolarity->getIndex() == NegativeFirst ?
                                               parameters->secondPhaseAmplitude->getValue() :
                                               parameters->firstPhaseAmplitude->getValue()) / dacLsb);
        dacNegative = dacBaseline + (int)(-1.0 * ((StimPolarity) parameters->stimPolarity->getIndex() == NegativeFirst ?
                                                      parameters->firstPhaseAmplitude->getValue() :
                                                      parameters->secondPhaseAmplitude->getValue()) / dacLsb);
    }

    dacBaseline = qBound(0, dacBaseline, 65535);
    dacPositive = qBound(0, dacPositive, 65535);
    dacNegative = qBound(0, dacNegative, 65535);

    rhxController->programStimReg(stream, 0, AbstractRHXController::DacBaseline, dacBaseline);
    rhxController->programStimReg(stream, 0, AbstractRHXController::DacPositive, dacPositive);
    rhxController->programStimReg(stream, 0, AbstractRHXController::DacNegative, dacNegative);
}

void ControllerInterface::setDigitalOutSequenceParameters(Channel* digOutChannel)
{
    if (rhxController->isSynthetic() || rhxController->isPlayback()) return;

    const int Never = 65535;

    StimParameters* parameters = digOutChannel->stimParameters;
    int channel = digOutChannel->getNativeChannelNumber();
    int stream = 16;
    double timestep = 1.0e6 / state->sampleRate->getNumericValue();  // time step in microseconds

    int numOfPulses = ((PulseOrTrain) parameters->pulseOrTrain->getIndex() == SinglePulse) ?
                1 : parameters->numberOfStimPulses->getValue();

    rhxController->configureStimTrigger(stream, channel, parameters->triggerSource->getIndex(),
                                        parameters->enabled->getValue(),
                                        ((TriggerEdgeOrLevel) parameters->triggerEdgeOrLevel->getIndex() == TriggerEdge),
                                        ((TriggerHighOrLow) parameters->triggerHighOrLow->getIndex() == TriggerLow));
    rhxController->configureStimPulses(stream, channel, numOfPulses, Monophasic, false);

    int postTriggerDelay = round(parameters->postTriggerDelay->getValue() / timestep);
    int firstPhaseDuration = round(parameters->firstPhaseDuration->getValue() / timestep);
    int refractoryPeriod = round(parameters->refractoryPeriod->getValue() / timestep);
    int pulseTrainPeriod = round(parameters->pulseTrainPeriod->getValue() / timestep);

    int eventStartStim = postTriggerDelay;
    int eventEndStim = eventStartStim + firstPhaseDuration;
    int eventEnd = eventEndStim + refractoryPeriod;
    int eventRepeatStim;

    if ((PulseOrTrain) parameters->pulseOrTrain->getIndex() == PulseTrain) {
        eventRepeatStim = eventStartStim + pulseTrainPeriod;
    } else {
        eventRepeatStim = Never;
    }

    rhxController->programStimReg(stream, channel, AbstractRHXController::EventStartStim, eventStartStim);
    rhxController->programStimReg(stream, channel, AbstractRHXController::EventEndStim, eventEndStim);
    rhxController->programStimReg(stream, channel, AbstractRHXController::EventRepeatStim, eventRepeatStim);
    rhxController->programStimReg(stream, channel, AbstractRHXController::EventEnd, eventEnd);
}

void ControllerInterface::manualStimTriggerOn(QString keyName)
{
    setManualStimTrigger(keyName, true);
}

void ControllerInterface::manualStimTriggerOff(QString keyName)
{
    setManualStimTrigger(keyName, false);
}

void ControllerInterface::setManualStimTrigger(int trigger, bool triggerOn)
{
    rhxController->setManualStimTrigger(trigger, triggerOn);
}

void ControllerInterface::setManualStimTrigger(QString keyName, bool triggerOn)
{
    if (keyName.toLower() == "f1") {
        rhxController->setManualStimTrigger(0, triggerOn);
    } else if (keyName.toLower() == "f2") {
        rhxController->setManualStimTrigger(1, triggerOn);
    } else if (keyName.toLower() == "f3") {
        rhxController->setManualStimTrigger(2, triggerOn);
    } else if (keyName.toLower() == "f4") {
        rhxController->setManualStimTrigger(3, triggerOn);
    } else if (keyName.toLower() == "f5") {
        rhxController->setManualStimTrigger(4, triggerOn);
    } else if (keyName.toLower() == "f6") {
        rhxController->setManualStimTrigger(5, triggerOn);
    } else if (keyName.toLower() == "f7") {
        rhxController->setManualStimTrigger(6, triggerOn);
    } else if (keyName.toLower() == "f8") {
        rhxController->setManualStimTrigger(7, triggerOn);
    }
}

void ControllerInterface::setChargeRecoveryParameters(bool mode, RHXRegisters::ChargeRecoveryCurrentLimit currentLimit,
                                                      double targetVoltage)
{
    if (rhxController->isSynthetic() || rhxController->isPlayback()) return;
    if (state->getControllerTypeEnum() != ControllerStimRecordUSB2) return;

    rhxController->setChargeRecoveryMode(mode);
    rhxController->enableAuxCommandsOnAllStreams();

    RHXRegisters chipRegisters(rhxController->getType(), rhxController->getSampleRate(), state->getStimStepSizeEnum());
    int commandSequenceLength;
    vector<unsigned int> commandList;

    commandSequenceLength = chipRegisters.createCommandListConfigChargeRecovery(commandList, currentLimit, targetVoltage);
    rhxController->uploadCommandList(commandList, AbstractRHXController::AuxCmd1, 0);
    rhxController->selectAuxCommandLength(AbstractRHXController::AuxCmd1, 0, commandSequenceLength - 1);

    chipRegisters.createCommandListDummy(commandList, 8192, chipRegisters.createRHXCommand(RHXRegisters::RHXCommandRegRead, 255));
    rhxController->uploadCommandList(commandList, AbstractRHXController::AuxCmd2, 0);
    rhxController->uploadCommandList(commandList, AbstractRHXController::AuxCmd3, 0);
    rhxController->uploadCommandList(commandList, AbstractRHXController::AuxCmd4, 0);

    rhxController->setMaxTimeStep(commandSequenceLength);
    rhxController->setContinuousRunMode(false);
    rhxController->setStimCmdMode(false);

    rhxController->run();
    while (rhxController->isRunning() ) {
        qApp->processEvents();
    }

    commandSequenceLength = chipRegisters.createCommandListRHSRegisterRead(commandList);
    rhxController->uploadCommandList(commandList, AbstractRHXController::AuxCmd1, 0);
    rhxController->selectAuxCommandLength(AbstractRHXController::AuxCmd1, 0, commandSequenceLength - 1);
    rhxController->run();
    while (rhxController->isRunning() ) {
        qApp->processEvents();
    }

    RHXDataBlock dataBlock(state->getControllerTypeEnum(), rhxController->getNumEnabledDataStreams());
    rhxController->readDataBlock(&dataBlock);
    rhxController->readDataBlock(&dataBlock);

    commandSequenceLength = chipRegisters.createCommandListRHSRegisterConfig(commandList, true);
    rhxController->uploadCommandList(commandList, AbstractRHXController::AuxCmd1, 0);
    rhxController->selectAuxCommandLength(AbstractRHXController::AuxCmd1, 0, commandSequenceLength - 1);
}

void ControllerInterface::manualStimTriggerPulse(QString keyName)
{
    if (keyName.toLower() == "f1") {
        rhxController->setManualStimTrigger(0, true);
        rhxController->setManualStimTrigger(0, false);
    } else if (keyName.toLower() == "f2") {
        rhxController->setManualStimTrigger(1, true);
        rhxController->setManualStimTrigger(1, false);
    } else if (keyName.toLower() == "f3") {
        rhxController->setManualStimTrigger(2, true);
        rhxController->setManualStimTrigger(2, false);
    } else if (keyName.toLower() == "f4") {
        rhxController->setManualStimTrigger(3, true);
        rhxController->setManualStimTrigger(3, false);
    } else if (keyName.toLower() == "f5") {
        rhxController->setManualStimTrigger(4, true);
        rhxController->setManualStimTrigger(4, false);
    } else if (keyName.toLower() == "f6") {
        rhxController->setManualStimTrigger(5, true);
        rhxController->setManualStimTrigger(5, false);
    } else if (keyName.toLower() == "f7") {
        rhxController->setManualStimTrigger(6, true);
        rhxController->setManualStimTrigger(6, false);
    } else if (keyName.toLower() == "f8") {
        rhxController->setManualStimTrigger(7, true);
        rhxController->setManualStimTrigger(7, false);
    }
}

void ControllerInterface::setDacHighpassFilterEnabled(bool enabled)
{
    rhxController->enableDacHighpassFilter(enabled);
}

void ControllerInterface::setDacHighpassFilterFrequency(double frequency)
{
    rhxController->setDacHighpassFilter(frequency);
}

void ControllerInterface::setDacChannel(int dac, const QString& channelName)
{
    if (channelName.toLower() == "off" || channelName.toLower() == "n/a") {
        rhxController->enableDac(dac, false);
        rhxController->selectDacDataStream(dac, 0);
        rhxController->selectDacDataChannel(dac, 0);
    } else {
        Channel* channel = state->signalSources->channelByName(channelName);
        if (!channel) return;
        if (channel->getSignalType() != AmplifierSignal) return;
        int stream = state->getControllerTypeEnum() == ControllerRecordUSB2 ? channel->getBoardStream() : channel->getCommandStream();
        rhxController->selectDacDataStream(dac, stream);
        rhxController->selectDacDataChannel(dac, channel->getChipChannel());
        rhxController->enableDac(dac, true);
    }
}

void ControllerInterface::setDacRefChannel(const QString& channelName)
{
    if (channelName.toLower() == "hardware" || channelName.toLower() == "off" || channelName.toLower() == "n/a") {
        rhxController->enableDacReref(false);
        rhxController->setDacRerefSource(0, 0);
    } else {
        Channel* channel = state->signalSources->channelByName(channelName);
        if (!channel) return;
        if (channel->getSignalType() != AmplifierSignal) return;
        rhxController->setDacRerefSource(channel->getCommandStream(), channel->getChipChannel());
        rhxController->enableDacReref(true);
    }
}

void ControllerInterface::setDacThreshold(int dac, int threshold)
{
    int threshLevel = qRound((double) threshold / 0.195) + 32768;
    rhxController->setDacThreshold(dac, threshLevel, threshold >= 0);
}

void ControllerInterface::setDacEnabled(int dac, bool enabled)
{
    rhxController->enableDac(dac, enabled);
}

void ControllerInterface::setTtlOutMode(bool mode1, bool mode2, bool mode3, bool mode4, bool mode5, bool mode6, bool mode7,
                                        bool mode8)
{
    rhxController->setTtlOutMode(mode1, mode2, mode3, mode4, mode5, mode6, mode7, mode8);
}

void ControllerInterface::enableFastSettle(bool enabled)
{
    if (state->getControllerTypeEnum() != ControllerStimRecordUSB2) {
        rhxController->selectAuxCommandBankAllPorts(RHXController::AuxCmd3, enabled ? 2 : 1);
    }
}

void ControllerInterface::enableExternalFastSettle(bool enabled)
{
    rhxController->enableExternalFastSettle(enabled);
}

void ControllerInterface::setExternalFastSettleChannel(int channel)
{
    rhxController->setExternalFastSettleChannel(channel - 1);
}

bool ControllerInterface::measureImpedances()
{
    ImpedanceReader zReader(state, rhxController);
    return zReader.measureImpedances();
}

bool ControllerInterface::saveImpedances()
{
    ImpedanceReader zReader(state, rhxController);
    return zReader.saveImpedances();
}

double ControllerInterface::swBufferPercentFull() const
{
    return max(waveformFifo->percentFull(), usbStreamFifo->percentFull());
}

void ControllerInterface::uploadAmpSettleSettings()
{
    if (state->uploadInProgress->getValue()) {
        sendTCPError("Error - Another upload cannot be started until the previous upload completes");
        return;
    }
    state->uploadInProgress->setValue(true);
    // Update values in hardware.
    setAmpSettleMode(state->useFastSettle->getValue());

    bool gSettle = state->headstageGlobalSettle->getValue();
    setGlobalSettlePolicy(gSettle, gSettle, gSettle, gSettle, false);

    updateChipCommandLists(false); // Update amplifier bandwidth (new desiredLowerSettleBandwidth)
    state->uploadInProgress->setValue(false);
}

void ControllerInterface::uploadChargeRecoverySettings()
{
    if (state->uploadInProgress->getValue()) {
        sendTCPError("Error - Another upload cannot be started until the previous upload completes");
        return;
    }
    state->uploadInProgress->setValue(true);
    // Update values in hardware.
    setChargeRecoveryParameters(state->chargeRecoveryMode->getValue(),
                                (RHXRegisters::ChargeRecoveryCurrentLimit) state->chargeRecoveryCurrentLimit->getIndex(),
                                state->chargeRecoveryTargetVoltage->getValue());
    state->uploadInProgress->setValue(false);
}

void ControllerInterface::uploadBandwidthSettings()
{
    if (state->uploadInProgress->getValue()) {
        sendTCPError("Error - Another upload cannot be started until the previous upload completes");
        return;
    }
    state->uploadInProgress->setValue(true);
    updateChipCommandLists(false);
    state->uploadInProgress->setValue(false);
}

void ControllerInterface::uploadStimParameters(Channel* channel)
{
    if (state->uploadInProgress->getValue()) {
        sendTCPError("Error - Another upload cannot be started until the previous upload completes");
        return;
    }
    state->uploadInProgress->setValue(true);
    if (channel->getSignalType() == AmplifierSignal) {
        setStimSequenceParameters(channel);
    } else if (channel->getSignalType() == BoardDacSignal) {
        setAnalogOutSequenceParameters(channel);
    } else if (channel->getSignalType() == BoardDigitalOutSignal) {
        setDigitalOutSequenceParameters(channel);
    }
    state->uploadInProgress->setValue(false);
}

void ControllerInterface::uploadStimParameters()
{
    vector<string> allChannels = state->signalSources->completeChannelsNameList();
    for (int i = 0; i < (int) allChannels.size(); i++) {
        Channel* channel = state->signalSources->channelByName(QString::fromStdString(allChannels[i]));
        uploadStimParameters(channel);
    }
}

void ControllerInterface::sendTCPError(QString errorMessage)
{
    emit TCPErrorMessage(errorMessage);
}

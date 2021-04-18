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

#include "tcpdataoutputthread.h"

TCPDataOutputThread::TCPDataOutputThread(WaveformFifo *waveformFifo_, const double sampleRate_, SystemState *state_, QObject *parent) :
    QThread(parent),
    tcpWaveformDataCommunicator(state_->tcpWaveformDataCommunicator),
    tcpSpikeDataCommunicator(state_->tcpSpikeDataCommunicator),
    waveformFifo(waveformFifo_),
    signalSources(state_->signalSources),
    sampleRate(sampleRate_),
    keepGoing(false),
    running(false),
    stopThread(false),
    parentObject(parent),
    connected(false),
    state(state_)
{
}

TCPDataOutputThread::~TCPDataOutputThread()
{
}

void TCPDataOutputThread::run()
{
    while (!stopThread) {
        if (keepGoing) {
            running = true;
            cout << "TCP setup" << '\n';

            // Any 'start up' code goes here.
            updateEnabledChannels();

            while (keepGoing && !stopThread) {

                if (closeRequested) {
                    this->closeInternal();
                    closeCompleted = true;
                    break;
                } else {
                    closeCompleted = false;
                }

                // If neither waveform nor spike ports are connected, just do a dummy read of the WaveformFifo
                if (tcpWaveformDataCommunicator->status != TCPCommunicator::Connected &&
                        tcpSpikeDataCommunicator->status != TCPCommunicator::Connected) {
                    if (waveformFifo->requestReadNewData(WaveformFifo::ReaderTCP, FramesPerBlock * state->tcpNumDataBlocksWrite->getValue())) {
                        waveformFifo->freeOldData(WaveformFifo::ReaderTCP);
                    }
                }

                // If at least one port is connected, get the correct # of filter bands and channels, read data from WaveformFifo, and output it
                else {

                    if (previousEnabledBands != state->signalSources->getTcpFilterBands()) {
                        updateEnabledChannels();
                    }

                    // Wait for 'tcpNumDataBlocksWrite' prior to write
                    if (waveformFifo->requestReadNewData(WaveformFifo::ReaderTCP, FramesPerBlock * state->tcpNumDataBlocksWrite->getValue())) {

                        if (enabledChannelNames.size() == 0) {
                            waveformFifo->freeOldData(WaveformFifo::ReaderTCP);
                            continue;
                        }

                        for (int i = 0; i < FramesPerBlock * state->tcpNumDataBlocksWrite->getValue(); ++i) {
                            if ((i % FramesPerBlock) == 0) {
                                waveformArray.replace(waveformArrayIndex, sizeof(TCPWaveformMagicNumber), (const char*)(&TCPWaveformMagicNumber), sizeof(TCPWaveformMagicNumber));
                                waveformArrayIndex += sizeof(TCPWaveformMagicNumber);
                            }
                            uint32_t timestamp = waveformFifo->getTimeStamp(WaveformFifo::ReaderTCP, i);
                            waveformArray.replace(waveformArrayIndex, sizeof(timestamp), (const char*)(&timestamp), sizeof(timestamp));
                            waveformArrayIndex += sizeof(timestamp);

                            // Grab digital in word and digital out word
                            uint16_t* boardDigitalInWaveform = waveformFifo->getDigitalWaveformPointer("DIGITAL-IN-WORD");
                            uint16_t digitalInWord = waveformFifo->getDigitalData(WaveformFifo::ReaderTCP, boardDigitalInWaveform, i);
                            bool digitalInWordSent = false;
                            uint16_t* boardDigitalOutWaveform = waveformFifo->getDigitalWaveformPointer("DIGITAL-OUT-WORD");
                            uint16_t digitalOutWord = waveformFifo->getDigitalData(WaveformFifo::ReaderTCP, boardDigitalOutWaveform, i);
                            bool digitalOutWordSent = false;

                            int stimChannelIndex = 0;

                            for (int channel = 0; channel < enabledChannelNames.size(); ++channel) {

                                Channel *thisChannel = signalSources->channelByName(enabledChannelNames[channel]);

                                // If this channel is an amplifier signal, read all enabled bands
                                if (thisChannel->getSignalType() == AmplifierSignal) {

                                    if (thisChannel->getOutputToTcp()) {
                                        string waveName = QString(enabledChannelNames[channel] + "|WIDE").toStdString();
                                        if (!waveformFifo->gpuWaveformPresent(waveName)) continue; // Error happened here - we should flag that there was a problem.
                                        GpuWaveformAddress waveformAddress = waveformFifo->getGpuWaveformAddress(waveName);
                                        if (waveformAddress.waveformIndex < 0) continue; // Error happened here - we should flag that there was a problem.
                                        uint16_t thisSample = waveformFifo->getGpuAmplifierDataRaw(WaveformFifo::ReaderTCP, waveformAddress, i);
                                        waveformArray.replace(waveformArrayIndex, sizeof(thisSample), (const char*)(&thisSample), sizeof(thisSample));
                                        waveformArrayIndex += sizeof(thisSample);
                                    }

                                    if (thisChannel->getOutputToTcpLow()) {
                                        string waveName = QString(enabledChannelNames[channel] + "|LOW").toStdString();
                                        if (!waveformFifo->gpuWaveformPresent(waveName)) continue; // Error happened here - we should flag that there was a problem.
                                        GpuWaveformAddress waveformAddress = waveformFifo->getGpuWaveformAddress(waveName);
                                        if (waveformAddress.waveformIndex < 0) continue; // Error happened here - we should flag that there was a problem.
                                        uint16_t thisSample = waveformFifo->getGpuAmplifierDataRaw(WaveformFifo::ReaderTCP, waveformAddress, i);
                                        waveformArray.replace(waveformArrayIndex, sizeof(thisSample), (const char*)(&thisSample), sizeof(thisSample));
                                        waveformArrayIndex += sizeof(thisSample);
                                    }

                                    if (thisChannel->getOutputToTcpHigh()) {
                                        string waveName = QString(enabledChannelNames[channel] + "|HIGH").toStdString();
                                        if (!waveformFifo->gpuWaveformPresent(waveName)) continue; // Error happened here - we should flag that there was a problem.
                                        GpuWaveformAddress waveformAddress = waveformFifo->getGpuWaveformAddress(waveName);
                                        if (waveformAddress.waveformIndex < 0) continue; // Error happened here - we should flag that there was a problem.
                                        uint16_t thisSample = waveformFifo->getGpuAmplifierDataRaw(WaveformFifo::ReaderTCP, waveformAddress, i);
                                        waveformArray.replace(waveformArrayIndex, sizeof(thisSample), (const char*)(&thisSample), sizeof(thisSample));
                                        waveformArrayIndex += sizeof(thisSample);
                                    }

                                    if (thisChannel->getOutputToTcpSpike()) {
                                        string waveName = QString(enabledChannelNames[channel] + "|SPK").toStdString();
                                        uint16_t* spikeWaveform = waveformFifo->getDigitalWaveformPointer(waveName);
                                        uint8_t spikeId = (uint8_t) waveformFifo->getDigitalData(WaveformFifo::ReaderTCP, spikeWaveform, i);
                                        if (spikeId != SpikeIdNoSpike) {
                                            // Create 14-byte chunk with magic num, native name, timestamp, and spike ID
                                            char nativeName[5];
                                            memcpy(nativeName, enabledChannelNames[channel].toLocal8Bit().constData(), sizeof(nativeName));

                                            // Put that chunk in spikeArray
                                            spikeArray.replace(spikeArrayIndex, sizeof(TCPSpikeMagicNumber), (const char*)(&TCPSpikeMagicNumber), sizeof(TCPSpikeMagicNumber));
                                            spikeArrayIndex += sizeof(TCPSpikeMagicNumber);

                                            spikeArray.replace(spikeArrayIndex, sizeof(nativeName), (const char*)(&nativeName), sizeof(nativeName));
                                            spikeArrayIndex += sizeof(nativeName);

                                            spikeArray.replace(spikeArrayIndex, sizeof(timestamp), (const char*)(&timestamp), sizeof(timestamp));
                                            spikeArrayIndex += sizeof(timestamp);

                                            spikeArray.replace(spikeArrayIndex, sizeof(spikeId), (const char*)(&spikeId), sizeof(spikeId));
                                            spikeArrayIndex += sizeof(spikeId);
                                        }
                                    }

                                    if (thisChannel->getOutputToTcpDc()) {
                                        string waveName = QString(enabledChannelNames[channel] + "|DC").toStdString();
                                        float *dcWaveform = waveformFifo->getAnalogWaveformPointer(waveName);
                                        float thisSampleFloat = waveformFifo->getAnalogData(WaveformFifo::ReaderTCP, dcWaveform, i);
                                        uint16_t thisSample = round((thisSampleFloat / -0.01923) + 512);
                                        waveformArray.replace(waveformArrayIndex, sizeof(thisSample), (const char*)(&thisSample), sizeof(thisSample));
                                        waveformArrayIndex += sizeof(thisSample);
                                    }

                                    if (thisChannel->getOutputToTcpStim()) {
                                        string waveName = QString(enabledChannelNames[channel] + "|STIM").toStdString();
                                        uint16_t *stimWaveform = waveformFifo->getDigitalWaveformPointer(waveName);
                                        uint16_t thisSampleUSB = waveformFifo->getDigitalData(WaveformFifo::ReaderTCP, stimWaveform, i);
                                        bool stimPolarityNegative = thisSampleUSB & (1 << 8);
                                        bool stimOn = thisSampleUSB & 1;
                                        uint8_t stimMagnitude;
                                        if (stimOn) {
                                            stimMagnitude = stimPolarityNegative ? negStimAmplitudes[stimChannelIndex++] : posStimAmplitudes[stimChannelIndex++];
                                        } else {
                                            stimMagnitude = 0;
                                            stimChannelIndex++;
                                        }
                                        uint16_t thisSampleNoMagnitude = thisSampleUSB & 65280;
                                        uint16_t thisSample = thisSampleNoMagnitude | stimMagnitude;
                                        waveformArray.replace(waveformArrayIndex, sizeof(thisSample), (const char*)(&thisSample), sizeof(thisSample));
                                        waveformArrayIndex += sizeof(thisSample);
                                    }
                                }

                                // If this channel is an auxinputsignal, read it if outputToTcp is true.
                                if (thisChannel->getSignalType() == AuxInputSignal) {

                                    if (thisChannel->getOutputToTcp()) {
                                        // Once every 4 samples, aux input actually gets a sample.
                                        if (i % 4 == 0) {
                                            string waveName = QString(enabledChannelNames[channel]).toStdString();
                                            float *auxWaveform = waveformFifo->getAnalogWaveformPointer(waveName);
                                            float thisSampleFloat = waveformFifo->getAnalogData(WaveformFifo::ReaderTCP, auxWaveform, i / 4);
                                            uint16_t thisSample = round((thisSampleFloat / 37.4e-6));
                                            waveformArray.replace(waveformArrayIndex, sizeof(thisSample), (const char*)(&thisSample), sizeof(thisSample));
                                            waveformArrayIndex += sizeof(thisSample);
                                            previousSample[channel] = thisSample;
                                        } else {
                                            // Every other data block, just repeat the previously gotten sample.
                                            uint16_t thisSample = previousSample[channel];
                                            waveformArray.replace(waveformArrayIndex, sizeof(thisSample), (const char*)(&thisSample), sizeof(thisSample));
                                            waveformArrayIndex += sizeof(thisSample);
                                        }
                                    }
                                }

                                // If this channel is a supplyvoltagesignal, read it if outputToTcp is true.
                                if (thisChannel->getSignalType() == SupplyVoltageSignal) {

                                    if (thisChannel->getOutputToTcp()) {
                                        // Once every data block, supply voltage actually gets a sample
                                        if (i % FramesPerBlock == 0) {
                                            string waveName = QString(enabledChannelNames[channel]).toStdString();
                                            float *vddWaveform = waveformFifo->getAnalogWaveformPointer(waveName);
                                            float thisSampleFloat = waveformFifo->getAnalogData(WaveformFifo::ReaderTCP, vddWaveform, i / FramesPerBlock);
                                            uint16_t thisSample = round((thisSampleFloat / 74.8e-6));
                                            waveformArray.replace(waveformArrayIndex, sizeof(thisSample), (const char*)(&thisSample), sizeof(thisSample));
                                            waveformArrayIndex += sizeof(thisSample);
                                            previousSample[channel] = thisSample;
                                        } else {
                                            // Every other data block, just repeat the previously g otten sample
                                            uint16_t thisSample = previousSample[channel];
                                            waveformArray.replace(waveformArrayIndex, sizeof(thisSample), (const char*)(&thisSample), sizeof(thisSample));
                                            waveformArrayIndex += sizeof(thisSample);
                                        }
                                    }
                                }

                                // If this channel is a boardadcsignal, read it if outputToTcp is true.
                                if (thisChannel->getSignalType() == BoardAdcSignal) {

                                    if (thisChannel->getOutputToTcp()) {
                                        string waveName = QString(enabledChannelNames[channel]).toStdString();
                                        float *adcWaveform = waveformFifo->getAnalogWaveformPointer(waveName);
                                        float thisSampleFloat = waveformFifo->getAnalogData(WaveformFifo::ReaderTCP, adcWaveform, i);
                                        uint16_t thisSample;
                                        if (state->getControllerTypeEnum() == ControllerRecordUSB2) {
                                            thisSample = round(thisSampleFloat / 50.354e-6);
                                        } else {
                                            thisSample = round(thisSampleFloat * 3200) + 32768;
                                        }
                                        waveformArray.replace(waveformArrayIndex, sizeof(thisSample), (const char*)(&thisSample), sizeof(thisSample));
                                        waveformArrayIndex += sizeof(thisSample);
                                    }
                                }

                                // If this channel is a boarddacsignal, read it if outputToTcp is true.
                                if (thisChannel->getSignalType() == BoardDacSignal) {

                                    if (thisChannel->getOutputToTcp()) {
                                        string waveName = QString(enabledChannelNames[channel]).toStdString();
                                        float *dacWaveform = waveformFifo->getAnalogWaveformPointer(waveName);
                                        float thisSampleFloat = waveformFifo->getAnalogData(WaveformFifo::ReaderTCP, dacWaveform, i);
                                        uint16_t thisSample = round(thisSampleFloat * 3200) + 32768;
                                        waveformArray.replace(waveformArrayIndex, sizeof(thisSample), (const char*)(&thisSample), sizeof(thisSample));
                                        waveformArrayIndex += sizeof(thisSample);
                                    }
                                }


                                // If this channel is a boarddigitalinsignal, read it if at least one digital in's outputtotcp is true.
                                if (thisChannel->getSignalType() == BoardDigitalInSignal) {

                                    if (numDigitalInChannels > 0) {
                                        if (!digitalInWordSent) {
                                            waveformArray.replace(waveformArrayIndex, sizeof(digitalInWord), (const char*)(&digitalInWord), sizeof(digitalInWord));
                                            waveformArrayIndex += sizeof(digitalInWord);
                                            digitalInWordSent = true;
                                        }
                                    }
                                }

                                // If this channel is a boarddigitaloutsignal, read it if at least one digital out's outputtotcp is true.
                                if (thisChannel->getSignalType() == BoardDigitalOutSignal) {

                                    if (numDigitalOutChannels > 0) {
                                        if (!digitalOutWordSent) {
                                            waveformArray.replace(waveformArrayIndex, sizeof(digitalOutWord), (const char*)(&digitalOutWord), sizeof(digitalOutWord));
                                            waveformArrayIndex += sizeof(digitalOutWord);
                                            digitalOutWordSent = true;
                                        }
                                    }
                                }
                            }
                        }
                        if (tcpWaveformDataCommunicator->status == TCPCommunicator::Connected)
                            tcpWaveformDataCommunicator->writeData(waveformArray.data(), waveformArrayIndex);
                        if (tcpSpikeDataCommunicator->status == TCPCommunicator::Connected)
                            tcpSpikeDataCommunicator->writeData(spikeArray.data(), spikeArrayIndex);
                        waveformArrayIndex = 0;
                        spikeArrayIndex = 0;
                        waveformFifo->freeOldData(WaveformFifo::ReaderTCP);
                    }
                }
                qApp->processEvents();
            }

            // Any 'finish up' code goes here.

            delete [] previousSample;
            running = false;
        } else {
            qApp->processEvents();
            usleep(1000);
        }
    }
}

void TCPDataOutputThread::updateEnabledChannels()
{
    // Always start with a clean slate
    channelNames = signalSources->completeChannelsNameList();

    enabledChannelNames.clear();
    enabledStimChannelNames.clear();

    posStimAmplitudes.resize(0);
    negStimAmplitudes.resize(0);

    totalEnabledBands = 0;
    numAuxChannels = 0;
    numVddChannels = 0;
    numAdcChannels = 0;
    numDacChannels = 0;
    numDigitalInChannels = 0;
    numDigitalOutChannels = 0;

    for (int i = 0; i < (int) channelNames.size(); ++i) {
        Channel* thisChannel = signalSources->channelByName(QString::fromStdString(channelNames[i]));
        QStringList thisChannelBands;
        switch (thisChannel->getSignalType()) {

        // Get totalEnabledBands (amplifierSignals)
        case AmplifierSignal:
            thisChannelBands = thisChannel->getTcpBandNames();
            if (thisChannelBands.size() > 0) enabledChannelNames.append(thisChannel->getNativeName());
            totalEnabledBands += thisChannelBands.size();

            // Get stim amplitudes for this channel
            if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
                if (thisChannel->getOutputToTcpStim()) {
                    enabledStimChannelNames.append(thisChannel->getNativeName());
                    double stimStepSizeuA = RHXRegisters::stimStepSizeToDouble(state->getStimStepSizeEnum()) * 1e6;
                    double firstPhaseAmplitudeuA = thisChannel->stimParameters->firstPhaseAmplitude->getValue();
                    double secondPhaseAmplitudeuA = thisChannel->stimParameters->secondPhaseAmplitude->getValue();
                    int firstPhaseAmplitude = round(firstPhaseAmplitudeuA / stimStepSizeuA);
                    firstPhaseAmplitude = qBound(0, firstPhaseAmplitude, 255);
                    int secondPhaseAmplitude = round(secondPhaseAmplitudeuA / stimStepSizeuA);
                    secondPhaseAmplitude = qBound(0, secondPhaseAmplitude, 255);
                    if (thisChannel->stimParameters->stimPolarity->getValue().toLower() == "negativefirst") {
                        negStimAmplitudes.push_back((uint8_t) firstPhaseAmplitude);
                        posStimAmplitudes.push_back((uint8_t) secondPhaseAmplitude);
                    } else {
                        posStimAmplitudes.push_back((uint8_t) firstPhaseAmplitude);
                        negStimAmplitudes.push_back((uint8_t) secondPhaseAmplitude);
                    }
                }
            }

            break;

        // Get numAuxChannels
        case AuxInputSignal:
            if (thisChannel->getOutputToTcp()) {
                enabledChannelNames.append(thisChannel->getNativeName());
                numAuxChannels += 1;
            }
            break;

        // Get numVddChannels
        case SupplyVoltageSignal:
            if (thisChannel->getOutputToTcp()) {
                enabledChannelNames.append(thisChannel->getNativeName());
                numVddChannels += 1;
            }
            break;

        // Get numAdcChannels
        case BoardAdcSignal:
            if (thisChannel->getOutputToTcp()) {
                enabledChannelNames.append(thisChannel->getNativeName());
                numAdcChannels += 1;
            }
            break;

        // Get numDacChannels
        case BoardDacSignal:
            if (thisChannel->getOutputToTcp()) {
                enabledChannelNames.append(thisChannel->getNativeName());
                numDacChannels += 1;
            }
            break;

        // Get numDigitalInChannels
        case BoardDigitalInSignal:
            if (thisChannel->getOutputToTcp()) {
                enabledChannelNames.append(thisChannel->getNativeName());
                numDigitalInChannels += 1;
            }
            break;

        case BoardDigitalOutSignal:
            if (thisChannel->getOutputToTcp()) {
                enabledChannelNames.append(thisChannel->getNativeName());
                numDigitalOutChannels += 1;
            }
            break;
        }
    }

    previousSample = new uint16_t[enabledChannelNames.size()];
    for (int i = 0; i < enabledChannelNames.size(); ++i) {
        previousSample[i] = 0;
    }

    digInWordPresent = 0;
    if (numDigitalInChannels > 0) {
        digInWordPresent = 1;
    }

    digOutWordPresent = 0;
    if (numDigitalOutChannels > 0) {
        digOutWordPresent = 1;
    }

    // Each frame has 4 bytes for timestamp, then 2 bytes per uint16 word.
    numBytesPerFrame = 4 + 2 * (totalEnabledBands + numAuxChannels + numVddChannels + numAdcChannels + numDacChannels + digInWordPresent + digOutWordPresent);
    // Each data block has 4 bytes for magic number, then 128 frames
    numBytesPerDataBlock = 4 + (FramesPerBlock * numBytesPerFrame);

    waveformArray.clear();
    waveformArray.resize(state->tcpNumDataBlocksWrite->getValue() * numBytesPerDataBlock);
    waveformArrayIndex = 0;

    // For each chunk of spike data, there are 4 bytes for magic number, 5 bytes for 5 characters of native channel name,
    // 4 bytes for timestamp, and 1 byte for spikeID.
    numBytesPerSpikeChunk = 4 + 5 + 4 + 1;
    // The absolute maximum # of chunks that could be sent is 4 per data block, for all amplifier signals
    // (actual numbers will likely be much less), but the spike array should be allocated this size.
    maxChunksPerDataBlock = 4 * signalSources->numAmplifierChannels();

    spikeArray.clear();
    spikeArray.resize(state->tcpNumDataBlocksWrite->getValue() * numBytesPerSpikeChunk * maxChunksPerDataBlock);
    spikeArrayIndex = 0;

    closeRequested = false;
    closeCompleted = false;
}

void TCPDataOutputThread::prepareToClose()
{
    closeRequested = true;
}

bool TCPDataOutputThread::isReadyToClose()
{
    return closeCompleted;
}

void TCPDataOutputThread::startRunning()
{
    keepGoing = true;
}

void TCPDataOutputThread::stopRunning()
{
    keepGoing = false;
}

void TCPDataOutputThread::closeInternal()
{
    tcpWaveformDataCommunicator->moveToThread(parentObject->thread());
    tcpSpikeDataCommunicator->moveToThread(parentObject->thread());
    keepGoing = false;
    stopThread = true;
}

void TCPDataOutputThread::closeExternal()
{
    keepGoing = false;
    stopThread = true;
}

bool TCPDataOutputThread::isActive() const
{
    return running;
}

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

#include <QElapsedTimer>
#include <iostream>
#include "rhxdatablock.h"
#include "softwarereferenceprocessor.h"
#include "rhxdatareader.h"
#include "signalsources.h"
#include "waveformprocessorthread.h"

WaveformProcessorThread::WaveformProcessorThread(SystemState* state_, int numDataStreams_, double sampleRate_,
                                                 DataStreamFifo *usbFifo_, WaveformFifo *waveformFifo_,
                                                 XPUController* xpuController_, QObject *parent) :
    QThread(parent),
    state(state_),
    signalSources(state_->signalSources),
    type(state_->getControllerTypeEnum()),
    sampleRate(sampleRate_),
    usbFifo(usbFifo_),
    waveformFifo(waveformFifo_),
    numDataStreams(numDataStreams_),
    xpuController(xpuController_),
    keepGoing(false),
    running(false),
    stopThread(false)
{
    cpuLoadHistory.resize(20, 0.0);
}

void WaveformProcessorThread::run()
{
    const int NumBlocks = 1;  // Note: GPU spike extraction works on single data blocks only; this value must be 1 if spikes are read.
    const int NumSamples = NumBlocks * RHXDataBlock::samplesPerDataBlock(type);
    uint16_t* usbData = nullptr;
    bool firstTime = true;
    bool softwareRefInfoUpdated = false;
    SoftwareReferenceProcessor swRefProcessor(type, numDataStreams, NumSamples);
    QElapsedTimer loopTimer, workTimer, reportTimer;

    while (!stopThread) {
        int numUsbWords = RHXDataBlock::dataBlockSizeInWords(type, numDataStreams);

        fill(cpuLoadHistory.begin(), cpuLoadHistory.end(), 0.0);

        if (keepGoing) {
            running = true;
            firstTime = true;
            softwareRefInfoUpdated = false;

            loopTimer.start();
            workTimer.start();
            reportTimer.start();

            xpuController->resetPrev();

            // Determine how many microseconds of data one block represents.
//            float oneBlockus = (numSamples / sampleRate) * 1e6;

            while (keepGoing && !stopThread) {
                // workTimer.restart();

                if (!softwareRefInfoUpdated) {
                    // Update software referencing information.
                    swRefProcessor.updateReferenceInfo(signalSources);
                    softwareRefInfoUpdated = true;
                }

                usbData = usbFifo->pointerToData(numUsbWords);  // Get pointer to new USB data, if available.
                if (usbData) {
                    if (state->getReportSpikes()) {
                        state->advanceSpikeTimer();
                    }
                    workTimer.restart();

                    // Perform any software referencing prior to filtering.
                    swRefProcessor.applySoftwareReferences(usbData);

                    // Check for space to write the waveform data.
                    while (!waveformFifo->requestWriteSpace(NumBlocks)) {
                        usleep(100);
                    }

                    // Get wide, low, and high pointers from WaveformFifo.
                    uint16_t* wide = waveformFifo->pointerToGpuWidebandWriteSpace();
                    uint16_t* low = waveformFifo->pointerToGpuLowpassWriteSpace();
                    uint16_t* high = waveformFifo->pointerToGpuHighpassWriteSpace();

                    uint32_t* spike = waveformFifo->pointerToGpuSpikeTimestampsWriteSpace();
                    uint8_t* spikeID = waveformFifo->pointerToGpuSpikeIdsWriteSpace();

                    // Process one data block through GPU, and write the results to WaveformFifo.
//                    auto start = chrono::steady_clock::now();

                    xpuController->processDataBlock(usbData, low, wide, high, spike, spikeID);
//                    auto end = chrono::steady_clock::now();

                    // Determine how long this processing took, and report if it's approaching real-time.
//                    float elapsedus = (float) chrono::duration_cast<chrono::microseconds>(end - start).count();
//                    float gpuAccel = oneBlockus / elapsedus;
//                    if (gpuAccel < 2.0)
//                        qDebug() << "Warning: GPU process time approaching real-time. Real-time data block length: " << oneBlockus << " us. Processing time: " << elapsedus << " us. GPU is " << gpuAccel << "x faster";

                    // Read and process waveform data from USB buffer, and write data to waveform FIFO.
                    RHXDataReader dataReader(type, numDataStreams, usbData, NumSamples);

                    float* analogWaveform = nullptr;
                    uint16_t* digitalWaveform = nullptr;

                    dataReader.readTimeStampData(waveformFifo->pointerToTimeStampWriteSpace());

                    QString spikingChannelNames("");

                    for (int group = 0; group < signalSources->numGroups(); group++) {
                        SignalGroup* signalGroup = signalSources->groupByIndex(group);
                        for (int signal = 0; signal < signalGroup->numChannels(); signal++) {
                            Channel* channel = signalGroup->channelByIndex(signal);
                            string waveName = channel->getNativeNameString();
                            if (channel->getSignalType() == AmplifierSignal) {
                                GpuWaveformAddress gpuWaveformAddress = waveformFifo->getGpuWaveformAddress(waveName + "|SPK");
                                digitalWaveform = waveformFifo->getDigitalWaveformPointer(waveName + "|SPK");
                                // Note: GPU spike extraction only works on single data blocks.
                                bool spikeFound = waveformFifo->extractGpuSpikeDataOneDataBlock(digitalWaveform, gpuWaveformAddress, firstTime);

                                if (state->getReportSpikes()) {
                                    if (spikeFound) {
                                        spikingChannelNames.append(QString::fromStdString(waveName) + ",");
                                        //state->spikeReport(QString::fromStdString(waveName));
                                    }
                                    // Find if a spike happened on this channel. If so, get its name
                                    // If there's a name, send the spikeReport() signal.
                                    // This signal should be ultimately received by ProbeMapWindow, which will then internally handle the time decay
                                }

                                if (signalSources->getControllerType() == ControllerStimRecordUSB2) {
                                    // Load DC amplifier data and stimulation markers.
                                    analogWaveform = waveformFifo->getAnalogWaveformPointer(waveName + "|DC");
                                    dataReader.readDcAmplifierData(waveformFifo->pointerToAnalogWriteSpace(analogWaveform),
                                                                   channel->getBoardStream(), channel->getChipChannel());
                                    digitalWaveform = waveformFifo->getDigitalWaveformPointer(waveName + "|STIM");
                                    dataReader.readStimParamData(waveformFifo->pointerToDigitalWriteSpace(digitalWaveform),
                                                              channel->getBoardStream(), channel->getChipChannel());
                                }
                            } else if (channel->getSignalType() == AuxInputSignal) {
                                analogWaveform = waveformFifo->getAnalogWaveformPointer(waveName);
                                dataReader.readAuxInData(waveformFifo->pointerToAnalogWriteSpace(analogWaveform),
                                                         channel->getBoardStream(), channel->getChipChannel());
                            } else if (channel->getSignalType() == SupplyVoltageSignal) {
                                analogWaveform = waveformFifo->getAnalogWaveformPointer(waveName);
                                dataReader.readSupplyVoltageData(waveformFifo->pointerToAnalogWriteSpace(analogWaveform),
                                                                 channel->getBoardStream());
                            } else if (channel->getSignalType() == BoardAdcSignal) {
                                analogWaveform = waveformFifo->getAnalogWaveformPointer(waveName);
                                dataReader.readBoardAdcData(waveformFifo->pointerToAnalogWriteSpace(analogWaveform),
                                                            channel->getNativeChannelNumber());
                            } else if (channel->getSignalType() == BoardDacSignal) {
                                analogWaveform = waveformFifo->getAnalogWaveformPointer(waveName);
                                dataReader.readBoardDacData(waveformFifo->pointerToAnalogWriteSpace(analogWaveform),
                                                            channel->getNativeChannelNumber());
                            } else if (channel->getSignalType() == BoardDigitalInSignal) {
                                analogWaveform = waveformFifo->getAnalogWaveformPointer(waveName);
                                dataReader.readDigInData(waveformFifo->pointerToAnalogWriteSpace(analogWaveform),
                                                         channel->getNativeChannelNumber());
                            } else if (channel->getSignalType() == BoardDigitalOutSignal) {
                                analogWaveform = waveformFifo->getAnalogWaveformPointer(waveName);
                                dataReader.readDigOutData(waveformFifo->pointerToAnalogWriteSpace(analogWaveform),
                                                         channel->getNativeChannelNumber());
                            }
                        }
                    }

                    if (state->getReportSpikes()) {
                        state->spikeReport(spikingChannelNames);
                    }

                    digitalWaveform = waveformFifo->getDigitalWaveformPointer("DIGITAL-IN-WORD");
                    dataReader.readDigInData(waveformFifo->pointerToDigitalWriteSpace(digitalWaveform));
                    digitalWaveform = waveformFifo->getDigitalWaveformPointer("DIGITAL-OUT-WORD");
                    dataReader.readDigOutData(waveformFifo->pointerToDigitalWriteSpace(digitalWaveform));

                    // Done reading and processing all waveforms.
                    waveformFifo->commitNewData();  // Commit waveform data we have just written.
                    usbFifo->freeData();  // Free raw data we just read from the USB buffer.

                    firstTime = false;

                    double workTime = (double) workTimer.nsecsElapsed();
                    double loopTime = (double) loopTimer.nsecsElapsed();

                    if (reportTimer.elapsed() >= 50) {
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

//                        cout << "                   WaveformProcessorThread CPU usage: " << (int) averageCpuLoad << "%" << EndOfLine;
//                        cout << "USB FIFO " << (int) usbFifo->percentFull() << "% full.  ";
//                        cout << "Waveform FIFO " << (int) waveformFifo->percentFull() << "% full." << EndOfLine;
                        reportTimer.restart();
                    }
                    workTimer.restart();
                    loopTimer.restart();
                } else {
                    usleep(100);    // Wait 100 microseconds.
                }
            }
            running = false;

            fill(cpuLoadHistory.begin(), cpuLoadHistory.end(), 0.0);
            emit cpuLoadPercent(0.0);
        } else {
            usleep(100);
        }
    }
}

void WaveformProcessorThread::startRunning(int numDataStreams_)
{
    numDataStreams = numDataStreams_;
    keepGoing = true;
}

void WaveformProcessorThread::stopRunning()
{
    keepGoing = false;
}

void WaveformProcessorThread::close()
{
    keepGoing = false;
    stopThread = true;
}

bool WaveformProcessorThread::isActive() const
{
    return running;
}

//  ------------------------------------------------------------------------
//
//  This file is part of the Intan Technologies RHD2000 Interface
//  Version 1.3
//  Copyright (C) 2013 Intan Technologies
//
//  ------------------------------------------------------------------------
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <QVector>
#include <QFile>
#include <QDataStream>
#include <queue>
#include <qmath.h>
#include <iostream>

#include "qtincludes.h"

#include "mainwindow.h"
#include "signalprocessor.h"
#include "globalconstants.h"
#include "randomnumber.h"
#include "signalsources.h"
#include "signalgroup.h"
#include "signalchannel.h"
#include "rhd2000datablock.h"

using namespace std;

// This class stores and processes short segments of waveform data
// acquired from the USB interface board.  The primary purpose of the
// class is to read from a queue of Rhd2000DataBlock objects and scale
// this raw data appropriately to generate wavefrom vectors with units
// of volts or microvolts.
//
// The class is also capable of applying a notch filter to amplifier
// waveform data, measuring the amplitude of a particular frequency
// component (useful in the electrode impedance measurements), and
// generating synthetic neural or ECG data for demonstration purposes.

// Constructor.
SignalProcessor::SignalProcessor()
{
    // Notch filter initial parameters.
    notchFilterEnabled = false;
    a1 = 0.0;
    a2 = 0.0;
    b0 = 0.0;
    b1 = 0.0;
    b2 = 0.0;

    // Highpass filter initial parameters.
    highpassFilterEnabled = false;
    aHpf = 0.0;
    bHpf = 0.0;

    // Set up random number generator in case we are asked to generate synthetic data.
    random = new RandomNumber();
    random->setGaussianAccuracy(6);

    // Other synthetic waveform variables.
    tPulse = 0.0;
    synthTimeStamp = 0;
}

// Allocate memory to store waveform data.
void SignalProcessor::allocateMemory(int numStreams)
{
    numDataStreams = numStreams;

    // The maximum number of Rhd2000DataBlock objects we will need is set by the need
    // to perform electrode impedance measurements at very low frequencies.
    const int maxNumBlocks = 120;

    // Allocate vector memory for waveforms from USB interface board and notch filter.
    allocateDoubleArray3D(amplifierPreFilter, numStreams, 32, SAMPLES_PER_DATA_BLOCK * maxNumBlocks);
    allocateDoubleArray3D(amplifierPostFilter, numStreams, 32, SAMPLES_PER_DATA_BLOCK * maxNumBlocks);
    allocateDoubleArray2D(highpassFilterState, numStreams, 32);
    allocateDoubleArray3D(prevAmplifierPreFilter, numStreams, 32, 2);
    allocateDoubleArray3D(prevAmplifierPostFilter, numStreams, 32, 2);
    allocateDoubleArray3D(auxChannel, numStreams, 3, (SAMPLES_PER_DATA_BLOCK / 4) * maxNumBlocks);
    allocateDoubleArray2D(supplyVoltage, numStreams, maxNumBlocks);
    allocateDoubleArray1D(tempRaw, numStreams);
    allocateDoubleArray1D(tempAvg, numStreams);
    allocateDoubleArray2D(boardAdc, 8, SAMPLES_PER_DATA_BLOCK * maxNumBlocks);
    allocateIntArray2D(boardDigIn, 16, SAMPLES_PER_DATA_BLOCK * maxNumBlocks);
    allocateIntArray2D(boardDigOut, 16, SAMPLES_PER_DATA_BLOCK * maxNumBlocks);

    // Initialize vector memory used in notch filter state.
    fillZerosDoubleArray3D(amplifierPostFilter);
    fillZerosDoubleArray3D(prevAmplifierPreFilter);
    fillZerosDoubleArray3D(prevAmplifierPostFilter);
    fillZerosDoubleArray2D(highpassFilterState);

    // Allocate vector memory for generating synthetic neural and ECG waveforms.
    allocateDoubleArray3D(synthSpikeAmplitude, numStreams, 32, 2);
    allocateDoubleArray3D(synthSpikeDuration, numStreams, 32, 2);
    allocateDoubleArray2D(synthRelativeSpikeRate, numStreams, 32);
    allocateDoubleArray2D(synthEcgAmplitude, numStreams, 32);

    // Assign random parameters for synthetic waveforms.
    for (int stream = 0; stream < numStreams; ++stream) {
        for (int channel = 0; channel < 32; ++channel) {
            synthEcgAmplitude[stream][channel] =
                    random->randomUniform(0.5, 3.0);
            for (int spikeNum = 0; spikeNum < 2; ++spikeNum) {
                synthSpikeAmplitude[stream][channel][spikeNum] =
                        random->randomUniform(-400.0, 100.0);
                synthSpikeDuration[stream][channel][spikeNum] =
                        random->randomUniform(0.3, 1.7);
                synthRelativeSpikeRate[stream][channel] =
                        random->randomUniform(0.1, 5.0);
            }
        }
    }

    // Initialize vector for averaging temperature readings over time.
    allocateDoubleArray2D(tempRawHistory, numStreams, maxNumBlocks);
    tempHistoryReset(4);
}

// Allocates memory for a 3-D array of doubles.
void SignalProcessor::allocateDoubleArray3D(QVector<QVector<QVector<double> > > &array3D,
                                            int xSize, int ySize, int zSize)
{
    int i, j;

    if (xSize == 0) return;
    array3D.resize(xSize);
    for (i = 0; i < xSize; ++i) {
        array3D[i].resize(ySize);
        for (j = 0; j < ySize; ++j) {
            array3D[i][j].resize(zSize);
        }
    }
}

// Allocates memory for a 2-D array of doubles.
void SignalProcessor::allocateDoubleArray2D(QVector<QVector<double> > &array2D,
                                            int xSize, int ySize)
{
    int i;

    if (xSize == 0) return;
    array2D.resize(xSize);
    for (i = 0; i < xSize; ++i) {
        array2D[i].resize(ySize);
    }
}

// Allocates memory for a 2-D array of integers.
void SignalProcessor::allocateIntArray2D(QVector<QVector<int> > &array2D,
                                         int xSize, int ySize)
{
    int i;

    array2D.resize(xSize);
    for (i = 0; i < xSize; ++i) {
        array2D[i].resize(ySize);
    }
}

// Allocates memory for a 1-D array of doubles.
void SignalProcessor::allocateDoubleArray1D(QVector<double> &array1D,
                                            int xSize)
{
    array1D.resize(xSize);
}

// Fill a 3-D array of doubles with zero.
void SignalProcessor::fillZerosDoubleArray3D(
        QVector<QVector<QVector<double> > > &array3D)
{
    int x, y;
    int xSize = array3D.size();

    if (xSize == 0) return;

    int ySize = array3D[0].size();

    for (x = 0; x < xSize; ++x) {
        for (y = 0; y < ySize; ++y) {
            array3D[x][y].fill(0.0);
        }
    }
}

// Fill a 2-D array of doubles with zero.
void SignalProcessor::fillZerosDoubleArray2D(
        QVector<QVector<double> > &array2D)
{
    int x;
    int xSize = array2D.size();

    if (xSize == 0) return;

    for (x = 0; x < xSize; ++x) {
        array2D[x].fill(0.0);
    }
}

// Creates lists (vectors, actually) of all enabled waveforms to expedite
// save-to-disk operations.
void SignalProcessor::createSaveList(SignalSources *signalSources, bool addTriggerChannel, int triggerChannel)
{
    int port, index;
    SignalChannel *currentChannel;

    synthTimeStamp = 0;  // for synthetic data mode

    saveListAmplifier.clear();
    saveListAuxInput.clear();
    saveListSupplyVoltage.clear();
    saveListBoardAdc.clear();
    saveListBoardDigitalIn.clear();
    saveListBoardDigitalOut.clear();
    saveListTempSensor.clear();

    saveListBoardDigIn = false;

    for (port = 0; port < signalSources->signalPort.size(); ++port) {
        for (index = 0; index < signalSources->signalPort[port].numChannels(); ++index) {
            currentChannel = signalSources->signalPort[port].channelByNativeOrder(index);
            // Maybe add this channel if it is the trigger channel.
            if (addTriggerChannel) {
                if (triggerChannel > 15 && currentChannel->signalType == BoardAdcSignal) {
                    if (currentChannel->nativeChannelNumber == triggerChannel - 16) {
                        currentChannel->enabled = true;
                    }
                } else if (triggerChannel < 16 && currentChannel->signalType == BoardDigInSignal) {
                    if (currentChannel->nativeChannelNumber == triggerChannel) {
                        currentChannel->enabled = true;
                    }
                }
            }
            // Add all enabled channels to their appropriate save list.
            if (currentChannel->enabled) {
                switch (currentChannel->signalType) {
                case AmplifierSignal:
                    saveListAmplifier.append(currentChannel);
                    break;
                case AuxInputSignal:
                    saveListAuxInput.append(currentChannel);
                    break;
                case SupplyVoltageSignal:
                    saveListSupplyVoltage.append(currentChannel);
                    break;
                case BoardAdcSignal:
                    saveListBoardAdc.append(currentChannel);
                    break;
                case BoardDigInSignal:
                    saveListBoardDigIn = true;
                    saveListBoardDigitalIn.append(currentChannel);
                    break;
                case BoardDigOutSignal:
                    saveListBoardDigitalOut.append(currentChannel);
                }
            }
            // Use supply voltage signal as a proxy for the presence of a temperature sensor,
            // since these always appear together on each chip.  Add all temperature sensors
            // list, whether or not the corresponding supply voltage signals are enabled.
            if (currentChannel->signalType == SupplyVoltageSignal) {
                saveListTempSensor.append(currentChannel);
            }
        }
    }
}

// Create filename (appended to the specified path) for timestamp data.
void SignalProcessor::createTimestampFilename(QString path)
{
    timestampFileName = path + "/" + "time" + ".dat";
}

// Create filename (appended to the specified path) for data files in
// "One File Per Signal Type" format.
void SignalProcessor::createSignalTypeFilenames(QString path)
{
    amplifierFileName = path + "/" + "amplifier" + ".dat";
    auxInputFileName = path + "/" + "auxiliary" + ".dat";
    supplyFileName = path + "/" + "supply" + ".dat";
    adcInputFileName = path + "/" + "analogin" + ".dat";
    digitalInputFileName = path + "/" + "digitalin" + ".dat";
    digitalOutputFileName = path + "/" + "digitalout" + ".dat";
}

// Open timestamp save file.
void SignalProcessor::openTimestampFile()
{
    timestampFile = new QFile(timestampFileName);

    if (!timestampFile->open(QIODevice::WriteOnly)) {
        cerr << "Cannot open file for writing: " <<
                qPrintable(timestampFile->errorString()) << endl;
    }

    timestampStream = new QDataStream(timestampFile);
    timestampStream->setVersion(QDataStream::Qt_4_8);

    // Set to little endian mode for compatibilty with MATLAB,
    // which is little endian on all platforms
    timestampStream->setByteOrder(QDataStream::LittleEndian);

    // Write 4-byte floating-point numbers (instead of the default 8-byte numbers)
    // to save disk space.
    timestampStream->setFloatingPointPrecision(QDataStream::SinglePrecision);
}

// Open data files for "One File Per Signal Type" format.
void SignalProcessor::openSignalTypeFiles(bool saveTtlOut)
{
    amplifierFile = 0;
    auxInputFile = 0;
    supplyFile = 0;
    adcInputFile = 0;
    digitalInputFile = 0;
    digitalOutputFile = 0;

    amplifierStream = 0;
    auxInputStream = 0;
    supplyStream = 0;
    adcInputStream = 0;
    digitalInputStream = 0;
    digitalOutputStream = 0;

    if (saveListAmplifier.size() > 0) {
        amplifierFile = new QFile(amplifierFileName);
        if (!amplifierFile->open(QIODevice::WriteOnly))
            cerr << "Cannot open file for writing: " <<
                    qPrintable(amplifierFile->errorString()) << endl;
        amplifierStream = new QDataStream(amplifierFile);
        amplifierStream->setVersion(QDataStream::Qt_4_8);
        amplifierStream->setByteOrder(QDataStream::LittleEndian);
        amplifierStream->setFloatingPointPrecision(QDataStream::SinglePrecision);
    }
    if (saveListAuxInput.size() > 0) {
        auxInputFile = new QFile(auxInputFileName);
        if (!auxInputFile->open(QIODevice::WriteOnly))
            cerr << "Cannot open file for writing: " <<
                    qPrintable(auxInputFile->errorString()) << endl;
        auxInputStream = new QDataStream(auxInputFile);
        auxInputStream->setVersion(QDataStream::Qt_4_8);
        auxInputStream->setByteOrder(QDataStream::LittleEndian);
        auxInputStream->setFloatingPointPrecision(QDataStream::SinglePrecision);
    }
    if (saveListSupplyVoltage.size() > 0) {
        supplyFile = new QFile(supplyFileName);
        if (!supplyFile->open(QIODevice::WriteOnly))
            cerr << "Cannot open file for writing: " <<
                    qPrintable(supplyFile->errorString()) << endl;
        supplyStream = new QDataStream(supplyFile);
        supplyStream->setVersion(QDataStream::Qt_4_8);
        supplyStream->setByteOrder(QDataStream::LittleEndian);
        supplyStream->setFloatingPointPrecision(QDataStream::SinglePrecision);
    }
    if (saveListBoardAdc.size() > 0) {
        adcInputFile = new QFile(adcInputFileName);
        if (!adcInputFile->open(QIODevice::WriteOnly))
            cerr << "Cannot open file for writing: " <<
                    qPrintable(adcInputFile->errorString()) << endl;
        adcInputStream = new QDataStream(adcInputFile);
        adcInputStream->setVersion(QDataStream::Qt_4_8);
        adcInputStream->setByteOrder(QDataStream::LittleEndian);
        adcInputStream->setFloatingPointPrecision(QDataStream::SinglePrecision);
    }
    if (saveListBoardDigitalIn.size() > 0) {
        digitalInputFile = new QFile(digitalInputFileName);
        if (!digitalInputFile->open(QIODevice::WriteOnly))
            cerr << "Cannot open file for writing: " <<
                    qPrintable(digitalInputFile->errorString()) << endl;
        digitalInputStream = new QDataStream(digitalInputFile);
        digitalInputStream->setVersion(QDataStream::Qt_4_8);
        digitalInputStream->setByteOrder(QDataStream::LittleEndian);
        digitalInputStream->setFloatingPointPrecision(QDataStream::SinglePrecision);
    }
    if (saveTtlOut) {
        digitalOutputFile = new QFile(digitalOutputFileName);
        if (!digitalOutputFile->open(QIODevice::WriteOnly))
            cerr << "Cannot open file for writing: " <<
                    qPrintable(digitalOutputFile->errorString()) << endl;
        digitalOutputStream = new QDataStream(digitalOutputFile);
        digitalOutputStream->setVersion(QDataStream::Qt_4_8);
        digitalOutputStream->setByteOrder(QDataStream::LittleEndian);
        digitalOutputStream->setFloatingPointPrecision(QDataStream::SinglePrecision);
    }
}

// Close timestamp save file.
void SignalProcessor::closeTimestampFile()
{
    timestampFile->close();
    delete timestampStream;
    delete timestampFile;
}

// Close data files for "One File Per Signal Type" format.
void SignalProcessor::closeSignalTypeFiles()
{
    if (amplifierFile) {
        amplifierFile->close();
        delete amplifierStream;
        delete amplifierFile;
    }
    if (auxInputFile) {
        auxInputFile->close();
        delete auxInputStream;
        delete auxInputFile;
    }
    if (supplyFile) {
        supplyFile->close();
        delete supplyStream;
        delete supplyFile;
    }
    if (adcInputFile) {
        adcInputFile->close();
        delete adcInputStream;
        delete adcInputFile;
    }
    if (digitalInputFile) {
        digitalInputFile->close();
        delete digitalInputStream;
        delete digitalInputFile;
    }
    if (digitalOutputFile) {
        digitalOutputFile->close();
        delete digitalOutputStream;
        delete digitalOutputFile;
    }
}

// Create filenames (appended to the specified path) for each waveform.
void SignalProcessor::createFilenames(SignalSources *signalSources, QString path)
{
    int port, index;
    SignalChannel *currentChannel;

    for (port = 0; port < signalSources->signalPort.size(); ++port) {
        for (index = 0; index < signalSources->signalPort[port].numChannels(); ++index) {
            currentChannel = signalSources->signalPort[port].channelByNativeOrder(index);
            // Only create filenames for enabled channels.
            if (currentChannel->enabled) {
                switch (currentChannel->signalType) {
                case AmplifierSignal:
                    currentChannel->saveFileName =
                            path + "/" + "amp-" + currentChannel->nativeChannelName + ".dat";
                    break;
                case AuxInputSignal:
                    currentChannel->saveFileName =
                            path + "/" + "aux-" + currentChannel->nativeChannelName + ".dat";
                    break;
                case SupplyVoltageSignal:
                    currentChannel->saveFileName =
                            path + "/" + "vdd-" + currentChannel->nativeChannelName + ".dat";
                    break;
                case BoardAdcSignal:
                    currentChannel->saveFileName =
                            path + "/" + "board-" + currentChannel->nativeChannelName + ".dat";
                    break;
                case BoardDigInSignal:
                    currentChannel->saveFileName =
                            path + "/" + "board-" + currentChannel->nativeChannelName + ".dat";
                    break;
                case BoardDigOutSignal:
                    currentChannel->saveFileName =
                            path + "/" + "board-" + currentChannel->nativeChannelName + ".dat";
                }
            }
        }
    }
}

// Open individual save data files for all enabled waveforms.
void SignalProcessor::openSaveFiles(SignalSources *signalSources)
{
    int port, index;
    SignalChannel *currentChannel;

    for (port = 0; port < signalSources->signalPort.size(); ++port) {
        for (index = 0; index < signalSources->signalPort[port].numChannels(); ++index) {
            currentChannel = signalSources->signalPort[port].channelByNativeOrder(index);
            // Only open files for enabled channels.
            if (currentChannel->enabled) {
                currentChannel->saveFile = new QFile(currentChannel->saveFileName);

                if (!currentChannel->saveFile->open(QIODevice::WriteOnly)) {
                    cerr << "Cannot open file for writing: " <<
                            qPrintable(currentChannel->saveFile->errorString()) << endl;
                }

                currentChannel->saveStream = new QDataStream(currentChannel->saveFile);
                currentChannel->saveStream->setVersion(QDataStream::Qt_4_8);

                // Set to little endian mode for compatibilty with MATLAB,
                // which is little endian on all platforms
                currentChannel->saveStream->setByteOrder(QDataStream::LittleEndian);

                // Write 4-byte floating-point numbers (instead of the default 8-byte numbers)
                // to save disk space.
                currentChannel->saveStream->setFloatingPointPrecision(QDataStream::SinglePrecision);
            }
        }
    }
}

// Close individual save data files for all enabled waveforms.
void SignalProcessor::closeSaveFiles(SignalSources *signalSources)
{
    int port, index;
    SignalChannel *currentChannel;

    for (port = 0; port < signalSources->signalPort.size(); ++port) {
        for (index = 0; index < signalSources->signalPort[port].numChannels(); ++index) {
            currentChannel = signalSources->signalPort[port].channelByNativeOrder(index);
            // Only close files for enabled channels.
            if (currentChannel->enabled) {
                currentChannel->saveFile->close();
                delete currentChannel->saveStream;
                delete currentChannel->saveFile;
            }
        }
    }
}

// Reads numBlocks blocks of raw USB data stored in a queue of Rhd2000DataBlock
// objects, loads this data into this SignalProcessor object, scaling the raw
// data to generate waveforms with units of volts or microvolts.
//
// If lookForTrigger is true, this function looks for a trigger on digital input
// triggerChannel with triggerPolarity.  If trigger is found, triggerTimeIndex
// returns timestamp of trigger point.  Otherwise, triggerTimeIndex returns -1,
// indicating no trigger was found.
//
// If saveToDisk is true, a disk-format binary datastream is written to QDataStream out.
// If saveTemp is true, temperature readings are also saved.  A timestampOffset can be
// used to reference the trigger point to zero.
//
// Returns number of bytes written to binary datastream out if saveToDisk == true.
int SignalProcessor::loadAmplifierData(queue<Rhd2000DataBlock> &dataQueue,
                                       int numBlocks, bool lookForTrigger, int triggerChannel,
                                       int triggerPolarity, int &triggerTimeIndex, bool addToBuffer, queue<Rhd2000DataBlock> &bufferQueue,
                                       bool saveToDisk, QDataStream &out, SaveFormat format, bool saveTemp,
                                       bool saveTtlOut, int timestampOffset)
{
    int block, t, channel, stream, i, j;
    int indexAmp = 0;
    int indexAux = 0;
    int indexSupply = 0;
    int indexAdc = 0;
    int indexDig = 0;
    int numWordsWritten = 0;

    int bufferIndex;
    qint16 tempQint16;
    quint16 tempQuint16;
    qint32 tempQint32;

    bool triggerFound = false;
    const double AnalogTriggerThreshold = 1.65;

    for (i = 0; i < saveListAmplifier.size(); ++i) {
        bufferArrayIndex[i] = 0;
    }

    if (lookForTrigger) {
        triggerTimeIndex = -1;
    }

    for (block = 0; block < numBlocks; ++block) {

        // Load and scale RHD2000 amplifier waveforms
        // (sampled at amplifier sampling rate)
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            for (channel = 0; channel < 32; ++channel) {
                for (stream = 0; stream < numDataStreams; ++stream) {
                    // Amplifier waveform units = microvolts
                    amplifierPreFilter[stream][channel][indexAmp] = 0.195 *
                            (dataQueue.front().amplifierData[stream][channel][t] - 32768);
                }
            }
            ++indexAmp;
        }

        // Load and scale RHD2000 auxiliary input waveforms
        // (sampled at 1/4 amplifier sampling rate)
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; t += 4) {
            for (stream = 0; stream < numDataStreams; ++stream) {
                // Auxiliary input waveform units = volts
                auxChannel[stream][0][indexAux] =
                        0.0000374 * dataQueue.front().auxiliaryData[stream][1][t + 1];
                auxChannel[stream][1][indexAux] =
                        0.0000374 * dataQueue.front().auxiliaryData[stream][1][t + 2];
                auxChannel[stream][2][indexAux] =
                        0.0000374 * dataQueue.front().auxiliaryData[stream][1][t + 3];
            }
            ++indexAux;
        }

        // Load and scale RHD2000 supply voltage and temperature sensor waveforms
        // (sampled at 1/60 amplifier sampling rate)
        for (stream = 0; stream < numDataStreams; ++stream) {
            // Supply voltage waveform units = volts
            supplyVoltage[stream][indexSupply] =
                    0.0000748 * dataQueue.front().auxiliaryData[stream][1][28];
            // Temperature sensor waveform units = degrees C
            tempRaw[stream] =
                    (dataQueue.front().auxiliaryData[stream][1][20] -
                     dataQueue.front().auxiliaryData[stream][1][12]) / 98.9 - 273.15;
        }
        ++indexSupply;

        // Average multiple temperature readings to improve accuracy
        tempHistoryPush(tempRaw);
        tempHistoryCalcAvg();

        // Load and scale USB interface board ADC waveforms
        // (sampled at amplifier sampling rate)
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            for (channel = 0; channel < 8; ++channel) {
                // ADC waveform units = volts
                boardAdc[channel][indexAdc] =
                        0.000050354 * dataQueue.front().boardAdcData[channel][t];
            }
            if (lookForTrigger && !triggerFound && triggerChannel >= 16) {
                if (triggerPolarity) {
                    // Trigger on logic low
                    if (boardAdc[triggerChannel - 16][indexAdc] < AnalogTriggerThreshold) {
                        triggerTimeIndex = dataQueue.front().timeStamp[t];
                        triggerFound = true;
                    }
                } else {
                    // Trigger on logic high
                    if (boardAdc[triggerChannel - 16][indexAdc] >= AnalogTriggerThreshold) {
                        triggerTimeIndex = dataQueue.front().timeStamp[t];
                        triggerFound = true;
                    }
                }
            }
            ++indexAdc;
        }

        // Load USB interface board digital input and output waveforms
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            for (channel = 0; channel < 16; ++channel) {
                boardDigIn[channel][indexDig] =
                        (dataQueue.front().ttlIn[t] & (1 << channel)) != 0;
                boardDigOut[channel][indexDig] =
                        (dataQueue.front().ttlOut[t] & (1 << channel)) != 0;
                }
            if (lookForTrigger && !triggerFound && triggerChannel < 16) {
                if (triggerPolarity) {
                    // Trigger on logic low
                    if (boardDigIn[triggerChannel][indexDig] == 0) {
                        triggerTimeIndex = dataQueue.front().timeStamp[t];
                        triggerFound = true;
                    }
                } else {
                    // Trigger on logic high
                    if (boardDigIn[triggerChannel][indexDig] == 1) {
                        triggerTimeIndex = dataQueue.front().timeStamp[t];
                        triggerFound = true;
                    }
                }
            }
            ++indexDig;
        }

        // Optionally send binary data to binary output stream
        if (saveToDisk) {
            switch (format) {
            case SaveFormatIntan:
                // Save timestamp data
                bufferIndex = 0;
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    tempQint32 = ((qint32) dataQueue.front().timeStamp[t]) - ((qint32) timestampOffset);
                    dataStreamBuffer[bufferIndex++] = tempQint32 & 0x000000ff;          // Save qint 32 in little-endian format
                    dataStreamBuffer[bufferIndex++] = (tempQint32 & 0x0000ff00) >> 8;
                    dataStreamBuffer[bufferIndex++] = (tempQint32 & 0x00ff0000) >> 16;
                    dataStreamBuffer[bufferIndex++] = (tempQint32 & 0xff000000) >> 24;
                }
                out.writeRawData(dataStreamBuffer, bufferIndex);     // Stream out all data at once to speed writing
                numWordsWritten += 2 * SAMPLES_PER_DATA_BLOCK;

                // Save amplifier data
                bufferIndex = 0;
                for (i = 0; i < saveListAmplifier.size(); ++i) {
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                        tempQuint16 = (quint16)
                            dataQueue.front().amplifierData[saveListAmplifier.at(i)->boardStream][saveListAmplifier.at(i)->chipChannel][t];
                        dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                        dataStreamBuffer[bufferIndex++] = (tempQuint16 & 0xff00) >> 8;  // (MSByte last)
                    }
                }
                out.writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                numWordsWritten += saveListAmplifier.size() * SAMPLES_PER_DATA_BLOCK;

                // Save auxiliary input data
                bufferIndex = 0;
                for (i = 0; i < saveListAuxInput.size(); ++i) {
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; t += 4) {
                        tempQuint16 = (quint16)
                            dataQueue.front().auxiliaryData[saveListAuxInput.at(i)->boardStream][1][t + saveListAuxInput.at(i)->chipChannel + 1];
                        dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                        dataStreamBuffer[bufferIndex++] = (tempQuint16 & 0xff00) >> 8;  // (MSByte last)
                    }
                }
                out.writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                numWordsWritten += saveListAuxInput.size() * SAMPLES_PER_DATA_BLOCK;

                // Save supply voltage data
                for (i = 0; i < saveListSupplyVoltage.size(); ++i) {
                    out << (quint16)
                           dataQueue.front().auxiliaryData[saveListSupplyVoltage.at(i)->boardStream][1][28];
                    ++numWordsWritten;
                }

                // Save temperature sensor data if saveTemp == true
                // Save as temperature in degrees C, multiplied by 100 and rounded to the nearest
                // signed integer.
                if (saveTemp) {
                    for (i = 0; i < saveListTempSensor.size(); ++i) {
                        out << (qint16) (100.0 * tempAvg[saveListTempSensor.at(i)->boardStream]);
                        ++numWordsWritten;
                    }
                }

                // Save board ADC data
                bufferIndex = 0;
                for (i = 0; i < saveListBoardAdc.size(); ++i) {
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                        tempQuint16 = (quint16)
                            dataQueue.front().boardAdcData[saveListBoardAdc.at(i)->nativeChannelNumber][t];
                        dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                        dataStreamBuffer[bufferIndex++] = (tempQuint16 & 0xff00) >> 8;  // (MSByte last)
                    }
                }
                out.writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                numWordsWritten += saveListBoardAdc.size() * SAMPLES_PER_DATA_BLOCK;

                // Save board digital input data
                if (saveListBoardDigIn) {
                    // If ANY digital inputs are enabled, we save ALL 16 channels, since
                    // we are writing 16-bit chunks of data.
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                        out << (quint16) dataQueue.front().ttlIn[t];
                        ++numWordsWritten;
                    }
                }

                // Save board digital output data, if saveTtlOut = true
                if (saveTtlOut) {
                    // Save all 16 channels, since we are writing 16-bit chunks of data.
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                        out << (quint16) dataQueue.front().ttlOut[t];
                        ++numWordsWritten;
                    }
                }

                break;

            case SaveFormatFilePerSignalType:
                int tAux;
                // Save timestamp data
                bufferIndex = 0;
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    tempQint32 = ((qint32) dataQueue.front().timeStamp[t]) - ((qint32) timestampOffset);
                    dataStreamBuffer[bufferIndex++] = tempQint32 & 0x000000ff;          // Save qint 32 in little-endian format
                    dataStreamBuffer[bufferIndex++] = (tempQint32 & 0x0000ff00) >> 8;
                    dataStreamBuffer[bufferIndex++] = (tempQint32 & 0x00ff0000) >> 16;
                    dataStreamBuffer[bufferIndex++] = (tempQint32 & 0xff000000) >> 24;
                }
                timestampStream->writeRawData(dataStreamBuffer, bufferIndex);     // Stream out all data at once to speed writing
                numWordsWritten += 2 * SAMPLES_PER_DATA_BLOCK;

                // Save amplifier data
                bufferIndex = 0;
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    for (i = 0; i < saveListAmplifier.size(); ++i) {
                        tempQint16 = (qint16)
                            (dataQueue.front().amplifierData[saveListAmplifier.at(i)->boardStream][saveListAmplifier.at(i)->chipChannel][t] - 32768);
                        dataStreamBuffer[bufferIndex++] = tempQint16 & 0x00ff;         // Save qint16 in little-endian format (LSByte first)
                        dataStreamBuffer[bufferIndex++] = (tempQint16 & 0xff00) >> 8;  // (MSByte last)
                    }
                }
                if (bufferIndex > 0) {
                    amplifierStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                    numWordsWritten += saveListAmplifier.size() * SAMPLES_PER_DATA_BLOCK;
                }

                // Save auxiliary input data
                bufferIndex = 0;
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    tAux = 4 * qFloor((double) t / 4.0);
                    for (i = 0; i < saveListAuxInput.size(); ++i) {
                        tempQuint16 = (quint16)
                            dataQueue.front().auxiliaryData[saveListAuxInput.at(i)->boardStream][1][tAux + saveListAuxInput.at(i)->chipChannel + 1];
                        dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                        dataStreamBuffer[bufferIndex++] = (tempQuint16 & 0xff00) >> 8;  // (MSByte last)
                    }
                }
                if (bufferIndex > 0) {
                    auxInputStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                    numWordsWritten += saveListAuxInput.size() * SAMPLES_PER_DATA_BLOCK;
                }

                // Save supply voltage data
                bufferIndex = 0;
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    for (i = 0; i < saveListSupplyVoltage.size(); ++i) {
                        tempQuint16 = (quint16)
                            dataQueue.front().auxiliaryData[saveListSupplyVoltage.at(i)->boardStream][1][28];
                        dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                        dataStreamBuffer[bufferIndex++] = (tempQuint16 & 0xff00) >> 8;  // (MSByte last)
                    }
                }
                if (bufferIndex > 0) {
                    supplyStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                    numWordsWritten += saveListSupplyVoltage.size() * SAMPLES_PER_DATA_BLOCK;
                }

                // Not saving temperature data in this save format.

                // Save board ADC data
                bufferIndex = 0;
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    for (i = 0; i < saveListBoardAdc.size(); ++i) {
                        tempQuint16 = (quint16)
                            dataQueue.front().boardAdcData[saveListBoardAdc.at(i)->nativeChannelNumber][t];
                        dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                        dataStreamBuffer[bufferIndex++] = (tempQuint16 & 0xff00) >> 8;  // (MSByte last)
                    }
                }
                if (bufferIndex > 0) {
                    adcInputStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                    numWordsWritten += saveListBoardAdc.size() * SAMPLES_PER_DATA_BLOCK;
                }

                // Save board digital input data
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    if (saveListBoardDigIn) {
                        // If ANY digital inputs are enabled, we save ALL 16 channels, since
                        // we are writing 16-bit chunks of data.
                        *(digitalInputStream) << (quint16) dataQueue.front().ttlIn[t];
                        ++numWordsWritten;
                    }
                }

                // Save board digital output data, if saveTtlOut = true
                if (saveTtlOut) {
                    // Save all 16 channels, since we are writing 16-bit chunks of data.
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                        *(digitalOutputStream) << (quint16) dataQueue.front().ttlOut[t];
                        ++numWordsWritten;
                    }
                }

                break;

            case SaveFormatFilePerChannel:
                // Save timestamp data
                bufferIndex = 0;
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    tempQint32 = ((qint32) dataQueue.front().timeStamp[t]) - ((qint32) timestampOffset);
                    dataStreamBuffer[bufferIndex++] = tempQint32 & 0x000000ff;          // Save qint 32 in little-endian format
                    dataStreamBuffer[bufferIndex++] = (tempQint32 & 0x0000ff00) >> 8;
                    dataStreamBuffer[bufferIndex++] = (tempQint32 & 0x00ff0000) >> 16;
                    dataStreamBuffer[bufferIndex++] = (tempQint32 & 0xff000000) >> 24;
                }
                timestampStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                numWordsWritten += 2 * SAMPLES_PER_DATA_BLOCK;

                // Save amplifier data to dataStreamBufferArray; In in effort to increase write speed we will
                // collect amplifier data from all data blocks and then write all data at the end of this function.
                for (i = 0; i < saveListAmplifier.size(); ++i) {
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                        tempQint16 = (qint16)
                            (dataQueue.front().amplifierData[saveListAmplifier.at(i)->boardStream][saveListAmplifier.at(i)->chipChannel][t] - 32768);
                        dataStreamBufferArray[i][bufferArrayIndex[i]++] = tempQint16 & 0x00ff;         // Save qint16 in little-endian format (LSByte first)
                        dataStreamBufferArray[i][bufferArrayIndex[i]++] = (tempQint16 & 0xff00) >> 8;  // (MSByte last)
                    }
                    numWordsWritten += SAMPLES_PER_DATA_BLOCK;
                }

                // Save auxiliary input data
                for (i = 0; i < saveListAuxInput.size(); ++i) {
                    bufferIndex = 0;
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; t += 4) {
                        for (j = 0; j < 4; ++j) {   // Aux data is sampled at 1/4 amplifier sampling rate; write each sample 4 times
                            tempQuint16 = (quint16)
                                dataQueue.front().auxiliaryData[saveListAuxInput.at(i)->boardStream][1][t + saveListAuxInput.at(i)->chipChannel + 1];
                            dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                            dataStreamBuffer[bufferIndex++] = (tempQuint16 & 0xff00) >> 8;  // (MSByte last)
                        }
                    }
                    saveListAuxInput.at(i)->saveStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                    numWordsWritten += SAMPLES_PER_DATA_BLOCK;
                }

                // Save supply voltage data
                for (i = 0; i < saveListSupplyVoltage.size(); ++i) {
                    bufferIndex = 0;
                    for (j = 0; j < SAMPLES_PER_DATA_BLOCK; ++j) {   // Vdd data is sampled at 1/60 amplifier sampling rate; write each sample 60 times
                        tempQuint16 = (quint16)
                            dataQueue.front().auxiliaryData[saveListSupplyVoltage.at(i)->boardStream][1][28];
                        dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                        dataStreamBuffer[bufferIndex++] = (tempQuint16 & 0xff00) >> 8;  // (MSByte last)
                    }
                    saveListSupplyVoltage.at(i)->saveStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                    numWordsWritten += SAMPLES_PER_DATA_BLOCK;
                }

                // Not saving temperature data in this save format.

                // Save board ADC data
                for (i = 0; i < saveListBoardAdc.size(); ++i) {
                    bufferIndex = 0;
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                        tempQuint16 = (quint16)
                            dataQueue.front().boardAdcData[saveListBoardAdc.at(i)->nativeChannelNumber][t];
                        dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                        dataStreamBuffer[bufferIndex++] = (tempQuint16 & 0xff00) >> 8;  // (MSByte last)
                    }
                    saveListBoardAdc.at(i)->saveStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                    numWordsWritten += SAMPLES_PER_DATA_BLOCK;
                }

                // Save board digital input data
                for (i = 0; i < saveListBoardDigitalIn.size(); ++i) {
                    bufferIndex = 0;
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                        tempQuint16 = (quint16)
                            ((dataQueue.front().ttlIn[t] & (1 << saveListBoardDigitalIn.at(i)->nativeChannelNumber)) != 0);
                        dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                    dataStreamBuffer[bufferIndex++] = 0;  // (MSB of individual digital input will always be zero)
                    }
                    saveListBoardDigitalIn.at(i)->saveStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                    numWordsWritten += SAMPLES_PER_DATA_BLOCK;
                }

                // Save board digital output data, if saveTtlOut = true
                if (saveTtlOut) {
                    for (i = 0; i < 16; ++i) {
                        bufferIndex = 0;
                        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                            tempQuint16 = (quint16)
                                ((dataQueue.front().ttlOut[t] & (1 << i)) != 0);
                            dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                        dataStreamBuffer[bufferIndex++] = 0;  // (MSB of individual digital input will always be zero)
                        }
                        saveListBoardDigitalOut.at(i)->saveStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                        numWordsWritten += SAMPLES_PER_DATA_BLOCK;
                    }
                }

                break;
            }
        }

        if (addToBuffer) {
            bufferQueue.push(dataQueue.front());
        }
        // We are done with this Rhd2000DataBlock object; remove it from dataQueue
        dataQueue.pop();
    }

    // If we are operating on the "One File Per Channel" format, we have saved all amplifier data from
    // multiple data blocks in dataStreamBufferArray.  Now we write it all at once, for each channel.
    if (saveToDisk && format == SaveFormatFilePerChannel) {
        for (i = 0; i < saveListAmplifier.size(); ++i) {
            saveListAmplifier.at(i)->saveStream->writeRawData(dataStreamBufferArray[i], bufferArrayIndex[i]);    // Stream out all amplifier data at once to speed writing
        }
    }

    // Return total number of bytes written to binary output stream
    return (2 * numWordsWritten);
}

// Save to entire contents of the buffer queue to disk, and empty the queue in the process.
// Returns number of bytes written to binary datastream out.
int SignalProcessor::saveBufferedData(queue<Rhd2000DataBlock> &bufferQueue, QDataStream &out, SaveFormat format,
                                      bool saveTemp, bool saveTtlOut, int timestampOffset)
{
    int t, i, j, stream;
    int numWordsWritten = 0;

    int bufferIndex;
    qint16 tempQint16;
    quint16 tempQuint16;
    qint32 tempQint32;

    switch (format) {
    case SaveFormatIntan:
        while (bufferQueue.empty() == false) {
            // Save timestamp data
            bufferIndex = 0;
            for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                tempQint32 = ((qint32) bufferQueue.front().timeStamp[t]) - ((qint32) timestampOffset);
                dataStreamBuffer[bufferIndex++] = tempQint32 & 0x000000ff;          // Save qint 32 in little-endian format
                dataStreamBuffer[bufferIndex++] = (tempQint32 & 0x0000ff00) >> 8;
                dataStreamBuffer[bufferIndex++] = (tempQint32 & 0x00ff0000) >> 16;
                dataStreamBuffer[bufferIndex++] = (tempQint32 & 0xff000000) >> 24;
            }
            out.writeRawData(dataStreamBuffer, bufferIndex);     // Stream out all data at once to speed writing
            numWordsWritten += 2 * SAMPLES_PER_DATA_BLOCK;

            // Save amplifier data
            bufferIndex = 0;
            for (i = 0; i < saveListAmplifier.size(); ++i) {
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    tempQuint16 = (quint16)
                        bufferQueue.front().amplifierData[saveListAmplifier.at(i)->boardStream][saveListAmplifier.at(i)->chipChannel][t];
                    dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                    dataStreamBuffer[bufferIndex++] = (tempQuint16 & 0xff00) >> 8;  // (MSByte last)
                }
            }
            out.writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
            numWordsWritten += saveListAmplifier.size() * SAMPLES_PER_DATA_BLOCK;

            // Save auxiliary input data
            bufferIndex = 0;
            for (i = 0; i < saveListAuxInput.size(); ++i) {
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; t += 4) {
                    tempQuint16 = (quint16)
                        bufferQueue.front().auxiliaryData[saveListAuxInput.at(i)->boardStream][1][t + saveListAuxInput.at(i)->chipChannel + 1];
                    dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                    dataStreamBuffer[bufferIndex++] = (tempQuint16 & 0xff00) >> 8;  // (MSByte last)
                }
            }
            out.writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
            numWordsWritten += saveListAuxInput.size() * SAMPLES_PER_DATA_BLOCK;

            // Save supply voltage data
            for (i = 0; i < saveListSupplyVoltage.size(); ++i) {
                out << (quint16)
                       bufferQueue.front().auxiliaryData[saveListSupplyVoltage.at(i)->boardStream][1][28];
                ++numWordsWritten;
            }

            // Save temperature sensor data if saveTemp == true
            // Save as temperature in degrees C, multiplied by 100 and rounded to the nearest
            // signed integer.
            if (saveTemp) {
                // Load and scale RHD2000 supply voltage and temperature sensor waveforms
                // (sampled at 1/60 amplifier sampling rate)

                for (stream = 0; stream < numDataStreams; ++stream) {
                    // Temperature sensor waveform units = degrees C
                    tempRaw[stream] =
                            (bufferQueue.front().auxiliaryData[stream][1][20] -
                             bufferQueue.front().auxiliaryData[stream][1][12]) / 98.9 - 273.15;
                }

                // Average multiple temperature readings to improve accuracy
                tempHistoryPush(tempRaw);
                tempHistoryCalcAvg();

                for (i = 0; i < saveListTempSensor.size(); ++i) {
                    out << (qint16) (100.0 * tempAvg[saveListTempSensor.at(i)->boardStream]);
                    ++numWordsWritten;
                }
            }

            // Save board ADC data
            bufferIndex = 0;
            for (i = 0; i < saveListBoardAdc.size(); ++i) {
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    tempQuint16 = (quint16)
                        bufferQueue.front().boardAdcData[saveListBoardAdc.at(i)->nativeChannelNumber][t];
                    dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                    dataStreamBuffer[bufferIndex++] = (tempQuint16 & 0xff00) >> 8;  // (MSByte last)
                }
            }
            out.writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
            numWordsWritten += saveListBoardAdc.size() * SAMPLES_PER_DATA_BLOCK;

            // Save board digital input data
            if (saveListBoardDigIn) {
                // If ANY digital inputs are enabled, we save ALL 16 channels, since
                // we are writing 16-bit chunks of data.
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    out << (quint16) bufferQueue.front().ttlIn[t];
                    ++numWordsWritten;
                }
            }

            // Save board digital output data, is saveTtlOut = true
            if (saveTtlOut) {
                // We save all 16 channels, since we are writing 16-bit chunks of data.
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    out << (quint16) bufferQueue.front().ttlOut[t];
                    ++numWordsWritten;
                }
            }
            // We are done with this Rhd2000DataBlock object; remove it from bufferQueue
            bufferQueue.pop();
        }
        break;
    case SaveFormatFilePerSignalType:
        while (bufferQueue.empty() == false) {
            int tAux;

            // Save timestamp data
            bufferIndex = 0;
            for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                tempQint32 = ((qint32) bufferQueue.front().timeStamp[t]) - ((qint32) timestampOffset);
                dataStreamBuffer[bufferIndex++] = tempQint32 & 0x000000ff;          // Save qint 32 in little-endian format
                dataStreamBuffer[bufferIndex++] = (tempQint32 & 0x0000ff00) >> 8;
                dataStreamBuffer[bufferIndex++] = (tempQint32 & 0x00ff0000) >> 16;
                dataStreamBuffer[bufferIndex++] = (tempQint32 & 0xff000000) >> 24;
            }
            timestampStream->writeRawData(dataStreamBuffer, bufferIndex);     // Stream out all data at once to speed writing
            numWordsWritten += 2 * SAMPLES_PER_DATA_BLOCK;

            // Save amplifier data
            bufferIndex = 0;
            for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                for (i = 0; i < saveListAmplifier.size(); ++i) {
                    tempQint16 = (qint16)
                        (bufferQueue.front().amplifierData[saveListAmplifier.at(i)->boardStream][saveListAmplifier.at(i)->chipChannel][t] - 32768);
                    dataStreamBuffer[bufferIndex++] = tempQint16 & 0x00ff;         // Save qint16 in little-endian format (LSByte first)
                    dataStreamBuffer[bufferIndex++] = (tempQint16 & 0xff00) >> 8;  // (MSByte last)
                }
            }
            if (bufferIndex > 0) {
                amplifierStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                numWordsWritten += saveListAmplifier.size() * SAMPLES_PER_DATA_BLOCK;
            }

            // Save auxiliary input data
            bufferIndex = 0;
            for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                tAux = 4 * qFloor((double) t / 4.0);
                for (i = 0; i < saveListAuxInput.size(); ++i) {
                    tempQuint16 = (quint16)
                        bufferQueue.front().auxiliaryData[saveListAuxInput.at(i)->boardStream][1][tAux + saveListAuxInput.at(i)->chipChannel + 1];
                    dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                    dataStreamBuffer[bufferIndex++] = (tempQuint16 & 0xff00) >> 8;  // (MSByte last)
                }
            }
            if (bufferIndex > 0) {
                auxInputStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                numWordsWritten += saveListAuxInput.size() * SAMPLES_PER_DATA_BLOCK;
            }

            // Save supply voltage data
            bufferIndex = 0;
            for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                for (i = 0; i < saveListSupplyVoltage.size(); ++i) {
                    tempQuint16 = (quint16)
                        bufferQueue.front().auxiliaryData[saveListSupplyVoltage.at(i)->boardStream][1][28];
                    dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                    dataStreamBuffer[bufferIndex++] = (tempQuint16 & 0xff00) >> 8;  // (MSByte last)
                }
            }
            if (bufferIndex > 0) {
                supplyStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                numWordsWritten += saveListSupplyVoltage.size() * SAMPLES_PER_DATA_BLOCK;
            }

            // Not saving temperature data in this save format.

            // Save board ADC data
            bufferIndex = 0;
            for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                for (i = 0; i < saveListBoardAdc.size(); ++i) {
                    tempQuint16 = (quint16)
                        bufferQueue.front().boardAdcData[saveListBoardAdc.at(i)->nativeChannelNumber][t];
                    dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                    dataStreamBuffer[bufferIndex++] = (tempQuint16 & 0xff00) >> 8;  // (MSByte last)
                }
            }
            if (bufferIndex > 0) {
                adcInputStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                numWordsWritten += saveListBoardAdc.size() * SAMPLES_PER_DATA_BLOCK;
            }

            // Save board digital input data
            if (saveListBoardDigIn) {
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    // If ANY digital inputs are enabled, we save ALL 16 channels, since
                    // we are writing 16-bit chunks of data.
                    *(digitalInputStream) << (quint16) bufferQueue.front().ttlIn[t];
                    ++numWordsWritten;
                }
            }

            // Save board digital output data, if saveTtlOut = true
            if (saveTtlOut) {
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    // We save all 16 channels, since we are writing 16-bit chunks of data.
                    *(digitalOutputStream) << (quint16) bufferQueue.front().ttlOut[t];
                    ++numWordsWritten;
                }
            }

            // We are done with this Rhd2000DataBlock object; remove it from bufferQueue
            bufferQueue.pop();
        }
        break;

    case SaveFormatFilePerChannel:
        while (bufferQueue.empty() == false) {
            // Save timestamp data
            bufferIndex = 0;
            for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                tempQint32 = ((qint32) bufferQueue.front().timeStamp[t]) - ((qint32) timestampOffset);
                dataStreamBuffer[bufferIndex++] = tempQint32 & 0x000000ff;          // Save qint 32 in little-endian format
                dataStreamBuffer[bufferIndex++] = (tempQint32 & 0x0000ff00) >> 8;
                dataStreamBuffer[bufferIndex++] = (tempQint32 & 0x00ff0000) >> 16;
                dataStreamBuffer[bufferIndex++] = (tempQint32 & 0xff000000) >> 24;
            }
            timestampStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
            numWordsWritten += 2 * SAMPLES_PER_DATA_BLOCK;

            // Save amplifier data
            for (i = 0; i < saveListAmplifier.size(); ++i) {
                bufferIndex = 0;
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    tempQint16 = (qint16)
                        (bufferQueue.front().amplifierData[saveListAmplifier.at(i)->boardStream][saveListAmplifier.at(i)->chipChannel][t] - 32768);
                    dataStreamBuffer[bufferIndex++] = tempQint16 & 0x00ff;         // Save qint16 in little-endian format (LSByte first)
                    dataStreamBuffer[bufferIndex++] = (tempQint16 & 0xff00) >> 8;  // (MSByte last)
                }
                saveListAmplifier.at(i)->saveStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                numWordsWritten += SAMPLES_PER_DATA_BLOCK;
            }

            // Save auxiliary input data
            for (i = 0; i < saveListAuxInput.size(); ++i) {
                bufferIndex = 0;
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; t += 4) {
                    for (j = 0; j < 4; ++j) {   // Aux data is sampled at 1/4 amplifier sampling rate; write each sample 4 times
                        tempQuint16 = (quint16)
                            bufferQueue.front().auxiliaryData[saveListAuxInput.at(i)->boardStream][1][t + saveListAuxInput.at(i)->chipChannel + 1];
                        dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                        dataStreamBuffer[bufferIndex++] = (tempQuint16 & 0xff00) >> 8;  // (MSByte last)
                    }
                }
                saveListAuxInput.at(i)->saveStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                numWordsWritten += SAMPLES_PER_DATA_BLOCK;
            }

            // Save supply voltage data
            for (i = 0; i < saveListSupplyVoltage.size(); ++i) {
                bufferIndex = 0;
                for (j = 0; j < SAMPLES_PER_DATA_BLOCK; ++j) {   // Vdd data is sampled at 1/60 amplifier sampling rate; write each sample 60 times
                    tempQuint16 = (quint16)
                        bufferQueue.front().auxiliaryData[saveListSupplyVoltage.at(i)->boardStream][1][28];
                    dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                    dataStreamBuffer[bufferIndex++] = (tempQuint16 & 0xff00) >> 8;  // (MSByte last)
                }
                saveListSupplyVoltage.at(i)->saveStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                numWordsWritten += SAMPLES_PER_DATA_BLOCK;
            }

            // Not saving temperature data in this save format.

            // Save board ADC data
            for (i = 0; i < saveListBoardAdc.size(); ++i) {
                bufferIndex = 0;
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    tempQuint16 = (quint16)
                        bufferQueue.front().boardAdcData[saveListBoardAdc.at(i)->nativeChannelNumber][t];
                    dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                    dataStreamBuffer[bufferIndex++] = (tempQuint16 & 0xff00) >> 8;  // (MSByte last)
                }
                saveListBoardAdc.at(i)->saveStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                numWordsWritten += SAMPLES_PER_DATA_BLOCK;
            }

            // Save board digital input data
            for (i = 0; i < saveListBoardDigitalIn.size(); ++i) {
                bufferIndex = 0;
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    tempQuint16 = (quint16)
                        ((bufferQueue.front().ttlIn[t] & (1 << saveListBoardDigitalIn.at(i)->nativeChannelNumber)) != 0);
                    dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                    dataStreamBuffer[bufferIndex++] = 0;  // (MSB of individual digital input will always be zero)
                }
                saveListBoardDigitalIn.at(i)->saveStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                numWordsWritten += SAMPLES_PER_DATA_BLOCK;
            }

            // Save board digital output data, if saveTtlOut = true
            if (saveTtlOut) {
                for (i = 0; i < 16; ++i) {
                    bufferIndex = 0;
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                        tempQuint16 = (quint16)
                            ((bufferQueue.front().ttlOut[t] & (1 << i)) != 0);
                        dataStreamBuffer[bufferIndex++] = tempQuint16 & 0x00ff;         // Save quint16 in little-endian format (LSByte first)
                        dataStreamBuffer[bufferIndex++] = 0;  // (MSB of individual digital input will always be zero)
                    }
                    saveListBoardDigitalOut.at(i)->saveStream->writeRawData(dataStreamBuffer, bufferIndex);    // Stream out all data at once to speed writing
                    numWordsWritten += SAMPLES_PER_DATA_BLOCK;
                }
            }

            // We are done with this Rhd2000DataBlock object; remove it from bufferQueue
            bufferQueue.pop();
        }
        break;
    }

    // Return total number of bytes written to binary output stream
    return (2 * numWordsWritten);
}

// This function behaves similarly to loadAmplifierData, but generates
// synthetic neural or ECG data for demonstration purposes when there is
// no USB interface board present.
// Returns number of bytes written to binary datastream out if saveToDisk == true.
int SignalProcessor::loadSyntheticData(int numBlocks, double sampleRate,
                                       bool saveToDisk, QDataStream &out,
                                       SaveFormat format, bool saveTemp, bool saveTtlOut)
{
    int block, t, tAux, channel, stream, i, j;
    int indexAux = 0;
    int indexSupply = 0;
    int indexAdc = 0;
    int indexDig = 0;
    int numWordsWritten = 0;

    double tStepMsec = 1000.0 / sampleRate;
    double spikeDelay, ecgValue;
    bool spikePresent;
    int spikeNum;

    // If the sample rate is 5 kS/s or higher, generate sythetic neural data;
    // otherwise, generate synthetic ECG data.
    if (sampleRate > 4999.9) {
        // Generate synthetic neural data.
        for (block = 0; block < numBlocks; ++block) {
            for (stream = 0; stream < numDataStreams; ++stream) {
                for (channel = 0; channel < 32; ++channel) {
                    spikePresent = false;
                    spikeNum = 0;
                    if (random->randomUniform() < synthRelativeSpikeRate.at(stream).at(channel) * tStepMsec) {
                        spikePresent = true;
                        spikeDelay = random->randomUniform(0.0, 0.3);  // add some random time jitter
                        if (random->randomUniform() < 0.3) spikeNum = 1;  // choose between one of two spike types
                    }
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                        // Create realistic background Gaussian noise of 2.4 uVrms (would be more in cortex)
                        amplifierPreFilter[stream][channel][SAMPLES_PER_DATA_BLOCK * block + t] =
                                2.4 * random->randomGaussian();
                        if (spikePresent) {
                            // Create synthetic spike
                            if (t * tStepMsec > spikeDelay &&
                                t * tStepMsec < synthSpikeDuration[stream][channel][spikeNum] + spikeDelay) {
                                amplifierPreFilter[stream][channel][SAMPLES_PER_DATA_BLOCK * block + t] +=
                                        synthSpikeAmplitude.at(stream).at(channel).at(spikeNum) *
                                        qExp(-2.0 * (t * tStepMsec - spikeDelay)) *
                                        qSin(TWO_PI * (t * tStepMsec - spikeDelay) /
                                             synthSpikeDuration.at(stream).at(channel).at(spikeNum));
                            }
                        }
                    }
                }
            }
        }
    } else {
        // Generate synthetic ECG data.
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK * numBlocks; ++t) {
            // Piece together half sine waves to model QRS complex, P wave, and T wave
            if (tPulse < 80.0) {
                ecgValue = 40.0 * qSin(TWO_PI * tPulse / 160.0); // P wave
            } else if (tPulse > 100.0 && tPulse < 120.0) {
                ecgValue = -250.0 * qSin(TWO_PI * (tPulse - 100.0) / 40.0); // Q
            } else if (tPulse > 120.0 && tPulse < 180.0) {
                ecgValue = 1000.0 * qSin(TWO_PI * (tPulse - 120.0) / 120.0); // R
            } else if (tPulse > 180.0 && tPulse < 260.0) {
                ecgValue = -120.0 * qSin(TWO_PI * (tPulse - 180.0) / 160.0); // S
            } else if (tPulse > 340.0 && tPulse < 400.0) {
                ecgValue = 60.0 * qSin(TWO_PI * (tPulse - 340.0) / 120.0); // T wave
            } else {
                ecgValue = 0.0;
            }
            for (stream = 0; stream < numDataStreams; ++stream) {
                for (channel = 0; channel < 32; ++channel) {
                    // Multiply basic ECG waveform by channel-specific amplitude, and
                    // add 2.4 uVrms noise.
                    amplifierPreFilter[stream][channel][t] =
                            synthEcgAmplitude[stream][channel] * ecgValue +
                            2.4 * random->randomGaussian();
                }
            }
            tPulse += tStepMsec;
        }
    }

    // Repeat ECG waveform with regular period.
    if (tPulse > 840.0) tPulse = 0.0;

    for (block = 0; block < numBlocks; ++block) {
        // Generate synthetic auxiliary input data.
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; t += 4) {
            for (stream = 0; stream < numDataStreams; ++stream) {
                // Just use DC values.
                auxChannel[stream][0][indexAux] = 0.5;
                auxChannel[stream][1][indexAux] = 1.0;
                auxChannel[stream][2][indexAux] = 2.0;
            }
            ++indexAux;
        }
        // Generate synthetic supply voltage and temperature data.
        for (stream = 0; stream < numDataStreams; ++stream) {
            supplyVoltage[stream][indexSupply] = 3.3;
            tempRaw[stream] = 25.0;
        }
        ++indexSupply;

        // Average multiple temperature readings to improve accuracy
        tempHistoryPush(tempRaw);
        tempHistoryCalcAvg();

        // Generate synthetic USB interface board ADC data.
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            for (channel = 0; channel < 8; ++channel) {
                boardAdc[channel][indexAdc] = 0.0;
            }
            ++indexAdc;
        }

        // Generate synthetic USB interface board digital I/O data.
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
            for (channel = 0; channel < 16; ++channel) {
                boardDigIn[channel][indexDig] = 0;
                boardDigOut[channel][indexDig] = 0;
            }
            ++indexDig;
        }
    }

    // Optionally send binary data to binary output stream
    if (saveToDisk) {
        switch (format) {
        case SaveFormatIntan:
            for (block = 0; block < numBlocks; ++block) {
                // Save timestamp data
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    out << (qint32) (synthTimeStamp++);
                    numWordsWritten += 2;
                }
                // Save amplifier data
                for (i = 0; i < saveListAmplifier.size(); ++i) {
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                        out << (quint16)
                               ((amplifierPreFilter[saveListAmplifier.at(i)->boardStream][saveListAmplifier.at(i)->chipChannel][60 * block + t] / 0.195) + 32768);
                        ++numWordsWritten;
                    }
                }
                // Save auxiliary input data
                for (i = 0; i < saveListAuxInput.size(); ++i) {
                    for (t = 0; t < (SAMPLES_PER_DATA_BLOCK / 4); ++t) {
                        out << (quint16)
                               (auxChannel[saveListAuxInput.at(i)->boardStream][saveListAuxInput.at(i)->chipChannel][15 * block + t] / 0.0000374);
                        ++numWordsWritten;
                    }
                }
                // Save supply voltage data
                for (i = 0; i < saveListSupplyVoltage.size(); ++i) {
                    out << (quint16) (supplyVoltage[saveListSupplyVoltage.at(i)->boardStream][block] / 0.0000748);
                    ++numWordsWritten;
                }
                // Save temperature sensor data if saveTemp == true
                // Save as temperature in degrees C, multiplied by 100 and rounded to the nearest
                // signed integer.
                if (saveTemp) {
                    for (i = 0; i < saveListTempSensor.size(); ++i) {
                        out << (qint16) (100.0 * tempAvg[saveListTempSensor.at(i)->boardStream]);
                        ++numWordsWritten;
                    }
                }
                // Save board ADC data
                for (i = 0; i < saveListBoardAdc.size(); ++i) {
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                        out << (quint16) 0;
                        ++numWordsWritten;
                    }
                }
                // Save board digital input data
                if (saveListBoardDigIn) {
                    // If ANY digital inputs are enabled, we save ALL 16 channels, since
                    // we are writing 16-bit chunks of data.
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                        out << (quint16) 0;
                        ++numWordsWritten;
                    }
                }
                // Save board digital output data
                if (saveTtlOut) {
                    // We save all 16 channels, since we are writing 16-bit chunks of data.
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                        out << (quint16) 0;
                        ++numWordsWritten;
                    }
                }
            }

            break;

        case SaveFormatFilePerSignalType:
            for (block = 0; block < numBlocks; ++block) {
                // Save timestamp data
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    *(timestampStream) << (qint32) (synthTimeStamp++);
                    numWordsWritten += 2;
                }

                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    // Save amplifier data
                    for (i = 0; i < saveListAmplifier.size(); ++i) {
                        *(amplifierStream) << (qint16)
                               (amplifierPreFilter[saveListAmplifier.at(i)->boardStream][saveListAmplifier.at(i)->chipChannel][60 * block + t] / 0.195);
                        ++numWordsWritten;
                    }
                    // Save auxiliary input data
                    tAux = qFloor((double) t / 4.0);
                    for (i = 0; i < saveListAuxInput.size(); ++i) {
                        *(auxInputStream) << (quint16)
                            (auxChannel[saveListAuxInput.at(i)->boardStream][saveListAuxInput.at(i)->chipChannel][15 * block + tAux] / 0.0000374);
                        ++numWordsWritten;
                    }
                    // Save supply voltage data
                    for (i = 0; i < saveListSupplyVoltage.size(); ++i) {
                        *(supplyStream) << (quint16) (supplyVoltage[saveListSupplyVoltage.at(i)->boardStream][block] / 0.0000748);
                        ++numWordsWritten;
                    }

                    // Not saving temperature data in this save format.

                    // Save board ADC data
                    for (i = 0; i < saveListBoardAdc.size(); ++i) {
                        *(adcInputStream) << (quint16) 0;
                        ++numWordsWritten;
                    }
                    // Save board digital input data
                    if (saveListBoardDigIn) {
                        *(digitalInputStream) << (quint16) 0;
                        ++numWordsWritten;
                    }
                    // Save board digital output data
                    if (saveTtlOut) {
                        *(digitalOutputStream) << (quint16) 0;
                        ++numWordsWritten;
                    }
                }
            }
            break;

        case SaveFormatFilePerChannel:
            for (block = 0; block < numBlocks; ++block) {
                // Save timestamp data
                for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                    *(timestampStream) << (qint32) (synthTimeStamp++);
                    numWordsWritten += 2;
                }

                // Save amplifier data
                for (i = 0; i < saveListAmplifier.size(); ++i) {
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                        *(saveListAmplifier.at(i)->saveStream) << (qint16)
                            (amplifierPreFilter[saveListAmplifier.at(i)->boardStream][saveListAmplifier.at(i)->chipChannel][60 * block + t] / 0.195);
                        ++numWordsWritten;
                    }
                }
                // Save auxiliary input data
                for (i = 0; i < saveListAuxInput.size(); ++i) {
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK / 4; ++t) {
                        for (j = 0; j < 4; ++j) {   // Aux data is sampled at 1/4 amplifier sampling rate; write each sample 4 times
                            *(saveListAuxInput.at(i)->saveStream) << (quint16)
                                (auxChannel[saveListAuxInput.at(i)->boardStream][saveListAuxInput.at(i)->chipChannel][15 * block + t] / 0.0000374);
                            ++numWordsWritten;
                        }
                    }
                }
                // Save supply voltage data
                for (i = 0; i < saveListSupplyVoltage.size(); ++i) {
                    for (j = 0; j < SAMPLES_PER_DATA_BLOCK; ++j) {   // Vdd data is sampled at 1/60 amplifier sampling rate; write each sample 60 times
                        *(saveListSupplyVoltage.at(i)->saveStream) << (quint16)
                            (supplyVoltage[saveListSupplyVoltage.at(i)->boardStream][block] / 0.0000748);
                        ++numWordsWritten;
                    }
                }
                // Not saving temperature data in this save format.

                // Save board ADC data
                for (i = 0; i < saveListBoardAdc.size(); ++i) {
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                        *(saveListBoardAdc.at(i)->saveStream) << (quint16) 0;
                        ++numWordsWritten;
                    }
                }
                // Save board digital input data
                for (i = 0; i < saveListBoardDigitalIn.size(); ++i) {
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                        *(saveListBoardDigitalIn.at(i)->saveStream) << (quint16) 0;
                        ++numWordsWritten;
                    }
                }
                // Save board digital output data
                for (i = 0; i < 16; ++i) {
                    for (t = 0; t < SAMPLES_PER_DATA_BLOCK; ++t) {
                        *(saveListBoardDigitalOut.at(i)->saveStream) << (quint16) 0;
                        ++numWordsWritten;
                    }
                }
            }
            break;
        }
    }

    // Return total number of bytes written to binary output stream
    return (2 * numWordsWritten);
}

// Returns the total number of bytes saved to disk per data block.
int SignalProcessor::bytesPerBlock(SaveFormat saveFormat, bool saveTemperature, bool saveTtlOut)
{
    int bytes = 0;
    bytes += 4 * SAMPLES_PER_DATA_BLOCK;  // timestamps
    bytes += 2 * SAMPLES_PER_DATA_BLOCK * saveListAmplifier.size();
    if (saveFormat == SaveFormatIntan) {
        bytes += 2 * (SAMPLES_PER_DATA_BLOCK / 4) * saveListAuxInput.size();
        bytes += 2 * (SAMPLES_PER_DATA_BLOCK / 60) * saveListSupplyVoltage.size();
        if (saveTemperature) {
            bytes += 2 * (SAMPLES_PER_DATA_BLOCK / 60) * saveListSupplyVoltage.size();
        }
    } else {
        bytes += 2 * SAMPLES_PER_DATA_BLOCK * saveListAuxInput.size();
        bytes += 2 * SAMPLES_PER_DATA_BLOCK * saveListSupplyVoltage.size();
    }
    bytes += 2 * SAMPLES_PER_DATA_BLOCK * saveListBoardAdc.size();
    if (saveFormat == SaveFormatIntan || saveFormat == SaveFormatFilePerSignalType) {
        if (saveListBoardDigIn) {
            bytes += 2 * SAMPLES_PER_DATA_BLOCK;
        }
    } else {
        bytes += 2 * SAMPLES_PER_DATA_BLOCK * saveListBoardDigitalIn.size();
    }
    if (saveTtlOut) {
        if (saveFormat == SaveFormatIntan || saveFormat == SaveFormatFilePerSignalType) {
            bytes += 2 * SAMPLES_PER_DATA_BLOCK;
        } else {
            bytes += 2 * SAMPLES_PER_DATA_BLOCK * 16;
        }
    }
    return bytes;
}

// Set notch filter parameters.  All filter parameters are given in Hz (or
// in Samples/s).  A bandwidth of 10 Hz is recommended for 50 or 60 Hz notch
// filters.  Narrower bandwidths will produce extended ringing in the time
// domain in response to large transients.
void SignalProcessor::setNotchFilter(double notchFreq, double bandwidth,
                                     double sampleFreq)
{
    double d;

    d = exp(-PI * bandwidth / sampleFreq);

    // Calculate biquad IIR filter coefficients.
    a1 = -(1.0 + d * d) * cos(2.0 * PI * notchFreq / sampleFreq);
    a2 = d * d;
    b0 = (1 + d * d) / 2.0;
    b1 = a1;
    b2 = b0;
}

// Enables or disables amplifier waveform notch filter.
void SignalProcessor::setNotchFilterEnabled(bool enable)
{
    notchFilterEnabled = enable;
}

// Set highpass filter parameters.  All filter parameters are given in Hz (or
// in Samples/s).
void SignalProcessor::setHighpassFilter(double cutoffFreq, double sampleFreq)
{
    aHpf = exp(-1.0 * TWO_PI * cutoffFreq / sampleFreq);
    bHpf = 1.0 - aHpf;
}

// Enables or disables amplifier waveform highpass filter.
void SignalProcessor::setHighpassFilterEnabled(bool enable)
{
    highpassFilterEnabled = enable;
}

// Runs notch filter only on amplifier channels that are visible
// on the display (according to channelVisible).
void SignalProcessor::filterData(int numBlocks,
                                 const QVector<QVector<bool> > &channelVisible)
{
    int t, channel, stream;
    int length = SAMPLES_PER_DATA_BLOCK * numBlocks;

    // Note: Throughout this function, and elsewhere in this source code, we access
    // multidimensional 'QVector' containers using the at() function, so instead of
    // writing:
    //            x = my3dQVector[i][j][k];
    // we write:
    //            x = my3dQVector.at(i).at(j).at(k);
    //
    // Web research suggests that using at() instead of [] to access the contents
    // of a multidimensional QVector results in faster code, and some preliminary
    // experiments at Intan bear this out.  Web research also suggests that the
    // opposite could be true of C++ STL (non-Qt) 'vector' containers, so some
    // experimentation may be needed to optimize the runtime performance of code.

    if (notchFilterEnabled) {
        for (stream = 0; stream < numDataStreams; ++stream) {
            for (channel = 0; channel < 32; ++channel) {
                if (channelVisible.at(stream).at(channel)) {
                    // Execute biquad IIR notch filter.  The filter "looks backwards" two timesteps,
                    // so we must use the prevAmplifierPreFilter and prevAmplifierPostFilter
                    // variables to store the last two samples from the previous block of
                    // waveform data so that the filter works smoothly across the "seams".
                    amplifierPostFilter[stream][channel][0] =
                            b2 * prevAmplifierPreFilter.at(stream).at(channel).at(0) +
                            b1 * prevAmplifierPreFilter.at(stream).at(channel).at(1) +
                            b0 * amplifierPreFilter.at(stream).at(channel).at(0) -
                            a2 * prevAmplifierPostFilter.at(stream).at(channel).at(0) -
                            a1 * prevAmplifierPostFilter.at(stream).at(channel).at(1);
                    amplifierPostFilter[stream][channel][1] =
                            b2 * prevAmplifierPreFilter.at(stream).at(channel).at(1) +
                            b1 * amplifierPreFilter.at(stream).at(channel).at(0) +
                            b0 * amplifierPreFilter.at(stream).at(channel).at(1) -
                            a2 * prevAmplifierPostFilter.at(stream).at(channel).at(1) -
                            a1 * amplifierPostFilter.at(stream).at(channel).at(0);
                    for (t = 2; t < length; ++t) {
                        amplifierPostFilter[stream][channel][t] =
                                b2 * amplifierPreFilter.at(stream).at(channel).at(t - 2) +
                                b1 * amplifierPreFilter.at(stream).at(channel).at(t - 1) +
                                b0 * amplifierPreFilter.at(stream).at(channel).at(t) -
                                a2 * amplifierPostFilter.at(stream).at(channel).at(t - 2) -
                                a1 * amplifierPostFilter.at(stream).at(channel).at(t - 1);
                    }
                }
            }
        }
    } else {
        // If the notch filter is disabled, simply copy the data without filtering.
        for (stream = 0; stream < numDataStreams; ++stream) {
            for (channel = 0; channel < 32; ++channel) {
                for (t = 0; t < length; ++t) {
                    amplifierPostFilter[stream][channel][t] =
                            amplifierPreFilter.at(stream).at(channel).at(t);
                }
            }
        }
    }

    // Save the last two data points from each waveform to use in successive IIR filter
    // calculations.
    for (stream = 0; stream < numDataStreams; ++stream) {
        for (channel = 0; channel < 32; ++channel) {
            prevAmplifierPreFilter[stream][channel][0] =
                    amplifierPreFilter.at(stream).at(channel).at(length - 2);
            prevAmplifierPreFilter[stream][channel][1] =
                    amplifierPreFilter.at(stream).at(channel).at(length - 1);
            prevAmplifierPostFilter[stream][channel][0] =
                    amplifierPostFilter.at(stream).at(channel).at(length - 2);
            prevAmplifierPostFilter[stream][channel][1] =
                    amplifierPostFilter.at(stream).at(channel).at(length - 1);
        }
    }

    // Apply first-order high-pass filter, if selected
    if (highpassFilterEnabled) {
        double temp;
        for (stream = 0; stream < numDataStreams; ++stream) {
            for (channel = 0; channel < 32; ++channel) {
                if (channelVisible.at(stream).at(channel)) {
                    for (t = 0; t < length; ++t) {
                        temp = amplifierPostFilter[stream][channel][t];
                        amplifierPostFilter[stream][channel][t] -= highpassFilterState[stream][channel];
                        highpassFilterState[stream][channel] =
                                aHpf * highpassFilterState[stream][channel] + bHpf * temp;
                    }
                }
            }
        }
    }

}

// Return the magnitude and phase (in degrees) of a selected frequency component (in Hz)
// for a selected amplifier channel on the selected USB data stream.
void SignalProcessor::measureComplexAmplitude(QVector<QVector<QVector<double> > > &measuredMagnitude,
                                              QVector<QVector<QVector<double> > > &measuredPhase,
                                              int capIndex, int stream, int chipChannel, int numBlocks,
                                              double sampleRate, double frequency, int numPeriods)
{
    int period = qRound(sampleRate / frequency);
    int startIndex = 0;
    int endIndex = startIndex + numPeriods * period - 1;

    // Move the measurement window to the end of the waveform to ignore start-up transient.
    while (endIndex < SAMPLES_PER_DATA_BLOCK * numBlocks - period) {
        startIndex += period;
        endIndex += period;
    }

    double iComponent, qComponent;

    // Measure real (iComponent) and imaginary (qComponent) amplitude of frequency component.
    amplitudeOfFreqComponent(iComponent, qComponent, amplifierPreFilter[stream][chipChannel],
                             startIndex, endIndex, sampleRate, frequency);
    // Calculate magnitude and phase from real (I) and imaginary (Q) components.
    measuredMagnitude[stream][chipChannel][capIndex] =
            qSqrt(iComponent * iComponent + qComponent * qComponent);
    measuredPhase[stream][chipChannel][capIndex] =
            RADIANS_TO_DEGREES *qAtan2(qComponent, iComponent);
}

// Returns the real and imaginary amplitudes of a selected frequency component in the vector
// data, between a start index and end index.
void SignalProcessor::amplitudeOfFreqComponent(double &realComponent, double &imagComponent,
                                               const QVector<double> &data, int startIndex,
                                               int endIndex, double sampleRate, double frequency)
{
    int length = endIndex - startIndex + 1;
    const double k = TWO_PI * frequency / sampleRate;  // precalculate for speed

    // Perform correlation with sine and cosine waveforms.
    double meanI = 0.0;
    double meanQ = 0.0;
    for (int t = startIndex; t <= endIndex; ++t) {
        meanI += data.at(t) * qCos(k * t);
        meanQ += data.at(t) * -1.0 * qSin(k * t);
    }
    meanI /= (double) length;
    meanQ /= (double) length;

    realComponent = 2.0 * meanI;
    imagComponent = 2.0 * meanQ;
}

// Returns the total number of temperature sensors connected to the interface board.
// Only returns valid value after createSaveList() has been called.
int SignalProcessor::getNumTempSensors()
{
    return saveListTempSensor.size();
}

// Reset the vector and variables used to calculate running average
// of temperature sensor readings.
void SignalProcessor::tempHistoryReset(int requestedLength)
{
    int stream;

    if (numDataStreams == 0) return;

    // Clear data in raw temperature sensor history vectors.
    for (stream = 0; stream < numDataStreams; ++stream) {
        tempRawHistory[stream].fill(0.0);
    }
    tempHistoryLength = 0;

    // Set number of samples used to average temperature sensor readings.
    // This number must be at least four, and must be an integer multiple of
    // four.  (See RHD2000 datasheet for details on temperature sensor operation.)

    tempHistoryMaxLength = requestedLength;

    if (tempHistoryMaxLength < 4) {
        tempHistoryMaxLength = 4;
    } else if (tempHistoryMaxLength > tempRawHistory[0].size()) {
        tempHistoryMaxLength = tempRawHistory[0].size();
    }

    int multipleOfFour = 4 * floor(((double)tempHistoryMaxLength) / 4.0);

    tempHistoryMaxLength = multipleOfFour;
}

// Push raw temperature sensor readings into the queue-like vector that
// stores the last tempHistoryLength readings.
void SignalProcessor::tempHistoryPush(QVector<double> &tempData)
{
    int stream, i;

    for (stream = 0; stream < numDataStreams; ++stream) {
        for (i = tempHistoryLength; i > 0; --i) {
            tempRawHistory[stream][i] = tempRawHistory[stream][i-1];
        }
        tempRawHistory[stream][0] = tempData[stream];
    }
    if (tempHistoryLength < tempHistoryMaxLength) {
        ++tempHistoryLength;
    }
}

// Calculate running average of temperature from stored raw sensor readings.
// Results are stored in the tempAvg vector.
void SignalProcessor::tempHistoryCalcAvg()
{
    int stream, i;

    for (stream = 0; stream <  numDataStreams; ++stream) {
        tempAvg[stream] = 0.0;
        for (i = 0; i < tempHistoryLength; ++i) {
            tempAvg[stream] += tempRawHistory[stream][i];
        }
        if (tempHistoryLength > 0) {
            tempAvg[stream] /= tempHistoryLength;
        }
    }
}


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

#include <cmath>
#include <cstring>
#include "rhxglobals.h"
#include "rhxdatablock.h"
#include "waveformfifo.h"

WaveformFifo::WaveformFifo(SignalSources *signalSources_, int bufferSizeInDataBlocks_, int memorySizeInDataBlocks_, int maxWriteSizeInDataBlocks_) :
    signalSources(signalSources_),
    bufferSizeInDataBlocks(bufferSizeInDataBlocks_),
    memorySizeInDataBlocks(memorySizeInDataBlocks_),
    maxWriteSizeInDataBlocks(maxWriteSizeInDataBlocks_),
    numReaders(NumberOfReaders)
{
    if (numReaders < 1) {
        cerr << "WaveformFifo constructor: numReaders must be one or greater." << '\n';
        numReaders = 1;
    }
    samplesPerDataBlock = RHXDataBlock::samplesPerDataBlock(signalSources->getControllerType());
    bufferSize = bufferSizeInDataBlocks * samplesPerDataBlock;
    memorySize = memorySizeInDataBlocks * samplesPerDataBlock;
    if (memorySizeInDataBlocks > bufferSizeInDataBlocks) {
        cerr << "WaveformFifo constructor: memorySizeInDataBlocks can be no larger than bufferSizeInDataBlocks." << '\n';
        return;
    }
    if (maxWriteSizeInDataBlocks < 1) {
        cerr << "WaveformFifo constructor: maxWriteSizeInDataBlocks must be one or greater." << '\n';
        return;
    }

    maxSpikesPerDataBlock = 4;  // TODO: change from hard-coded value to...?  Need to coordinate value with GPU.

    int maxWriteSizeInSamples = maxWriteSizeInDataBlocks * samplesPerDataBlock;
    bufferAllocateSize = bufferSize + maxWriteSizeInSamples;
    bufferAllocateSizeInBlocks = bufferSizeInDataBlocks + maxWriteSizeInDataBlocks;

    usedWordsNewData = new Semaphore [numReaders];
    bufferReadIndex.resize(numReaders);
    bufferMemoryIndex.resize(numReaders);
    numWordsToBeRead.resize(numReaders);
    if (bufferSize < memorySize + 2 * maxWriteSizeInSamples) {
        cerr << "WaveformFifo: bufferSize too small to support requested memorySize and maxWriteSizeInBlocks." << '\n';
    }
    updateForRescan();
}

WaveformFifo::~WaveformFifo()
{
    freeMemory();
    delete [] usedWordsNewData;
}

void WaveformFifo::allocateAnalogBuffer(vector<float*> &bufferArray, const string& waveName)
{
    memoryNeededGB += sizeof(float) * bufferAllocateSize / (1024.0 * 1024.0 * 1024.0);
    float* buffer = nullptr;
    try {
        buffer = new float [bufferAllocateSize];
    } catch (std::bad_alloc&) {
        memoryAllocated = false;
        cerr << "WaveformFifo::allocateAnalogBuffer(): unable to allocate memory." << '\n';
    }
    bufferArray.push_back(buffer);
    analogWaveformIndices[waveName] = buffer;
}

void WaveformFifo::allocateDigitalBuffer(vector<uint16_t*> &bufferArray, const string& waveName)
{
    memoryNeededGB += sizeof(uint16_t) * bufferAllocateSize / (1024.0 * 1024.0 * 1024.0);
    uint16_t* buffer = nullptr;
    try {
        buffer = new (std::nothrow) uint16_t [bufferAllocateSize];
    } catch (std::bad_alloc&) {
        memoryAllocated = false;
        cerr << "WaveformFifo::allocateDigitalBuffer(): unable to allocate memory." << '\n';
    }
    bufferArray.push_back(buffer);
    digitalWaveformIndices[waveName] = buffer;
}

void WaveformFifo::allocateMemory()
{
    if (!analogWaveformIndices.empty() || !digitalWaveformIndices.empty()) {
        freeMemory();
    }

    timeStampBuffer = nullptr;
    gpuAmplifierWidebandBuffer = nullptr;
    gpuAmplifierLowpassBuffer = nullptr;
    gpuAmplifierHighpassBuffer = nullptr;
    gpuSpikeTimestamps = nullptr;
    gpuSpikeIds = nullptr;

    memoryNeededGB = (sizeof(uint32_t) * bufferAllocateSize +
                      3 * sizeof(uint16_t) * bufferAllocateSize * numAmplifierChannels +
                      (sizeof(uint32_t) + sizeof(uint8_t)) * bufferAllocateSizeInBlocks * numAmplifierChannels * maxSpikesPerDataBlock) /
                     (1024.0 * 1024.0 * 1024.0);

    memoryAllocated = true;
    try {
        timeStampBuffer = new uint32_t [bufferAllocateSize];
        gpuAmplifierWidebandBuffer = new uint16_t [bufferAllocateSize * numAmplifierChannels];
        gpuAmplifierLowpassBuffer = new uint16_t [bufferAllocateSize * numAmplifierChannels];
        gpuAmplifierHighpassBuffer = new uint16_t [bufferAllocateSize * numAmplifierChannels];
        gpuSpikeTimestamps = new uint32_t [bufferAllocateSizeInBlocks * numAmplifierChannels * maxSpikesPerDataBlock];
        gpuSpikeIds = new uint8_t [bufferAllocateSizeInBlocks * numAmplifierChannels * maxSpikesPerDataBlock];
    } catch (std::bad_alloc&) {
        memoryAllocated = false;
        cerr << "WaveformFifo::allocateMemory(): unable to allocate " << memoryNeededGB << " GB of memory." << '\n';
    }

    if (!gpuAmplifierWidebandBuffer || !gpuAmplifierLowpassBuffer || !gpuAmplifierHighpassBuffer) {
        cerr << "WaveformFifo::allocateMemory(): unable to allocate GPU filter output buffer memory." << '\n';
    }

    if (!gpuSpikeTimestamps || !gpuSpikeIds) {
        cerr << "WaveformFifo::allocateMemory(): unable to allocate GPU spike detector output buffer memory." << '\n';
    }

    allocateDigitalBuffer(boardDigInWordBuffer, "DIGITAL-IN-WORD");
    allocateDigitalBuffer(boardDigOutWordBuffer, "DIGITAL-OUT-WORD");

    int channelsPerStream = RHXDataBlock::channelsPerStream(signalSources->getControllerType());
    int gpuWaveformIndex = 0;
    for (int group = 0; group < signalSources->numGroups(); group++) {
        SignalGroup* signalGroup = signalSources->groupByIndex(group);
        for (int signal = 0; signal < signalGroup->numChannels(); signal++) {
            Channel* signalChannel = signalGroup->channelByIndex(signal);
            string waveName = signalChannel->getNativeNameString();
            switch (signalChannel->getSignalType()) {
            case AmplifierSignal:
                gpuWaveformIndex = signalChannel->getBoardStream() * channelsPerStream + signalChannel->getChipChannel();
                gpuWaveformAddresses[waveName + "|WIDE"] = { GpuWaveformWideband, gpuWaveformIndex };
                gpuWaveformAddresses[waveName + "|LOW"] = { GpuWaveformLowpass, gpuWaveformIndex };
                gpuWaveformAddresses[waveName + "|HIGH"] = { GpuWaveformHighpass, gpuWaveformIndex };
                gpuWaveformAddresses[waveName + "|SPK"] = { GpuWaveformSpike, gpuWaveformIndex };
                allocateDigitalBuffer(amplifierSpikeBuffer, waveName + "|SPK");
                if (signalSources->getControllerType() == ControllerStimRecordUSB2) {
                    allocateAnalogBuffer(dcAmplifierBuffer, waveName + "|DC");
                    allocateDigitalBuffer(stimFlagsBuffer, waveName + "|STIM");
                }
                break;
            case AuxInputSignal:
                allocateAnalogBuffer(auxInputBuffer, waveName);
                break;
            case SupplyVoltageSignal:
                allocateAnalogBuffer(supplyVoltageBuffer, waveName);
                break;
            case BoardAdcSignal:
                allocateAnalogBuffer(boardAdcBuffer, waveName);
                break;
            case BoardDacSignal:
                allocateAnalogBuffer(boardDacBuffer, waveName);
                break;
            case BoardDigitalInSignal:
                allocateAnalogBuffer(boardDigInBuffer, waveName);
                break;
            case BoardDigitalOutSignal:
                allocateAnalogBuffer(boardDigOutBuffer, waveName);
                break;
            }
        }
    }

    cout << "WaveformFifo: Allocated " << memoryNeededGB << " GBytes for waveform buffers." << '\n';
}

void WaveformFifo::freeMemory()
{
    // Free all allocated buffer memory.
    delete [] timeStampBuffer;

    delete [] gpuAmplifierWidebandBuffer;
    delete [] gpuAmplifierLowpassBuffer;
    delete [] gpuAmplifierHighpassBuffer;

    delete [] gpuSpikeTimestamps;
    delete [] gpuSpikeIds;

    for (map<string, float*>::const_iterator i = analogWaveformIndices.begin(); i != analogWaveformIndices.end(); ++i) {
        delete [] i->second;
    }
    analogWaveformIndices.clear();
    for (map<string, uint16_t*>::const_iterator i = digitalWaveformIndices.begin(); i != digitalWaveformIndices.end(); ++i) {
        delete [] i->second;
    }
    digitalWaveformIndices.clear();
}

bool WaveformFifo::requestWriteSpace(int numDataBlocks)
{
    lock_guard<mutex> lock(mtx);

    if (numDataBlocks > maxWriteSizeInDataBlocks) {
        cerr << "Waveform::requestWriteSpace: numDataBlocks exceeds maxWriteSizeInDataBlocks." << '\n';
        return false;
    }
    int numWords = numDataBlocks * samplesPerDataBlock;
    if (freeWords.tryAcquire(numWords)) {
        numWordsToBeWritten = numWords;
        return true;
    } else {
        return false;   // insufficient free space available in buffer
    }
}

void WaveformFifo::commitNewData()
{
    lock_guard<mutex> lock(mtx);

    bufferWriteIndex += numWordsToBeWritten;
    if (bufferWriteIndex == bufferSize) {
        bufferWriteIndex = 0;
    } else if (bufferWriteIndex > bufferSize) {
        // Copy 'overhanging' data to beginning of buffer.
        // Note: You can avoid this potentially time-consuming memory copy by always writing the same
        // number of samples, and making the buffer size an integer multiple of this number.

        //cout << "WaveformFifo::commitNewData: copying 'overhanging' data to beginning of buffer." << EndOfLine;

        std::memcpy(timeStampBuffer, &timeStampBuffer[bufferSize], sizeof(uint32_t) * (bufferWriteIndex - bufferSize));

        float* analogWaveformBuffer = nullptr;
        for (map<string, float*>::const_iterator i = analogWaveformIndices.begin(); i != analogWaveformIndices.end(); ++i) {
            analogWaveformBuffer = i->second;
            std::memcpy(analogWaveformBuffer, &analogWaveformBuffer[bufferSize], sizeof(float) * (bufferWriteIndex - bufferSize));
        }

        uint16_t* digitalWaveformBuffer = nullptr;
        for (map<string, uint16_t*>::const_iterator i = digitalWaveformIndices.begin(); i != digitalWaveformIndices.end(); ++i) {
            digitalWaveformBuffer = i->second;
            std::memcpy(digitalWaveformBuffer, &digitalWaveformBuffer[bufferSize], sizeof(float) * (bufferWriteIndex - bufferSize));
        }

        std::memcpy(gpuAmplifierWidebandBuffer, &gpuAmplifierWidebandBuffer[bufferSize * numAmplifierChannels],
                sizeof(uint16_t) * (bufferWriteIndex - bufferSize) * numAmplifierChannels);
        std::memcpy(gpuAmplifierLowpassBuffer, &gpuAmplifierLowpassBuffer[bufferSize * numAmplifierChannels],
                sizeof(uint16_t) * (bufferWriteIndex - bufferSize) * numAmplifierChannels);
        std::memcpy(gpuAmplifierHighpassBuffer, &gpuAmplifierHighpassBuffer[bufferSize * numAmplifierChannels],
                sizeof(uint16_t) * (bufferWriteIndex - bufferSize) * numAmplifierChannels);

        bufferWriteIndex -= bufferSize;
    }
    for (int reader = 0; reader < numReaders; ++reader) {
        usedWordsNewData[reader].release(numWordsToBeWritten);
    }
}

bool WaveformFifo::requestReadNewData(Reader reader, int numWords)
{
    lock_guard<mutex> lock(mtx);

    if (usedWordsNewData[reader].available() >= numWords + samplesPerDataBlock) {  // Add one data block to allow spike detection
                                                                                   // pipline to complete.
        if (usedWordsNewData[reader].tryAcquire(numWords)) {
            numWordsToBeRead[reader] = numWords;
            return true;
        } else {
            return false;
        }
    } else {
        return false;   // insufficient data available in buffer
    }
}

MinMax<float> WaveformFifo::getMinMaxData(Reader reader, const float* waveform, int timeIndex, int numSamples) const
{
    MinMax<float> result;

    if (timeIndex + numSamples > numWordsToBeRead[reader] || timeIndex < -numWordsInMemory(reader)) {
        cerr << "Error: WaveformFifo::getMinMaxData: timeIndex out of range." << '\n';
        return result;
    }

    int index = bufferReadIndex[reader] + timeIndex;
    if (index < 0) index += bufferSize;
    else if (index >= bufferSize) index -= bufferSize;
    for (int i = 0; i < numSamples; ++i) {
        result.update(waveform[index]);
        if (++index == bufferSize) index = 0;
    }
    return result;
}

void WaveformFifo::getMinMaxGpuAmplifierData(MinMax<float> &init, Reader reader, GpuWaveformAddress waveformAddress, int timeIndex, int numSamples) const
{
    if (timeIndex + numSamples > numWordsToBeRead[reader] || timeIndex < -numWordsInMemory(reader)) {
        cerr << "Error: WaveformFifo::getMinMaxGpuAmplifierData: timeIndex out of range.  timeIndex = " << timeIndex <<
             "; numSamples = " << numSamples << '\n';
        return;
    }

    int index = bufferReadIndex[reader] + timeIndex;
    if (index < 0) index += bufferSize;
    else if (index >= bufferSize) index -= bufferSize;
    int channelIndex = waveformAddress.waveformIndex;
    if (waveformAddress.waveformType == GpuWaveformWideband) {
        for (int i = 0; i < numSamples; ++i) {
            init.update(0.195F * (((float) gpuAmplifierWidebandBuffer[numAmplifierChannels * index + channelIndex]) - 32768.0F));
            if (++index == bufferSize) index = 0;
        }
    } else if (waveformAddress.waveformType == GpuWaveformLowpass) {
        for (int i = 0; i < numSamples; ++i) {
            init.update(0.195F * (((float) gpuAmplifierLowpassBuffer[numAmplifierChannels * index + channelIndex]) - 32768.0F));
            if (++index == bufferSize) index = 0;
        }
    } else if (waveformAddress.waveformType == GpuWaveformHighpass) {
        for (int i = 0; i < numSamples; ++i) {
            init.update(0.195F * (((float) gpuAmplifierHighpassBuffer[numAmplifierChannels * index + channelIndex]) - 32768.0F));
            if (++index == bufferSize) index = 0;
        }
    }
}

void WaveformFifo::getMinMaxData(MinMax<float> &init, Reader reader, const float* waveform, int timeIndex, int numSamples) const
{
    if (timeIndex + numSamples > numWordsToBeRead[reader] || timeIndex < -numWordsInMemory(reader)) {
        cerr << "Error: WaveformFifo::getMinMaxData: timeIndex out of range.  timeIndex = " << timeIndex <<
             "; numSamples = " << numSamples << "; numWordsInMemory = " << numWordsInMemory(reader) << '\n';
        return;
    }

    int index = bufferReadIndex[reader] + timeIndex;
    if (index < 0) index += bufferSize;
    else if (index >= bufferSize) index -= bufferSize;
    for (int i = 0; i < numSamples; ++i) {
        init.update(waveform[index]);
        if (++index == bufferSize) index = 0;
    }
}

uint16_t WaveformFifo::getStimData(Reader reader, const uint16_t* stimFlags, int timeIndex, int numSamples) const
{
    if (timeIndex + numSamples > numWordsToBeRead[reader] || timeIndex < -numWordsInMemory(reader)) {
        cerr << "Error: WaveformFifo::getStimData: timeIndex out of range.  timeIndex = " << timeIndex <<
             "; numSamples = " << numSamples << '\n';
        return 0;
    }

    int index = bufferReadIndex[reader] + timeIndex;
    if (index < 0) index += bufferSize;
    else if (index >= bufferSize) index -= bufferSize;
    uint16_t result = 0;
    for (int i = 0; i < numSamples; ++i) {
        result |= stimFlags[index];
        if (++index == bufferSize) index = 0;
    }
    return result;
}

// Count the number of spikes in a given time period, assuming all spikes are represented as ones.
uint16_t WaveformFifo::getRasterData(Reader reader, const uint16_t* rasterData, int timeIndex, int numSamples) const
{
    if (timeIndex + numSamples > numWordsToBeRead[reader] || timeIndex < -numWordsInMemory(reader)) {
        cerr << "Error: WaveformFifo::getRasterData: timeIndex out of range.  timeIndex = " << timeIndex <<
             "; numSamples = " << numSamples << '\n';
        return 0;
    }

    int index = bufferReadIndex[reader] + timeIndex;
    if (index < 0) index += bufferSize;
    else if (index >= bufferSize) index -= bufferSize;
    uint16_t result = 0;
    for (int i = 0; i < numSamples; ++i) {
        result += rasterData[index];
        if (++index == bufferSize) index = 0;
    }
    return result;
}

bool WaveformFifo::extractGpuSpikeDataOneDataBlock(uint16_t* waveform, GpuWaveformAddress waveformAddress, bool firstTime) const
{
    bool spikeFound = false;
    if (waveformAddress.waveformType != GpuWaveformSpike) {
        cerr << "Error: WaveformFifo::extractGpuSpikeDataOneDataBlock: waveform is not GpuWaveformSpike type." << '\n';
        return spikeFound;
    }
    if (bufferWriteIndex % samplesPerDataBlock != 0) {
        cerr << "Error: WaveformFifo::extractGpuSpikeDataOneDataBlock: bufferWriteIndex is not an integer multiple of samplesPerDataBlock." << '\n';
        return spikeFound;
    }

    int bufferWriteIndexPrev = bufferWriteIndex - samplesPerDataBlock;
    if (bufferWriteIndexPrev < 0) bufferWriteIndexPrev += bufferSize;

    // Read GPU spike detector output data and create lists of spike IDs along with corresponding timestamps.
    vector<uint32_t> spikeTimeStampList;
    vector<uint16_t> spikeIdList;
    uint32_t spikeTimeStamp;
    uint8_t spikeId;
    int bufferWriteIndexBlock = bufferWriteIndex / samplesPerDataBlock;
    int index = bufferWriteIndexBlock * numAmplifierChannels * maxSpikesPerDataBlock + waveformAddress.waveformIndex;
    for (int k = 0; k < maxSpikesPerDataBlock; ++k) {
        spikeId = gpuSpikeIds[index];
        if (spikeId != SpikeIdNoSpike) {
            spikeTimeStamp = gpuSpikeTimestamps[index];
            spikeFound = true;
            // cout << "found spike " << (int) spikeId << " at timestamp " << spikeTimeStamp << " in channel " << waveformAddress.waveformIndex << EndOfLine;
            spikeTimeStampList.push_back(spikeTimeStamp);
            spikeIdList.push_back((uint16_t) spikeId);
        }
        index += numAmplifierChannels;
        if (index >= bufferAllocateSizeInBlocks * numAmplifierChannels * maxSpikesPerDataBlock) {
            cerr << "Error!  Indexing outside of GPU spike timestamp allocated memory."  << '\n';
        }
    }

    // Initialize spike output to all zeros (i.e., no spikes)
    for (int i = bufferWriteIndex; i < bufferWriteIndex + samplesPerDataBlock; ++i) {
        waveform[i]= 0;
    }

    for (int j = 0; j < (int) spikeTimeStampList.size(); ++j) {
        bool found = false;
        // First, search for spike timestamp in current datablock.
        for (int i = bufferWriteIndex; i < bufferWriteIndex + samplesPerDataBlock; ++i) {
            if (timeStampBuffer[i] == spikeTimeStampList[j]) {
                found = true;
                waveform[i] = spikeIdList[j];
                break;
            }
        }
        if (!found && !firstTime) {   // If we don't find timestamp in current datablock, search previous datablock.
            for (int i = bufferWriteIndexPrev + samplesPerDataBlock - 1; i >= bufferWriteIndexPrev; --i) {
                if (timeStampBuffer[i] == spikeTimeStampList[j]) {
                    found = true;
                    waveform[i] = spikeIdList[j];
                    break;
                }
            }
        }
        if (!found && !firstTime) {
            cout << "Error:: WaveformFifo::extractGpuSpikeDataOneDataBlock: timestamp " << spikeTimeStampList[j] << " not found!" << '\n';
        }
    }
    return spikeFound;
}

float WaveformFifo::getGpuAmplifierData(Reader reader, GpuWaveformAddress waveformAddress, int timeIndex) const
{
    if (timeIndex >= numWordsToBeRead[reader] || timeIndex < -numWordsInMemory(reader)) {
        cerr << "Error: WaveformFifo::getGpuAmplifierData: timeIndex out of range: " << timeIndex << '\n';
        return 0.0F;
    }

    int index = bufferReadIndex[reader] + timeIndex;
    if (index < 0) index += bufferSize;
    else if (index >= bufferSize) index -= bufferSize;
    int channelIndex = waveformAddress.waveformIndex;
    if (waveformAddress.waveformType == GpuWaveformWideband) {
        return 0.195F * (((float) gpuAmplifierWidebandBuffer[numAmplifierChannels * index + channelIndex]) - 32768.0F);
    } else if (waveformAddress.waveformType == GpuWaveformLowpass) {
        return 0.195F * (((float) gpuAmplifierLowpassBuffer[numAmplifierChannels * index + channelIndex]) - 32768.0F);
    } else if (waveformAddress.waveformType == GpuWaveformHighpass) {
        return 0.195F * (((float) gpuAmplifierHighpassBuffer[numAmplifierChannels * index + channelIndex]) - 32768.0F);
    } else {
        return 0.0F;
    }
}

uint16_t WaveformFifo::getGpuAmplifierDataRaw(Reader reader, GpuWaveformAddress waveformAddress, int timeIndex) const
{
    // Return 'zero' if time index is not present in buffer.
    if (timeIndex >= numWordsToBeRead[reader] || timeIndex < -numWordsInMemory(reader)) {
        cerr << "WaveformFifo::getGpuAmplifierDataRaw: time index " << timeIndex << " not present in buffer." << '\n';
        return 32768U;
    }

    int index = bufferReadIndex[reader] + timeIndex;
    if (index < 0) index += bufferSize;
    else if (index >= bufferSize) index -= bufferSize;
    int channelIndex = waveformAddress.waveformIndex;
    if (waveformAddress.waveformType == GpuWaveformWideband) {
        return gpuAmplifierWidebandBuffer[numAmplifierChannels * index + channelIndex];
    } else if (waveformAddress.waveformType == GpuWaveformLowpass) {
        return gpuAmplifierLowpassBuffer[numAmplifierChannels * index + channelIndex];
    } else if (waveformAddress.waveformType == GpuWaveformHighpass) {
        return gpuAmplifierHighpassBuffer[numAmplifierChannels * index + channelIndex];
    } else {
        return 32768U;
    }
}

void WaveformFifo::copyGpuAmplifierData(Reader reader, float* dest, GpuWaveformAddress waveformAddress, int timeIndex, int numSamples) const
{
    if (timeIndex + numSamples > numWordsToBeRead[reader] || timeIndex < -numWordsInMemory(reader)) {
        cerr << "Error: WaveformFifo::copyGpuAmplifierData: timeIndex out of range." << '\n';
        return;
    }

    float* pWrite = dest;
    int index = bufferReadIndex[reader] + timeIndex;
    if (index < 0) index += bufferSize;
    else if (index >= bufferSize) index -= bufferSize;
    int channelIndex = waveformAddress.waveformIndex;
    if (waveformAddress.waveformType == GpuWaveformWideband) {
        for (int i = 0; i < numSamples; ++i) {
            *pWrite = 0.195F * (((float) gpuAmplifierWidebandBuffer[numAmplifierChannels * index + channelIndex]) - 32768.0F);
            if (++index == bufferSize) index = 0;
            ++pWrite;
        }
    } else if (waveformAddress.waveformType == GpuWaveformLowpass) {
        for (int i = 0; i < numSamples; ++i) {
            *pWrite = 0.195F * (((float) gpuAmplifierLowpassBuffer[numAmplifierChannels * index + channelIndex]) - 32768.0F);
            if (++index == bufferSize) index = 0;
            ++pWrite;
        }
    } else if (waveformAddress.waveformType == GpuWaveformHighpass) {
        for (int i = 0; i < numSamples; ++i) {
            *pWrite = 0.195F * (((float) gpuAmplifierHighpassBuffer[numAmplifierChannels * index + channelIndex]) - 32768.0F);
            if (++index == bufferSize) index = 0;
            ++pWrite;
        }
    }
}

void WaveformFifo::copyGpuAmplifierDataRaw(Reader reader, uint16_t* dest, GpuWaveformAddress waveformAddress, int timeIndex,
                                           int numSamples, int downsampleFactor) const
{
    if (timeIndex + numSamples > numWordsToBeRead[reader] || timeIndex < -numWordsInMemory(reader)) {
        cerr << "Error: WaveformFifo::copyGpuAmplifierDataRaw: timeIndex out of range." << '\n';
        return;
    }

    uint16_t* pWrite = dest;
    int index = bufferReadIndex[reader] + timeIndex;
    if (index < 0) index += bufferSize;
    else if (index >= bufferSize) index -= bufferSize;
    int channelIndex = waveformAddress.waveformIndex;
    if (waveformAddress.waveformType == GpuWaveformWideband) {
        for (int i = 0; i < numSamples; ++i) {
            *pWrite = gpuAmplifierWidebandBuffer[numAmplifierChannels * index + channelIndex];
            index += downsampleFactor;
            if (index >= bufferSize) index -= bufferSize;
            ++pWrite;
        }
    } else if (waveformAddress.waveformType == GpuWaveformLowpass) {
        for (int i = 0; i < numSamples; ++i) {
            *pWrite = gpuAmplifierLowpassBuffer[numAmplifierChannels * index + channelIndex];
            index += downsampleFactor;
            if (index >= bufferSize) index -= bufferSize;
            ++pWrite;
        }
    } else if (waveformAddress.waveformType == GpuWaveformHighpass) {
        for (int i = 0; i < numSamples; ++i) {
            *pWrite = gpuAmplifierHighpassBuffer[numAmplifierChannels * index + channelIndex];
            index += downsampleFactor;
            if (index >= bufferSize) index -= bufferSize;
            ++pWrite;
        }
    }
}

void WaveformFifo::copyGpuAmplifierDataArrayRaw(Reader reader, uint16_t* dest, const vector<GpuWaveformAddress>& waveformAddresses,
                                                int timeIndex, int numSamples, int downsampleFactor) const
{
    if (timeIndex + numSamples > numWordsToBeRead[reader] || timeIndex < -numWordsInMemory(reader)) {
        cerr << "Error: WaveformFifo::copyGpuAmplifierDataArrayRaw: timeIndex out of range." << '\n';
        return;
    }

    uint16_t* pWrite = dest;
    int index = bufferReadIndex[reader] + timeIndex;
    if (index < 0) index += bufferSize;
    else if (index >= bufferSize) index -= bufferSize;
    vector<int> channelIndex(waveformAddresses.size());
    for (int j = 0; j < (int) waveformAddresses.size(); ++j) {
        channelIndex[j] = waveformAddresses[j].waveformIndex;
    }
    if (waveformAddresses[0].waveformType == GpuWaveformWideband) {
        for (int i = 0; i < numSamples; ++i) {
            for (int j = 0; j < (int) waveformAddresses.size(); ++j) {
                *pWrite = gpuAmplifierWidebandBuffer[numAmplifierChannels * index + channelIndex[j]];
                ++pWrite;
            }
            index += downsampleFactor;
            if (index >= bufferSize) index -= bufferSize;
        }
    } else if (waveformAddresses[0].waveformType == GpuWaveformLowpass) {
        for (int i = 0; i < numSamples; ++i) {
            for (int j = 0; j < (int) waveformAddresses.size(); ++j) {
                *pWrite = gpuAmplifierLowpassBuffer[numAmplifierChannels * index + channelIndex[j]];
                ++pWrite;
            }
            index += downsampleFactor;
            if (index >= bufferSize) index -= bufferSize;
        }
    } else if (waveformAddresses[0].waveformType == GpuWaveformHighpass) {
        for (int i = 0; i < numSamples; ++i) {
            for (int j = 0; j < (int) waveformAddresses.size(); ++j) {
                *pWrite = gpuAmplifierHighpassBuffer[numAmplifierChannels * index + channelIndex[j]];
                ++pWrite;
            }
            index += downsampleFactor;
            if (index >= bufferSize) index -= bufferSize;
        }
    }
}

void WaveformFifo::copyAnalogData(Reader reader, float* dest, const float* waveform, int timeIndex, int numSamples) const
{
    if (timeIndex + numSamples > numWordsToBeRead[reader] || timeIndex < -numWordsInMemory(reader)) {
        cerr << "Error: WaveformFifo::copyAnalogData: timeIndex out of range." << '\n';
        return;
    }

    float* pWrite = dest;
    int index = bufferReadIndex[reader] + timeIndex;
    if (index < 0) index += bufferSize;
    else if (index >= bufferSize) index -= bufferSize;
    for (int i = 0; i < numSamples; ++i) {
        *pWrite = waveform[index];
        if (++index == bufferSize) index = 0;
        ++pWrite;
    }
}

void WaveformFifo::copyAnalogDataArray(Reader reader, float* dest, const vector<float*>& waveforms, int timeIndex,
                                       int numSamples) const
{
    if (timeIndex + numSamples > numWordsToBeRead[reader] || timeIndex < -numWordsInMemory(reader)) {
        cerr << "Error: WaveformFifo::copyAnalogArrayData: timeIndex out of range." << '\n';
        return;
    }

    float* pWrite = dest;
    int index = bufferReadIndex[reader] + timeIndex;
    if (index < 0) index += bufferSize;
    else if (index >= bufferSize) index -= bufferSize;
    for (int i = 0; i < numSamples; ++i) {
        for (int j = 0; j < (int) waveforms.size(); ++j) {
            *pWrite = waveforms[j][index];
            ++pWrite;
        }
        if (++index == bufferSize) index = 0;
    }
}

void WaveformFifo::copyDigitalData(Reader reader, uint16_t* dest, const uint16_t* waveform, int timeIndex, int numSamples) const
{
    if (timeIndex + numSamples > numWordsToBeRead[reader] || timeIndex < -numWordsInMemory(reader)) {
        cerr << "Error: WaveformFifo::copyDigitalData: timeIndex out of range." << '\n';
        return;
    }

    uint16_t* pWrite = dest;
    int index = bufferReadIndex[reader] + timeIndex;
    if (index < 0) index += bufferSize;
    else if (index >= bufferSize) index -= bufferSize;
    for (int i = 0; i < numSamples; ++i) {
        *pWrite = waveform[index];
        if (++index == bufferSize) index = 0;
        ++pWrite;
    }
}

void WaveformFifo::copyDigitalDataArray(Reader reader, uint16_t* dest, const vector<uint16_t*>& waveforms, int timeIndex,
                                        int numSamples) const
{
    if (timeIndex + numSamples > numWordsToBeRead[reader] || timeIndex < -numWordsInMemory(reader)) {
        cerr << "Error: WaveformFifo::copyDigitalDataArray: timeIndex out of range." << '\n';
        return;
    }

    uint16_t* pWrite = dest;
    int index = bufferReadIndex[reader] + timeIndex;
    if (index < 0) index += bufferSize;
    else if (index >= bufferSize) index -= bufferSize;
    for (int i = 0; i < numSamples; ++i) {
        for (int j = 0; j < (int) waveforms.size(); ++j) {
            *pWrite = waveforms[j][index];
            ++pWrite;
        }
        if (++index == bufferSize) index = 0;
    }
}

void WaveformFifo::copyTimeStamps(Reader reader, uint32_t* dest, int timeIndex, int numSamples) const
{
    if (timeIndex + numSamples > numWordsToBeRead[reader] || timeIndex < -numWordsInMemory(reader)) {
        cerr << "Error: WaveformFifo::copyTimeStamps: timeIndex out of range." << '\n';
        return;
    }

    uint32_t* pWrite = dest;
    int index = bufferReadIndex[reader] + timeIndex;
    if (index < 0) index += bufferSize;
    else if (index >= bufferSize) index -= bufferSize;
    for (int i = 0; i < numSamples; ++i) {
        *pWrite = timeStampBuffer[index];
        if (++index == bufferSize) index = 0;
        ++pWrite;
    }
}

// Call once after all reading is complete.
void WaveformFifo::freeOldData(Reader reader)
{
    lock_guard<mutex> lock(mtx);

    int bufferWriteIndexFrozen = bufferWriteIndex;  // Save this value in case it changes from another thread.

    // What is the minimum distance between the write index and the memory indexes, currently?
    int minDistanceOld = bufferSize;
    for (int r = 0; r < numReaders; ++r) {
        int distance = bufferMemoryIndex[r] - bufferWriteIndexFrozen;
        if (distance < 0) distance += bufferSize;
        if (distance < minDistanceOld) {
            minDistanceOld = distance;
        }
    }

    bufferReadIndex[reader] += numWordsToBeRead[reader];
    if (bufferReadIndex[reader] >= bufferSize) {
        bufferReadIndex[reader] -= bufferSize;
    }
    int excess = numWordsInMemory(reader) - memorySize;
    if (excess > 0) {
        bufferMemoryIndex[reader] += excess;
        if (bufferMemoryIndex[reader] >= bufferSize) {
            bufferMemoryIndex[reader] -= bufferSize;
        }
    }

    // What is the minimum distance between the write index and the memory indexes after adjusting indexes?
    int minDistanceNew = bufferSize;
    int minIndex = 0;
    for (int r = 0; r < numReaders; ++r) {
        int distance = bufferMemoryIndex[r] - bufferWriteIndexFrozen;
        if (distance < 0) distance += bufferSize;
        if (distance < minDistanceNew) {
            minDistanceNew = distance;
            minIndex = r;
        }
    }

    int maxWriteSize = maxWriteSizeInDataBlocks * RHXDataBlock::samplesPerDataBlock(signalSources->getControllerType());
    if (minDistanceNew < maxWriteSize) {
        cout << "WaveformFifo: Running out of space!  Consumer number " << minIndex << " is not reading data quickly enough." << '\n';
    }

//    cout << reader << ": " << minDistanceNew - minDistanceOld << EndOfLine;
    freeWords.release(minDistanceNew - minDistanceOld);
//    cout << reader << ":freeWords.release(" << minDistanceNew - minDistanceOld << "); Waveform FIFO is " << percentFull() << "% full." << EndOfLine;
//    cout << "freeWords.available() = " << freeWords.available() << EndOfLine;
}

// Returns number of 'old' words in memory, not including newly written words.
int WaveformFifo::numWordsInMemory(Reader reader) const
{
    if (bufferReadIndex[reader] >= bufferMemoryIndex[reader]) {
        return bufferReadIndex[reader] - bufferMemoryIndex[reader];
    } else {
        return bufferReadIndex[reader] - bufferMemoryIndex[reader] + bufferSize;
    }
}

double WaveformFifo::percentFull() const
{
//    return 100.0 * (1.0 - ((double)freeWords.available() / (double)bufferSize));
    return max(100.0 * (1.0 - ((double)freeWords.available() / (double)(bufferSize - memorySize))), 0.0);
}

void WaveformFifo::resetBuffer()
{
    lock_guard<mutex> lock(mtx);

    freeWords.acquire(freeWords.available());
    for (int reader = 0; reader < numReaders; ++reader) {
        usedWordsNewData[reader].acquire(usedWordsNewData[reader].available());
        bufferReadIndex[reader] = 0;
        bufferMemoryIndex[reader] = 0;
        numWordsToBeRead[reader] = 0;
    }
    bufferWriteIndex = 0;
    numWordsToBeWritten = 0;
    freeWords.release(bufferSize);
}

void WaveformFifo::pauseBuffer()
{
    for (int reader = 0; reader < numReaders; ++reader) {
        requestReadNewData((Reader) reader, usedWordsNewData[reader].available() - samplesPerDataBlock);
        // Subtract one data block to compensate for data block added for spike detection pipline (see requestReadNewData()).
        freeOldData((Reader) reader);
    }
}

float* WaveformFifo::getAnalogWaveformPointer(const string& waveName) const
{
    map<string, float*>::const_iterator p = analogWaveformIndices.find(waveName);
    if (p == analogWaveformIndices.end()) {
        cerr << "ERROR: WaveformFifo:getAnalogWaveformPointer: " << waveName << " not found." << '\n';
        return nullptr;
    }
    return p->second;
}

uint16_t* WaveformFifo::getDigitalWaveformPointer(const string& waveName) const
{
    map<string, uint16_t*>::const_iterator p = digitalWaveformIndices.find(waveName);
    if (p == digitalWaveformIndices.end()) {
        cerr << "ERROR: WaveformFifo:getDigitalWaveformPointer: " << waveName << " not found." << '\n';
        return nullptr;
    }
    return p->second;
}

GpuWaveformAddress WaveformFifo::getGpuWaveformAddress(const string& waveName) const
{
    map<string, GpuWaveformAddress>::const_iterator p = gpuWaveformAddresses.find(waveName);
    if (p == gpuWaveformAddresses.end()) {
        return GpuWaveformAddress{ GpuWaveformWideband, -1 };
        cerr << "ERROR: WaveformFifo::getGpuWaveformAddress: " << waveName << " not found." << '\n';
    }
    return p->second;
}

bool WaveformFifo::gpuWaveformPresent(const string& waveName) const
{
    map<string, GpuWaveformAddress>::const_iterator p = gpuWaveformAddresses.find(waveName);
    if (p == gpuWaveformAddresses.end()) {
        return false;
    }
    return true;
}

void WaveformFifo::updateForRescan()
{
    numAmplifierChannels = signalSources->numAmplifierChannels();
    allocateMemory();
    resetBuffer();
}

//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.1.0
//
//  Copyright (c) 2020-2022 Intan Technologies
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

#ifndef WAVEFORMFIFO_H
#define WAVEFORMFIFO_H

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include "rhxglobals.h"
#include "rhx-semaphore.h"
#include "minmax.h"
#include "signalsources.h"

using namespace std;

// Multi-waveform FIFO implemented as a circular buffer.  Additional buffer space is allocated
// beyond the end of the buffer to permit continuous writes to the buffer up to a specified
// length.  If data is written beyond the end of the buffer, the extra data is copied to the
// beginning of the buffer after writing has completed.
//
// The buffer also has a "memory" that maintains a specified number of old data words from
// previous writes.

enum GpuWaveformType {
    GpuWaveformWideband,
    GpuWaveformLowpass,
    GpuWaveformHighpass,
    GpuWaveformSpike
};

struct GpuWaveformAddress
{
    GpuWaveformType waveformType;
    int waveformIndex;
};

const uint8_t SpikeIdNoSpike = 0x00u;
const uint8_t SpikeIdSpikeType1 = 0x01u;
const uint8_t SpikeIdSpikeType2 = 0x02u;
const uint8_t SpikeIdSpikeType3 = 0x04u;
const uint8_t SpikeIdSpikeType4 = 0x08u;
const uint8_t SpikeIdValidSpikeMask = 0x0fu;
const uint8_t SpikeIdUnclassifiedSpike = 0x40u;
const uint8_t SpikeIdLikelyArtifact = 0x80u;

class WaveformFifo
{
public:
    enum Reader {
        ReaderDisplay = 0,
        ReaderDisk,
        ReaderAudio,
        ReaderTCP,
        NumberOfReaders   // Don't use this last enum; used only by constructor to count total number of readers.
    };

    WaveformFifo(SignalSources *signalSources_, int bufferSizeInDataBlocks_, int memorySizeInDataBlocks_, int maxWriteSizeInDataBlocks_, SystemState* state_);
    ~WaveformFifo();

    // Three methods used (in this sequence) for writing to buffer:

    // 1:.
    bool requestWriteSpace(int numDataBlocks);   // Call once before writing a block of data

    // 2:
    inline float* pointerToAnalogWriteSpace(const float* waveform) const  // Call for each waveform, then write data to location.
    {
        return (float*) (&waveform[bufferWriteIndex]);
    }

    inline uint16_t* pointerToDigitalWriteSpace(const uint16_t* waveform) const  // Call for each waveform, then write data to location.
    {
        return (uint16_t*) (&waveform[bufferWriteIndex]);
    }

    inline uint16_t* pointerToGpuWidebandWriteSpace() const
    {
        return &gpuAmplifierWidebandBuffer[bufferWriteIndex * numAmplifierChannels];
    }

    inline uint16_t* pointerToGpuLowpassWriteSpace() const
    {
        return &gpuAmplifierLowpassBuffer[bufferWriteIndex * numAmplifierChannels];
    }

    inline uint16_t* pointerToGpuHighpassWriteSpace() const
    {
        return &gpuAmplifierHighpassBuffer[bufferWriteIndex * numAmplifierChannels];
    }

    inline uint32_t* pointerToGpuSpikeTimestampsWriteSpace() const
    {
        return &gpuSpikeTimestamps[(bufferWriteIndex/samplesPerDataBlock) * numAmplifierChannels * maxSpikesPerDataBlock];
    }

    inline uint8_t* pointerToGpuSpikeIdsWriteSpace() const
    {
        return &gpuSpikeIds[(bufferWriteIndex/samplesPerDataBlock) * numAmplifierChannels * maxSpikesPerDataBlock];
    }

    bool extractGpuSpikeDataOneDataBlock(uint16_t* waveform, GpuWaveformAddress waveformAddress, bool firstTime) const;

    inline uint32_t* pointerToTimeStampWriteSpace() const
    {
        return &timeStampBuffer[bufferWriteIndex];
    }

    // 3:
    void commitNewData();   // Call once after all writing is complete.


    // Methods used (in this sequence) for reading from buffer:

    // 1:
    bool requestReadNewData(Reader reader, int numWords); // Call once before reading a block of data.

    // 2:

    // Return one word from an analog waveform buffer.  Valid values of timeIndex range from -numWordsInMemory()
    // to (numWordsToBeRead - 1).  The parameter numWordsToBeRead is set by requestReadNewData().  The most
    // recently written data is found between timeIndex values of zero and numWordsToBeRead.
    inline float getAnalogData(Reader reader, const float* waveform, int timeIndex) const  // Call many times to read all data.
    {
        if (timeIndex >= numWordsToBeRead[reader] || timeIndex < -numWordsInMemory(reader)) {
            cerr << "Error: WaveformFifo::getAnalogData: timeIndex " << timeIndex << " out of range.\n";
            return 0.0F;
        }

        int index = bufferReadIndex[reader] + timeIndex;
        if (index < 0) index += bufferSize;
        else if (index >= bufferSize) index -= bufferSize;
        return waveform[index];
    }

    // Return one word from a digital waveform buffer.  Valid values of timeIndex range from -numWordsInMemory()
    // to (numWordsToBeRead - 1).  The parameter numWordsToBeRead is set by requestReadNewData().  The most
    // recently written data is found between timeIndex values of zero and numWordsToBeRead.
    inline uint16_t getDigitalData(Reader reader, const uint16_t* waveform, int timeIndex) const  // Call many times to read all data.
    {
        if (timeIndex >= numWordsToBeRead[reader] || timeIndex < -numWordsInMemory(reader)) {
            cerr << "Error: WaveformFifo::getDigitalData: timeIndex " << timeIndex << " out of range.\n";
            return 0;
        }

        int index = bufferReadIndex[reader] + timeIndex;
        if (index < 0) index += bufferSize;
        else if (index >= bufferSize) index -= bufferSize;
        return waveform[index];
    }

    // Return one word from an analog waveform buffer, but convert to digital using a threshold value.  Valid values
    // of timeIndex range from -numWordsInMemory() to (numWordsToBeRead - 1).  The parameter numWordsToBeRead is set by
    // requestReadNewData().  The most recently written data is found between timeIndex values of zero and numWordsToBeRead.
    inline uint16_t getAnalogDataAsDigital(Reader reader, const float* waveform, int timeIndex, float threshold) const
    {
        if (timeIndex >= numWordsToBeRead[reader] || timeIndex < -numWordsInMemory(reader)) {
            cerr << "Error: WaveformFifo::getAnalogDataAsDigital: timeIndex " << timeIndex << " out of range.\n";
            return 0;
        }

        int index = bufferReadIndex[reader] + timeIndex;
        if (index < 0) index += bufferSize;
        else if (index >= bufferSize) index -= bufferSize;
        return (waveform[index] >= threshold) ? 0x01u : 0;
    }

    inline uint32_t getTimeStamp(Reader reader, int timeIndex) const
    {
        if (timeIndex >= numWordsToBeRead[reader] || timeIndex < -numWordsInMemory(reader)) {
            cerr << "Error: WaveformFifo::getTimeStamp: timeIndex " << timeIndex << " out of range.\n";
            return 0;
        }

        int index = bufferReadIndex[reader] + timeIndex;
        if (index < 0) index += bufferSize;
        else if (index >= bufferSize) index -= bufferSize;
        return timeStampBuffer[index];
    }

    float getGpuAmplifierData(Reader reader, GpuWaveformAddress waveformAddress, int timeIndex) const;
    uint16_t getGpuAmplifierDataRaw(Reader reader, GpuWaveformAddress waveformAddress, int timeIndex) const;

    // Faster than many repeated getAnalogData()'s, etc.:
    void copyGpuAmplifierData(Reader reader, float* dest, GpuWaveformAddress waveformAddress, int timeIndex, int numSamples) const;
    void copyGpuAmplifierDataRaw(Reader reader, uint16_t* dest, GpuWaveformAddress waveformAddress, int timeIndex,
                                 int numSamples, int downsampleFactor = 1) const;
    void copyGpuAmplifierDataArrayRaw(Reader reader, uint16_t* dest, const vector<GpuWaveformAddress>& waveformAddresses,
                                      int timeIndex, int numSamples, int downsampleFactor = 1) const;
    void copyAnalogData(Reader reader, float* dest, const float* waveform, int timeIndex, int numSamples) const;
    void copyAnalogDataArray(Reader reader, float* dest, const vector<float*>& waveforms, int timeIndex, int numSamples) const;
    void copyDigitalData(Reader reader, uint16_t* dest, const uint16_t* waveform, int timeIndex, int numSamples) const;
    void copyDigitalDataArray(Reader reader, uint16_t* dest, const vector<uint16_t*>& waveforms, int timeIndex, int numSamples) const;
    void copyTimeStamps(Reader reader, uint32_t* dest, int timeIndex, int numSamples) const;

    MinMax<float> getMinMaxData(Reader reader, const float* waveform, int timeIndex, int numSamples) const;
    void getMinMaxGpuAmplifierData(MinMax<float> &init, Reader reader, GpuWaveformAddress waveformAddress, int timeIndex, int numSamples) const;
    void getMinMaxData(MinMax<float> &init, Reader reader,  const float* waveform, int timeIndex, int numSamples) const;
    uint16_t getStimData(Reader reader, const uint16_t* stimFlags, int timeIndex, int numSamples) const;
    uint16_t getRasterData(Reader reader, const uint16_t* rasterData, int timeIndex, int numSamples) const;

    // 3:
    void freeOldData(Reader reader); // Call once after all reading is complete.

    int numWordsInMemory(Reader reader) const; // Return length of old data stored in memory.
    double percentFull() const;

    void resetBuffer();
    void pauseBuffer();

    float* getAnalogWaveformPointer(const string& waveName) const;
    uint16_t* getDigitalWaveformPointer(const string& waveName) const;
    GpuWaveformAddress getGpuWaveformAddress(const string& waveName) const;
    bool gpuWaveformPresent(const string& waveName) const;

    void updateForRescan();

    bool memoryWasAllocated(double& memoryRequestedGB) const { memoryRequestedGB += memoryNeededGB; return memoryAllocated; }

private:
    SystemState *state;
    mutex mtx;
    SignalSources *signalSources;
    int numAmplifierChannels;
    int maxSpikesPerDataBlock;
    int samplesPerDataBlock;

    // Buffer for timestamps
    uint32_t* timeStampBuffer;

    // Buffers for GPU-processed amplifier waveforms
    uint16_t* gpuAmplifierWidebandBuffer;
    uint16_t* gpuAmplifierLowpassBuffer;
    uint16_t* gpuAmplifierHighpassBuffer;

    // Buffers for GPU-processed spike detection data
    uint32_t* gpuSpikeTimestamps;
    uint8_t* gpuSpikeIds;

    // Buffers for amplifier waveforms (with stream and channel indexing)
    vector<float*> amplifierWidebandBuffer;
    vector<float*> amplifierLfpBandBuffer;
    vector<float*> amplifierSpikeBandBuffer;

    // Buffers for spike detection  (same stream and channel indexing as above)
    vector<uint16_t*> amplifierSpikeBuffer;

    // Buffers for stimulation-related waveforms (same stream and channel indexing as above)
    vector<float*> dcAmplifierBuffer;
    vector<uint16_t*> stimFlagsBuffer;

    // Buffers for auxiliary inputs on chips (e.g., accelerometers) (with stream and channel indexing)
    vector<float*> auxInputBuffer;

    // Buffers for supply voltages on chips (with stream and channel indexing)
    vector<float*> supplyVoltageBuffer;

    // Buffers for controller-based analog and digital inputs and outputs (implicit indexing)
    vector<float*> boardDacBuffer;
    vector<float*> boardAdcBuffer;
    vector<float*> boardDigInBuffer;            // individual digital in channels (using float for easy plotting)
    vector<float*> boardDigOutBuffer;           // individual digital out channels (using float for easy plotting)
    vector<uint16_t*> boardDigInWordBuffer;     // all 16 digital in channels saved as uint16 word (for quick saving)
    vector<uint16_t*> boardDigOutWordBuffer;    // all 16 digital out channels saved as uint16 word (for quick saving)

    int bufferSizeInDataBlocks;
    int memorySizeInDataBlocks;
    int maxWriteSizeInDataBlocks;
    int bufferSize;
    int memorySize;
    int numReaders;
    int bufferAllocateSize;
    int bufferAllocateSizeInBlocks;

    Semaphore freeWords;
    Semaphore* usedWordsNewData;
    int bufferWriteIndex;
    vector<int> bufferReadIndex;
    vector<int> bufferMemoryIndex;
    int numWordsToBeWritten;
    vector<int> numWordsToBeRead;

    map<string, float*> analogWaveformIndices;
    map<string, uint16_t*> digitalWaveformIndices;
    map<string, GpuWaveformAddress> gpuWaveformAddresses;

    bool memoryAllocated;
    double memoryNeededGB;

    void allocateAnalogBuffer(vector<float*> &bufferArray, const string& waveName);
    void allocateDigitalBuffer(vector<uint16_t*> &bufferArray, const string& waveName);
    void allocateMemory();
    void freeMemory();
};

#endif // WAVEFORMFIFO_H

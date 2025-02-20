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

#ifndef RHXDATABLOCK_H
#define RHXDATABLOCK_H

#include <cstdint>
#include <fstream>
#include "rhxglobals.h"

const int USBHeaderSizeInBytes = 8;
const uint64_t HeaderRecordUSB2 = 0xc691199927021942UL;
const uint64_t HeaderRecordUSB3 = 0xd7a22aaa38132a53UL;
const uint64_t HeaderStimRecordUSB2 = 0x8d542c8a49712f0bUL;

class RHXDataBlock
{
public:
    RHXDataBlock(ControllerType type_, int numDataStreams_);
    ~RHXDataBlock();
    RHXDataBlock(const RHXDataBlock &obj);  // copy constructor

    uint32_t timeStamp(int t) const;
    int amplifierData(int stream, int channel, int t) const;
    int auxiliaryData(int stream, int channel, int t) const;
    int boardAdcData(int channel, int t) const;
    int ttlIn(int channel, int t) const;
    int ttlOut(int channel, int t) const;

    int dcAmplifierData(int stream, int channel, int t) const;
    int complianceLimit(int stream, int channel, int t) const;
    int stimOn(int stream, int channel, int t) const;
    int stimPol(int stream, int channel, int t) const;
    int ampSettle(int stream, int channel, int t) const;
    int chargeRecov(int stream, int channel, int t) const;
    int boardDacData(int channel, int t) const;

    static int samplesPerDataBlock(ControllerType type_);
    int samplesPerDataBlock() const;
    static int blocksFor30Hz(AmplifierSampleRate rate);
    static int maxChannelsPerStream() { return 32; }
    static int channelsPerStream(ControllerType type_);
    int channelsPerStream() const { return channelsPerStream(type); }
    static int numAuxChannels(ControllerType type_);
    int numAuxChannels() const { return numAuxChannels(type); }

    static unsigned int dataBlockSizeInWords(ControllerType type_, int numDataStreams_);
    unsigned int dataBlockSizeInWords() const { return dataBlockSizeInWords(type, numDataStreams); }

    void fillFromUsbBuffer(uint8_t* usbBuffer, int blockIndex);

    void print(int stream) const;
    void write(std::ofstream &saveOut, int numDataStreams) const;

    static bool checkUsbHeader(const uint8_t* usbBuffer, int index, ControllerType type_);
    bool checkUsbHeader(const uint8_t* usbBuffer, int index) const;
    int getChipID(int stream, int auxCmdSlot, int &register59Value) const;

    static uint64_t headerMagicNumber(ControllerType type_);
    uint64_t headerMagicNumber() const;

private:
    ControllerType type;
    int numDataStreams;

    uint32_t* timeStampInternal;
    int* amplifierDataInternal;
    int* auxiliaryDataInternal;
    int* boardAdcDataInternal;
    int* ttlInInternal;
    int* ttlOutInternal;

    // Stim/Record Controller only:
    int* dcAmplifierDataInternal;
    int* complianceLimitInternal;
    int* stimOnInternal;
    int* stimPolInternal;
    int* ampSettleInternal;
    int* chargeRecovInternal;
    int* boardDacDataInternal;

    void allocateMemory();

    inline uint32_t convertUsbTimeStamp(const uint8_t* usbBuffer, int index)
    {
        uint32_t x1 = usbBuffer[index];
        uint32_t x2 = usbBuffer[index + 1];
        uint32_t x3 = usbBuffer[index + 2];
        uint32_t x4 = usbBuffer[index + 3];
        return (x4 << 24) + (x3 << 16) + (x2 << 8) + (x1 << 0);
    }

    inline int convertUsbWord(const uint8_t* usbBuffer, int index)
    {
        unsigned int x1 = (unsigned int)usbBuffer[index];
        unsigned int x2 = (unsigned int)usbBuffer[index + 1];
        unsigned int result = (x2 << 8) | (x1 << 0);
        return (int)result;
    }

    void writeWordLittleEndian(std::ofstream &outputStream, int dataWord) const;
};

#endif // RHXDATABLOCK_H

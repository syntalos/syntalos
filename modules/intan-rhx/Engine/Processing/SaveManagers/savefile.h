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

#ifndef SAVEFILE_H
#define SAVEFILE_H

#include <QString>
#include <QFile>
#include <QDataStream>
#include <vector>
#include <string>
#include "signalsources.h"

class SaveFile
{
public:
    SaveFile(const QString& fileName_, int bufferSize_);
    //SaveFile(const QString& fileName_, int bufferSize_ = 262144); // 262144 = 2^18 bytes = 256K
    //SaveFile(const QString& fileName_, int bufferSize_ = 2048);
    ~SaveFile();

    void writeInt32(int32_t word);
    void writeInt32(const int32_t* wordArray, int numSamples);
    void writeUInt32(uint32_t word);
    void writeUInt32(const uint32_t* wordArray, int numSamples);
    void writeInt16(int16_t word);
    void writeInt16(const int16_t* wordArray, int numSamples);
    void writeUInt16(uint16_t word);
    void writeUInt16(const uint16_t* wordArray, int numSamples);
    void writeBitAsUInt16(uint16_t word, int bit);
    void writeBitAsUInt16(const uint16_t* wordArray, int numSamples, int bit);
    void writeUInt16StimData(const uint16_t* wordArray, int numSamples, uint8_t posAmplitude, uint8_t negAmplitude);
    void writeUInt16StimDataArray(const uint16_t* wordArray, int numSamples, int numWaveforms,
                                  const std::vector<uint8_t>& posAmplitudes, const std::vector<uint8_t>& negAmplitudes);
    void writeUInt16AsSigned(const uint16_t* wordArray, int numSamples);
    void writeUInt8(uint8_t byte);
    void writeDouble(double x);
    void writeQString(const QString& s);
    void writeQStringAsAsciiText(const QString& s);
    void writeStringAsCharArray(const std::string& s);
    void writeSignalSources(const SignalSources* signalSources);
    void writeSignalGroup(const SignalGroup* signalGroup);
    void close();
    void flush();
    void forceFlush();
    bool isOpen() const { return file != nullptr; }
    void openForAppend();
    inline int64_t getNumBytesWritten() const { return numBytesWritten; }
    inline void resetNumBytesWritten() { numBytesWritten = 0; }

private:
    int bufferSize;
    int bufferSizeMinus4;
    int bufferSizeMinus2;
    int bufferIndex;
    int64_t numBytesWritten;
    char* buffer;

    QString fileName;
    QFile* file;
    QDataStream* dataStream;

    void createDataStream(QFile* file_);
};

#endif // SAVEFILE_H

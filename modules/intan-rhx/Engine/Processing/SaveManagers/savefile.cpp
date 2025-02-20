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

#include <iostream>
#include "savefile.h"

SaveFile::SaveFile(const QString& fileName_, int bufferSize_) :
    bufferSize(bufferSize_),
    fileName(fileName_),
    file(nullptr),
    dataStream(nullptr)
{
    file = new QFile(fileName);
    if (!file->open(QIODevice::WriteOnly)) {
        std::cerr << "SaveFile: Cannot open file " << fileName.toStdString() << " for writing: " <<
                qPrintable(file->errorString()) << '\n';
        if (file) delete file;
        file = nullptr;
        return;
    }
    createDataStream(file);

    buffer = new char [bufferSize];
    bufferIndex = 0;
    bufferSizeMinus4 = bufferSize - 4;  // Precompute to save time.
    bufferSizeMinus2 = bufferSize - 2;  // Precompute to save time.

    resetNumBytesWritten();
}

SaveFile::~SaveFile()
{
    close();
    delete [] buffer;
}

void SaveFile::writeInt32(int32_t word)
{
    if (bufferIndex > bufferSizeMinus4) flush();
    buffer[bufferIndex++] = (char)  (word & 0x000000ff);
    buffer[bufferIndex++] = (char) ((word & 0x0000ff00) >> 8);
    buffer[bufferIndex++] = (char) ((word & 0x00ff0000) >> 16);
    buffer[bufferIndex++] = (char) ((word & 0xff000000) >> 24);
}

void SaveFile::writeInt32(const int32_t* wordArray, int numSamples)
{
    const int32_t* word = wordArray;
    const int WordSize = 4;
    if (bufferIndex > bufferSize - WordSize * numSamples) flush();
    while (WordSize * numSamples > bufferSize) {
        for (int i = 0; i < bufferSize / WordSize; ++i) {
            buffer[bufferIndex++] = (char)  ((*word) & 0x000000ff);
            buffer[bufferIndex++] = (char) (((*word) & 0x0000ff00) >> 8);
            buffer[bufferIndex++] = (char) (((*word) & 0x00ff0000) >> 16);
            buffer[bufferIndex++] = (char) (((*word) & 0xff000000) >> 24);
            ++word;
        }
        flush();
        numSamples -= bufferSize / WordSize;
    }
    for (int i = 0; i < numSamples; ++i) {
        buffer[bufferIndex++] = (char)  ((*word) & 0x000000ff);
        buffer[bufferIndex++] = (char) (((*word) & 0x0000ff00) >> 8);
        buffer[bufferIndex++] = (char) (((*word) & 0x00ff0000) >> 16);
        buffer[bufferIndex++] = (char) (((*word) & 0xff000000) >> 24);
        ++word;
    }
}

void SaveFile::writeUInt32(uint32_t word)
{
    if (bufferIndex > bufferSizeMinus4) flush();
    buffer[bufferIndex++] = (char)  (word & 0x000000ff);
    buffer[bufferIndex++] = (char) ((word & 0x0000ff00) >> 8);
    buffer[bufferIndex++] = (char) ((word & 0x00ff0000) >> 16);
    buffer[bufferIndex++] = (char) ((word & 0xff000000) >> 24);
}

void SaveFile::writeUInt32(const uint32_t* wordArray, int numSamples)
{
    const uint32_t* word = wordArray;
    const int WordSize = 4;
    if (bufferIndex > bufferSize - WordSize * numSamples) flush();
    while (WordSize * numSamples > bufferSize) {
        for (int i = 0; i < bufferSize / WordSize; ++i) {
            buffer[bufferIndex++] = (char)  ((*word) & 0x000000ff);
            buffer[bufferIndex++] = (char) (((*word) & 0x0000ff00) >> 8);
            buffer[bufferIndex++] = (char) (((*word) & 0x00ff0000) >> 16);
            buffer[bufferIndex++] = (char) (((*word) & 0xff000000) >> 24);
            ++word;
        }
        flush();
        numSamples -= bufferSize / WordSize;
    }
    for (int i = 0; i < numSamples; ++i) {
        buffer[bufferIndex++] = (char)  ((*word) & 0x000000ff);
        buffer[bufferIndex++] = (char) (((*word) & 0x0000ff00) >> 8);
        buffer[bufferIndex++] = (char) (((*word) & 0x00ff0000) >> 16);
        buffer[bufferIndex++] = (char) (((*word) & 0xff000000) >> 24);
        ++word;
    }
}

void SaveFile::writeInt16(int16_t word)
{
    if (bufferIndex > bufferSizeMinus2) flush();
    buffer[bufferIndex++] = (char)  (word & 0x00ffU);
    buffer[bufferIndex++] = (char) ((word & 0xff00U) >> 8);
}

void SaveFile::writeInt16(const int16_t* wordArray, int numSamples)
{
    const int16_t* word = wordArray;
    const int WordSize = 2;
    if (bufferIndex > bufferSize - WordSize * numSamples) flush();
    while (WordSize * numSamples > bufferSize) {
        for (int i = 0; i < bufferSize / WordSize; ++i) {
            buffer[bufferIndex++] = (char)  ((*word) & 0x00ffU);
            buffer[bufferIndex++] = (char) (((*word) & 0xff00U) >> 8);
            ++word;
        }
        flush();
        numSamples -= bufferSize / WordSize;
    }
    for (int i = 0; i < numSamples; ++i) {
        buffer[bufferIndex++] = (char)  ((*word) & 0x00ffU);
        buffer[bufferIndex++] = (char) (((*word) & 0xff00U) >> 8);
        ++word;
    }
}

void SaveFile::writeUInt16(uint16_t word)
{
    if (bufferIndex > bufferSizeMinus2) flush();
    buffer[bufferIndex++] = (char)  (word & 0x00ffU);
    buffer[bufferIndex++] = (char) ((word & 0xff00U) >> 8);
}

void SaveFile::writeUInt16(const uint16_t* wordArray, int numSamples)
{
    const uint16_t* word = wordArray;
    const int WordSize = 2;
    if (bufferIndex > bufferSize - WordSize * numSamples) flush();
    while (WordSize * numSamples > bufferSize) {
        for (int i = 0; i < bufferSize / WordSize; ++i) {
            buffer[bufferIndex++] = (char)  ((*word) & 0x00ffU);
            buffer[bufferIndex++] = (char) (((*word) & 0xff00U) >> 8);
            ++word;
        }
        flush();
        numSamples -= bufferSize / WordSize;
    }
    for (int i = 0; i < numSamples; ++i) {
        buffer[bufferIndex++] = (char)  ((*word) & 0x00ffU);
        buffer[bufferIndex++] = (char) (((*word) & 0xff00U) >> 8);
        ++word;
    }
}

void SaveFile::writeBitAsUInt16(uint16_t word, int bit)
{
    const uint16_t Mask = 0x0001U << bit;
    if (bufferIndex > bufferSizeMinus2) flush();
    char bitResult = ((word & Mask) != 0) ? (char) 1 : (char) 0;
    buffer[bufferIndex++] = bitResult;
    buffer[bufferIndex++] = (char) 0;
}

void SaveFile::writeBitAsUInt16(const uint16_t* wordArray, int numSamples, int bit)
{
    const uint16_t* word = wordArray;
    const uint16_t Mask = 0x0001U << bit;
    const int WordSize = 2;
    if (bufferIndex > bufferSize - WordSize * numSamples) flush();
    while (WordSize * numSamples > bufferSize) {
        for (int i = 0; i < bufferSize / WordSize; ++i) {
            char bitResult = (((*word) & Mask) != 0) ? (char) 1 : (char) 0;
            buffer[bufferIndex++] = bitResult;
            buffer[bufferIndex++] = (char) 0;
            ++word;
        }
        flush();
        numSamples -= bufferSize / WordSize;
    }
    for (int i = 0; i < numSamples; ++i) {
        char bitResult = (((*word) & Mask) != 0) ? (char) 1 : (char) 0;
        buffer[bufferIndex++] = (char) bitResult;
        buffer[bufferIndex++] = (char) 0;
        ++word;
    }
}

void SaveFile::writeUInt16StimData(const uint16_t* wordArray, int numSamples, uint8_t posAmplitude, uint8_t negAmplitude)
{
    const uint16_t* word = wordArray;
    const int WordSize = 2;
    if (bufferIndex > bufferSize - WordSize * numSamples) flush();
    while (WordSize * numSamples > bufferSize) {
        for (int i = 0; i < bufferSize / WordSize; ++i) {
            uint16_t stimWord = (*word) & 0xfffeU;     // Set LSB (stim on marker) to zero.
            bool stimOn = ((*word) & 0x0001U) != 0;
            if (stimOn) {   // If stim on, add amplitude to 8 LSBs.
                bool polarityIsNegative = (stimWord & 0x0100U) != 0;
                stimWord = stimWord | (polarityIsNegative ? negAmplitude : posAmplitude);
            } else {
                stimWord = stimWord & 0xfe00U;  // Zero out polarity bit if stim is off.
            }
            buffer[bufferIndex++] = (char)  (stimWord & 0x00ffU);
            buffer[bufferIndex++] = (char) ((stimWord & 0xff00U) >> 8);
            ++word;
        }
        flush();
        numSamples -= bufferSize / WordSize;
    }
    for (int i = 0; i < numSamples; ++i) {
        uint16_t stimWord = (*word) & 0xfffeU;     // Set LSB (stim on marker) to zero.
        bool stimOn = ((*word) & 0x0001U) != 0;
        if (stimOn) {   // If stim on, add amplitude to 8 LSBs.
            bool polarityIsNegative = (stimWord & 0x0100U) != 0;
            stimWord = stimWord | (polarityIsNegative ? negAmplitude : posAmplitude);
        } else {
            stimWord = stimWord & 0xfe00U;  // Zero out polarity bit if stim is off.
        }
        buffer[bufferIndex++] = (char)  (stimWord & 0x00ffU);
        buffer[bufferIndex++] = (char) ((stimWord & 0xff00U) >> 8);
        ++word;
    }
}

void SaveFile::writeUInt16StimDataArray(const uint16_t* wordArray, int numSamples, int numWaveforms,
                                        const std::vector<uint8_t>& posAmplitudes, const std::vector<uint8_t>& negAmplitudes)
{
    const uint16_t* word = wordArray;
    const int WordSize = 2;
    int numWords = numSamples * numWaveforms;
    if (bufferIndex > bufferSize - WordSize * numWords) flush();
    int waveformIndex = 0;
    while (WordSize * numWords > bufferSize) {
        for (int i = 0; i < bufferSize / WordSize; ++i) {
            uint16_t stimWord = (*word) & 0xfffeU;     // Set LSB (stim on marker) to zero.
            bool stimOn = ((*word) & 0x0001U) != 0;
            if (stimOn) {   // If stim on, add amplitude to 8 LSBs.
                bool polarityIsNegative = (stimWord & 0x0100U) != 0;
                stimWord = stimWord | (polarityIsNegative ? negAmplitudes[waveformIndex] : posAmplitudes[waveformIndex]);
            } else {
                stimWord = stimWord & 0xfe00U;  // Zero out polarity bit if stim is off.
            }
            buffer[bufferIndex++] = (char)  (stimWord & 0x00ffU);
            buffer[bufferIndex++] = (char) ((stimWord & 0xff00U) >> 8);
            ++word;
            ++waveformIndex;
            if (waveformIndex == numWaveforms) waveformIndex = 0;
        }
        flush();
        numWords -= bufferSize / WordSize;
    }
    for (int i = 0; i < numWords; ++i) {
        uint16_t stimWord = (*word) & 0xfffeU;     // Set LSB (stim on marker) to zero.
        bool stimOn = ((*word) & 0x0001U) != 0;
        if (stimOn) {   // If stim on, add amplitude to 8 LSBs.
            bool polarityIsNegative = (stimWord & 0x0100U) != 0;
            stimWord = stimWord | (polarityIsNegative ? negAmplitudes[waveformIndex] : posAmplitudes[waveformIndex]);
        } else {
            stimWord = stimWord & 0xfe00U;  // Zero out polarity bit if stim is off.
        }
        buffer[bufferIndex++] = (char)  (stimWord & 0x00ffU);
        buffer[bufferIndex++] = (char) ((stimWord & 0xff00U) >> 8);
        ++word;
        ++waveformIndex;
        if (waveformIndex == numWaveforms) waveformIndex = 0;
    }
    if (bufferIndex > bufferSize) {    // Check for overrun since this function can write large numbers of bytes. Note: Was previously >=, likely overreporting this error
        std::cerr << "SaveFile::writeUInt16StimDataArray: buffer size exceeded.\n";
    }
}

void SaveFile::writeUInt16AsSigned(const uint16_t* wordArray, int numSamples)
{
    const uint16_t* word = wordArray;
    const int WordSize = 2;
    if (bufferIndex > bufferSize - WordSize * numSamples) flush();
    while (WordSize * numSamples > bufferSize) {
        for (int i = 0; i < bufferSize / WordSize; ++i) {
            uint16_t asSigned = (*word) ^ 0x8000U;   // convert from offset to two's complement
            buffer[bufferIndex++] = (char)  (asSigned & 0x00ffU);
            buffer[bufferIndex++] = (char) ((asSigned & 0xff00U) >> 8);
            ++word;
        }
        flush();
        numSamples -= bufferSize / WordSize;
    }
    for (int i = 0; i < numSamples; ++i) {
        uint16_t asSigned = (*word) ^ 0x8000U;   // convert from offset to two's complement
        buffer[bufferIndex++] = (char)  (asSigned & 0x00ffU);
        buffer[bufferIndex++] = (char) ((asSigned & 0xff00U) >> 8);
        ++word;
    }
}

void SaveFile::writeUInt8(uint8_t byte)
{
    if (bufferIndex > bufferSize - 1) flush();
    buffer[bufferIndex++] = (char) byte;
}

void SaveFile::writeDouble(double x)
{
    if (bufferIndex > 0) flush();
    *dataStream << x;   // Low-performance write; this method is not fast like writeIntXX and writeUIntXX.
    numBytesWritten += 4; // There are 32 bits per double since we set floating point precision to single precision.
}

void SaveFile::writeQString(const QString& s)
{
    if (bufferIndex > 0) flush();
    *dataStream << s;   // Low-performance write; this method is not fast like writeIntXX and writeUIntXX.
    numBytesWritten += 4 + 2 * s.size();  // A QString consists of a 32-bit 'string length' field plus 16-bit characters.
}

void SaveFile::writeQStringAsAsciiText(const QString& s)
{
    if (bufferIndex > 0) flush();
    // Low-performance write; this method is not fast like writeIntXX and writeUIntXX.
    dataStream->writeRawData(s.toLatin1().constData(), s.length());
    numBytesWritten += s.size();
}

void SaveFile::writeStringAsCharArray(const std::string& s)
{
    int length = (int) s.length();
    if (bufferIndex > bufferSize - length) flush();
    for (int i = 0; i < length; ++i) {
        buffer[bufferIndex++] = s[i];
    }
    // Does not write 0 at end of string.
}

void SaveFile::writeSignalSources(const SignalSources* signalSources)
{
    writeInt16(signalSources->numGroups());
    for (int group = 0; group < signalSources->numGroups(); ++group) {
        writeSignalGroup(signalSources->groupByIndex(group));
    }
}

void SaveFile::writeSignalGroup(const SignalGroup* signalGroup)
{
    writeQString(signalGroup->getName());
    writeQString(signalGroup->getPrefix());
    writeInt16(signalGroup->isEnabled());
    writeInt16(signalGroup->numChannels());
    writeInt16(signalGroup->numChannels(AmplifierSignal));

    for (int i = 0; i < signalGroup->numChannels(); ++i) {
        Channel* channel = signalGroup->channelByIndex(i);
        writeQString(channel->getNativeName());
        writeQString(channel->getCustomName());
        writeInt16(channel->getNativeChannelNumber());
        writeInt16(channel->getUserOrder());
        int signalType = (int) channel->getSignalType();
        if (signalGroup->getControllerType() != ControllerStimRecord) {
            signalType = Channel::convertToRHDSignalType(channel->getSignalType());
        }
        writeInt16(signalType);
        writeInt16(channel->isEnabled() ? 1 : 0);
        writeInt16(channel->getChipChannel());
        if (signalGroup->getControllerType() == ControllerStimRecord) {
            writeInt16(channel->getCommandStream());  // TODO: eventually add to new RH? file format?
        }
        writeInt16(channel->getBoardStream());

        writeInt16(1);  // Always set to 'trigger on voltage threshold'
        writeInt16(channel->getSignalType() == AmplifierSignal ? channel->getSpikeThreshold() : 0);
        writeInt16(0);
        writeInt16(0);

        writeDouble(channel->getImpedanceMagnitude());
        writeDouble(channel->getImpedancePhase());
    }
}

void SaveFile::close()
{
    if (!file) return;
    flush();
    file->close();
    if (dataStream) {
        delete dataStream;
        dataStream = nullptr;
    }
    delete file;
    file = nullptr;
}

void SaveFile::flush()
{
    if (!dataStream) return;
    dataStream->writeRawData(buffer, bufferIndex);
    numBytesWritten += bufferIndex;
    bufferIndex = 0;
}


// For Windows 10, it appears that there's an internal buffer of 16 KB when writing to files.
// This is most cumbersome when writing spike.dat files, which can go long periods with minimal data writing.
// Even when flushing, if less than 16 KB of data is to be written, it doesn't appear to actually get written,
// unless the file is closed. A flush can thus be forced by closing then reopening the file.
void SaveFile::forceFlush()
{
    if (!file) return;
    flush();
    file->close();
    if (dataStream) {
        delete dataStream;
        dataStream = nullptr;
    }

    if (!file->open(QIODevice::Append)) {
        std::cerr << "SaveFile:: Cannot open file " << fileName.toStdString() << " for writing: " <<
                qPrintable(file->errorString()) << '\n';
        if (file) delete file;
        file = nullptr;
        return;
    }
    createDataStream(file);
}

void SaveFile::openForAppend()
{
    if (isOpen()) return;

    file = new QFile(fileName);
    if (!file->open(QIODevice::Append)) {
        std::cerr << "SaveFile: Cannot open file " << fileName.toStdString() << " for appended writing: " <<
                qPrintable(file->errorString()) << '\n';
        if (file) delete file;
        file = nullptr;
        return;
    }
    createDataStream(file);
}

void SaveFile::createDataStream(QFile* file_)
{
    dataStream = new QDataStream(file_);

    // Maintain bit-level compatibility with existing code.
    dataStream->setVersion(QDataStream::Qt_5_11);

    // Set to little endian mode for compatibilty with MATLAB, which is little endian on all platforms.
    dataStream->setByteOrder(QDataStream::LittleEndian);

    // Write 4-byte floating-point numbers (instead of the default 8-byte numbers) to save disk space.
    dataStream->setFloatingPointPrecision(QDataStream::SinglePrecision);
}

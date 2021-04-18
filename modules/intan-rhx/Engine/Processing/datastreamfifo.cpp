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

#include <iostream>
#include <cstring>
#include "rhxglobals.h"
#include "datastreamfifo.h"

using namespace std;

// Create a circular buffer for USB data.  If data will be read using a pointer returned from
// pointerToData(), then a maxReadLength must be defined to allocate extra space beyond the 'end'
// of the circular buffer to maintain contiguous data arrays during these reads.  If data will only
// be read using readFromBuffer(), then maxReadLength can be omitted.
DataStreamFifo::DataStreamFifo(int bufferSize_, int maxReadLength_) :
    bufferSize(bufferSize_),
    maxReadLength(maxReadLength_)
{
    int bufferSizeWithExtra = bufferSize + maxReadLength;
    memoryNeededGB = sizeof(uint16_t) * bufferSizeWithExtra / (1024.0 * 1024.0 * 1024.0);
    cout << "DataStreamFifo: Allocating " << 2 * bufferSizeWithExtra / 1.0e6 << " MBytes for FIFO buffer." << '\n';
    buffer = nullptr;

    memoryAllocated = true;
    try {
        buffer = new uint16_t [bufferSizeWithExtra];
    } catch (std::bad_alloc&) {
        memoryAllocated = false;
        cerr << "Error: DataStreamFifo constructor could not allocate " << memoryNeededGB << " GB of memory." << '\n';
    }

    if (!buffer) {
        cerr << "Error: DataStreamFifo constructor could not allocate " << 2 * bufferSizeWithExtra << " bytes of memory." << '\n';
    }
    resetBuffer();
}

DataStreamFifo::~DataStreamFifo()
{
    delete [] buffer;
}

bool DataStreamFifo::writeToBuffer(const uint8_t* dataSource, int numWords)
{
    const uint8_t* pRead = dataSource;
    uint16_t highByte, lowByte;

    if (freeWords.tryAcquire(numWords)) {
        for (int i = 0; i < numWords; ++i) {
            // TODO: Try using bitfields or unions to speed up 2 x byte --> uint16.
            lowByte = (uint16_t) (*pRead);
            pRead++;
            highByte = (uint16_t) (*pRead);
            pRead++;
            buffer[bufferWriteIndex++] = lowByte | (highByte << 8);
            if (bufferWriteIndex >= bufferSize) {
                bufferWriteIndex = 0;
            }
        }
        usedWords.release(numWords);
        return true;
    } else {
        cerr << "DataStreamFifo: Buffer overrun on request of " << numWords << " words." << '\n';
        cerr << "   ...only " << freeWords.available() << " words are available." << '\n';
        return false;  // Buffer overrun error
    }
}

bool DataStreamFifo::dataAvailable(unsigned int numWords) const
{
    return ((unsigned int)(usedWords.available()) >= numWords);
}

int DataStreamFifo::wordsAvailable() const
{
    return usedWords.available();
}

double DataStreamFifo::percentFull() const
{
    return 100.0 * ((double)usedWords.available() / (double)bufferSize);
}

// Copy numWords of data from the circular buffer to memory location dataSink.
bool DataStreamFifo::readFromBuffer(uint16_t *dataSink, int numWords)
{
    if (!usedWords.tryAcquire(numWords)) {
        return false;  // Not enough data available in buffer
    }

    if (bufferReadIndex + numWords <= bufferSize) {
        std::memcpy(dataSink, &buffer[bufferReadIndex], BytesPerWord * numWords);
        bufferReadIndex += numWords;
        if (bufferReadIndex == bufferSize) {
            bufferReadIndex = 0;
        }
    } else {
        int numWordsFirstPart = bufferSize - bufferReadIndex;
        std::memcpy(dataSink, &buffer[bufferReadIndex], BytesPerWord * numWordsFirstPart);
        bufferReadIndex = 0;
        int numWordsSecondPart = numWords - numWordsFirstPart;
        std::memcpy(&dataSink[numWordsFirstPart], buffer, BytesPerWord * numWordsSecondPart);
        bufferReadIndex = numWordsSecondPart;
    }
    freeWords.release(numWords);
    return true;
}

// Alternate method of reading data: Return a pointer to the data in the circular buffer, extending
// the data beyond the 'end' of the buffer if necessary to ensure a contiguous array.  We assume that
// the user will call freeData(numWordsToRead) after reading the data at this location.
// This method returns nullptr if there is insufficient data in the circular buffer.
uint16_t* DataStreamFifo::pointerToData(int numWordsBeToRead_)
{
    numWordsToBeRead = numWordsBeToRead_;
    if (numWordsToBeRead > maxReadLength) {
        cerr << "DataStreamFifo::pointerToData: numWordsToBeRead exceeds maxReadLength." << '\n';
        return nullptr;
    }
    if (!usedWords.tryAcquire(numWordsToBeRead)) {
        return nullptr;  // not enough data available to read
    }
    if (bufferReadIndex + numWordsToBeRead > bufferSize) {
        // Our read will overrun the end of the buffer; copy data to the extra space allocated after the
        // 'end' of the buffer to ensure a contiguous array of data for reading.
//        cout << "DataStreamFifo::pointerToData: copying data to ensure contiguous array of data for reading." << EndOfLine;
        int extraWords = bufferReadIndex + numWordsToBeRead - bufferSize;
        std::memcpy(&buffer[bufferSize], &buffer[0], BytesPerWord * extraWords);
        // Note: You can avoid this potentially time-consuming memory copy by always reading the same
        // number of words, and making the buffer size an integer multiple of this number.
    }

    return &buffer[bufferReadIndex];
}

// This function should only be called after first calling pointerToData() and then reading
// the data returned by that pointer.
void DataStreamFifo::freeData()
{
    bufferReadIndex = (bufferReadIndex + numWordsToBeRead) % bufferSize; // okay to use % operator since first quantity must be positive
    freeWords.release(numWordsToBeRead);
}

void DataStreamFifo::resetBuffer()
{
    freeWords.acquire(freeWords.available());
    usedWords.acquire(usedWords.available());
    bufferWriteIndex = 0;
    bufferReadIndex = 0;
    freeWords.release(bufferSize);
    numWordsToBeRead = 0;
}


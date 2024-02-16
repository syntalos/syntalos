//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.3.1
//
//  Copyright (c) 2020-2023 Intan Technologies
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
#ifndef DATASTREAMFIFO_H
#define DATASTREAMFIFO_H

#include "rhx-semaphore.h"

class DataStreamFifo
{
public:
    DataStreamFifo(int bufferSize_, int maxReadLength_ = 0);
    ~DataStreamFifo();

    bool writeToBuffer(const uint8_t* dataSource, int numWords,
                       uint64_t timestamp = 0, bool writeTimestamp = false);
    bool dataAvailable(unsigned int numWords) const;

    bool readFromBuffer(uint16_t *dataSink, int numWords);
    uint16_t* pointerToData(int numWordsToBeRead_);
    void freeData();

    void resetBuffer();
    int wordsAvailable() const;
    double percentFull() const;

    bool memoryWasAllocated(double& memoryRequestedGB) const { memoryRequestedGB += memoryNeededGB; return memoryAllocated; }

private:
    uint16_t* buffer;
    int bufferSize;
    int maxReadLength;
    int numWordsToBeRead;
    Semaphore freeWords;
    Semaphore usedWords;
    int bufferWriteIndex;
    int bufferReadIndex;

    bool memoryAllocated;
    double memoryNeededGB;
};

#endif // DATASTREAMFIFO_H

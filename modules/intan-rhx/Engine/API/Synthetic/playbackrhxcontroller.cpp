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
#include "playbackrhxcontroller.h"

PlaybackRHXController::PlaybackRHXController(ControllerType type_, AmplifierSampleRate sampleRate_, DataFileReader* dataFileReader_) :
    AbstractRHXController(type_, sampleRate_),
    dataFileReader(dataFileReader_)
{
}

PlaybackRHXController::~PlaybackRHXController()
{
}

// For a physical board, read data block from the USB interface. Fill given dataBlock from USB buffer.
bool PlaybackRHXController::readDataBlock(RHXDataBlock *dataBlock)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    unsigned int numBytesToRead = BytesPerWord * RHXDataBlock::dataBlockSizeInWords(type, numDataStreams);

    if (numBytesToRead > usbBufferSize) {
        std::cerr << "Error in PlaybackRHXController::readDataBlock: USB buffer size exceeded.  " <<
                "Increase value of MAX_NUM_BLOCKS.\n";
        return false;
    }

    dataBlock->fillFromUsbBuffer(usbBuffer, 0);

    return true;
}

// For a physical board, read a certain number of USB data blocks, and append them to queue.
// Return true if data blocks were available.
bool PlaybackRHXController::readDataBlocks(int numBlocks, std::deque<RHXDataBlock*> &dataQueue)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    unsigned int numWordsToRead = numBlocks * RHXDataBlock::dataBlockSizeInWords(type, numDataStreams);

    if (numWordsInFifo() < numWordsToRead)
        return false;

    unsigned int numBytesToRead = BytesPerWord * numWordsToRead;

    if (numBytesToRead > usbBufferSize) {
        std::cerr << "Error in PlaybackRHXController::readDataBlocks: USB buffer size exceeded.  " <<
                "Increase value of MAX_NUM_BLOCKS.\n";
        return false;
    }

    for (int i = 0; i < numBlocks; ++i) {
        RHXDataBlock* dataBlock = new RHXDataBlock(type, numDataStreams);
        dataBlock->fillFromUsbBuffer(usbBuffer, i);
        dataQueue.push_back(dataBlock);
    }

    return true;
}

// For a physical board, read a certain number of USB data blocks, and write the raw bytes to a buffer.
// Return total number of bytes read.
long PlaybackRHXController::readDataBlocksRaw(int numBlocks, uint8_t *buffer)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    return dataFileReader->readPlaybackDataBlocksRaw(numBlocks, buffer);
}

// Set the delay for sampling the MISO line on a particular SPI port (PortA - PortH), in integer clock steps, where each
// clock step is 1/2800 of a per-channel sampling period.  Note: Cable delay must be updated after sampleRate is changed,
// since cable delay calculations are based on the clock frequency!
void PlaybackRHXController::setCableDelay(BoardPort port, int delay)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    if ((delay < 0) || (delay > 15)) {
        std::cerr << "Warning in PlaybackRHXController::setCableDelay: delay out of range: " << delay << '\n';
        if (delay < 0) delay = 0;
        else if (delay > 15) delay = 15;
    }

    switch (port) {
    case PortA:
        cableDelay[0] = delay;
        break;
    case PortB:
        cableDelay[1] = delay;
        break;
    case PortC:
        cableDelay[2] = delay;
        break;
    case PortD:
        cableDelay[3] = delay;
        break;
    case PortE:
        cableDelay[4] = delay;
        break;
    case PortF:
        cableDelay[5] = delay;
        break;
    case PortG:
        cableDelay[6] = delay;
        break;
    case PortH:
        cableDelay[7] = delay;
        break;
    default:
        std::cerr << "Error in PlaybackRHXController::setCableDelay: unknown port.\n";
    }
}

// Assign a particular data source (e.g., PortA1, PortA2, PortB1,...) to one of the eight available USB data streams (0-7).
// Used only with ControllerRecordUSB2.
void PlaybackRHXController::setDataSource(int stream, BoardDataSource dataSource)
{
    if (type != ControllerRecordUSB2) return;

    if ((stream < 0) || (stream > 7)) {
        std::cerr << "Error in PlaybackRHXController::setDataSource: stream out of range.\n";
        return;
    }
    boardDataSources[stream] = dataSource;
}

// Set the per-channel sampling rate of the RHD/RHS chips connected to the FPGA.
bool PlaybackRHXController::setSampleRate(AmplifierSampleRate newSampleRate)
{
    std::lock_guard<std::mutex> lockOk(okMutex);
    sampleRate = newSampleRate;
    return true;
}

// Enable or disable one of the 32 available USB data streams (0-31).
void PlaybackRHXController::enableDataStream(int stream, bool enabled)
{
    std::lock_guard<std::mutex> lockOk(okMutex);

    if (stream < 0 || stream > (maxNumDataStreams() - 1)) {
        std::cerr << "Error in PlaybackRHXController::enableDataStream: stream out of range.\n";
        return;
    }

    if (enabled) {
        if (dataStreamEnabled[stream] == 0) {
            dataStreamEnabled[stream] = 1;
            numDataStreams++;
        }
    } else {
        if (dataStreamEnabled[stream] == 1) {
            dataStreamEnabled[stream] = 0;
            numDataStreams--;
        }
    }
}

// Return 4-bit "board mode" input.
int PlaybackRHXController::getBoardMode()
{
    std::lock_guard<std::mutex> lockOk(okMutex);
    return boardMode(type);
}

// Return number of SPI ports and if I/O expander board is present.
int PlaybackRHXController::getNumSPIPorts(bool &expanderBoardDetected)
{
    expanderBoardDetected = true;
    return (type == ControllerRecordUSB3 ? 8 : 4);
}

// Not used in playback mode.
int PlaybackRHXController::findConnectedChips(std::vector<ChipType>& /* chipType */, std::vector<int>& /* portIndex */,
                                              std::vector<int>& /* commandStream */, std::vector<int>& /* numChannelsOnPort */,
                                              bool /* synthMaxChannels */, bool /* returnToFastSettle */,
                                              bool /* usePreviousDelay */, int /* selectedPort */, int /* lastDetectedChip */,
                                              int /* lastDetectedNumStreams */)
{
    return 1;
}

// Return the number of 16-bit words in the USB FIFO.  The user should never attempt to read more data than the
// FIFO currently contains, as it is not protected against underflow.
unsigned int PlaybackRHXController::numWordsInFifo()
{
    numWordsHasBeenUpdated = true;
    return lastNumWordsInFifo;
}

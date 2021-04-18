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
#include "intanfilesavemanager.h"

using namespace std;

// Intan save file format (*.rhd, *.rhs)
IntanFileSaveManager::IntanFileSaveManager(WaveformFifo* waveformFifo_, SystemState* state_) :
    SaveManager(waveformFifo_, state_),
    saveFile(nullptr),
    subdirName("")
{
}

IntanFileSaveManager::~IntanFileSaveManager()
{
    closeAllSaveFiles();
}

bool IntanFileSaveManager::openAllSaveFiles()
{
    dateTimeStamp = getDateTimeStamp();
    bool firstTime = (subdirName == "");
    if (firstTime) {  // If subdirectory does not yet exist, create one with initial time/date stamp.
        subdirName = state->filename->getBaseFilename() + dateTimeStamp;
        QDir dir(state->filename->getPath());
        if (!dir.mkdir(subdirName)) {
            return false;       // Cannot create subdirectory.
        }
    }
    QString subdirPath = state->filename->getPath() + "/" + subdirName + "/";
    if (firstTime) {
        // Write settings file.
        state->saveGlobalSettings(subdirPath + "settings.xml");
    }

    saveFile = new SaveFile(subdirPath + state->filename->getBaseFilename() + dateTimeStamp + intanFileExtension());
    if (!saveFile->isOpen()) {
        closeAllSaveFiles();
        return false;
    }
    liveNotesFileName = subdirPath + "notes.txt";
    writeIntanFileHeader(saveFile);
    getAllWaveformPointers();
    return true;
}

void IntanFileSaveManager::closeAllSaveFiles()
{
    if (liveNotesFile) {
        liveNotesFile->close();
        delete liveNotesFile;
        liveNotesFile = nullptr;
    }

    if (saveFile) {
        saveFile->close();
        delete saveFile;
        saveFile = nullptr;
    }
}

int64_t IntanFileSaveManager::writeToSaveFiles(int numSamples, int timeIndex)
{
    float* vArray = new float [numSamples];
    uint16_t* uint16Array = new uint16_t [numSamples];
    int samplesPerDataBlock = RHXDataBlock::samplesPerDataBlock(type);

    for (int block = 0; block < numSamples / samplesPerDataBlock; ++block) {
        // Save timestamp data.
        for (int t = 0; t < samplesPerDataBlock; ++t) {
            saveFile->writeInt32((int) waveformFifo->getTimeStamp(WaveformFifo::ReaderDisk, timeIndex + t) - timeStampOffset);
        }

        // Save amplifier data.
        for (int i = 0; i < (int) saveList.amplifier.size(); ++i) {
            waveformFifo->copyGpuAmplifierDataRaw(WaveformFifo::ReaderDisk, uint16Array, amplifierGPUWaveform[i], timeIndex, samplesPerDataBlock);
            saveFile->writeUInt16(uint16Array, samplesPerDataBlock);
        }

        if (type == ControllerStimRecordUSB2) {
            // Save DC amplifier data.
            if (state->saveDCAmplifierWaveforms->getValue()) {
                for (int i = 0; i < (int) saveList.amplifier.size(); ++i) {
                    waveformFifo->copyAnalogData(WaveformFifo::ReaderDisk, vArray, dcAmplifierWaveform[i], timeIndex, samplesPerDataBlock);
                    convertDcAmplifierValue(uint16Array, vArray, samplesPerDataBlock);
                    saveFile->writeUInt16(uint16Array, samplesPerDataBlock);
                }
            }

            // Save stimulation data.
            for (int i = 0; i < (int) saveList.amplifier.size(); ++i) {
                waveformFifo->copyDigitalData(WaveformFifo::ReaderDisk, uint16Array, stimFlagsWaveform[i], timeIndex, samplesPerDataBlock);
                saveFile->writeUInt16StimData(uint16Array, samplesPerDataBlock, posStimAmplitudes[i], negStimAmplitudes[i]);
            }
        }

        if (type != ControllerStimRecordUSB2) {
            // Save auxiliary input data.
            float v;
            for (int i = 0; i < (int) saveList.auxInput.size(); ++i) {
                for (int t = 0; t < samplesPerDataBlock; t += 4) {
                    v = waveformFifo->getAnalogData(WaveformFifo::ReaderDisk, auxInputWaveform[i], timeIndex + t);
                    saveFile->writeUInt16(convertAuxInputValue(v));
                }
            }

            // Save supply voltage data.
            for (int i = 0; i < (int) saveList.supplyVoltage.size(); ++i) {
                for (int t = 0; t < samplesPerDataBlock; t += samplesPerDataBlock) {
                    v = waveformFifo->getAnalogData(WaveformFifo::ReaderDisk, supplyVoltageWaveform[i], timeIndex + t);
                    saveFile->writeUInt16(convertSupplyVoltageValue(v));
                }
            }
        }

        // Save board ADC data.
        for (int i = 0; i < (int) saveList.boardAdc.size(); ++i) {
            waveformFifo->copyAnalogData(WaveformFifo::ReaderDisk, vArray, boardAdcWaveform[i], timeIndex, samplesPerDataBlock);
            convertBoardAdcValue(uint16Array, vArray, samplesPerDataBlock);
            saveFile->writeUInt16(uint16Array, samplesPerDataBlock);
        }

        if (type == ControllerStimRecordUSB2) {
            // Save board DAC data.
            for (int i = 0; i < (int) saveList.boardDac.size(); ++i) {
                waveformFifo->copyAnalogData(WaveformFifo::ReaderDisk, vArray, boardDacWaveform[i], timeIndex, samplesPerDataBlock);
                convertBoardDacValue(uint16Array, vArray, samplesPerDataBlock);
                saveFile->writeUInt16(uint16Array, samplesPerDataBlock);
            }
        }

        // Save board digital input data.
        if (!saveList.boardDigitalIn.empty()) {
            // If ANY digital inputs are enabled, we save ALL 16 channels, since we are writing 16-bit chunks of data.
            waveformFifo->copyDigitalData(WaveformFifo::ReaderDisk, uint16Array, boardDigitalInWaveform, timeIndex, samplesPerDataBlock);
            saveFile->writeUInt16(uint16Array, samplesPerDataBlock);
        }

        // Save board digital output data, optionally.
        if (!saveList.boardDigitalOut.empty()) {
            // Save all 16 channels, since we are writing 16-bit chunks of data.
            waveformFifo->copyDigitalData(WaveformFifo::ReaderDisk, uint16Array, boardDigitalOutWaveform, timeIndex, samplesPerDataBlock);
            saveFile->writeUInt16(uint16Array, samplesPerDataBlock);
        }

        timeIndex += samplesPerDataBlock;
    }

    delete [] vArray;
    delete [] uint16Array;

    return saveFile->getNumBytesWritten();
}

int IntanFileSaveManager::maxSamplesInFile() const
{
    return round(60 * state->newSaveFilePeriodMinutes->getValue() * state->sampleRate->getNumericValue());
}

double IntanFileSaveManager::bytesPerMinute() const
{
    double bytes = 0.0;
    bytes += 4.0; // timestamp
    bytes += 2.0 * saveList.amplifier.size();
    bytes += 2.0 * (double) saveList.auxInput.size() / 4.0;
    bytes += 2.0 * (double) saveList.supplyVoltage.size() / (double) RHXDataBlock::samplesPerDataBlock(type);
    bytes += 2.0 * saveList.boardAdc.size();
    if (type == ControllerStimRecordUSB2) {
        if (state->saveDCAmplifierWaveforms->getValue()) {
            bytes += 2.0 * saveList.amplifier.size();
        }
        bytes += 2.0 * count(saveList.stimEnabled.begin(), saveList.stimEnabled.end(), true);
        bytes += 2.0 * saveList.boardDac.size();
    }
    if (!saveList.boardDigitalIn.empty()) {
        bytes += 2.0;
    }
    if (!saveList.boardDigitalOut.empty()) {
        bytes += 2.0;
    }
    double samplesPerMinute = 60.0 * state->sampleRate->getNumericValue();
    return bytes * samplesPerMinute;
}

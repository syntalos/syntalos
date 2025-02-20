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
#include "fileperchannelsavemanager.h"

// One file per signal type file format
FilePerChannelSaveManager::FilePerChannelSaveManager(WaveformFifo* waveformFifo_, SystemState* state_) :
    SaveManager(waveformFifo_, state_),
    infoFile(nullptr),
    timeStampFile(nullptr)
{
    saveSpikeSnapshot = false;
    samplesPreDetect = 0;
    samplesPostDetect = 0;

    tenthOfSecondTimestamps = (int) round(state->sampleRate->getNumericValue() / 10);

    int numSpikeChannels = (int) state->signalSources->getSaveSignalList().amplifier.size();
    if (state->saveSpikeData->getValue() && numSpikeChannels > 0) {
        spikeCounter = new int[numSpikeChannels];
        mostRecentSpikeTimestamp = new int[numSpikeChannels];
        lastForceFlushTimestamp = new int[numSpikeChannels];
        for (int i = 0; i < numSpikeChannels; ++i) {
            spikeCounter[i] = 0;
            mostRecentSpikeTimestamp[i] = 0;
            lastForceFlushTimestamp[i] = 0;
        }
    } else {
        spikeCounter = nullptr;
        mostRecentSpikeTimestamp = nullptr;
        lastForceFlushTimestamp = nullptr;
    }
}

FilePerChannelSaveManager::~FilePerChannelSaveManager()
{
    if (spikeCounter)
        delete [] spikeCounter;
    if (mostRecentSpikeTimestamp)
        delete [] mostRecentSpikeTimestamp;
    if (lastForceFlushTimestamp)
        delete [] lastForceFlushTimestamp;
}

bool FilePerChannelSaveManager::openAllSaveFiles()
{
    const QString DataFileExtension = ".dat";
    int bufferSize = calculateBufferSize(state);
    //int bufferSize = 128;

    dateTimeStamp = getDateTimeStamp();

    QString subdirName, subdirPath;
    if (state->createNewDirectory->getValue()) {
        subdirName = state->filename->getBaseFilename() + dateTimeStamp;
        QDir dir(state->filename->getPath());
        if (!dir.mkdir(subdirName)) {
            return false; // Cannot create subdirectory.
        }
        subdirPath = state->filename->getPath() + "/" + subdirName + "/";
    } else {
        subdirName = state->filename->getFullFilename();
        subdirPath = subdirName + "/";
    }

    // Write settings file.
    state->saveGlobalSettings(subdirPath + "settings.xml");

    liveNotesFileName = subdirPath + "notes.txt";
    infoFile = new SaveFile(subdirPath + "info" + intanFileExtension(), bufferSize);
    if (!infoFile->isOpen()) {
        return false;
    }
    timeStampFile = new SaveFile(subdirPath + "time" + DataFileExtension, bufferSize);
    if (!timeStampFile->isOpen()) {
        return false;
    }

    getAllWaveformPointers();

    for (int i = 0; i < (int) saveList.amplifier.size(); ++i) {
        if (state->saveWidebandAmplifierWaveforms->getValue()) {
            amplifierFiles.push_back(new SaveFile(subdirPath + "amp-" + QString::fromStdString(saveList.amplifier[i]) +
                                                  DataFileExtension, bufferSize));
            if (!amplifierFiles.back()->isOpen()) {
                closeAllSaveFiles();
                return false;
            }
        }
        if (state->saveLowpassAmplifierWaveforms->getValue()) {
            lowpassAmplifierFiles.push_back(new SaveFile(subdirPath + "low-" + QString::fromStdString(saveList.amplifier[i]) +
                                                         DataFileExtension, bufferSize));
            if (!lowpassAmplifierFiles.back()->isOpen()) {
                closeAllSaveFiles();
                return false;
            }
        }
        if (state->saveHighpassAmplifierWaveforms->getValue()) {
            highpassAmplifierFiles.push_back(new SaveFile(subdirPath + "high-" + QString::fromStdString(saveList.amplifier[i]) +
                                                          DataFileExtension, bufferSize));
            if (!highpassAmplifierFiles.back()->isOpen()) {
                closeAllSaveFiles();
                return false;
            }
        }
        if (state->saveSpikeData->getValue()) {
            spikeFiles.push_back(new SaveFile(subdirPath + "spike-" + QString::fromStdString(saveList.amplifier[i]) +
                                              DataFileExtension, bufferSize));
            if (!spikeFiles.back()->isOpen()) {
                closeAllSaveFiles();
                return false;
            }

            //  Write spike file header.

            SaveFile* spikeFile = spikeFiles.back();

            spikeFile->writeUInt32(SpikeFileMagicNumberSingleChannel);

            const uint16_t SpikeFileVersionNumber = 1;
            spikeFile->writeUInt16(SpikeFileVersionNumber);

            // Write base filename with time/date stamp
            spikeFile->writeStringAsCharArray(subdirName.toStdString());
            spikeFile->writeUInt8(0);  // 0 to terminate string

            // Write amplifier native name as zero-terminated string.
            spikeFile->writeStringAsCharArray(saveList.amplifier[i]);
            spikeFile->writeUInt8(0);  // 0 to terminate string

            // Write amplifier custom names as comma-separated list, zero-terminated string.
            Channel* channel = state->signalSources->channelByName(saveList.amplifier[i]);
            std::string customName = "";
            if (channel) customName = channel->getCustomName().toStdString();
            spikeFile->writeStringAsCharArray(customName);
            spikeFile->writeUInt8(0);  // 0 to terminate string

            double sampleRate = state->sampleRate->getNumericValue();
            spikeFile->writeDouble(sampleRate);  // Write sample rate.

            // Write number of pre-detect samples and number of post-detect samples in spike snapshots.
            saveSpikeSnapshot = state->saveSpikeSnapshots->getValue();
            if (saveSpikeSnapshot) {
                double samplesPerMillisecond = sampleRate / 1000.0;
                samplesPreDetect = round(-(double)state->spikeSnapshotPreDetect->getValue() * samplesPerMillisecond);
                samplesPostDetect = round((double)state->spikeSnapshotPostDetect->getValue() * samplesPerMillisecond);
            } else {
                samplesPreDetect = 0;
                samplesPostDetect = 0;
            }
            spikeFile->writeUInt32(samplesPreDetect);
            spikeFile->writeUInt32(samplesPostDetect);
        }
        if (type == ControllerStimRecord) {
            if (saveList.stimEnabled[i]) {
                stimFiles.push_back(new SaveFile(subdirPath + "stim-" + QString::fromStdString(saveList.amplifier[i]) +
                                                 DataFileExtension, bufferSize));
                if (!stimFiles.back()->isOpen()) {
                    closeAllSaveFiles();
                    return false;
                }
            }
            if (state->saveDCAmplifierWaveforms->getValue()) {
                dcAmplifierFiles.push_back(new SaveFile(subdirPath + "dc-" + QString::fromStdString(saveList.amplifier[i]) +
                                                        DataFileExtension, bufferSize));
                if (!dcAmplifierFiles.back()->isOpen()) {
                    closeAllSaveFiles();
                    return false;
                }
            }
        }
    }
    if (type != ControllerStimRecord) {
        for (int i = 0; i < (int) saveList.auxInput.size(); ++i) {
            auxInputFiles.push_back(new SaveFile(subdirPath + "aux-" + QString::fromStdString(saveList.auxInput[i]) +
                                                 DataFileExtension, bufferSize));
            if (!auxInputFiles.back()->isOpen()) {
                closeAllSaveFiles();
                return false;
            }
        }
        for (int i = 0; i < (int) saveList.supplyVoltage.size(); ++i) {
            supplyVoltageFiles.push_back(new SaveFile(subdirPath + "vdd-" + QString::fromStdString(saveList.supplyVoltage[i]) +
                                                      DataFileExtension, bufferSize));
            if (!supplyVoltageFiles.back()->isOpen()) {
                closeAllSaveFiles();
                return false;
            }
        }
    }
    for (int i = 0; i < (int) saveList.boardAdc.size(); ++i) {
        analogInputFiles.push_back(new SaveFile(subdirPath + "board-" + QString::fromStdString(saveList.boardAdc[i]) +
                                                DataFileExtension, bufferSize));
        if (!analogInputFiles.back()->isOpen()) {
            closeAllSaveFiles();
            return false;
        }
    }
    if (type == ControllerStimRecord) {
        for (int i = 0; i < (int) saveList.boardDac.size(); ++i) {
            analogOutputFiles.push_back(new SaveFile(subdirPath + "board-" + QString::fromStdString(saveList.boardDac[i]) +
                                                     DataFileExtension, bufferSize));
            if (!analogOutputFiles.back()->isOpen()) {
                closeAllSaveFiles();
                return false;
            }
        }
    }
    for (int i = 0; i < (int) saveList.boardDigitalIn.size(); ++i) {
        digitalInputFiles.push_back(new SaveFile(subdirPath + "board-" + QString::fromStdString(saveList.boardDigitalIn[i]) +
                                                 DataFileExtension, bufferSize));
        if (!digitalInputFiles.back()->isOpen()) {
            closeAllSaveFiles();
            return false;
        }
        digitalInputFileIndices.push_back(signalSources->channelByName(saveList.boardDigitalIn[i])->getNativeChannelNumber());
    }
    if (!saveList.boardDigitalOut.empty()) {
        for (int i = 0; i < (int) saveList.boardDigitalOut.size(); ++i) {
            digitalOutputFiles.push_back(new SaveFile(subdirPath + "board-" + QString::fromStdString(saveList.boardDigitalOut[i]) +
                                                      DataFileExtension, bufferSize));
            if (!digitalOutputFiles.back()->isOpen()) {
                closeAllSaveFiles();
                return false;
            }
            digitalOutputFileIndices.push_back(signalSources->channelByName(saveList.boardDigitalOut[i])->getNativeChannelNumber());
        }
    }

    writeIntanFileHeader(infoFile);
    infoFile->close();
    return true;
}

void FilePerChannelSaveManager::closeAllSaveFiles()
{
    if (liveNotesFile) {
        liveNotesFile->close();
        delete liveNotesFile;
        liveNotesFile = nullptr;
    }

    if (timeStampFile) {
        timeStampFile->close();
        delete timeStampFile;
        timeStampFile = nullptr;
    }

    for (int i = 0; i < (int) amplifierFiles.size(); ++i) {
        if (amplifierFiles[i]) {
            amplifierFiles[i]->close();
            delete amplifierFiles[i];
            amplifierFiles[i] = nullptr;
        }
    }
    amplifierFiles.clear();

    for (int i = 0; i < (int) lowpassAmplifierFiles.size(); ++i) {
        if (lowpassAmplifierFiles[i]) {
            lowpassAmplifierFiles[i]->close();
            delete lowpassAmplifierFiles[i];
            lowpassAmplifierFiles[i] = nullptr;
        }
    }
    lowpassAmplifierFiles.clear();

    for (int i = 0; i < (int) highpassAmplifierFiles.size(); ++i) {
        if (highpassAmplifierFiles[i]) {
            highpassAmplifierFiles[i]->close();
            delete highpassAmplifierFiles[i];
            highpassAmplifierFiles[i] = nullptr;
        }
    }
    highpassAmplifierFiles.clear();

    for (int i = 0; i < (int) spikeFiles.size(); ++i) {
        if (spikeFiles[i]) {
            spikeFiles[i]->close();
            delete spikeFiles[i];
            spikeFiles[i] = nullptr;
        }
    }
    spikeFiles.clear();

    for (int i = 0; i < (int) auxInputFiles.size(); ++i) {
        if (auxInputFiles[i]) {
            auxInputFiles[i]->close();
            delete auxInputFiles[i];
            auxInputFiles[i] = nullptr;
        }
    }
    auxInputFiles.clear();

    for (int i = 0; i < (int) supplyVoltageFiles.size(); ++i) {
        if (supplyVoltageFiles[i]) {
            supplyVoltageFiles[i]->close();
            delete supplyVoltageFiles[i];
            supplyVoltageFiles[i] = nullptr;
        }
    }
    supplyVoltageFiles.clear();

    for (int i = 0; i < (int) dcAmplifierFiles.size(); ++i) {
        if (dcAmplifierFiles[i]) {
            dcAmplifierFiles[i]->close();
            delete dcAmplifierFiles[i];
            dcAmplifierFiles[i] = nullptr;
        }
    }
    dcAmplifierFiles.clear();

    for (int i = 0; i < (int) stimFiles.size(); ++i) {
        if (stimFiles[i]) {
            stimFiles[i]->close();
            delete stimFiles[i];
            stimFiles[i] = nullptr;
        }
    }
    stimFiles.clear();

    for (int i = 0; i < (int) analogInputFiles.size(); ++i) {
        if (analogInputFiles[i]) {
            analogInputFiles[i]->close();
            delete analogInputFiles[i];
            analogInputFiles[i] = nullptr;
        }
    }
    analogInputFiles.clear();

    for (int i = 0; i < (int) analogOutputFiles.size(); ++i) {
        if (analogOutputFiles[i]) {
            analogOutputFiles[i]->close();
            delete analogOutputFiles[i];
            analogOutputFiles[i] = nullptr;
        }
    }
    analogOutputFiles.clear();

    for (int i = 0; i < (int) digitalInputFiles.size(); ++i) {
        if (digitalInputFiles[i]) {
            digitalInputFiles[i]->close();
            delete digitalInputFiles[i];
            digitalInputFiles[i] = nullptr;
        }
    }
    digitalInputFiles.clear();
    digitalInputFileIndices.clear();

    for (int i = 0; i < (int) digitalOutputFiles.size(); ++i) {
        if (digitalOutputFiles[i]) {
            digitalOutputFiles[i]->close();
            delete digitalOutputFiles[i];
            digitalOutputFiles[i] = nullptr;
        }
    }
    digitalOutputFiles.clear();
    digitalOutputFileIndices.clear();
}

int64_t FilePerChannelSaveManager::writeToSaveFiles(int numSamples, int timeIndex)
{
    float* vArray = new float [numSamples];
    uint16_t* uint16Array = new uint16_t [numSamples];
    int64_t numBytesWritten = 0;

    // Save timestamp data.
    for (int t = 0; t < numSamples; ++t) {
        timeStampFile->writeInt32((int) waveformFifo->getTimeStamp(WaveformFifo::ReaderDisk, timeIndex + t) - timeStampOffset);
    }
    numBytesWritten += timeStampFile->getNumBytesWritten();

    // Save amplifier data.
    int downsampleFactor = (int) state->lowpassWaveformDownsampleRate->getNumericValue();
    for (int i = 0; i < (int) saveList.amplifier.size(); ++i) {
        if (state->saveWidebandAmplifierWaveforms->getValue()) {
            waveformFifo->copyGpuAmplifierDataRaw(WaveformFifo::ReaderDisk, uint16Array, amplifierGPUWaveform[i], timeIndex,
                                                  numSamples);
            amplifierFiles[i]->writeUInt16AsSigned(uint16Array, numSamples);
            numBytesWritten += amplifierFiles[i]->getNumBytesWritten();
        }
        if (state->saveLowpassAmplifierWaveforms->getValue()) {
            waveformFifo->copyGpuAmplifierDataRaw(WaveformFifo::ReaderDisk, uint16Array, amplifierLowpassGPUWaveform[i], timeIndex,
                                                  numSamples, downsampleFactor);
            lowpassAmplifierFiles[i]->writeUInt16AsSigned(uint16Array, numSamples / downsampleFactor);
            numBytesWritten += lowpassAmplifierFiles[i]->getNumBytesWritten();
        }
        if (state->saveHighpassAmplifierWaveforms->getValue()) {
            waveformFifo->copyGpuAmplifierDataRaw(WaveformFifo::ReaderDisk, uint16Array, amplifierHighpassGPUWaveform[i], timeIndex,
                                                  numSamples);
            highpassAmplifierFiles[i]->writeUInt16AsSigned(uint16Array, numSamples);
            numBytesWritten += highpassAmplifierFiles[i]->getNumBytesWritten();
        }
    }

    // Save spike data.
    if (state->saveSpikeData->getValue()) {
        for (int i = 0; i < (int) saveList.amplifier.size(); ++i) {
            for (int t = timeIndex - samplesPostDetect; t < timeIndex + numSamples - samplesPostDetect; ++t) {
                uint8_t spikeId = (uint8_t) waveformFifo->getDigitalData(WaveformFifo::ReaderDisk, spikeWaveform[i], t);
                if (spikeId != SpikeIdNoSpike) {
                    mostRecentSpikeTimestamp[i] = waveformFifo->getTimeStamp(WaveformFifo::ReaderDisk, t) - timeStampOffset;
                    spikeFiles[i]->writeInt32(mostRecentSpikeTimestamp[i]); // Write 32-bit timestamp
                    spikeCounter[i]++;
                    spikeFiles[i]->writeUInt8(spikeId);     // Write 8-bit spike ID
                    if (saveSpikeSnapshot) {                // Optionally, write spike snapshot
                        for (int tSnap = t - samplesPreDetect; tSnap < t + samplesPostDetect; ++tSnap) {
                            spikeFiles[i]->writeUInt16(waveformFifo->getGpuAmplifierDataRaw(WaveformFifo::ReaderDisk,
                                                                                            amplifierHighpassGPUWaveform[i],
                                                                                            tSnap));
                        }
                    }
                }
            }
        }

        // Force flush all channel files for which enough spikes have accumulated and the last forced flush was at least 0.1 s ago
        for (int i = 0; i < (int) saveList.amplifier.size(); ++i) {
            if ((spikeCounter[i] >= 1) && (mostRecentSpikeTimestamp[i] - lastForceFlushTimestamp[i] >= tenthOfSecondTimestamps)) {
                spikeCounter[i] = 0;
                lastForceFlushTimestamp[i] = mostRecentSpikeTimestamp[i];
                spikeFiles[i]->forceFlush();
            }
        }
    }

    if (type == ControllerStimRecord) {
        // Save DC amplifier data.
        if (state->saveDCAmplifierWaveforms->getValue()) {
            for (int i = 0; i < (int) saveList.amplifier.size(); ++i) {
                waveformFifo->copyAnalogData(WaveformFifo::ReaderDisk, vArray, dcAmplifierWaveform[i], timeIndex, numSamples);
                convertDcAmplifierValue(uint16Array, vArray, numSamples);
                dcAmplifierFiles[i]->writeUInt16(uint16Array, numSamples);
                numBytesWritten += dcAmplifierFiles[i]->getNumBytesWritten();
            }
        }

        // Save stimulation data.
        int iFile = 0;  // Used to index stimFiles; stimFiles will be shorter than saveList.amplifier if stim is disabled
                        // in some channels (as is usually the case).
        for (int i = 0; i < (int) saveList.amplifier.size(); ++i) {
            if (saveList.stimEnabled[i]) {
                waveformFifo->copyDigitalData(WaveformFifo::ReaderDisk, uint16Array, stimFlagsWaveform[i], timeIndex, numSamples);
                stimFiles[iFile]->writeUInt16StimData(uint16Array, numSamples, posStimAmplitudes[i], negStimAmplitudes[i]);
                numBytesWritten += stimFiles[iFile]->getNumBytesWritten();
                ++iFile;
            }
        }
    }

    if (type != ControllerStimRecord) {
        // Save auxiliary input data.
        for (int i = 0; i < (int) saveList.auxInput.size(); ++i) {
            waveformFifo->copyAnalogData(WaveformFifo::ReaderDisk, vArray, auxInputWaveform[i], timeIndex, numSamples);
            convertAuxInputValue(uint16Array, vArray, numSamples);
            auxInputFiles[i]->writeUInt16(uint16Array, numSamples);
            numBytesWritten += auxInputFiles[i]->getNumBytesWritten();
        }

        // Save supply voltage data.
        for (int i = 0; i < (int) saveList.supplyVoltage.size(); ++i) {
            waveformFifo->copyAnalogData(WaveformFifo::ReaderDisk, vArray, supplyVoltageWaveform[i], timeIndex, numSamples);
            convertSupplyVoltageValue(uint16Array, vArray, numSamples);
            supplyVoltageFiles[i]->writeUInt16(uint16Array, numSamples);
            numBytesWritten += supplyVoltageFiles[i]->getNumBytesWritten();
        }
    }

    // Save board ADC data.
    for (int i = 0; i < (int) saveList.boardAdc.size(); ++i) {
        waveformFifo->copyAnalogData(WaveformFifo::ReaderDisk, vArray, boardAdcWaveform[i], timeIndex, numSamples);
        convertBoardAdcValue(uint16Array, vArray, numSamples);
        analogInputFiles[i]->writeUInt16(uint16Array, numSamples);
        numBytesWritten += analogInputFiles[i]->getNumBytesWritten();
    }

    if (type == ControllerStimRecord) {
        // Save board DAC data.
        for (int i = 0; i < (int) saveList.boardDac.size(); ++i) {
            waveformFifo->copyAnalogData(WaveformFifo::ReaderDisk, vArray, boardDacWaveform[i], timeIndex, numSamples);
            convertBoardDacValue(uint16Array, vArray, numSamples);
            analogOutputFiles[i]->writeUInt16(uint16Array, numSamples);
            numBytesWritten += analogOutputFiles[i]->getNumBytesWritten();
        }
    }

    // Save board digital input data.
    for (int i = 0; i < (int) saveList.boardDigitalIn.size(); ++i) {
        waveformFifo->copyDigitalData(WaveformFifo::ReaderDisk, uint16Array, boardDigitalInWaveform, timeIndex, numSamples);
        digitalInputFiles[i]->writeBitAsUInt16(uint16Array, numSamples, digitalInputFileIndices[i]);
        numBytesWritten += digitalInputFiles[i]->getNumBytesWritten();
    }

    // Save board digital output data, optionally.
    if (!saveList.boardDigitalOut.empty()) {
        for (int i = 0; i < (int) saveList.boardDigitalOut.size(); ++i) {
            waveformFifo->copyDigitalData(WaveformFifo::ReaderDisk, uint16Array, boardDigitalOutWaveform, timeIndex, numSamples);
            digitalOutputFiles[i]->writeBitAsUInt16(uint16Array, numSamples, digitalOutputFileIndices[i]);
            numBytesWritten += digitalOutputFiles[i]->getNumBytesWritten();
        }
    }
    delete [] vArray;
    delete [] uint16Array;

    return numBytesWritten;
}

double FilePerChannelSaveManager::bytesPerMinute() const
{
    double bytes = 0.0;
    bytes += 4.0; // timestamp
    bytes += 2.0 * saveList.amplifier.size();
    bytes += 2.0 * saveList.auxInput.size();
    bytes += 2.0 * saveList.supplyVoltage.size();
    bytes += 2.0 * saveList.boardAdc.size();
    if (type == ControllerStimRecord) {
        if (state->saveDCAmplifierWaveforms->getValue()) {
            bytes += 2.0 * saveList.amplifier.size();
        }
        bytes += 2.0 * count(saveList.stimEnabled.begin(), saveList.stimEnabled.end(), true);
        bytes += 2.0 * saveList.boardDac.size();
    }
    bytes += 2.0 * saveList.boardDigitalIn.size();
    if (!saveList.boardDigitalOut.empty()) {
        bytes += 2.0 * saveList.boardDigitalOut.size();
    }
    double samplesPerMinute = 60.0 * state->sampleRate->getNumericValue();
    return bytes * samplesPerMinute;
}

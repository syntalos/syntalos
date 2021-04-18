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
#include "filepersignaltypesavemanager.h"

using namespace std;

// One file per signal type file format
FilePerSignalTypeSaveManager::FilePerSignalTypeSaveManager(WaveformFifo* waveformFifo_, SystemState* state_) :
    SaveManager(waveformFifo_, state_),
    infoFile(nullptr),
    timeStampFile(nullptr),
    amplifierFile(nullptr),
    lowpassAmplifierFile(nullptr),
    highpassAmplifierFile(nullptr),
    spikeFile(nullptr),
    auxInputFile(nullptr),
    supplyVoltageFile(nullptr),
    dcAmplifierFile(nullptr),
    stimFile(nullptr),
    analogInputFile(nullptr),
    analogOutputFile(nullptr),
    digitalInputFile(nullptr),
    digitalOutputFile(nullptr)
{
    saveAuxInsWithAmps = false;
    saveSpikeSnapshot = false;
    samplesPreDetect = 0;
    samplesPostDetect = 0;
}

FilePerSignalTypeSaveManager::~FilePerSignalTypeSaveManager()
{
    closeAllSaveFiles();
}

bool FilePerSignalTypeSaveManager::openAllSaveFiles()
{
    const QString DataFileExtension = ".dat";
    dateTimeStamp = getDateTimeStamp();
    QString subdirName = state->filename->getBaseFilename() + dateTimeStamp;
    QDir dir(state->filename->getPath());
    if (!dir.mkdir(subdirName)) {
        return false;       // Cannot create subdirectory.
    }
    QString subdirPath = state->filename->getPath() + "/" + subdirName + "/";

    saveAuxInsWithAmps = state->saveAuxInWithAmpWaveforms->getValue();

    // Write settings file.
    state->saveGlobalSettings(subdirPath + "settings.xml");

    infoFile = new SaveFile(subdirPath + "info" + intanFileExtension());
    if (!infoFile->isOpen()) {
        closeAllSaveFiles();
        return false;
    }
    timeStampFile = new SaveFile(subdirPath + "time" + DataFileExtension);
    if (!timeStampFile->isOpen()) {
        closeAllSaveFiles();
        return false;
    }
    liveNotesFileName = subdirPath + "notes.txt";

    getAllWaveformPointers();

    if (!saveList.amplifier.empty()) {
        if (state->saveWidebandAmplifierWaveforms->getValue()) {
            amplifierFile = new SaveFile(subdirPath + "amplifier" + DataFileExtension);
            if (!amplifierFile->isOpen()) {
                closeAllSaveFiles();
                return false;
            }
        }
        if (state->saveLowpassAmplifierWaveforms->getValue()) {
            lowpassAmplifierFile = new SaveFile(subdirPath + "lowpass" + DataFileExtension);
            if (!lowpassAmplifierFile->isOpen()) {
                closeAllSaveFiles();
                return false;
            }
        }
        if (state->saveHighpassAmplifierWaveforms->getValue()) {
            highpassAmplifierFile = new SaveFile(subdirPath + "highpass" + DataFileExtension);
            if (!highpassAmplifierFile->isOpen()) {
                closeAllSaveFiles();
                return false;
            }
        }
        if (state->saveSpikeData->getValue()) {
            spikeFile = new SaveFile(subdirPath + "spike" + DataFileExtension);
            if (!spikeFile->isOpen()) {
                closeAllSaveFiles();
                return false;
            }

            //  Write spike file header.

            spikeFile->writeUInt32(SpikeFileMagicNumberAllChannels);

            const uint16_t SpikeFileVersionNumber = 1;
            spikeFile->writeUInt16(SpikeFileVersionNumber);

            // Write base filename with time/date stamp
            spikeFile->writeStringAsCharArray(subdirName.toStdString());
            spikeFile->writeUInt8(0);  // 0 to terminate string

            // Write amplifier native names as comma-separated list, zero-terminated string.
            for (unsigned long i = 0; i < saveList.amplifier.size(); ++i) {
                spikeFile->writeStringAsCharArray(saveList.amplifier[i]);
                if (i != saveList.amplifier.size() - 1) {
                    spikeFile->writeStringAsCharArray(",");
                }
            }
            spikeFile->writeUInt8(0);  // 0 to terminate string of amplifier native names

            // Write amplifier custom names as comma-separated list, zero-terminated string.
            for (unsigned long i = 0; i < saveList.amplifier.size(); ++i) {
                Channel* channel = state->signalSources->channelByName(saveList.amplifier[i]);
                string customName = "";
                if (channel) customName = channel->getCustomName().toStdString();
                spikeFile->writeStringAsCharArray(customName);
                if (i != saveList.amplifier.size() - 1) {
                    spikeFile->writeStringAsCharArray(",");
                }
            }
            spikeFile->writeUInt8(0);  // 0 to terminate string of amplifier custom names

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
        if (type == ControllerStimRecordUSB2) {
            stimFile = new SaveFile(subdirPath + "stim" + DataFileExtension);
            if (!stimFile->isOpen()) {
                closeAllSaveFiles();
                return false;
            }
            if (state->saveDCAmplifierWaveforms->getValue()) {
                dcAmplifierFile = new SaveFile(subdirPath + "dcamplifier" + DataFileExtension);
                if (!dcAmplifierFile->isOpen()) {
                    closeAllSaveFiles();
                    return false;
                }
            }
        }
    }
    if (type != ControllerStimRecordUSB2) {
        if (!saveList.auxInput.empty() && !saveAuxInsWithAmps) {
            auxInputFile = new SaveFile(subdirPath + "auxiliary" + DataFileExtension);
            if (!auxInputFile->isOpen()) {
                closeAllSaveFiles();
                return false;
            }
        }
        if (!saveList.supplyVoltage.empty()) {
            supplyVoltageFile = new SaveFile(subdirPath + "supply" + DataFileExtension);
            if (!supplyVoltageFile->isOpen()) {
                closeAllSaveFiles();
                return false;
            }
        }
    }
    if (!saveList.boardAdc.empty()) {
        analogInputFile = new SaveFile(subdirPath + "analogin" + DataFileExtension);
        if (!analogInputFile->isOpen()) {
            closeAllSaveFiles();
            return false;
        }
    }
    if (type == ControllerStimRecordUSB2 && !saveList.boardDac.empty()) {
        analogOutputFile = new SaveFile(subdirPath + "analogout" + DataFileExtension);
        if (!analogOutputFile->isOpen()) {
            closeAllSaveFiles();
            return false;
        }
    }
    if (!saveList.boardDigitalIn.empty()) {
        digitalInputFile = new SaveFile(subdirPath + "digitalin" + DataFileExtension);
        if (!digitalInputFile->isOpen()) {
            closeAllSaveFiles();
            return false;
        }
    }
    if (!saveList.boardDigitalOut.empty()) {
        digitalOutputFile = new SaveFile(subdirPath + "digitalout" + DataFileExtension);
        if (!digitalOutputFile->isOpen()) {
            closeAllSaveFiles();
            return false;
        }
    }

    writeIntanFileHeader(infoFile);
    infoFile->close();
    return true;
}

void FilePerSignalTypeSaveManager::closeAllSaveFiles()
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

    if (amplifierFile) {
        amplifierFile->close();
        delete amplifierFile;
        amplifierFile = nullptr;
    }

    if (lowpassAmplifierFile) {
        lowpassAmplifierFile->close();
        delete lowpassAmplifierFile;
        lowpassAmplifierFile = nullptr;
    }

    if (highpassAmplifierFile) {
        highpassAmplifierFile->close();
        delete highpassAmplifierFile;
        highpassAmplifierFile = nullptr;
    }

    if (spikeFile) {
        spikeFile->close();
        delete spikeFile;
        spikeFile = nullptr;
    }

    if (auxInputFile) {
        auxInputFile->close();
        delete auxInputFile;
        auxInputFile = nullptr;
    }

    if (supplyVoltageFile) {
        supplyVoltageFile->close();
        delete supplyVoltageFile;
        supplyVoltageFile = nullptr;
    }

    if (dcAmplifierFile) {
        dcAmplifierFile->close();
        delete dcAmplifierFile;
        dcAmplifierFile = nullptr;
    }

    if (stimFile) {
        stimFile->close();
        delete stimFile;
        stimFile = nullptr;
    }

    if (analogInputFile) {
        analogInputFile->close();
        delete analogInputFile;
        analogInputFile = nullptr;
    }

    if (analogOutputFile) {
        analogOutputFile->close();
        delete analogOutputFile;
        analogOutputFile = nullptr;
    }

    if (digitalInputFile) {
        digitalInputFile->close();
        delete digitalInputFile;
        digitalInputFile = nullptr;
    }

    if (digitalOutputFile) {
        digitalOutputFile->close();
        delete digitalOutputFile;
        digitalOutputFile = nullptr;
    }
}

int64_t FilePerSignalTypeSaveManager::writeToSaveFiles(int numSamples, int timeIndex)
{
    int maxColumns = 1;
    maxColumns = max(maxColumns, (int) saveList.amplifier.size());
    maxColumns = max(maxColumns, (int) saveList.auxInput.size());
    maxColumns = max(maxColumns, (int) saveList.supplyVoltage.size());
    maxColumns = max(maxColumns, (int) saveList.boardAdc.size());
    maxColumns = max(maxColumns, (int) saveList.boardDac.size());
    float* vArray = new float [maxColumns * numSamples];
    uint16_t* uint16Array = new uint16_t [maxColumns * numSamples];

    uint16_t* uint16Array2 = nullptr;
    if (saveAuxInsWithAmps) {
        uint16Array2 = new uint16_t [(int) (saveList.amplifier.size() + saveList.auxInput.size()) * numSamples];
    }

    int64_t numBytesWritten = 0;

    // Save timestamp data.
    for (int t = 0; t < numSamples; ++t) {
        timeStampFile->writeInt32((int) waveformFifo->getTimeStamp(WaveformFifo::ReaderDisk, timeIndex + t) - timeStampOffset);
    }
    numBytesWritten += timeStampFile->getNumBytesWritten();

    // Save amplifier data.
    if (amplifierFile) {
        waveformFifo->copyGpuAmplifierDataArrayRaw(WaveformFifo::ReaderDisk, uint16Array, amplifierGPUWaveform, timeIndex, numSamples);
        if (!saveAuxInsWithAmps) {
            amplifierFile->writeUInt16AsSigned(uint16Array, numSamples * (int) saveList.amplifier.size());
        } else {
            waveformFifo->copyAnalogDataArray(WaveformFifo::ReaderDisk, vArray, auxInputWaveform, timeIndex, numSamples);
            mergeAmpAndAuxValues(uint16Array2, uint16Array, vArray, numSamples, (int) saveList.amplifier.size(), (int) auxInputWaveform.size());
            // Note: When amplifier data and auxiliary input data are saved together in the same amplifier.dat file, we save
            // auxiliary input data as *signed* 16-bit numbers instead of unsigned to maintain consistency with the amplifier data.
            amplifierFile->writeUInt16AsSigned(uint16Array2, numSamples * (int) (saveList.amplifier.size() + auxInputWaveform.size()));
        }
        numBytesWritten += amplifierFile->getNumBytesWritten();
    }
    if (lowpassAmplifierFile) {
        int downsampleFactor = (int) state->lowpassWaveformDownsampleRate->getNumericValue();
        waveformFifo->copyGpuAmplifierDataArrayRaw(WaveformFifo::ReaderDisk, uint16Array, amplifierLowpassGPUWaveform, timeIndex, numSamples,
                                                   downsampleFactor);
        lowpassAmplifierFile->writeUInt16AsSigned(uint16Array, (numSamples / downsampleFactor) * (int) saveList.amplifier.size());
        numBytesWritten += lowpassAmplifierFile->getNumBytesWritten();
    }
    if (highpassAmplifierFile) {
        waveformFifo->copyGpuAmplifierDataArrayRaw(WaveformFifo::ReaderDisk, uint16Array, amplifierHighpassGPUWaveform, timeIndex, numSamples);
        highpassAmplifierFile->writeUInt16AsSigned(uint16Array, numSamples * (int) saveList.amplifier.size());
        numBytesWritten += highpassAmplifierFile->getNumBytesWritten();
    }

    // Save spike data.
    if (spikeFile) {
        for (int t = timeIndex - samplesPostDetect; t < timeIndex + numSamples - samplesPostDetect; ++t) {
            for (int i = 0; i < (int) saveList.amplifier.size(); ++i) {
                uint8_t spikeId = (uint8_t) waveformFifo->getDigitalData(WaveformFifo::ReaderDisk, spikeWaveform[i], t);
                if (spikeId != SpikeIdNoSpike) {
                    spikeFile->writeStringAsCharArray(saveList.amplifier[i]);   // Write channel name (e.g., "A-000")
                    spikeFile->writeInt32(waveformFifo->getTimeStamp(WaveformFifo::ReaderDisk, t) - timeStampOffset);  // Write 32-bit timestamp
                    spikeFile->writeUInt8(spikeId);                             // Write 8-bit spike ID
                    if (saveSpikeSnapshot) {                                    // Optionally, write spike snapshot
                        for (int tSnap = t - samplesPreDetect; tSnap < t + samplesPostDetect; ++tSnap) {
                            spikeFile->writeUInt16(waveformFifo->getGpuAmplifierDataRaw(WaveformFifo::ReaderDisk,
                                                                                        amplifierHighpassGPUWaveform[i],
                                                                                        tSnap));
                        }
                    }
                }
            }
        }
    }

    if (type == ControllerStimRecordUSB2) {
        // Save DC amplifier data.
        if (state->saveDCAmplifierWaveforms->getValue()) {
            waveformFifo->copyAnalogDataArray(WaveformFifo::ReaderDisk, vArray, dcAmplifierWaveform, timeIndex, numSamples);
            convertDcAmplifierValue(uint16Array, vArray, numSamples * (int) dcAmplifierWaveform.size());
            dcAmplifierFile->writeUInt16(uint16Array, numSamples * (int) dcAmplifierWaveform.size());
            numBytesWritten += dcAmplifierFile->getNumBytesWritten();
        }

        // Save stimulation data.
        waveformFifo->copyDigitalDataArray(WaveformFifo::ReaderDisk, uint16Array, stimFlagsWaveform, timeIndex, numSamples);
        stimFile->writeUInt16StimDataArray(uint16Array, numSamples, (int) stimFlagsWaveform.size(),
                                           posStimAmplitudes, negStimAmplitudes);
        numBytesWritten += stimFile->getNumBytesWritten();
    }

    if (type != ControllerStimRecordUSB2) {
        // Save auxiliary input data.
        if (!saveList.auxInput.empty() && !saveAuxInsWithAmps) {
            waveformFifo->copyAnalogDataArray(WaveformFifo::ReaderDisk, vArray, auxInputWaveform, timeIndex, numSamples);
            convertAuxInputValue(uint16Array, vArray, numSamples * (int) auxInputWaveform.size());
            auxInputFile->writeUInt16(uint16Array, numSamples * (int) auxInputWaveform.size());
            numBytesWritten += auxInputFile->getNumBytesWritten();
        }

        // Save supply voltage data.
        if (!saveList.supplyVoltage.empty()) {
            waveformFifo->copyAnalogDataArray(WaveformFifo::ReaderDisk, vArray, supplyVoltageWaveform, timeIndex, numSamples);
            convertSupplyVoltageValue(uint16Array, vArray, numSamples * (int) supplyVoltageWaveform.size());
            supplyVoltageFile->writeUInt16(uint16Array, numSamples * (int) supplyVoltageWaveform.size());
            numBytesWritten += supplyVoltageFile->getNumBytesWritten();
        }
    }

    // Save board ADC data.
    if (!saveList.boardAdc.empty()) {
        waveformFifo->copyAnalogDataArray(WaveformFifo::ReaderDisk, vArray, boardAdcWaveform, timeIndex, numSamples);
        convertBoardAdcValue(uint16Array, vArray, numSamples * (int) boardAdcWaveform.size());
        analogInputFile->writeUInt16(uint16Array, numSamples * (int) boardAdcWaveform.size());
        numBytesWritten += analogInputFile->getNumBytesWritten();
    }

    if (type == ControllerStimRecordUSB2) {
        // Save board DAC data.
        if (!saveList.boardDac.empty()) {
            waveformFifo->copyAnalogDataArray(WaveformFifo::ReaderDisk, vArray, boardDacWaveform, timeIndex, numSamples);
            convertBoardDacValue(uint16Array, vArray, numSamples * (int) boardDacWaveform.size());
            analogOutputFile->writeUInt16(uint16Array, numSamples * (int) boardDacWaveform.size());
            numBytesWritten += analogOutputFile->getNumBytesWritten();
        }
    }

    // Save board digital input data.
    if (!saveList.boardDigitalIn.empty()) {
        // If ANY digital inputs are enabled, we save ALL 16 channels, since we are writing 16-bit chunks of data.
        waveformFifo->copyDigitalData(WaveformFifo::ReaderDisk, uint16Array, boardDigitalInWaveform, timeIndex, numSamples);
        digitalInputFile->writeUInt16(uint16Array, numSamples);
        numBytesWritten += digitalInputFile->getNumBytesWritten();
    }

    // Save board digital output data, optionally.
    if (!saveList.boardDigitalOut.empty()) {
        // Save all 16 channels, since we are writing 16-bit chunks of data.
        waveformFifo->copyDigitalData(WaveformFifo::ReaderDisk, uint16Array, boardDigitalOutWaveform, timeIndex, numSamples);
        digitalOutputFile->writeUInt16(uint16Array, numSamples);
        numBytesWritten += digitalOutputFile->getNumBytesWritten();
    }

    delete [] vArray;
    delete [] uint16Array;
    if (uint16Array2) delete [] uint16Array2;

    return numBytesWritten;
}

double FilePerSignalTypeSaveManager::bytesPerMinute() const
{
    double bytes = 0.0;
    bytes += 4.0; // timestamp
    bytes += 2.0 * saveList.amplifier.size();
    bytes += 2.0 * saveList.auxInput.size();
    bytes += 2.0 * saveList.supplyVoltage.size();
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

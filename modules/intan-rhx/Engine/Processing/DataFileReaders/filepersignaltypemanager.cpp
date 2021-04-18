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

#include <QFileInfo>
#include <iostream>
#include "rhxglobals.h"
#include "datafilereader.h"
#include "filepersignaltypemanager.h"

FilePerSignalTypeManager::FilePerSignalTypeManager(const QString& fileName_, IntanHeaderInfo* info_, bool& canReadFile,
                                                   QString& report, DataFileReader* parent) :
    DataFileManager(fileName_, info_, parent),
    timeFile(nullptr),
    amplifierFile(nullptr),
    dcAmplifierFile(nullptr),
    stimFile(nullptr),
    auxInputFile(nullptr),
    supplyVoltageFile(nullptr),
    analogInFile(nullptr),
    analogOutFile(nullptr),
    digitalInFile(nullptr),
    digitalOutFile(nullptr)
{
    QFileInfo fileInfo(fileName);
    QString path = fileInfo.path();

    totalNumSamples = 0;
    QString limitingFile = "none";

    timeFile = new DataFile(path + "/" + "time.dat");
    if (!timeFile->isOpen()) {
        canReadFile = false;
        report += "Error: time.dat file not found." + EndOfLine;
        return;
    } else {
        totalNumSamples = timeFile->fileSize() / 4;
    }
    if (info->numEnabledAmplifierChannels > 0) {
        amplifierFile = new DataFile(path + "/" + "amplifier.dat");
        if (!amplifierFile->isOpen()) {
            report += "Warning: Could not open amplifier.dat" + EndOfLine;
            info->removeAllChannels(AmplifierSignal);
        } else {
            int64_t numAmpSamples = amplifierFile->fileSize() / (info->numEnabledAmplifierChannels * 2);
            if (numAmpSamples < totalNumSamples) {
                totalNumSamples = numAmpSamples;
                limitingFile = amplifierFile->getFileName();
            }
        }
    }
    if (info->controllerType == ControllerStimRecordUSB2) {
        // Always try opening dcamplifier.dat since the old RHS software does not report dcAmplifierDataSaved reliably.
        dcAmplifierFile = new DataFile(path + "/" + "dcamplifier.dat");
        if (!dcAmplifierFile->isOpen()) {
            info->dcAmplifierDataSaved = false;
            // If dcamplifier.dat is not present, update dcAmplifierWasSaved booleans.
            for (int i = 0; i < (int) dcAmplifierWasSaved.size(); ++i) {
                for (int j = 0; j < (int) dcAmplifierWasSaved[i].size(); ++j) {
                    dcAmplifierWasSaved[i][j] = false;
                }
            }
        } else {
            info->dcAmplifierDataSaved = true;
            int64_t numDcAmpSamples = dcAmplifierFile->fileSize() / (info->numEnabledAmplifierChannels * 2);
            if (numDcAmpSamples < totalNumSamples) {
                totalNumSamples = numDcAmpSamples;
                limitingFile = dcAmplifierFile->getFileName();
            }
        }
    }
    if (info->stimDataPresent) {
        stimFile = new DataFile(path + "/" + "stim.dat");
        if (!stimFile->isOpen()) {
            report += "Warning: Could not open stim.dat" + EndOfLine;
            info->stimDataPresent = false;
        } else {
            if (stimFile->fileSize() == 0) {
                info->stimDataPresent = false;
            } else {
                int64_t numStimSamples = stimFile->fileSize() / (info->numEnabledAmplifierChannels * 2);
                if (numStimSamples < totalNumSamples) {
                    totalNumSamples = numStimSamples;
                    limitingFile = stimFile->getFileName();
                }
            }
        }
    }
    if (info->numEnabledAuxInputChannels > 0) {
        auxInputFile = new DataFile(path + "/" + "auxiliary.dat");
        if (!auxInputFile->isOpen()) {
            report += "Warning: Could not open auxiliary.dat" + EndOfLine;
            info->removeAllChannels(AuxInputSignal);
        } else {
            int64_t numAuxInSamples = auxInputFile->fileSize() / (info->numEnabledAuxInputChannels * 2);
            if (numAuxInSamples < totalNumSamples) {
                totalNumSamples = numAuxInSamples;
                limitingFile = auxInputFile->getFileName();
            }
        }
    }
    if (info->numEnabledSupplyVoltageChannels > 0) {
        supplyVoltageFile = new DataFile(path + "/" + "supply.dat");
        if (!supplyVoltageFile->isOpen()) {
            report += "Warning: Could not open supply.dat" + EndOfLine;
            info->removeAllChannels(SupplyVoltageSignal);
        } else {
            int64_t numSupplySamples = supplyVoltageFile->fileSize() / (info->numEnabledSupplyVoltageChannels * 2);
            if (numSupplySamples < totalNumSamples) {
                totalNumSamples = numSupplySamples;
                limitingFile = supplyVoltageFile->getFileName();
            }
        }
    }
    if (info->numEnabledBoardAdcChannels > 0) {
        analogInFile = new DataFile(path + "/" + "analogin.dat");
        if (!analogInFile->isOpen()) {
            report += "Warning: Could not open analogin.dat" + EndOfLine;
            info->removeAllChannels(BoardAdcSignal);
        } else {
            int64_t numAnalogInSamples = analogInFile->fileSize() / (info->numEnabledBoardAdcChannels * 2);
            if (numAnalogInSamples < totalNumSamples) {
                totalNumSamples = numAnalogInSamples;
                limitingFile = analogInFile->getFileName();
            }
        }
    }
    if (info->numEnabledBoardDacChannels > 0) {
        analogOutFile = new DataFile(path + "/" + "analogout.dat");
        if (!analogOutFile->isOpen()) {
            report += "Warning: Could not open analogout.dat" + EndOfLine;
            info->removeAllChannels(BoardDacSignal);
        } else {
            int64_t numAnalogOutSamples = analogOutFile->fileSize() / (info->numEnabledBoardDacChannels * 2);
            if (numAnalogOutSamples < totalNumSamples) {
                totalNumSamples = numAnalogOutSamples;
                limitingFile = analogOutFile->getFileName();
            }
        }
    }
    if (info->numEnabledDigitalInChannels > 0) {
        digitalInFile = new DataFile(path + "/" + "digitalin.dat");
        if (!digitalInFile->isOpen()) {
            report += "Warning: Could not open digitalin.dat" + EndOfLine;
            info->removeAllChannels(BoardDigitalInSignal);
        } else {
            int64_t numDigitalInSamples = digitalInFile->fileSize() / 2;
            if (numDigitalInSamples < totalNumSamples) {
                totalNumSamples = numDigitalInSamples;
                limitingFile = digitalInFile->getFileName();
            }
        }
    }
    if (info->numEnabledDigitalOutChannels > 0) {
        digitalOutFile = new DataFile(path + "/" + "digitalout.dat");
        if (!digitalOutFile->isOpen()) {
            report += "Warning: Could not open digitalout.dat" + EndOfLine;
            info->removeAllChannels(BoardDigitalOutSignal);
        } else {
            int64_t numDigitalOutSamples = digitalOutFile->fileSize() / 2;
            if (numDigitalOutSamples < totalNumSamples) {
                totalNumSamples = numDigitalOutSamples;
                limitingFile = digitalOutFile->getFileName();
            }
        }
    }
    if (limitingFile != "none") {
        report += "Warning: Data limited by short file " + limitingFile + EndOfLine;
    }

    report += "Total recording time: " + timeString(totalNumSamples) + EndOfLine;

    firstTimeStamp = timeFile->readTimeStamp();
    lastTimeStamp = firstTimeStamp + totalNumSamples - 1;
    timeFile->seek(0);

    readIndex = 0;

    // Read and store contents of live notes file, if present.
    QFile* liveNotesFile = openLiveNotes();
    if (liveNotesFile) {
        readLiveNotes(liveNotesFile);
        liveNotesFile->close();
        delete liveNotesFile;
    }

    canReadFile = true;
}

FilePerSignalTypeManager::~FilePerSignalTypeManager()
{
    if (timeFile) delete timeFile;
    if (amplifierFile) delete amplifierFile;
    if (dcAmplifierFile) delete dcAmplifierFile;
    if (stimFile) delete stimFile;
    if (auxInputFile) delete auxInputFile;
    if (supplyVoltageFile) delete supplyVoltageFile;
    if (analogInFile) delete analogInFile;
    if (analogOutFile) delete analogOutFile;
    if (digitalInFile) delete digitalInFile;
    if (digitalOutFile) delete digitalOutFile;
}

void FilePerSignalTypeManager::loadDataFrame()
{
    int numDataStreams = info->numDataStreams;
    int channelsPerStream = RHXDataBlock::channelsPerStream(info->controllerType);

    timeStamp = timeFile->readTimeStamp();

    for (int i = 0; i < numDataStreams; ++i) {
        for (int j = 0; j < channelsPerStream; ++j) {
            if (amplifierWasSaved[i][j]) {
                amplifierData[i][j] = amplifierFile->readWord() ^ 0x8000U;  // convert from two's complement to offset
            } else {
                amplifierData[i][j] = 32768U;
            }
        }
    }
    if (info->dcAmplifierDataSaved) {
        for (int i = 0; i < numDataStreams; ++i) {
            for (int j = 0; j < channelsPerStream; ++j) {
                if (dcAmplifierWasSaved[i][j]) {
                    dcAmplifierData[i][j] = dcAmplifierFile->readWord();
                } else {
                    dcAmplifierData[i][j] = 512U;
                }
            }
        }
    }
    if (info->stimDataPresent) {
        for (int i = 0; i < numDataStreams; ++i) {
            for (int j = 0; j < channelsPerStream; ++j) {
                if (stimWasSaved[i][j]) {
                    uint16_t word = stimFile->readWord();
                    stimData[i][j].amplitude = word & 0x00ffU;
                    stimData[i][j].stimOn = (word & 0x00ffU) ? 1U : 0;
                    stimData[i][j].stimPol = (word & 0x0100U) ? 1U : 0;
                    stimData[i][j].ampSettle = (word & 0x2000U) ? 1U : 0;
                    stimData[i][j].chargeRecov = (word & 0x4000U) ? 1U : 0;
                    stimData[i][j].complianceLimit = (word & 0x8000U) ? 1U : 0;
                    if (stimData[i][j].amplitude != 0) {
                        if (stimData[i][j].stimPol != 0) {
                            if (!posStimAmplitudeFound[i][j]) {
                                posStimAmplitudeFound[i][j] = true;
                                dataFileReader->recordPosStimAmplitude(i, j, stimData[i][j].amplitude);
                            }
                        } else {
                            if (!negStimAmplitudeFound[i][j]) {
                                negStimAmplitudeFound[i][j] = true;
                                dataFileReader->recordNegStimAmplitude(i, j, stimData[i][j].amplitude);
                            }
                        }
                    }
                } else {
                    stimData[i][j].clear();
                }
            }
        }
    }
    if (info->controllerType != ControllerStimRecordUSB2) {
        for (int i = 0; i < numDataStreams; ++i) {
            for (int j = 0; j < 3; ++j) {
                if (auxInputWasSaved[i][j]) {
                    auxInputData[i][j] = auxInputFile->readWord();
                } else {
                    auxInputData[i][j] = 0;
                }
            }
            if (supplyVoltageWasSaved[i]) {
                supplyVoltageData[i] = supplyVoltageFile->readWord();
            } else {
                supplyVoltageData[i] = 0;
            }
        }
    }
    for (int i = 0; i < 8; ++i) {
        if (analogInWasSaved[i]) {
            analogInData[i] = analogInFile->readWord();
        } else {
            analogInData[i] = (info->controllerType == ControllerRecordUSB2) ? 0 : 32768U;
        }
    }
    for (int i = 0; i < 8; ++i) {
        if (analogOutWasSaved[i]) {
            analogOutData[i] = analogOutFile->readWord();
        } else {
            analogOutData[i] = 32768U;
        }
    }
    if (info->numEnabledDigitalInChannels > 0) {
        digitalInData = digitalInFile->readWord();
    } else {
        digitalInData = 0;
    }
    if (info->numEnabledDigitalOutChannels > 0) {
        digitalOutData = digitalOutFile->readWord();
    } else {
        digitalOutData = 0;
    }
}

QFile* FilePerSignalTypeManager::openLiveNotes()
{
    QFileInfo fileInfo(fileName);
    QString path = fileInfo.path();
    QFile* liveNotesFile = new QFile(path + "/" + "notes.txt");
    if (!liveNotesFile->open(QIODevice::ReadOnly)) {
        delete liveNotesFile;
        liveNotesFile = nullptr;
    }
    return liveNotesFile;
}

int64_t FilePerSignalTypeManager::jumpToTimeStamp(int64_t target)
{
    if (target < firstTimeStamp) target = firstTimeStamp;
    if (target > lastTimeStamp) target = lastTimeStamp;
    target -= firstTimeStamp;   // firstTimeStamp can be negative in triggered recordings.

    timeFile->seek(target * 4);
    if (amplifierFile) amplifierFile->seek(target * 2 * info->numEnabledAmplifierChannels);
    if (dcAmplifierFile) dcAmplifierFile->seek(target * 2 * info->numEnabledAmplifierChannels);
    if (stimFile) stimFile->seek(target * 2 * info->numEnabledAmplifierChannels);
    if (auxInputFile) auxInputFile->seek(target * 2 * info->numEnabledAuxInputChannels);
    if (supplyVoltageFile) supplyVoltageFile->seek(target * 2 * info->numEnabledSupplyVoltageChannels);
    if (analogInFile) analogInFile->seek(target * 2 * info->numEnabledBoardAdcChannels);
    if (analogOutFile) analogOutFile->seek(target * 2 * info->numEnabledBoardDacChannels);
    if (digitalInFile) digitalInFile->seek(target * 2);
    if (digitalOutFile) digitalOutFile->seek(target * 2);

    readIndex = target;
    return readIndex + firstTimeStamp;  // Return actual timestamp jumped to, which should be same as target.
}

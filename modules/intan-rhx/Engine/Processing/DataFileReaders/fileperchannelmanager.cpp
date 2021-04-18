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
#include "fileperchannelmanager.h"

FilePerChannelManager::FilePerChannelManager(const QString& fileName_, IntanHeaderInfo* info_, bool& canReadFile,
                                             QString& report, DataFileReader* parent) :
    DataFileManager(fileName_, info_, parent),
    timeFile(nullptr)
{
    QFileInfo fileInfo(fileName);
    QString path = fileInfo.path();

    totalNumSamples = 0;
    QString limitingFile = "none";

    // Set up vectors for storing pointers to all data files (or nullptr if data file is absent).
    int numDataStreams = info->numDataStreams;
    int channelsPerStream = RHXDataBlock::channelsPerStream(info->controllerType);
    amplifierFiles.resize(numDataStreams);
    for (int i = 0; i < (int) amplifierFiles.size(); ++i) {
        amplifierFiles[i].resize(channelsPerStream, nullptr);
    }
    if (info->controllerType == ControllerStimRecordUSB2) {
        dcAmplifierFiles.resize(numDataStreams);
        for (int i = 0; i < (int) dcAmplifierFiles.size(); ++i) {
            dcAmplifierFiles[i].resize(channelsPerStream, nullptr);
        }
    }
    if (info->stimDataPresent) {
        stimFiles.resize(numDataStreams);
        for (int i = 0; i < (int) stimFiles.size(); ++i) {
            stimFiles[i].resize(channelsPerStream, nullptr);
        }
    }
    auxInputFiles.resize(numDataStreams);
    for (int i = 0; i < (int) auxInputFiles.size(); ++i) {
        auxInputFiles[i].resize(3, nullptr);
    }
    supplyVoltageFiles.resize(numDataStreams, nullptr);
    analogInFiles.resize(8, nullptr);
    analogOutFiles.resize(8, nullptr);
    digitalInFiles.resize(16, nullptr);
    digitalOutFiles.resize(16, nullptr);

    timeFile = new DataFile(path + "/" + "time.dat");
    if (!timeFile->isOpen()) {
        canReadFile = false;
        report += "Error: time.dat file not found." + EndOfLine;
        return;
    } else {
        totalNumSamples = timeFile->fileSize() / 4;
    }

    QString name;
    for (int stream = 0; stream < numDataStreams; ++stream) {
        for (int channel = 0; channel < channelsPerStream; ++ channel) {
            if (amplifierWasSaved[stream][channel]) {
                name = info->getChannelName(AmplifierSignal, stream, channel);
                amplifierFiles[stream][channel] = new DataFile(path + "/" + "amp-" + name + ".dat");
                if (!amplifierFiles[stream][channel]->isOpen()) {
                    report += "Warning: Could not open " + amplifierFiles[stream][channel]->getFileName() + EndOfLine;
                    delete amplifierFiles[stream][channel];
                    amplifierFiles[stream][channel] = nullptr;
                    amplifierWasSaved[stream][channel] = false;
                } else {
                    int64_t numAmpSamples = amplifierFiles[stream][channel]->fileSize() / 2;
                    if (numAmpSamples < totalNumSamples) {
                        totalNumSamples = numAmpSamples;
                        limitingFile = amplifierFiles[stream][channel]->getFileName();
                    }
                }
            }
        }
    }

    // Always try opening dc files since the old RHS software does not report dcAmplifierDataSaved reliably.
    // (dcAmplifierDataSaved is always marked 'false' in non-traditional-Intan file formats.)
    info->dcAmplifierDataSaved = false;     // Assume no dc files are present until we find one.
    if (info->controllerType == ControllerStimRecordUSB2) {
        for (int stream = 0; stream < numDataStreams; ++stream) {
            for (int channel = 0; channel < channelsPerStream; ++ channel) {
                name = info->getChannelName(AmplifierSignal, stream, channel);
                dcAmplifierFiles[stream][channel] = new DataFile(path + "/" + "dc-" + name + ".dat");
                if (!dcAmplifierFiles[stream][channel]->isOpen()) {
                    report += "Warning: Could not open " + dcAmplifierFiles[stream][channel]->getFileName() + EndOfLine;
                    delete dcAmplifierFiles[stream][channel];
                    dcAmplifierFiles[stream][channel] = nullptr;
                    dcAmplifierWasSaved[stream][channel] = false;
                } else {
                    info->dcAmplifierDataSaved = true;  // Now set to true if we find at least one dc data file.
                    dcAmplifierWasSaved[stream][channel] = true;
                    int64_t numAmpSamples = dcAmplifierFiles[stream][channel]->fileSize() / 2;
                    if (numAmpSamples < totalNumSamples) {
                        totalNumSamples = numAmpSamples;
                        limitingFile = dcAmplifierFiles[stream][channel]->getFileName();
                    }
                }
            }
        }
    }

    if (info->stimDataPresent) {
        for (int stream = 0; stream < numDataStreams; ++stream) {
            for (int channel = 0; channel < channelsPerStream; ++ channel) {
                if (stimWasSaved[stream][channel]) {
                    name = info->getChannelName(AmplifierSignal, stream, channel);
                    stimFiles[stream][channel] = new DataFile(path + "/" + "stim-" + name + ".dat");
                    if (!stimFiles[stream][channel]->isOpen()) {
                        report += "Warning: Could not open " + stimFiles[stream][channel]->getFileName() + EndOfLine;
                        delete stimFiles[stream][channel];
                        stimFiles[stream][channel] = nullptr;
                        stimWasSaved[stream][channel] = false;
                    } else if (stimFiles[stream][channel]->fileSize() == 0) {
                        delete stimFiles[stream][channel];
                        stimFiles[stream][channel] = nullptr;
                        stimWasSaved[stream][channel] = false;
                    } else {
                        int64_t numAmpSamples = stimFiles[stream][channel]->fileSize() / 2;
                        if (numAmpSamples < totalNumSamples) {
                            totalNumSamples = numAmpSamples;
                            limitingFile = stimFiles[stream][channel]->getFileName();
                        }
                    }
                }
            }
        }
    }

    for (int stream = 0; stream < numDataStreams; ++stream) {
        for (int channel = 0; channel < 3; ++ channel) {
            if (auxInputWasSaved[stream][channel]) {
                name = info->getChannelName(AuxInputSignal, stream, channel);
                auxInputFiles[stream][channel] = new DataFile(path + "/" + "aux-" + name + ".dat");
                if (!auxInputFiles[stream][channel]->isOpen()) {
                    report += "Warning: Could not open " + auxInputFiles[stream][channel]->getFileName() + EndOfLine;
                    delete auxInputFiles[stream][channel];
                    auxInputFiles[stream][channel] = nullptr;
                    auxInputWasSaved[stream][channel] = false;
                } else {
                    int64_t numAmpSamples = auxInputFiles[stream][channel]->fileSize() / 2;
                    if (numAmpSamples < totalNumSamples) {
                        totalNumSamples = numAmpSamples;
                        limitingFile = auxInputFiles[stream][channel]->getFileName();
                    }
                }
            }
        }
    }

    for (int stream = 0; stream < numDataStreams; ++stream) {
        if (supplyVoltageWasSaved[stream]) {
            name = info->getChannelName(SupplyVoltageSignal, stream, 0);
            supplyVoltageFiles[stream] = new DataFile(path + "/" + "vdd-" + name + ".dat");
            if (!supplyVoltageFiles[stream]->isOpen()) {
                report += "Warning: Could not open " + supplyVoltageFiles[stream]->getFileName() + EndOfLine;
                delete supplyVoltageFiles[stream];
                supplyVoltageFiles[stream] = nullptr;
                supplyVoltageWasSaved[stream] = false;
            } else {
                int64_t numAmpSamples = supplyVoltageFiles[stream]->fileSize() / 2;
                if (numAmpSamples < totalNumSamples) {
                    totalNumSamples = numAmpSamples;
                    limitingFile = supplyVoltageFiles[stream]->getFileName();
                }
            }
        }
    }

    for (int channel = 0; channel < (int) analogInFiles.size(); ++channel) {
        if (analogInWasSaved[channel]) {
            name = info->getChannelName(BoardAdcSignal, 0, channel);
            analogInFiles[channel] = new DataFile(path + "/" + "board-" + name + ".dat");
            if (!analogInFiles[channel]->isOpen()) {
                report += "Warning: Could not open " + analogInFiles[channel]->getFileName() + EndOfLine;
                delete analogInFiles[channel];
                analogInFiles[channel] = nullptr;
                analogInWasSaved[channel] = false;
            } else {
                int64_t numAmpSamples = analogInFiles[channel]->fileSize() / 2;
                if (numAmpSamples < totalNumSamples) {
                    totalNumSamples = numAmpSamples;
                    limitingFile = analogInFiles[channel]->getFileName();
                }
            }
        }
    }

    for (int channel = 0; channel < (int) analogOutFiles.size(); ++channel) {
        if (analogOutWasSaved[channel]) {
            name = info->getChannelName(BoardDacSignal, 0, channel);
            analogOutFiles[channel] = new DataFile(path + "/" + "board-" + name + ".dat");
            if (!analogOutFiles[channel]->isOpen()) {
                report += "Could not open " + analogOutFiles[channel]->getFileName() + EndOfLine;
                delete analogOutFiles[channel];
                analogOutFiles[channel] = nullptr;
                analogOutWasSaved[channel] = false;
            } else {
                int64_t numAmpSamples = analogOutFiles[channel]->fileSize() / 2;
                if (numAmpSamples < totalNumSamples) {
                    totalNumSamples = numAmpSamples;
                    limitingFile = analogOutFiles[channel]->getFileName();
                }
            }
        }
    }

    for (int channel = 0; channel < (int) digitalInFiles.size(); ++channel) {
        if (digitalInWasSaved[channel]) {
            name = info->getChannelName(BoardDigitalInSignal, 0, channel);
            digitalInFiles[channel] = new DataFile(path + "/" + "board-" + name + ".dat");
            if (!digitalInFiles[channel]->isOpen()) {
                report += "Warning: Could not open " + digitalInFiles[channel]->getFileName() + EndOfLine;
                delete digitalInFiles[channel];
                digitalInFiles[channel] = nullptr;
                digitalInWasSaved[channel] = false;
            } else {
                int64_t numAmpSamples = digitalInFiles[channel]->fileSize() / 2;
                if (numAmpSamples < totalNumSamples) {
                    totalNumSamples = numAmpSamples;
                    limitingFile = digitalInFiles[channel]->getFileName();
                }
            }
        }
    }

    for (int channel = 0; channel < (int) digitalOutFiles.size(); ++channel) {
        if (digitalOutWasSaved[channel]) {
            name = info->getChannelName(BoardDigitalOutSignal, 0, channel);
            digitalOutFiles[channel] = new DataFile(path + "/" + "board-" + name + ".dat");
            if (!digitalOutFiles[channel]->isOpen()) {
                report += "Warning: Could not open " + digitalOutFiles[channel]->getFileName() + EndOfLine;
                delete digitalOutFiles[channel];
                digitalOutFiles[channel] = nullptr;
                digitalOutWasSaved[channel] = false;
            } else {
                int64_t numAmpSamples = digitalOutFiles[channel]->fileSize() / 2;
                if (numAmpSamples < totalNumSamples) {
                    totalNumSamples = numAmpSamples;
                    limitingFile = digitalOutFiles[channel]->getFileName();
                }
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

FilePerChannelManager::~FilePerChannelManager()
{
    if (timeFile) delete timeFile;
    for (int i = 0; i < (int) amplifierFiles.size(); ++i) {
        for (int j = 0; j < (int) amplifierFiles[i].size(); ++j) {
            if (amplifierFiles[i][j]) delete amplifierFiles[i][j];
        }
    }
    for (int i = 0; i < (int) dcAmplifierFiles.size(); ++i) {
        for (int j = 0; j < (int) dcAmplifierFiles[i].size(); ++j) {
            if (dcAmplifierFiles[i][j]) delete dcAmplifierFiles[i][j];
        }
    }
    for (int i = 0; i < (int) stimFiles.size(); ++i) {
        for (int j = 0; j < (int) stimFiles[i].size(); ++j) {
            if (stimFiles[i][j]) delete stimFiles[i][j];
        }
    }
    for (int i = 0; i < (int) auxInputFiles.size(); ++i) {
        for (int j = 0; j < (int) auxInputFiles[i].size(); ++j) {
            if (auxInputFiles[i][j]) delete auxInputFiles[i][j];
        }
    }
    for (int i = 0; i < (int) supplyVoltageFiles.size(); ++i) {
        if (supplyVoltageFiles[i]) delete supplyVoltageFiles[i];
    }
    for (int i = 0; i < (int) analogInFiles.size(); ++i) {
        if (analogInFiles[i]) delete analogInFiles[i];
    }
    for (int i = 0; i < (int) analogOutFiles.size(); ++i) {
        if (analogOutFiles[i]) delete analogOutFiles[i];
    }
    for (int i = 0; i < (int) digitalInFiles.size(); ++i) {
        if (digitalInFiles[i]) delete digitalInFiles[i];
    }
    for (int i = 0; i < (int) digitalOutFiles.size(); ++i) {
        if (digitalOutFiles[i]) delete digitalOutFiles[i];
    }
}

void FilePerChannelManager::loadDataFrame()
{
    int numDataStreams = info->numDataStreams;
    int channelsPerStream = RHXDataBlock::channelsPerStream(info->controllerType);

    timeStamp = timeFile->readTimeStamp();

    for (int i = 0; i < numDataStreams; ++i) {
        for (int j = 0; j < channelsPerStream; ++j) {
            if (amplifierWasSaved[i][j]) {
                amplifierData[i][j] = amplifierFiles[i][j]->readWord() ^ 0x8000U;  // convert from two's complement to offset
            } else {
                amplifierData[i][j] = 32768U;
            }
        }
    }
    if (info->dcAmplifierDataSaved) {
        for (int i = 0; i < numDataStreams; ++i) {
            for (int j = 0; j < channelsPerStream; ++j) {
                if (dcAmplifierWasSaved[i][j]) {
                    dcAmplifierData[i][j] = dcAmplifierFiles[i][j]->readWord();
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
                    uint16_t word = stimFiles[i][j]->readWord();
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
                    auxInputData[i][j] = auxInputFiles[i][j]->readWord();
                } else {
                    auxInputData[i][j] = 0;
                }
            }
            if (supplyVoltageWasSaved[i]) {
                supplyVoltageData[i] = supplyVoltageFiles[i]->readWord();
            } else {
                supplyVoltageData[i] = 0;
            }
        }
    }
    for (int i = 0; i < 8; ++i) {
        if (analogInWasSaved[i]) {
            analogInData[i] = analogInFiles[i]->readWord();
        } else {
            analogInData[i] = (info->controllerType == ControllerRecordUSB2) ? 0 : 32768U;
        }
    }
    for (int i = 0; i < 8; ++i) {
        if (analogOutWasSaved[i]) {
            analogOutData[i] = analogOutFiles[i]->readWord();
        } else {
            analogOutData[i] = 32768U;
        }
    }
    digitalInData = 0;
    for (int i = 0; i < 16; ++i) {
        if (digitalInWasSaved[i]) {
            digitalInData |= (digitalInFiles[i]->readWord() << i);
        }
    }
    digitalOutData = 0;
    for (int i = 0; i < 16; ++i) {
        if (digitalOutWasSaved[i]) {
            digitalOutData |= (digitalOutFiles[i]->readWord() << i);
        }
    }
}

QFile* FilePerChannelManager::openLiveNotes()
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

int64_t FilePerChannelManager::jumpToTimeStamp(int64_t target)
{
    if (target < firstTimeStamp) target = firstTimeStamp;
    if (target > lastTimeStamp) target = lastTimeStamp;
    target -= firstTimeStamp;   // firstTimeStamp can be negative in triggered recordings.

    int numDataStreams = info->numDataStreams;
    int channelsPerStream = RHXDataBlock::channelsPerStream(info->controllerType);

    timeFile->seek(target * 4);
    for (int i = 0; i < numDataStreams; ++i) {
        for (int j = 0; j < channelsPerStream; ++j) {
            if (amplifierFiles[i][j]) amplifierFiles[i][j]->seek(target * 2);
        }
    }
    if (info->dcAmplifierDataSaved) {
        for (int i = 0; i < numDataStreams; ++i) {
            for (int j = 0; j < channelsPerStream; ++j) {
                if (dcAmplifierFiles[i][j]) dcAmplifierFiles[i][j]->seek(target * 2);
            }
        }
    }
    if (info->stimDataPresent) {
        for (int i = 0; i < numDataStreams; ++i) {
            for (int j = 0; j < channelsPerStream; ++j) {
                if (stimFiles[i][j]) stimFiles[i][j]->seek(target * 2);
            }
        }
    }
    if (info->controllerType != ControllerStimRecordUSB2) {
        for (int i = 0; i < numDataStreams; ++i) {
            for (int j = 0; j < 3; ++j) {
                if (auxInputFiles[i][j]) auxInputFiles[i][j]->seek(target * 2);
            }
            if (supplyVoltageFiles[i]) supplyVoltageFiles[i]->seek(target * 2);
        }
    }
    for (int i = 0; i < 8; ++i) {
        if (analogInFiles[i]) analogInFiles[i]->seek(target * 2);
        if (analogOutFiles[i]) analogOutFiles[i]->seek(target * 2);
    }
    for (int i = 0; i < 16; ++i) {
        if (digitalInFiles[i]) digitalInFiles[i]->seek(target * 2);
        if (digitalOutFiles[i]) digitalOutFiles[i]->seek(target * 2);
    }

    readIndex = target;
    return readIndex + firstTimeStamp;  // Return actual timestamp jumped to, which should be same as target.
}

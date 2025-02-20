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

#include <QFileInfo>
#include <iostream>
#include <thread>
#include "rhxglobals.h"
#include "datafilereader.h"
#include "fileperchannelmanager.h"

FilePerChannelManager::FilePerChannelManager(const QString& fileName_, IntanHeaderInfo* info_, bool& canReadFile,
                                             QString& report, DataFileReader* parent) :
    DataFileManager(fileName_, info_, parent),
    timeFile(nullptr)
{
    // TODO - somehow keep jumpToPosition dialog up-to-date
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
    if (info->controllerType == ControllerStimRecord) {
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
    if (info->controllerType == ControllerStimRecord) {
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
    report += "If the recording is still underway, data written beyond this point can still be read." + EndOfLine;

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
    // Recalculate totalNumSamples and lastTimestamp at each load.
    //updateEndOfData();

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
    if (info->controllerType != ControllerStimRecord) {
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
    if (info->controllerType != ControllerStimRecord) {
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

// Update totalNumSamples and lastTimeStamp with the end of the (for now, just) amplifier data file
void FilePerChannelManager::updateEndOfData()
{
    // TODO - polish up to get the actual end of all data, not just assume from end of shortest amp file
    int tempTotalNumSamples = 0;
    for (int stream = 0; stream < info->numDataStreams; ++stream) {
        for (uint channel = 0; channel < amplifierFiles[stream].size(); ++channel) {
            if (amplifierWasSaved[stream][channel]) {
                int64_t numAmpSamples = amplifierFiles[stream][channel]->fileSize() / 2;
                if (numAmpSamples < tempTotalNumSamples || tempTotalNumSamples == 0) {
                    tempTotalNumSamples = numAmpSamples;
                }
            }
        }
    }
    totalNumSamples = tempTotalNumSamples;
    lastTimeStamp = firstTimeStamp + totalNumSamples - 1;
}

int64_t FilePerChannelManager::getLastTimeStamp()
{
    updateEndOfData();
    return lastTimeStamp;
}

long FilePerChannelManager::readDataBlocksRaw(int numBlocks, uint8_t* buffer)
{
    // Store these values locally to speed up execution below.
    ControllerType type = info->controllerType;
    int samplesPerDataBlock = RHXDataBlock::samplesPerDataBlock(type);  // Use RHX standard of samples per data block, not file's
    int numDataStreams = info->numDataStreams;
    int channelsPerStream = RHXDataBlock::channelsPerStream(type);

    updateEndOfData();
//    // ORIGINAL - STOP AS NORMAL WHEN EOF IS REACHED
//    if (readIndex + numBlocks * samplesPerDataBlock > totalNumSamples) {
//        emit dataFileReader->sendSetCommand("RunMode", "Stop");
//        dataFileReader->setStatusBarEOF();
//        return 0;
//    }

    // If liveReading, determine if readIndex is at least 1 second behind real time. If it is, boost playback speed to 5.0
    if (dataFileReader->getLive() && dataFileReader->getPlaybackSpeed() < 5.0) {
        int oneSecondOfSamples = (int) AbstractRHXController::getSampleRate(info->sampleRate);
        if (readIndex + numBlocks * samplesPerDataBlock < totalNumSamples - oneSecondOfSamples) {
            dataFileReader->setPlaybackSpeed(5.0);
        }
    }

    // When EOF is reached, keep updating end of data, trying continually for 2 seconds. If no change, then stop as normal
    // If enough data is found after updating, then set 1.0 playback speed and set runmode on.
    if (readIndex + numBlocks * samplesPerDataBlock > totalNumSamples) {
        //emit dataFileReader->sendSetCommand("RunMode", "Stop");
        QElapsedTimer shortTimer;
        shortTimer.start();
        bool enoughDataFound = false;
        // Limit updateEndOfData() call frequency to limit file-size pings
        while (!shortTimer.hasExpired(2000) && !enoughDataFound) {
            std::this_thread::sleep_for(std::chrono::microseconds(2000)); // Only repeat checking data size after waiting 2 ms
            updateEndOfData();
            enoughDataFound = readIndex + numBlocks * samplesPerDataBlock <= totalNumSamples;
        }

        // If not enough data has been found, stop as normal
        if (!enoughDataFound) {
            emit dataFileReader->sendSetCommand("RunMode", "Stop");
            dataFileReader->setStatusBarEOF();
            return 0;
        }

        // If enough data has now been found, set normal playback speed and begin running again
        dataFileReader->setPlaybackSpeed(1.0);
        // TODO - animate hitting 'run' again
    }

    uint16_t word;
    uint8_t* pWrite = buffer;
    for (int block = 0; block < numBlocks; ++block) {
        for (int sample = 0; sample < samplesPerDataBlock; ++sample) {
            // Write header magic number.
            uint64_t header = RHXDataBlock::headerMagicNumber(info->controllerType);
            pWrite[0] = (header & 0x00000000000000ffUL) >> 0;
            pWrite[1] = (header & 0x000000000000ff00UL) >> 8;
            pWrite[2] = (header & 0x0000000000ff0000UL) >> 16;
            pWrite[3] = (header & 0x00000000ff000000UL) >> 24;
            pWrite[4] = (header & 0x000000ff00000000UL) >> 32;
            pWrite[5] = (header & 0x0000ff0000000000UL) >> 40;
            pWrite[6] = (header & 0x00ff000000000000UL) >> 48;
            pWrite[7] = (header & 0xff00000000000000UL) >> 56;
            pWrite += 8;

            // Load one-sample data frame.
            loadDataFrame();

            // Write timestamp.
            pWrite[0] = (timeStamp & 0x000000ffU) >> 0;
            pWrite[1] = (timeStamp & 0x0000ff00U) >> 8;
            pWrite[2] = (timeStamp & 0x00ff0000U) >> 16;
            pWrite[3] = (timeStamp & 0xff000000U) >> 24;
            pWrite += 4;

            // Write amplifier and auxiliary data.
            switch (info->controllerType) {
            case ControllerRecordUSB2:
            case ControllerRecordUSB3:
                // Write auxiliary command 0-2 results.
                for (int channel = 0; channel < 3; ++channel) {
                    for (int stream = 0; stream < numDataStreams; ++stream) {
                        word = 0;
                        if (channel == 1) {
                            switch (sample % 4) {
                            case 0:
                                if (sample == 124) {
                                    word = supplyVoltageData[stream];
                                } else {
                                    word = 0x0049U; // ROM register 40 read result
                                }
                                break;
                            case 1:
                                word = auxInputData[stream][0];
                                break;
                            case 2:
                                word = auxInputData[stream][1];
                                break;
                            case 3:
                                word = auxInputData[stream][2];
                                break;
                            }
                        }
                        pWrite[0] = (word & 0x00ffU) >> 0;
                        pWrite[1] = (word & 0xff00U) >> 8;
                        pWrite += 2;
                    }
                }
                // Write amplifier data.
                for (int channel = 0; channel < channelsPerStream; ++channel) {
                    for (int stream = 0; stream < numDataStreams; ++stream) {
                        word = amplifierData[stream][channel];
                        pWrite[0] = (word & 0x00ffU) >> 0;
                        pWrite[1] = (word & 0xff00U) >> 8;
                        pWrite += 2;
                    }
                }
                break;
            case ControllerStimRecord:
                // Write auxiliary command 1-3 results.
                for (int channel = 1; channel < 4; ++channel) {
                    for (int stream = 0; stream < numDataStreams; ++stream) {
                        if (channel == 2) {  // Write compliance limit data.
                            pWrite[0] = (stimData[stream][7].complianceLimit << 7) | (stimData[stream][6].complianceLimit << 6) |
                                        (stimData[stream][5].complianceLimit << 5) | (stimData[stream][4].complianceLimit << 4) |
                                        (stimData[stream][3].complianceLimit << 3) | (stimData[stream][2].complianceLimit << 2) |
                                        (stimData[stream][1].complianceLimit << 1) | (stimData[stream][0].complianceLimit << 0);
                            pWrite[1] = (stimData[stream][15].complianceLimit << 7) | (stimData[stream][14].complianceLimit << 6) |
                                        (stimData[stream][13].complianceLimit << 5) | (stimData[stream][12].complianceLimit << 4) |
                                        (stimData[stream][11].complianceLimit << 3) | (stimData[stream][10].complianceLimit << 2) |
                                        (stimData[stream][ 9].complianceLimit << 1) | (stimData[stream][ 8].complianceLimit << 0);
                            pWrite[2] = 0;  // All zeros in MSBs signals read from register 40 (compliance limit)
                            pWrite[3] = 0;
                        } else {
                            pWrite[0] = 0;
                            pWrite[1] = 0;
                            pWrite[2] = 0;
                            pWrite[3] = 0;
                        }
                        pWrite += 4;
                    }
                }
                // Write amplifier data.
                for (int channel = 0; channel < channelsPerStream; ++channel) {
                    for (int stream = 0; stream < numDataStreams; ++stream) {
                        word = dcAmplifierData[stream][channel];
                        pWrite[0] = (word & 0x00ffU) >> 0;
                        pWrite[1] = (word & 0xff00U) >> 8;
                        word = amplifierData[stream][channel];
                        pWrite[2] = (word & 0x00ffU) >> 0;
                        pWrite[3] = (word & 0xff00U) >> 8;
                        pWrite += 4;
                    }
                }
                // Write auxiliary command 0 results.
                for (int stream = 0; stream < numDataStreams; ++stream) {
                    pWrite[0] = 0;
                    pWrite[1] = 0;
                    pWrite[2] = 0;
                    pWrite[3] = 0;
                    pWrite += 4;
                }
                break;
            }

            // Write filler words.
            int numFillerWords = 0;
            if (type == ControllerRecordUSB2) {
                numFillerWords = numDataStreams;
            } else if (type == ControllerRecordUSB3) {
                numFillerWords = numDataStreams % 4;
            }
            for (int i = 0; i < numFillerWords; ++i) {
                pWrite[0] = 0;
                pWrite[1] = 0;
                pWrite += 2;
            }

            // Write stimulation data (ControllerStimRecord only).
            if (type == ControllerStimRecord) {
                // Write stimulation on/off data.
                for (int stream = 0; stream < numDataStreams; ++stream) {
                    pWrite[0] = (stimData[stream][7].stimOn << 7) | (stimData[stream][6].stimOn << 6) |
                                (stimData[stream][5].stimOn << 5) | (stimData[stream][4].stimOn << 4) |
                                (stimData[stream][3].stimOn << 3) | (stimData[stream][2].stimOn << 2) |
                                (stimData[stream][1].stimOn << 1) | (stimData[stream][0].stimOn << 0);
                    pWrite[1] = (stimData[stream][15].stimOn << 7) | (stimData[stream][14].stimOn << 6) |
                                (stimData[stream][13].stimOn << 5) | (stimData[stream][12].stimOn << 4) |
                                (stimData[stream][11].stimOn << 3) | (stimData[stream][10].stimOn << 2) |
                                (stimData[stream][ 9].stimOn << 1) | (stimData[stream][ 8].stimOn << 0);
                    pWrite += 2;
                }
                // Write stimulation polarity data.
                for (int stream = 0; stream < numDataStreams; ++stream) {
                    pWrite[0] = (stimData[stream][7].stimPol << 7) | (stimData[stream][6].stimPol << 6) |
                                (stimData[stream][5].stimPol << 5) | (stimData[stream][4].stimPol << 4) |
                                (stimData[stream][3].stimPol << 3) | (stimData[stream][2].stimPol << 2) |
                                (stimData[stream][1].stimPol << 1) | (stimData[stream][0].stimPol << 0);
                    pWrite[1] = (stimData[stream][15].stimPol << 7) | (stimData[stream][14].stimPol << 6) |
                                (stimData[stream][13].stimPol << 5) | (stimData[stream][12].stimPol << 4) |
                                (stimData[stream][11].stimPol << 3) | (stimData[stream][10].stimPol << 2) |
                                (stimData[stream][ 9].stimPol << 1) | (stimData[stream][ 8].stimPol << 0);
                    pWrite += 2;
                }
                // Write amplifier settle data.
                for (int stream = 0; stream < numDataStreams; ++stream) {
                    pWrite[0] = (stimData[stream][7].ampSettle << 7) | (stimData[stream][6].ampSettle << 6) |
                                (stimData[stream][5].ampSettle << 5) | (stimData[stream][4].ampSettle << 4) |
                                (stimData[stream][3].ampSettle << 3) | (stimData[stream][2].ampSettle << 2) |
                                (stimData[stream][1].ampSettle << 1) | (stimData[stream][0].ampSettle << 0);
                    pWrite[1] = (stimData[stream][15].ampSettle << 7) | (stimData[stream][14].ampSettle << 6) |
                                (stimData[stream][13].ampSettle << 5) | (stimData[stream][12].ampSettle << 4) |
                                (stimData[stream][11].ampSettle << 3) | (stimData[stream][10].ampSettle << 2) |
                                (stimData[stream][ 9].ampSettle << 1) | (stimData[stream][ 8].ampSettle << 0);
                    pWrite += 2;
                }
                // Write charge recovery data.
                for (int stream = 0; stream < numDataStreams; ++stream) {
                    pWrite[0] = (stimData[stream][7].chargeRecov << 7) | (stimData[stream][6].chargeRecov << 6) |
                                (stimData[stream][5].chargeRecov << 5) | (stimData[stream][4].chargeRecov << 4) |
                                (stimData[stream][3].chargeRecov << 3) | (stimData[stream][2].chargeRecov << 2) |
                                (stimData[stream][1].chargeRecov << 1) | (stimData[stream][0].chargeRecov << 0);
                    pWrite[1] = (stimData[stream][15].chargeRecov << 7) | (stimData[stream][14].chargeRecov << 6) |
                                (stimData[stream][13].chargeRecov << 5) | (stimData[stream][12].chargeRecov << 4) |
                                (stimData[stream][11].chargeRecov << 3) | (stimData[stream][10].chargeRecov << 2) |
                                (stimData[stream][ 9].chargeRecov << 1) | (stimData[stream][ 8].chargeRecov << 0);
                    pWrite += 2;
                }
            }

            // Write Analog Out data (ControllerStimRecord only).
            if (type == ControllerStimRecord) {
                for (int i = 0; i < 8; ++i) {
                    word = analogOutData[i];
                    pWrite[0] = (word & 0x00ffU) >> 0;
                    pWrite[1] = (word & 0xff00U) >> 8;
                    pWrite += 2;
                }
            }

            // Write Analog In data.
            for (int i = 0; i < 8; ++i) {
                word = analogInData[i];
                pWrite[0] = (word & 0x00ffU) >> 0;
                pWrite[1] = (word & 0xff00U) >> 8;
                pWrite += 2;
            }

            // Write Digital In data.
            word = digitalInData;
            pWrite[0] = (word & 0x00ffU) >> 0;
            pWrite[1] = (word & 0xff00U) >> 8;
            pWrite += 2;

            // Write Digital Out data.
            word = digitalOutData;
            pWrite[0] = (word & 0x00ffU) >> 0;
            pWrite[1] = (word & 0xff00U) >> 8;
            pWrite += 2;

            readIndex++;
        }
    }

    dataFileReader->setStatusBarReady();

    return pWrite - buffer;
}

int64_t FilePerChannelManager::blocksPresent()
{
    // Should remain accurate even if data file continues growing
    updateEndOfData();
    return totalNumSamples / info->samplesPerDataBlock;
}

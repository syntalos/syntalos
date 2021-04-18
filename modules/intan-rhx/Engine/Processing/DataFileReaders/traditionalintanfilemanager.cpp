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
#include "traditionalintanfilemanager.h"

TraditionalIntanFileManager::TraditionalIntanFileManager(const QString& fileName_, IntanHeaderInfo* info_, bool& canReadFile,
                                                         QString& report, DataFileReader* parent) :
    DataFileManager(fileName_, info_, parent),
    dataFile(nullptr)
{
    dataFile = new DataFile(fileName);

    totalNumSamples = info->numSamplesInFile;
    dataFile->seek(info->headerSizeInBytes);

    readIndex = 0;
    positionInDataBlock = 0;
    consecutiveFileIndex = 0;
    atEndOfCurrentFile = false;

    // Allocate buffers for reading entire data block at once.
    samplesPerDataBlock = info->samplesPerDataBlock;
    timeStampBuffer.resize(samplesPerDataBlock);
    amplifierDataBuffer.resize(samplesPerDataBlock * info->numEnabledAmplifierChannels);
    if (info->dcAmplifierDataSaved) {   // This parameter is accurate in traditional Intan format headers, even with old software.
        dcAmplifierDataBuffer.resize(samplesPerDataBlock * info->numEnabledAmplifierChannels);
    }
    if (info->stimDataPresent) {
        stimDataBuffer.resize(samplesPerDataBlock * info->numEnabledAmplifierChannels);
    }
    auxInputDataBuffer.resize((samplesPerDataBlock / 4) * info->numEnabledAuxInputChannels);
    supplyVoltageDataBuffer.resize(info->numEnabledSupplyVoltageChannels);
    analogInDataBuffer.resize(samplesPerDataBlock * info->numEnabledBoardAdcChannels);
    analogOutDataBuffer.resize(samplesPerDataBlock * info->numEnabledBoardDacChannels);
    if (info->numEnabledDigitalInChannels > 0) {
        digitalInDataBuffer.resize(samplesPerDataBlock);
    }
    if (info->numEnabledDigitalOutChannels > 0) {
        digitalOutDataBuffer.resize(samplesPerDataBlock);
    }
    tempSensorBuffer.resize(info->numTempSensors);

    // Find all time-consecutive data files and add their filenames and number of samples to a list to facilitate
    // seamless playback.
    QFileInfo fileInfo(fileName);
    QDir directory(fileInfo.path());
    QString fileExtension = (info->fileType == RHDHeaderFile) ? ".rhd" : ".rhs";
    QStringList nameFilters;
    nameFilters.append("*" + fileExtension);
    QList<QFileInfo> infoList = directory.entryInfoList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);

    while (infoList.first().baseName() != fileInfo.baseName()) {
        infoList.removeFirst();
    }
    while (infoList.last().baseName().section('_', 0, 0) != fileInfo.baseName().section('_', 0, 0)) {
        infoList.removeLast();
    }

    firstTimeStamp = info->firstTimeStamp;
    lastTimeStamp = info->lastTimeStamp;

    vector<int64_t> numSamplesInFiles;
    numSamplesInFiles.push_back(info->numSamplesInFile);
    bool discontinuity = false;
    int i;
    for (i = 1; i < (int) infoList.size(); ++i) {
        IntanHeaderInfo info2;
        QString errorMsg2;
        DataFileReader::readHeader(infoList.at(i).path() + "/" + infoList.at(i).fileName(), info2, errorMsg2);
        if (info2 != *info) {   // Headers must be equal.
            discontinuity = true;
            break;
        } else if (info2.firstTimeStamp != (lastTimeStamp + 1) &&  // Time stamps must be contiguous.
                   info2.firstTimeStamp != (lastTimeStamp + 2) &&  // (Allow for one or two missing time stamps.)
                   info2.firstTimeStamp != (lastTimeStamp + 3)) {
            discontinuity = true;
//            cout << "   discontinuity: " << lastTimeStamp << " to " << info2.firstTimeStamp << " (" << info2.firstTimeStamp - lastTimeStamp - 1 << " missing)" << EndOfLine;
            break;
        } else {
            numSamplesInFiles.push_back(info2.numSamplesInFile);
            totalNumSamples += info2.numSamplesInFile;
            lastTimeStamp += info2.numSamplesInFile;
        }
    }
    if (discontinuity) {
        while ((int) infoList.size() > i) {
            infoList.removeLast();
        }
    }

    consecutiveFiles.resize(infoList.size());
    bool multipleContiguousFiles = infoList.size() > 1;
    if (multipleContiguousFiles) {
        report += "Multiple contiguous data files found:" + EndOfLine;
    }
    for (int i = 0; i < (int) infoList.size(); ++i) {
        consecutiveFiles[i].fileName = infoList[i].path() + "/" + infoList[i].fileName();
//        qDebug() << "Added file name: " << consecutiveFiles[i].fileName;
        consecutiveFiles[i].numSamplesInFile = numSamplesInFiles[i];
        if (multipleContiguousFiles) {
            report += "  " + infoList[i].fileName() + EndOfLine;
        }
    }

    report += "Total recording time: " + timeString(totalNumSamples) + EndOfLine;

    // Read and store contents of live notes file, if present.
    QFile* liveNotesFile = openLiveNotes();
    if (liveNotesFile) {
        readLiveNotes(liveNotesFile);
        liveNotesFile->close();
        delete liveNotesFile;
    }

    canReadFile = true;
}

TraditionalIntanFileManager::~TraditionalIntanFileManager()
{
    if (dataFile) delete dataFile;
}

QString TraditionalIntanFileManager::currentFileName() const
{
    return consecutiveFiles[consecutiveFileIndex].fileName;
}

void TraditionalIntanFileManager::loadNextDataBlock()
{
    for (int i = 0; i < (int) timeStampBuffer.size(); ++i) {
        timeStampBuffer[i] = dataFile->readTimeStamp();
    }
    for (int i = 0; i < (int) amplifierDataBuffer.size(); ++i) {
        amplifierDataBuffer[i] = dataFile->readWord();
    }
    for (int i = 0; i < (int) dcAmplifierDataBuffer.size(); ++i) {
        dcAmplifierDataBuffer[i] = dataFile->readWord();
    }
    for (int i = 0; i < (int) stimDataBuffer.size(); ++i) {
        stimDataBuffer[i] = dataFile->readWord();
    }
    for (int i = 0; i < (int) auxInputDataBuffer.size(); ++i) {
        auxInputDataBuffer[i] = dataFile->readWord();
    }
    for (int i = 0; i < (int) supplyVoltageDataBuffer.size(); ++i) {
        supplyVoltageDataBuffer[i] = dataFile->readWord();
    }
    for (int i = 0; i < (int) tempSensorBuffer.size(); ++i) {
        tempSensorBuffer[i] = dataFile->readSignedWord();
    }
    for (int i = 0; i < (int) analogInDataBuffer.size(); ++i) {
        analogInDataBuffer[i] = dataFile->readWord();
    }
    for (int i = 0; i < (int) analogOutDataBuffer.size(); ++i) {
        analogOutDataBuffer[i] = dataFile->readWord();
    }
    for (int i = 0; i < (int) digitalInDataBuffer.size(); ++i) {
        digitalInDataBuffer[i] = dataFile->readWord();
    }
    for (int i = 0; i < (int) digitalOutDataBuffer.size(); ++i) {
        digitalOutDataBuffer[i] = dataFile->readWord();
    }

    atEndOfCurrentFile = dataFile->atEnd();
}

void TraditionalIntanFileManager::loadDataFrame()
{
    int numDataStreams = info->numDataStreams;
    int channelsPerStream = RHXDataBlock::channelsPerStream(info->controllerType);

    if (positionInDataBlock == 0) loadNextDataBlock();

    timeStamp = timeStampBuffer[positionInDataBlock];

    int index = positionInDataBlock;
    for (int i = 0; i < numDataStreams; ++i) {
        for (int j = 0; j < channelsPerStream; ++j) {
            if (amplifierWasSaved[i][j]) {
                amplifierData[i][j] = amplifierDataBuffer[index];
                index += samplesPerDataBlock;
            } else {
                amplifierData[i][j] = 32768U;
            }
        }
    }
    if (info->dcAmplifierDataSaved) {
        index = positionInDataBlock;
        for (int i = 0; i < numDataStreams; ++i) {
            for (int j = 0; j < channelsPerStream; ++j) {
                if (dcAmplifierWasSaved[i][j]) {
                    dcAmplifierData[i][j] = dcAmplifierDataBuffer[index];
                    index += samplesPerDataBlock;
                } else {
                    dcAmplifierData[i][j] = 512U;
                }
            }
        }
    }
    if (info->stimDataPresent) {
        index = positionInDataBlock;
        for (int i = 0; i < numDataStreams; ++i) {
            for (int j = 0; j < channelsPerStream; ++j) {
                if (stimWasSaved[i][j]) {
                    uint16_t word = stimDataBuffer[index];
                    index += samplesPerDataBlock;
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
        index = positionInDataBlock / 4;
        for (int i = 0; i < numDataStreams; ++i) {
            for (int j = 0; j < 3; ++j) {
                if (auxInputWasSaved[i][j]) {
                    auxInputData[i][j] = auxInputDataBuffer[index];
                    index += samplesPerDataBlock / 4;
                } else {
                    auxInputData[i][j] = 0;
                }
            }
        }
        index = 0;
        for (int i = 0; i < numDataStreams; ++i) {
            if (supplyVoltageWasSaved[i]) {
                supplyVoltageData[i] = supplyVoltageDataBuffer[index];
                ++index;
            } else {
                supplyVoltageData[i] = 0;
            }
        }
    }
    index = positionInDataBlock;
    for (int i = 0; i < 8; ++i) {
        if (analogInWasSaved[i]) {
            analogInData[i] = analogInDataBuffer[index];
            index += samplesPerDataBlock;
        } else {
            analogInData[i] = (info->controllerType == ControllerRecordUSB2) ? 0 : 32768U;
        }
    }
    index = positionInDataBlock;
    for (int i = 0; i < 8; ++i) {
        if (analogOutWasSaved[i]) {
            analogOutData[i] = analogOutDataBuffer[index];
            index += samplesPerDataBlock;
        } else {
            analogOutData[i] = 32768U;
        }
    }
    index = positionInDataBlock;
    if (info->numEnabledDigitalInChannels > 0) {
        digitalInData = digitalInDataBuffer[index];
    } else {
        digitalInData = 0;
    }
    index = positionInDataBlock;
    if (info->numEnabledDigitalOutChannels > 0) {
        digitalOutData = digitalOutDataBuffer[index];
    } else {
        digitalOutData = 0;
    }

    if (++positionInDataBlock == samplesPerDataBlock) {
        positionInDataBlock = 0;
        if (atEndOfCurrentFile) {
//            cout << "Closing data file " << consecutiveFiles[consecutiveFileIndex].fileName.toStdString() << EndOfLine;
            if (consecutiveFileIndex + 1 < (int) consecutiveFiles.size()) {
                dataFile->close();
                delete dataFile;
                ++consecutiveFileIndex;
//                cout << "Opening data file " << consecutiveFiles[consecutiveFileIndex].fileName.toStdString() << EndOfLine;
                dataFile = new DataFile(consecutiveFiles[consecutiveFileIndex].fileName);
                if (dataFile->isOpen()) {
                    atEndOfCurrentFile = false;
                    dataFile->seek(info->headerSizeInBytes);
                } else {
                    cerr << "Error: Could not open data file " << consecutiveFiles[consecutiveFileIndex].fileName.toStdString()
                         << '\n';
                }
            }
        }
    }
}

QFile* TraditionalIntanFileManager::openLiveNotes()
{
    QFileInfo fileInfo(fileName);
    QString path = fileInfo.path();
    QString baseName = fileInfo.baseName();
    QFile* liveNotesFile = new QFile(path + "/" + baseName + ".txt");
    if (!liveNotesFile->open(QIODevice::ReadOnly)) {
        delete liveNotesFile;
        liveNotesFile = nullptr;
    }
    return liveNotesFile;
}

int64_t TraditionalIntanFileManager::jumpToTimeStamp(int64_t target)
{
    if (target < firstTimeStamp) target = firstTimeStamp;
    if (target > lastTimeStamp) target = lastTimeStamp;
    target -= firstTimeStamp;   // firstTimeStamp can be negative in triggered recordings.

    target = info->samplesPerDataBlock * (target / info->samplesPerDataBlock);  // Round down to nearest data block boundary.
    if (target < 0) target = 0;
    positionInDataBlock = 0;

    consecutiveFileIndex = 0;
    int64_t cumulativeSamples = 0;
    while (target > cumulativeSamples + consecutiveFiles[consecutiveFileIndex].numSamplesInFile) {
        ++consecutiveFileIndex;
        cumulativeSamples += consecutiveFiles[consecutiveFileIndex - 1].numSamplesInFile;
    }
    int64_t targetDataBlockInFile = (target - cumulativeSamples) / info->samplesPerDataBlock;
    if (targetDataBlockInFile < 0) targetDataBlockInFile = 0;

    dataFile->close();
    delete dataFile;
    dataFile = new DataFile(consecutiveFiles[consecutiveFileIndex].fileName);
    dataFile->seek(info->headerSizeInBytes + targetDataBlockInFile * info->bytesPerDataBlock);

    readIndex = target;
    return readIndex + firstTimeStamp;  // Return actual timestamp jumped to, which will be within one data block of target.
}

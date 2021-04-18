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
#include "datafilereader.h"
#include "datafilemanager.h"


DataFileManager::DataFileManager(const QString& fileName_, IntanHeaderInfo* info_, DataFileReader* parent) :
    fileName(fileName_),
    info(info_),
    dataFileReader(parent)
{
    // Set up boolean arrays marking which signals are present in data file.
    int channelsPerStream = RHXDataBlock::channelsPerStream(info->controllerType);
    amplifierWasSaved.resize(info->numDataStreams);
    for (int i = 0; i < (int) amplifierWasSaved.size(); ++i) {
        amplifierWasSaved[i].resize(channelsPerStream, false);
    }
    if (info->controllerType == ControllerStimRecordUSB2) {
        // Can't be confident of value of dcAmplifierDataSaved since old RHS software saves it as 0 in non-Intan format,
        // so always allocate dcAmplifierWasSaved.
        dcAmplifierWasSaved.resize(info->numDataStreams);
        for (int i = 0; i < (int) dcAmplifierWasSaved.size(); ++i) {
            dcAmplifierWasSaved[i].resize(channelsPerStream, false);
        }
    }
    if (info->stimDataPresent) {
        stimWasSaved.resize(info->numDataStreams);
        for (int i = 0; i < (int) stimWasSaved.size(); ++i) {
            stimWasSaved[i].resize(channelsPerStream, false);
        }
    }
    auxInputWasSaved.resize(info->numDataStreams);
    for (int i = 0; i < (int) auxInputWasSaved.size(); ++i) {
        auxInputWasSaved[i].resize(3, false);
    }
    supplyVoltageWasSaved.resize(info->numDataStreams, false);
    analogInWasSaved.resize(8, false);
    analogOutWasSaved.resize(8, false);
    digitalInWasSaved.resize(16, false);
    digitalOutWasSaved.resize(16, false);

    for (int i = 0; i < info->numGroups(); ++i) {
        for (int j = 0; j < info->groups[i].numChannels(); ++j) {
            SignalType signalType = info->groups[i].channels[j].signalType;
            bool enabled = info->groups[i].channels[j].enabled;
            if (enabled) {  // Aren't ALL signals in the header file enabled?  We'll check again anyway.
                int stream = info->groups[i].channels[j].boardStream;
                int channel = info->groups[i].channels[j].chipChannel;
                int order = info->groups[i].channels[j].nativeOrder;
                if (signalType == AmplifierSignal) {
                    amplifierWasSaved[stream][channel] = true;
                    if (info->dcAmplifierDataSaved) {
                        dcAmplifierWasSaved[stream][channel] = true;
                    }
                    if (info->stimDataPresent) {
                        stimWasSaved[stream][channel] = true;
                    }
                } else if (signalType == AuxInputSignal) {
                    auxInputWasSaved[stream][channel] = true;
                } else if (signalType == SupplyVoltageSignal) {
                    supplyVoltageWasSaved[stream] = true;
                } else if (signalType == BoardAdcSignal) {
                    analogInWasSaved[order] = true;
                } else if (signalType == BoardDacSignal) {
                    analogOutWasSaved[order] = true;
                } else if (signalType == BoardDigitalInSignal) {
                    digitalInWasSaved[order] = true;
                } else if (signalType == BoardDigitalOutSignal) {
                    digitalOutWasSaved[order] = true;
                }
            }
        }
    }

    // Set up vectors for storing a single data frame.
    amplifierData.resize(info->numDataStreams);
    for (int i = 0; i < (int) amplifierData.size(); ++i) {
        amplifierData[i].resize(channelsPerStream, false);
    }
    if (info->controllerType == ControllerStimRecordUSB2) {
        dcAmplifierData.resize(info->numDataStreams);
        for (int i = 0; i < (int) dcAmplifierData.size(); ++i) {
            dcAmplifierData[i].resize(channelsPerStream, false);
        }
        stimData.resize(info->numDataStreams);
        posStimAmplitudeFound.resize(info->numDataStreams);
        negStimAmplitudeFound.resize(info->numDataStreams);
        for (int i = 0; i < info->numDataStreams; ++i) {
            stimData[i].resize(channelsPerStream);
            posStimAmplitudeFound[i].resize(channelsPerStream, false);
            negStimAmplitudeFound[i].resize(channelsPerStream, false);
        }
    }
    if (info->controllerType != ControllerStimRecordUSB2) {
        auxInputData.resize(info->numDataStreams);
        for (int i = 0; i < (int) auxInputData.size(); ++i) {
            auxInputData[i].resize(3, false);
        }
        supplyVoltageData.resize(info->numDataStreams, false);
    }
    analogInData.resize(8, false);
    analogOutData.resize(8, false);
}

DataFileManager::~DataFileManager()
{
}

void DataFileManager::readLiveNotes(QFile* liveNotesFile)
{
    QTextStream inStream(liveNotesFile);
    QString line;
    while (inStream.readLineInto(&line, 2048)) {
        string timeString = line.section(',', 1, 1).toStdString();
        if (timeString.empty()) {
            liveNotes[timeString] = line.section(',', 2).toStdString();
        }
    }
}

QString DataFileManager::getLastLiveNote()
{
    if (liveNotes.empty()) return QString("");
    map<string, string>::const_iterator p = liveNotes.find(dataFileReader->filePositionString().toStdString());
    if (p != liveNotes.end()) {
        lastLiveNote = dataFileReader->filePositionString() + QString(": ") + QString::fromStdString(p->second);
    }
    return lastLiveNote;
}

QString DataFileManager::timeString(int64_t samples)
{
    bool isNegative = false;
    int timeInSeconds = round((double) samples / AbstractRHXController::getSampleRate(info->sampleRate));
    if (timeInSeconds < 0) {
        isNegative = true;
        timeInSeconds = -timeInSeconds;
    }
    QTime time(0, 0);
    QString timeStr = time.addSecs(timeInSeconds).toString("HH:mm:ss");
    if (isNegative) {
        timeStr = "-" + timeStr;
    }
    return timeStr;
}

long DataFileManager::readDataBlocksRaw(int numBlocks, uint8_t* buffer)
{
    // Store these values locally to speed up execution below.
    ControllerType type = info->controllerType;
    int samplesPerDataBlock = RHXDataBlock::samplesPerDataBlock(type);  // Use RHX standard of samples per data block, not file's
    int numDataStreams = info->numDataStreams;
    int channelsPerStream = RHXDataBlock::channelsPerStream(type);

    if (readIndex + numBlocks * samplesPerDataBlock > totalNumSamples) {   // End of file
        emit dataFileReader->sendSetCommand("RunMode", "Stop");
        dataFileReader->setStatusBarEOF();
        return 0;
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
            case ControllerStimRecordUSB2:
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

            // Write stimulation data (ControllerStimRecordUSB2 only).
            if (type == ControllerStimRecordUSB2) {
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

            // Write Analog Out data (ControllerStimRecordUSB2 only).
            if (type == ControllerStimRecordUSB2) {
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

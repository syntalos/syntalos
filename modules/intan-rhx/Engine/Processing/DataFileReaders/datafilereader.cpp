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
#include "abstractrhxcontroller.h"
#include "traditionalintanfilemanager.h"
#include "filepersignaltypemanager.h"
#include "fileperchannelmanager.h"
#include "datafilereader.h"
#include "advancedstartupdialog.h"

int IntanHeaderInfo::groupIndex(const QString& prefix) const
{
    int index = -1;
    for (int i = 0; i < (int) groups.size(); ++i) {
        if (groups[i].prefix == prefix) {
            index = i;
            break;
        }
    }
    return index;
}

int IntanHeaderInfo::numChannels() const
{
    int num = 0;
    for (int i = 0; i < (int) groups.size(); ++i) {
        num += groups[i].numChannels();
    }
    return num;
}

int IntanHeaderInfo::numAmplifierChannels() const
{
    int num = 0;
    for (int i = 0; i < (int) groups.size(); ++i) {
        num += groups[i].numAmplifierChannels;
    }
    return num;
}

int IntanHeaderInfo::adjustNumChannels(SignalType signalType, int delta)
{
    if (signalType == AmplifierSignal) return numEnabledAmplifierChannels += delta;
    else if (signalType == AuxInputSignal) return numEnabledAuxInputChannels += delta;
    else if (signalType == SupplyVoltageSignal) return numEnabledSupplyVoltageChannels += delta;
    else if (signalType == BoardAdcSignal) return numEnabledBoardAdcChannels += delta;
    else if (signalType == BoardDacSignal) return numEnabledBoardDacChannels += delta;
    else if (signalType == BoardDigitalInSignal) return numEnabledDigitalInChannels += delta;
    else if (signalType == BoardDigitalOutSignal) return numEnabledDigitalOutChannels += delta;
    else return 0;
}

bool IntanHeaderInfo::removeChannel(const QString& nativeChannelName)
{
    for (int i = 0; i < (int) groups.size(); ++i) {
        HeaderFileGroup& group = groups[i];
        for (auto j = group.channels.begin(); j != group.channels.end(); ) {
            if ((*j).nativeChannelName == nativeChannelName) {
                j = group.channels.erase(j);
                return true;  // This function will only erase one instance of the named channel; we assume it appears only once.
            } else {
                ++j;
            }
        }
    }
    return false;
}

void IntanHeaderInfo::removeAllChannels(SignalType signalType)
{
    for (int i = 0; i < (int) groups.size(); ++i) {
        HeaderFileGroup& group = groups[i];
        for (auto j = group.channels.begin(); j != group.channels.end(); ) {
            if ((*j).signalType == signalType) {
                j = group.channels.erase(j);
            } else {
                ++j;
            }
        }
    }
    if (signalType == AmplifierSignal) numEnabledAmplifierChannels = 0;
    else if (signalType == AuxInputSignal) numEnabledAuxInputChannels = 0;
    else if (signalType == SupplyVoltageSignal) numEnabledSupplyVoltageChannels = 0;
    else if (signalType == BoardAdcSignal) numEnabledBoardAdcChannels = 0;
    else if (signalType == BoardDacSignal) numEnabledBoardDacChannels = 0;
    else if (signalType == BoardDigitalInSignal) numEnabledDigitalInChannels = 0;
    else if (signalType == BoardDigitalOutSignal) numEnabledDigitalOutChannels = 0;
}

QString IntanHeaderInfo::getChannelName(SignalType signalType, int stream, int channel)
{
    for (int i = 0; i < (int) groups.size(); ++i) {
        HeaderFileGroup& fileGroup = groups[i];
        for (int j = 0; j < (int) fileGroup.numChannels(); ++j) {
            HeaderFileChannel& fileChannel = fileGroup.channels[j];
            if (fileChannel.signalType == signalType) {
                if (signalType == AmplifierSignal || signalType == AuxInputSignal) {
                    if (fileChannel.boardStream == stream && fileChannel.chipChannel == channel) return fileChannel.nativeChannelName;
                } else if (signalType == SupplyVoltageSignal) {
                    if (fileChannel.boardStream == stream) return fileChannel.nativeChannelName;
                } else if (signalType == BoardAdcSignal || signalType == BoardDacSignal ||
                           signalType == BoardDigitalInSignal || signalType == BoardDigitalOutSignal)  {
                    if (fileChannel.nativeOrder == channel) return fileChannel.nativeChannelName;
                }
            }
        }
    }
    std::cerr << "IntanHeaderInfo::getChannelName(" << (int) signalType << ", " << stream << ", " << channel << ") not found.\n";
    return QString("");
}

void IntanHeaderInfo::printInfo() const
{
    std::cout << "headerSizeInBytes: " << headerSizeInBytes << '\n';
    std::cout << "bytesPerDataBlock: " << bytesPerDataBlock << '\n';
    std::cout << "numDataStreams: " << numDataStreams << '\n';
    std::cout << "numSPIPorts: " << numSPIPorts << '\n';
    std::cout << "expanderConnected: " << expanderConnected << '\n';
    std::cout << "sampleRate: " << sampleRate << '\n';
    std::cout << "stimStepSize: " << stimStepSize << '\n';
    std::cout << "controllerType: " << controllerType << '\n';
    std::cout << "fileType: " << fileType << '\n';
    std::cout << "boardMode: " << boardMode << '\n';
    std::cout << "samplesPerDataBlock: " << samplesPerDataBlock << '\n';
    std::cout << "dataFileVersionNumber: " << dataFileMainVersionNumber << "." << dataFileSecondaryVersionNumber << '\n';
    std::cout << "numEnabledAmplifierChannels: " << numEnabledAmplifierChannels << '\n';
    std::cout << "numEnabledAuxInputChannels: " << numEnabledAuxInputChannels << '\n';
    std::cout << "numEnabledSupplyVoltageChannels: " << numEnabledSupplyVoltageChannels << '\n';
    std::cout << "numEnabledBoardAdcChannels: " << numEnabledBoardAdcChannels << '\n';
    std::cout << "numEnabledBoardDacChannels: " << numEnabledBoardDacChannels << '\n';
    std::cout << "numEnabledBoardDigitalInChannels: " << numEnabledDigitalInChannels << '\n';
    std::cout << "numEnabledBoardDigitalOutChannels: " << numEnabledDigitalOutChannels << '\n';
    std::cout << "numTempSensors: " << numTempSensors << '\n';
    std::cout << "dcAmplifierDataSaved: " << dcAmplifierDataSaved << '\n';
    std::cout << "groups.size() = " << groups.size() << '\n';
    for (int i = 0; i < (int) groups.size(); ++i) {
        std::cout << "  groups[" << i << "].numChannels() = " << groups[i].numChannels() << '\n';
    }
}

// This 'equal to' operator doesn't compare every element, but it compares all the important elements.
bool operator ==(const IntanHeaderInfo &a, const IntanHeaderInfo &b)
{
    if (a.headerSizeInBytes != b.headerSizeInBytes) return false;
    if (a.bytesPerDataBlock != b.bytesPerDataBlock) return false;
    if (a.numDataStreams != b.numDataStreams) return false;
    if (a.numSPIPorts != b.numSPIPorts) return false;
    if (a.expanderConnected != b.expanderConnected) return false;
    if (a.sampleRate != b.sampleRate) return false;
    if (a.stimStepSize != b.stimStepSize) return false;
    if (a.controllerType != b.controllerType) return false;
    if (a.fileType != b.fileType) return false;
    if (a.boardMode != b.boardMode) return false;
    if (a.samplesPerDataBlock != b.samplesPerDataBlock) return false;
    if (a.dataFileMainVersionNumber != b.dataFileMainVersionNumber) return false;
    if (a.dataFileSecondaryVersionNumber != b.dataFileSecondaryVersionNumber) return false;
    if (a.numEnabledAmplifierChannels != b.numEnabledAmplifierChannels) return false;
    if (a.numEnabledAuxInputChannels != b.numEnabledAuxInputChannels) return false;
    if (a.numEnabledSupplyVoltageChannels != b.numEnabledSupplyVoltageChannels) return false;
    if (a.numEnabledBoardAdcChannels != b.numEnabledBoardAdcChannels) return false;
    if (a.numEnabledBoardDacChannels != b.numEnabledBoardDacChannels) return false;
    if (a.numEnabledDigitalInChannels != b.numEnabledDigitalInChannels) return false;
    if (a.numEnabledDigitalOutChannels != b.numEnabledDigitalOutChannels) return false;
    if (a.numTempSensors != b.numTempSensors) return false;
    if (a.dcAmplifierDataSaved != b.dcAmplifierDataSaved) return false;
    if (a.groups.size() != b.groups.size()) return false;
    for (int i = 0; i < (int) a.groups.size(); ++i) {
        if (a.groups[i].numChannels() != b.groups[i].numChannels()) return false;
        for (int j = 0; j < (int) a.groups[i].numChannels(); ++j) {
            if (a.groups[i].channels[j].signalType != b.groups[i].channels[j].signalType) return false;
        }
    }
    return true;
}

bool operator !=(const IntanHeaderInfo &a, const IntanHeaderInfo &b)
{
    return !(a == b);
}


DataFileReader::DataFileReader(const QString& fileName, bool& canReadFile, QString& report, uint8_t playbackPortsInt, QObject* parent) :
    QObject(parent)
{
    playbackPorts = AdvancedStartupDialog::portsIntToBool(playbackPortsInt);
    report.clear();
    canReadFile = false;
    canReadFile = readHeader(fileName, headerInfo, report);
    if (!canReadFile) return;
    //qDebug() << "Original header info:";
    //printHeader(headerInfo);
    applyPlaybackPorts(headerInfo, report);
    //qDebug() << "After applying playback ports header info:";
    //printHeader(headerInfo);

    dataBlockPeriodInNsec = 1.0e9 * ((double)RHXDataBlock::samplesPerDataBlock(headerInfo.controllerType)) /
            AbstractRHXController::getSampleRate(headerInfo.sampleRate);

    // Determine data file format.
    // DataFileFormat format;
    if (headerInfo.dataSizeInBytes > 0) {
        // format = TraditionalIntanFormat;  // Traditional Intan .rhd/.rhs file format
        dataFileManager = new TraditionalIntanFileManager(fileName, &headerInfo, canReadFile, report, this);
    } else {
        QFileInfo fileInfo(fileName);
        QDir directory(fileInfo.path());

        QStringList nameFilters;
        nameFilters.append("*.dat");

        QList<QFileInfo> infoList = directory.entryInfoList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);

        QStringList perSignalTypeFileNames;
        perSignalTypeFileNames.append("amplifier");
        perSignalTypeFileNames.append("dcamplifier");
        perSignalTypeFileNames.append("stim");
        perSignalTypeFileNames.append("auxiliary");
        perSignalTypeFileNames.append("supply");
        perSignalTypeFileNames.append("analogin");
        perSignalTypeFileNames.append("analogout");
        perSignalTypeFileNames.append("digitalin");
        perSignalTypeFileNames.append("digitalout");
        // Don't check for "time" since time.dat is also found in "one file per channel" format.

        bool foundPerSignalTypeFile = false;
        for (int i = 0; i < infoList.size(); ++i) {
            if (perSignalTypeFileNames.contains(infoList.at(i).baseName().toLower())) {
                foundPerSignalTypeFile = true;
            }
        }
        if (foundPerSignalTypeFile) {
            // format = FilePerSignalTypeFormat;  // "One file per signal type" format
            dataFileManager = new FilePerSignalTypeManager(fileName, &headerInfo, canReadFile, report, this);
        } else {
            // format = FilePerChannelFormat; // "One file per channel" format
            dataFileManager = new FilePerChannelManager(fileName, &headerInfo, canReadFile, report, this);
        }
    }

    playbackSpeed = 1.0;
    live = false;
    timeDeficitInNsec = 0.0;
    timer.start();
}

DataFileReader::~DataFileReader()
{
    if (dataFileManager) delete dataFileManager;
}

void DataFileReader::applyPlaybackPorts(IntanHeaderInfo &info, QString &report)
{
    for (int i = 0; i < info.numGroups(); ++i) {
        HeaderFileGroup *group = &info.groups[i];
        QString prefix = QString(QChar('A' + i));
        if (group->prefix == prefix && !playbackPorts[i] && group->enabled) {
            group->enabled = false;
            group->channels.clear();
            group->numAmplifierChannels = 0;
            report += "Port " + prefix + " present, but deactivated via Playback Control" + EndOfLine;
        }
    }
}

long DataFileReader::readPlaybackDataBlocksRaw(int numBlocks, uint8_t* buffer)
{
    double elapsedTime = (double)timer.nsecsElapsed();
    double targetTime = (double)numBlocks * dataBlockPeriodInNsec / playbackSpeed;
    double excessTime = elapsedTime - (targetTime - timeDeficitInNsec);

    if (excessTime < 0.0) return 0; // Not enough time has passed; wait for the data to be ready

    timer.start();

    timeDeficitInNsec = excessTime; // Remember excess time and subtract it from next time meausurement;
                                    // We need to do this to maintain the sample rate accurately.
    if (timeDeficitInNsec > targetTime) {   // But don't let the deficit be too large in any one pass.
        timeDeficitInNsec = targetTime;
    }

    return dataFileManager->readDataBlocksRaw(numBlocks, buffer);
}

QString DataFileReader::filePositionString() const
{
    return dataFileManager->timeString(dataFileManager->getCurrentTimeStamp());
}

QString DataFileReader::startPositionString() const
{
    return dataFileManager->timeString(dataFileManager->getFirstTimeStamp());
}

QString DataFileReader::endPositionString() const
{
    return dataFileManager->timeString(dataFileManager->getLastTimeStamp());
}


bool DataFileReader::readHeader(const QString& fileName, IntanHeaderInfo& info, QString& report)
{
    int16_t int16Buffer;
    uint32_t uint32Buffer;
    float floatBuffer;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        report = "Error: Cannot open file " + fileName;
        return false;
    }

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_5_11);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

    stream >> uint32Buffer;
    if (uint32Buffer == DataFileMagicNumberRHD) {
        info.fileType = RHDHeaderFile;
    } else if (uint32Buffer == DataFileMagicNumberRHS) {
        info.fileType = RHSHeaderFile;
    } else {
        report = "Header Error: Invalid Intan header identifier: " + QString::number(uint32Buffer, 16).toUpper();
        return false;
    }

    stream >> int16Buffer;
    info.dataFileMainVersionNumber = int16Buffer;
    stream >> int16Buffer;
    info.dataFileSecondaryVersionNumber = int16Buffer;

    if (info.fileType == RHDHeaderFile && info.dataFileMainVersionNumber == 1) {
        info.samplesPerDataBlock = 60;
    } else {
        info.samplesPerDataBlock = 128;
    }

    stream >> floatBuffer;
    info.sampleRate = AbstractRHXController::nearestSampleRate(floatBuffer);
    if ((int)info.sampleRate == -1) {
        report = "Header Error: Invalid sample rate: " + QString::number(floatBuffer);
        return false;
    }

    stream >> int16Buffer;
    info.dspEnabled = (int16Buffer != 0);

    stream >> floatBuffer;
    info.actualDspCutoffFreq = floatBuffer;
    stream >> floatBuffer;
    info.actualLowerBandwidth = floatBuffer;
    if (info.fileType == RHSHeaderFile) {
        stream >> floatBuffer;
        info.actualLowerSettleBandwidth = floatBuffer;
    } else {
        info.actualLowerSettleBandwidth = 0.0;
    }
    stream >> floatBuffer;
    info.actualUpperBandwidth = floatBuffer;

    stream >> floatBuffer;
    info.desiredDspCutoffFreq = floatBuffer;
    stream >> floatBuffer;
    info.desiredLowerBandwidth = floatBuffer;
    if (info.fileType == RHSHeaderFile) {
        stream >> floatBuffer;
        info.desiredLowerSettleBandwidth = floatBuffer;
    } else {
        info.desiredLowerSettleBandwidth = 0.0;
    }
    stream >> floatBuffer;
    info.desiredUpperBandwidth = floatBuffer;

    stream >> int16Buffer;
    if (int16Buffer == 0) {
        info.notchFilterEnabled = false;
        info.notchFilterFreq = 0.0;
    } else if (int16Buffer == 1) {
        info.notchFilterEnabled = true;
        info.notchFilterFreq = 50.0;
    } else if (int16Buffer == 2) {
        info.notchFilterEnabled = true;
        info.notchFilterFreq = 60.0;
    } else {
        report = "Header Error: Invalid notch filter mode: " + QString::number(int16Buffer);
        return false;
    }

    stream >> floatBuffer;
    info.actualImpedanceTestFreq = floatBuffer;
    stream >> floatBuffer;
    info.desiredImpedanceTestFreq = floatBuffer;

    if (info.fileType == RHSHeaderFile) {
        info.stimDataPresent = true;
        stream >> int16Buffer;
        info.ampSettleMode = (int16Buffer != 0);
        stream >> int16Buffer;
        info.chargeRecoveryMode = (int16Buffer != 0);
        stream >> floatBuffer;
        info.stimStepSize = AbstractRHXController::nearestStimStepSize(floatBuffer);
        if ((int)info.stimStepSize == -1) {
            report = "Header Error: Invalid stim step size: " + QString::number(floatBuffer);
            return false;
        }
        stream >> floatBuffer;
        info.chargeRecoveryCurrentLimit = floatBuffer;
        stream >> floatBuffer;
        info.chargeRecoveryTargetVoltage = floatBuffer;
    } else {
        info.stimDataPresent = false;
        info.ampSettleMode = false;
        info.chargeRecoveryMode = false;
        info.stimStepSize = (StimStepSize) -1;
        info.chargeRecoveryCurrentLimit = 0.0;
        info.chargeRecoveryTargetVoltage = 0.0;
    }

    stream >> info.note1;
    stream >> info.note2;
    stream >> info.note3;

    if (info.fileType == RHSHeaderFile) {
        stream >> int16Buffer;
        info.dcAmplifierDataSaved = (int16Buffer != 0); // Warning: The old RHS software saves this value as 0 for non-Intan format!
    } else {
        info.dcAmplifierDataSaved = false;
    }

    if (info.fileType == RHDHeaderFile && info.dataFileVersionNumber() > 1.09) {
        stream >> int16Buffer;
        info.numTempSensors = int16Buffer;
    } else {
        info.numTempSensors = 0;
    }

    if ((info.fileType == RHDHeaderFile && info.dataFileVersionNumber() > 1.29) ||
            info.fileType == RHSHeaderFile) {
        stream >> int16Buffer;
        info.boardMode = int16Buffer;
    } else {
        info.boardMode = RHDUSBInterfaceBoardMode;
    }

    if (info.fileType == RHSHeaderFile) {   // Note: Synth mode recordings from old software saves boardMode = 0.
        info.controllerType = ControllerStimRecord;
    } else if (info.dataFileMainVersionNumber == 2) {
        info.controllerType = ControllerRecordUSB3;
    } else if (info.dataFileMainVersionNumber == 1) {
        info.controllerType = ControllerRecordUSB2;
    } else if (info.boardMode == RHDUSBInterfaceBoardMode) {
        info.controllerType = ControllerRecordUSB2;
    } else if (info.boardMode == RHDControllerBoardMode) {
        info.controllerType = ControllerRecordUSB3;
    } else if (info.boardMode == RHSControllerBoardMode) {
        info.controllerType = ControllerStimRecord;
    } else {
        report = "Header Error: Invalid board mode: " + QString::number(info.boardMode);
        return false;
    }

    if ((info.fileType == RHDHeaderFile && info.dataFileVersionNumber() > 1.99) ||
            info.fileType == RHSHeaderFile) {
        stream >> info.refChannelName;
    } else {
        info.refChannelName.clear();
    }

    stream >> int16Buffer;
    if ((int16Buffer < 0) || (int16Buffer > 12)) {
        report = "Header Error: Invalid number of signal groups: " + QString::number(int16Buffer);
        return false;
    }
    info.groups.resize(int16Buffer);

    info.numDataStreams = 0;
    info.numEnabledAmplifierChannels = 0;
    info.numEnabledAuxInputChannels = 0;
    info.numEnabledSupplyVoltageChannels = 0;
    info.numEnabledBoardAdcChannels = 0;
    info.numEnabledBoardDacChannels = 0;
    info.numEnabledDigitalInChannels = 0;
    info.numEnabledDigitalOutChannels = 0;

    bool moreThanFourSPIPorts = false;
    info.expanderConnected = false;

    for (int i = 0; i < info.numGroups(); i++) {
        HeaderFileGroup& group = info.groups[i];
        stream >> group.name;
        stream >> group.prefix;
        if (group.prefix.toUpper() == "E" || group.prefix.toUpper() == "F" ||
                group.prefix.toUpper() == "G" || group.prefix.toUpper() == "H") {
            moreThanFourSPIPorts = true;
        }
        stream >> int16Buffer;
        group.enabled = (int16Buffer != 0);
        stream >> int16Buffer;
        if ((int16Buffer < 0) || (int16Buffer > 2 * (64 + 3 + 1))) {
            report = "Header Error: Invalid number of channels in signal group " + QString::number(i) + ": " +
                    QString::number(int16Buffer);
            return false;
        }
        group.channels.resize(int16Buffer);
        stream >> int16Buffer;
        if ((int16Buffer < 0) || (int16Buffer > 2 * 64)) {
            report = "Header Error: Invalid number of amplifier channels in signal group " + QString::number(i) + ": " +
                    QString::number(int16Buffer);
            return false;
        }
        group.numAmplifierChannels = int16Buffer;

        for (int j = 0; j < group.numChannels(); ++j) {
            HeaderFileChannel& channel = group.channels[j];
            stream >> channel.nativeChannelName;
            stream >> channel.customChannelName;
            stream >> int16Buffer;
            channel.nativeOrder = int16Buffer;
            stream >> int16Buffer;
            channel.customOrder = int16Buffer;

            stream >> int16Buffer;
            if (info.fileType == RHDHeaderFile) {
                if ((int16Buffer < 0) || (int16Buffer > 5)) {
                    report = "Header Error: Invalid signal type in group " + QString::number(i) + ", channel " +
                            QString::number(j) + ": " + QString::number(int16Buffer);
                    return false;
                }
                channel.signalType = Channel::convertRHDIntToSignalType(int16Buffer);
            } else if (info.fileType == RHSHeaderFile) {
                if ((int16Buffer < 0) || (int16Buffer > 6)) {
                    report = "Header Error: Invalid signal type in group " + QString::number(i) + ", channel " +
                            QString::number(j) + ": " + QString::number(int16Buffer);
                    return false;
                }
                channel.signalType = Channel::convertRHSIntToSignalType(int16Buffer);
            }

            stream >> int16Buffer;
            channel.enabled = (int16Buffer != 0);

            if (channel.enabled) info.adjustNumChannels(channel.signalType, 1);

            if (channel.signalType == BoardAdcSignal || channel.signalType == BoardDacSignal ||
                    channel.signalType == BoardDigitalInSignal || channel.signalType == BoardDigitalOutSignal) {
                if (channel.nativeOrder > 1) info.expanderConnected = true;
            }

            stream >> int16Buffer;
            channel.chipChannel = int16Buffer;
            if (info.fileType == RHSHeaderFile) {
                stream >> int16Buffer;
                channel.commandStream = int16Buffer;
            }
            stream >> int16Buffer;
            channel.boardStream = int16Buffer;
            if (channel.boardStream + 1 > info.numDataStreams) {
                info.numDataStreams = channel.boardStream + 1;
            }

            if (info.fileType == RHDHeaderFile) {
                channel.commandStream = channel.boardStream;  // reasonable guess; probably not used in file playback
            }
            stream >> int16Buffer;
            channel.spikeScopeTriggerMode = int16Buffer;
            stream >> int16Buffer;
            channel.spikeScopeVoltageThreshold = int16Buffer;
            stream >> int16Buffer;
            channel.spikeScopeTriggerChannel = int16Buffer;
            stream >> int16Buffer;
            channel.spikeScopeTriggerPolarity = int16Buffer;
            stream >> floatBuffer;
            channel.impedanceMagnitude = floatBuffer;
            stream >> floatBuffer;
            channel.impedancePhase = floatBuffer;
        }
    }

    if (info.controllerType == ControllerRecordUSB2) {
        info.numSPIPorts = 4;
    } else {
        info.numSPIPorts = moreThanFourSPIPorts ? 8 : 4;
    }

    info.headerOnly = file.atEnd();
    info.headerSizeInBytes = file.pos();
    info.dataSizeInBytes = file.size() - (int64_t)info.headerSizeInBytes;

    info.bytesPerDataBlock = info.samplesPerDataBlock * 4;  // timestamps
    int bytesPerAmp = 2;
    if (info.fileType == RHSHeaderFile) {
        bytesPerAmp += 2;  // stim data;
        if (info.dcAmplifierDataSaved) bytesPerAmp += 2;  // dc amplifiers
    }
    info.bytesPerDataBlock += info.samplesPerDataBlock * bytesPerAmp * info.numEnabledAmplifierChannels;  // amplifiers
    info.bytesPerDataBlock += (info.samplesPerDataBlock / 4) * 2 * info.numEnabledAuxInputChannels;  // aux inputs
    info.bytesPerDataBlock += 2 * info.numEnabledSupplyVoltageChannels;  // supply voltage
    info.bytesPerDataBlock += info.samplesPerDataBlock * 2 * info.numEnabledBoardAdcChannels;  // analog inputs
    info.bytesPerDataBlock += info.samplesPerDataBlock * 2 * info.numEnabledBoardDacChannels;  // analog outputs
    info.bytesPerDataBlock += 2 * info.numTempSensors;  // temperature sensors
    if (info.numEnabledDigitalInChannels > 0) {
        info.bytesPerDataBlock += 2 * info.samplesPerDataBlock;  // digital inputs
    }
    if (info.numEnabledDigitalOutChannels > 0) {
        info.bytesPerDataBlock += 2 * info.samplesPerDataBlock;  // digital outputs
    }

    info.numDataBlocksInFile = info.dataSizeInBytes / info.bytesPerDataBlock;
    info.numSamplesInFile = info.samplesPerDataBlock * info.numDataBlocksInFile;
    info.timeInFile = (double)info.numSamplesInFile / AbstractRHXController::getSampleRate(info.sampleRate);

    int64_t extraBytes = info.dataSizeInBytes - info.bytesPerDataBlock * info.numDataBlocksInFile;
    if (extraBytes != 0) {
        report = "Warning: " + QString::number(extraBytes) + " extra bytes in file." + EndOfLine;
    }

    if (info.numDataBlocksInFile > 0) {
        stream >> info.firstTimeStamp;
        file.seek(info.headerSizeInBytes + (info.numDataBlocksInFile - 1) * info.bytesPerDataBlock +
                  4 * (info.samplesPerDataBlock - 1));
        stream >> info.lastTimeStamp;
    } else {
        info.firstTimeStamp = -1;
        info.lastTimeStamp = -1;
    }

    file.close();
    return true;
}

void DataFileReader::printHeader(const IntanHeaderInfo& info)
{
    std::cout << "File header contents:\n";
    std::cout << "Header file type: " << ((info.fileType == RHDHeaderFile) ? "RHD file\n" : "RHS file\n");
    std::string controllerString;
    if (info.controllerType == ControllerRecordUSB2) controllerString = "RHD USB interface board";
    else if (info.controllerType == ControllerRecordUSB3) controllerString = "RHD recording controller";
    else if (info.controllerType == ControllerStimRecord) controllerString = "RHS stim/recording controller";
    std::cout << "Controller type: " << controllerString << '\n';
    std::cout << "Data file version: " << info.dataFileMainVersionNumber << "." << info.dataFileSecondaryVersionNumber << '\n';
    std::cout << "Number of samples per data block: " << info.samplesPerDataBlock << '\n';
    std::cout << "sample rate: " << AbstractRHXController::getSampleRateString(info.sampleRate) << '\n';
    std::cout << "DSP enabled: " << (info.dspEnabled ? "true\n" : "false\n");

    // ...

    if (info.fileType == RHSHeaderFile) {
        std::cout << "stim step size: " << AbstractRHXController::getStimStepSizeString(info.stimStepSize) << '\n';
    }

    // ...

    std::cout << "board mode: " << info.boardMode << '\n';

    // ...

    std::cout << "total number of data streams: " << info.numDataStreams << '\n';
    std::cout << "total number of amplifier channels: " << info.numEnabledAmplifierChannels << '\n';
    std::cout << "total number of auxiliary input channels: " << info.numEnabledAuxInputChannels << '\n';
    std::cout << "total number of supply voltage channels: " << info.numEnabledSupplyVoltageChannels << '\n';
    std::cout << "total number of analog input channels: " << info.numEnabledBoardAdcChannels << '\n';
    std::cout << "total number of analog output channels: " << info.numEnabledBoardDacChannels << '\n';
    std::cout << "total number of digital input channels: " << info.numEnabledDigitalInChannels << '\n';
    std::cout << "total number of digital output channels: " << info.numEnabledDigitalOutChannels << '\n';

    std::cout << "number of SPI ports: " << info.numSPIPorts << '\n';
    std::cout << "expander connected: " << (info.expanderConnected ? "true" : "false") << '\n';
    std::cout << "header only: " << (info.headerOnly ? "true" : "false") << '\n';
    std::cout << "header size in bytes: " << info.headerSizeInBytes << '\n';
    std::cout << "bytes per data block: " << info.bytesPerDataBlock << '\n';
    std::cout << "data size in bytes: " << info.dataSizeInBytes << '\n';
    std::cout << "number of data blocks in file: " << info.numDataBlocksInFile << '\n';
    std::cout << "number of samples in file: " << info.numSamplesInFile << '\n';
    std::cout << "time in file: " << info.timeInFile << " s" << '\n';
}

int64_t DataFileReader::blocksPresent()
{
    return dataFileManager->blocksPresent();
}

void DataFileReader::jumpToStart()
{
    dataFileManager->jumpToTimeStamp(dataFileManager->getFirstTimeStamp());
    setStatusBarReady();
}

void DataFileReader::jumpToEnd()
{
    dataFileManager->jumpToTimeStamp(dataFileManager->getLastTimeStamp());
    setStatusBarReady();
}

void DataFileReader::jumpToPosition(const QString& targetTime)
{
    QTime timeCalc = QTime::fromString(targetTime, "HH:mm:ss");
    int64_t target = round(((double)timeCalc.msecsSinceStartOfDay() / 1000.0) *
                           AbstractRHXController::getSampleRate(headerInfo.sampleRate));
    dataFileManager->jumpToTimeStamp(target);
    setStatusBarReady();
}

void DataFileReader::jumpRelative(double jumpInSeconds)
{
    int deltaTimeStamp = round(jumpInSeconds * AbstractRHXController::getSampleRate(headerInfo.sampleRate));
    int64_t target = dataFileManager->getCurrentTimeStamp() + deltaTimeStamp;
    dataFileManager->jumpToTimeStamp(target);
    setStatusBarReady();
}

void DataFileReader::setStatusBarReady()
{
    QString liveNote = dataFileManager->getLastLiveNote();
    if (liveNote.isEmpty()) {
        emit setStatusBar(tr("Playback of file ") +
                          dataFileManager->currentFileName());
    } else {
        emit setStatusBar(tr("Live note at ") + liveNote);
    }
    emit setTimeLabel(filePositionString());
}

void DataFileReader::setStatusBarEOF()
{
    emit setStatusBar(tr("End of file ") +
                      dataFileManager->currentFileName());
    emit setTimeLabel(filePositionString());
}

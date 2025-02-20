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

#ifndef DATAFILEREADER_H
#define DATAFILEREADER_H

#include <QObject>
#include <QString>
#include "signalsources.h"
#include "datafilemanager.h"

class DataFileManager;

enum HeaderFileType {
    RHDHeaderFile,
    RHSHeaderFile
};

enum DataFileFormat {
    TraditionalIntanFormat,
    FilePerSignalTypeFormat,
    FilePerChannelFormat
};

struct HeaderFileChannel
{
    QString nativeChannelName;
    QString customChannelName;
    int nativeOrder;
    int customOrder;
    SignalType signalType;
    bool enabled;
    int chipChannel;
    int commandStream;
    int boardStream;
    int spikeScopeTriggerMode;      // 0 = trigger on digital input; 1 = trigger on voltage threshold
    int spikeScopeVoltageThreshold;
    int spikeScopeTriggerChannel;
    int spikeScopeTriggerPolarity;  // 0 = trigger on falling edge; 1 = trigger on rising edge
    double impedanceMagnitude;
    double impedancePhase;
    int channelNumber() const { return nativeChannelName.section('-', -1).toInt(); }
    int endingNumber(int numChars = 1) const { return QStringView{nativeChannelName}.right(numChars).toInt(); }
};


struct HeaderFileGroup
{
    QString name;
    QString prefix;
    bool enabled;
    int numAmplifierChannels;
    std::vector<HeaderFileChannel> channels;
    int numChannels() const { return (int) channels.size(); }
};


struct IntanHeaderInfo
{
public:
    HeaderFileType fileType;
    ControllerType controllerType;
    int dataFileMainVersionNumber;
    int dataFileSecondaryVersionNumber;
    double dataFileVersionNumber() const { return dataFileMainVersionNumber + (double)dataFileSecondaryVersionNumber / 10.0; }

    int samplesPerDataBlock;

    AmplifierSampleRate sampleRate;
    bool dspEnabled;
    double actualDspCutoffFreq;
    double actualLowerBandwidth;
    double actualLowerSettleBandwidth;
    double actualUpperBandwidth;
    double desiredDspCutoffFreq;
    double desiredLowerBandwidth;
    double desiredLowerSettleBandwidth;
    double desiredUpperBandwidth;

    bool notchFilterEnabled;
    double notchFilterFreq;

    double desiredImpedanceTestFreq;
    double actualImpedanceTestFreq;

    bool ampSettleMode;         // false = switch lower bandwidth; true = traditional fast settle
    bool chargeRecoveryMode;    // false = current-limited charge recovery circuit; true = charge recovery switch

    bool stimDataPresent;
    StimStepSize stimStepSize;
    double chargeRecoveryCurrentLimit;
    double chargeRecoveryTargetVoltage;

    QString note1;
    QString note2;
    QString note3;

    bool dcAmplifierDataSaved;

    int numTempSensors;
    int boardMode;

    QString refChannelName;

    std::vector<HeaderFileGroup> groups;
    int numGroups() const { return (int) groups.size(); }
    int groupIndex(const QString& prefix) const;
    int numChannels() const;
    int numAmplifierChannels() const;
    bool removeChannel(const QString& nativeChannelName);
    void removeAllChannels(SignalType signalType);
    QString getChannelName(SignalType signalType, int stream, int channel);

    int numDataStreams;
    int numEnabledAmplifierChannels;
    int numEnabledAuxInputChannels;
    int numEnabledSupplyVoltageChannels;
    int numEnabledBoardAdcChannels;
    int numEnabledBoardDacChannels;
    int numEnabledDigitalInChannels;
    int numEnabledDigitalOutChannels;
    int adjustNumChannels(SignalType signalType, int delta);

    int numSPIPorts;
    bool expanderConnected;

    bool headerOnly;
    int headerSizeInBytes;
    int bytesPerDataBlock;
    int64_t dataSizeInBytes;
    int64_t numDataBlocksInFile;
    int64_t numSamplesInFile;
    double timeInFile;

    int32_t firstTimeStamp;
    int32_t lastTimeStamp;

    void printInfo() const;
};

bool operator ==(const IntanHeaderInfo &a, const IntanHeaderInfo &b);
bool operator !=(const IntanHeaderInfo &a, const IntanHeaderInfo &b);


class DataFileReader : public QObject
{
    Q_OBJECT
public:
    DataFileReader(const QString& fileName, bool& canReadFile, QString& report, uint8_t playbackPortsInt, QObject* parent = nullptr);
    ~DataFileReader();

    ControllerType controllerType() const { return headerInfo.controllerType; }
    AmplifierSampleRate sampleRate() const { return headerInfo.sampleRate; }
    StimStepSize stimStepSize() const { return headerInfo.stimStepSize; }
    int numSPIPorts() const { return headerInfo.numSPIPorts; }
    bool expanderConnected() const { return headerInfo.expanderConnected; }
    int numDataStreams() const { return headerInfo.numDataStreams; }

    const IntanHeaderInfo* getHeaderInfo() const { return &headerInfo; }

    long readPlaybackDataBlocksRaw(int numBlocks, uint8_t* buffer);

    void recordPosStimAmplitude(int stream, int channel, int amplitude) { emit setPosStimAmplitude(stream, channel, amplitude); }
    void recordNegStimAmplitude(int stream, int channel, int amplitude) { emit setNegStimAmplitude(stream, channel, amplitude); }

    QString currentFileName() const { return dataFileManager->currentFileName(); }
    QString filePositionString() const;
    QString startPositionString() const;
    QString endPositionString() const;

    int64_t getCurrentTimeStamp() const { return dataFileManager->getCurrentTimeStamp(); }

    static bool readHeader(const QString& fileName, IntanHeaderInfo& info, QString& report);
    void applyPlaybackPorts(IntanHeaderInfo& info, QString& report);
    static void printHeader(const IntanHeaderInfo& info);

    int64_t blocksPresent();

signals:
    void setPosStimAmplitude(int stream, int channel, int amplitude);
    void setNegStimAmplitude(int stream, int channel, int amplitude);
    void setStatusBar(QString);
    void setTimeLabel(QString);
    void sendSetCommand(QString, QString);

public slots:
    void jumpToEnd();
    void jumpToStart();
    void jumpToPosition(const QString& targetTime);
    void jumpRelative(double jumpInSeconds);
    void setStatusBarReady();
    void setStatusBarEOF();
    void setPlaybackSpeed(double playbackSpeed_) { playbackSpeed = playbackSpeed_; }
    void setLive(bool live_) { live = live_; }
    double getPlaybackSpeed() { return playbackSpeed; }
    bool getLive() { return live; }

private:
    IntanHeaderInfo headerInfo;
    DataFileManager* dataFileManager;

    double playbackSpeed;
    bool live;
    QElapsedTimer timer;
    double dataBlockPeriodInNsec;
    double timeDeficitInNsec;
    QVector<bool> playbackPorts;

    int applyPlaybackPort(int portIndex, HeaderFileGroup *group, QString &report);
};

#endif // DATAFILEREADER_H

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

#ifndef SYSTEMSTATE_H
#define SYSTEMSTATE_H

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <map>

#include "rhxglobals.h"
#include "abstractrhxcontroller.h"
#include "probemapdatastructures.h"
#include "stimparameters.h"
#include "stateitem.h"
#include "rhxregisters.h"
#include "tcpcommunicator.h"
#ifdef __APPLE__
    #include <OpenCL/opencl.h>
#else
    #include "CL/cl.h"
#endif

using namespace std;

class SignalSources;
class Channel;
class BooleanItem;
class XMLInterface;
class ControllerInterface;

struct CPUInfo {
    cl_platform_id platformId;
    cl_device_id deviceId;
    QString name;
    float diagnosticTime;
    int rank;
    bool used;
};

struct GPUInfo : public CPUInfo {
    cl_platform_id platformId;
    cl_device_id deviceId;
};


const int SnippetSize = 50;
const int FramesPerBlock = 128;
const int NotchBandwidth = 10;

bool RestrictAlways(const SystemState*);
bool RestrictIfRunning(const SystemState* state);
bool RestrictIfRecording(const SystemState* state);
bool RestrictIfStimController(const SystemState* state);
bool RestrictIfNotStimController(const SystemState* state);
bool RestrictIfNotStimControllerOrRunning(const SystemState* state);
bool RestrictIfStimController(const SystemState* state);
bool RestrictIfStimControllerOrRunning(const SystemState* state);

class SystemState : public QObject
{
    Q_OBJECT
public:
    SystemState(const AbstractRHXController* controller_, StimStepSize stimStepSize_, int numSPIPorts_, bool expanderConnected_);
    ~SystemState();

    AmplifierSampleRate getSampleRateEnum() const;
    FileFormat getFileFormatEnum() const;
    ControllerType getControllerTypeEnum() const;
    StimStepSize getStimStepSizeEnum() const;
    RHXRegisters::ChargeRecoveryCurrentLimit getChargeRecoveryCurrentLimitEnum() const;

    void addStateSingleItem(SingleItemList &hList, StateSingleItem* item) const;
    StateSingleItem* locateStateSingleItem(SingleItemList &hList, const QString& parameterName) const;
    void addStateFilenameItem(FilenameItemList &hList, StateFilenameItem* item) const;
    StateFilenameItem* locateStateFilenameItem(FilenameItemList &hList, const QString& fullParameterName, QString &pathOrBase_) const;

    void holdUpdate();
    void releaseUpdate();
    void forceUpdate();

    int getSerialIndex(const QString& channelName) const;
    int usedXPUIndex() const;

    QStringList getAttributes(XMLGroup xmlGroup) const;
    QVector<StateSingleItem*> getHeaderStateItems() const;
    QStringList getTCPDataOutputChannels() const;

    void clearProbeMapSettings();
    void printProbeMapSettings() const;

    void setupGlobalSettingsLoadSave(ControllerInterface* controllerInterface);
    bool loadGlobalSettings(const QString& filename, QString &errorMessage) const;
    bool saveGlobalSettings(const QString& filename) const;

    void updateForChangeHeadstages();

    void enableLogging(bool enable); // Toggled externally, probably from ControlWindow
    void writeToLog(QString message); // Callable from anywhere with access to SystemState

    void setReportSpikes(bool enable);
    bool getReportSpikes();

    void setDecayTime(double time);
    double getDecayTime();

    void spikeReport(QString names);
    void advanceSpikeTimer();

    // Intrinsic variables that shouldn't be changed solely through software (e.g. hardware-related, or set in software upon startup)
    SignalSources* signalSources;
    DiscreteItemList* controllerType;
    BooleanItem* synthetic;
    BooleanItem* playback;
    DiscreteItemList* sampleRate;
    DiscreteItemList* stimStepSize;
    int numSPIPorts;  // no direct relation to commands - should leave as is
    BooleanItem* expanderConnected;
    int highDPIScaleFactor;  // scale factor for high-DPI monitors (e.g., Retina displays)
    QRect availableScreenResolution;

    vector<ChipType> chipType;  // set when SPI ports are scanned for connected headstages

    bool running;  // streaming data from the board
    bool sweeping;  // rewinding or fast-forwarding (but not fast-forwarding in data file playback mode)

    CPUInfo cpuInfo;
    QVector<GPUInfo> gpuList;

    bool logErrors;
    QString logFileName;

    // Read-only variables
    StringItem *softwareVersion;
    BooleanItem *uploadInProgress;
    BooleanItem *headstagePresent;

    // Saving data
    DiscreteItemList *fileFormat;
    BooleanItem *saveAuxInWithAmpWaveforms;
    BooleanItem *saveWidebandAmplifierWaveforms;
    BooleanItem *saveLowpassAmplifierWaveforms;
    DiscreteItemList *lowpassWaveformDownsampleRate;
    BooleanItem *saveHighpassAmplifierWaveforms;
    BooleanItem *saveSpikeData;
    BooleanItem *saveSpikeSnapshots;
    IntRangeItem *spikeSnapshotPreDetect;
    IntRangeItem *spikeSnapshotPostDetect;
    BooleanItem *saveDCAmplifierWaveforms;
    IntRangeItem *newSaveFilePeriodMinutes;
    StateFilenameItem* filename;
    bool recording;
    bool triggerSet;
    bool triggered;
    DiscreteItemList* triggerSource;
    DiscreteItemList* triggerPolarity;
    DoubleRangeItem* triggerAnalogVoltageThreshold;
    IntRangeItem* preTriggerBuffer;
    IntRangeItem* postTriggerBuffer;
    BooleanItem* saveTriggerSource;
    StringItem *note1;
    StringItem *note2;
    StringItem *note3;

    // TCP
    IntRangeItem* tcpNumDataBlocksWrite;
    TCPCommunicator *tcpCommandCommunicator;
    TCPCommunicator *tcpWaveformDataCommunicator;
    TCPCommunicator *tcpSpikeDataCommunicator;

    // XML
    ProbeMapSettings probeMapSettings;

    // Software Audio
    BooleanItem *audioEnabled;
    DiscreteItemList *audioFilter;
    IntRangeItem *audioVolume;
    IntRangeItem *audioThreshold;

    // Hardware Audio/Analog Out
    IntRangeItem *analogOutGainIndex;
    IntRangeItem *analogOutNoiseSlicerIndex;
    BooleanItem *analogOut1LockToSelected;
    BooleanItem *analogOutHighpassFilterEnabled;
    DoubleRangeItem *analogOutHighpassFilterFrequency;
    StringItem *analogOut1Channel;
    StringItem *analogOut2Channel;
    StringItem *analogOut3Channel;
    StringItem *analogOut4Channel;
    StringItem *analogOut5Channel;
    StringItem *analogOut6Channel;
    StringItem *analogOut7Channel;
    StringItem *analogOut8Channel;
    StringItem *analogOutRefChannel;
    IntRangeItem *analogOut1Threshold;
    IntRangeItem *analogOut2Threshold;
    IntRangeItem *analogOut3Threshold;
    IntRangeItem *analogOut4Threshold;
    IntRangeItem *analogOut5Threshold;
    IntRangeItem *analogOut6Threshold;
    IntRangeItem *analogOut7Threshold;
    IntRangeItem *analogOut8Threshold;
    BooleanItem *analogOut1ThresholdEnabled;
    BooleanItem *analogOut2ThresholdEnabled;
    BooleanItem *analogOut3ThresholdEnabled;
    BooleanItem *analogOut4ThresholdEnabled;
    BooleanItem *analogOut5ThresholdEnabled;
    BooleanItem *analogOut6ThresholdEnabled;
    BooleanItem *analogOut7ThresholdEnabled;
    BooleanItem *analogOut8ThresholdEnabled;

    // Impedance testing
    BooleanItem *impedancesHaveBeenMeasured;
    BooleanItem *impedanceFreqValid;
    DoubleRangeItem *desiredImpedanceFreq;
    DoubleRangeItem *actualImpedanceFreq;
    StateFilenameItem *impedanceFilename;

    // Filtering
    BooleanItem *dspEnabled;
    DoubleRangeItem *desiredDspCutoffFreq;
    DoubleRangeItem *actualDspCutoffFreq;
    DoubleRangeItem *desiredLowerBandwidth;
    DoubleRangeItem *actualLowerBandwidth;
    DoubleRangeItem *desiredLower3dBCutoff;
    DoubleRangeItem *actualLower3dBCutoff;
    DoubleRangeItem *desiredUpperBandwidth;
    DoubleRangeItem *actualUpperBandwidth;

    DiscreteItemList *notchFreq;

    IntRangeItem *lowOrder;
    DiscreteItemList *lowType;
    DoubleRangeItem *lowSWCutoffFreq;
    IntRangeItem *highOrder;
    DiscreteItemList *highType;
    DoubleRangeItem *highSWCutoffFreq;

    // Display options
    DiscreteItemList* filterDisplay1;
    DiscreteItemList* filterDisplay2;
    DiscreteItemList* filterDisplay3;
    DiscreteItemList* filterDisplay4;
    DiscreteItemList* arrangeBy;
    BooleanItem* showDisabledChannels;
    BooleanItem* showAuxInputs;
    BooleanItem* showSupplyVoltages;
    StringItem* backgroundColor;
    StringItem* displaySettings;  // This is only set when a settings file is saved, and only accessed when a settings file is loaded.

    // Playback options
    BooleanItem* runAfterJumpToPosition;

    // Display trigger
    BooleanItem* triggerModeDisplay;
    DiscreteItemList* triggerSourceDisplay;
    DiscreteItemList* triggerPolarityDisplay;
    DiscreteItemList* triggerPositionDisplay;

    // Plotting waveforms
    BooleanItem* rollMode;
    BooleanItem* clipWaveforms;
    DiscreteItemList* tScale;
    DiscreteItemList* yScaleLow;
    DiscreteItemList* yScaleWide;
    DiscreteItemList* yScaleHigh;
    DiscreteItemList* yScaleAux;
    DiscreteItemList* yScaleAnalog;
    DiscreteItemList* yScaleDC;
    int numTabs;
    QList<QList<QString> > pinnedWaveforms;
    DiscreteItemList* displayLabelText;
    DiscreteItemList* labelWidth;

    // ISI
    ChannelNameItem* isiChannel;
    DiscreteItemList* tSpanISI;
    DiscreteItemList* binSizeISI;
    BooleanItem *yAxisLogISI;
    BooleanItem *saveCsvFileISI;
    BooleanItem *saveMatFileISI;
    BooleanItem *savePngFileISI;

    // PSTH
    ChannelNameItem* psthChannel;
    DiscreteItemList* tSpanPreTriggerPSTH;
    DiscreteItemList* tSpanPostTriggerPSTH;
    DiscreteItemList* binSizePSTH;
    DiscreteItemList* maxNumTrialsPSTH;
    DiscreteItemList* digitalTriggerPSTH;
    DiscreteItemList* triggerPolarityPSTH;
    BooleanItem *saveCsvFilePSTH;
    BooleanItem *saveMatFilePSTH;
    BooleanItem *savePngFilePSTH;

    // Spectrogram
    ChannelNameItem* spectrogramChannel;
    DiscreteItemList* displayModeSpectrogram;
    DiscreteItemList* tScaleSpectrogram;
    DiscreteItemList* fftSizeSpectrogram;
    IntRangeItem* fMinSpectrogram;
    IntRangeItem* fMaxSpectrogram;
    IntRangeItem* fMarkerSpectrogram;
    BooleanItem* showFMarkerSpectrogram;
    IntRangeItem* numHarmonicsSpectrogram;
    DiscreteItemList* digitalDisplaySpectrogram;
    BooleanItem *saveCsvFileSpectrogram;
    BooleanItem *saveMatFileSpectrogram;
    BooleanItem *savePngFileSpectrogram;

    // Spike sorting
    ChannelNameItem* spikeScopeChannel;
    DiscreteItemList* yScaleSpikeScope;
    DiscreteItemList* tScaleSpikeScope;
    DiscreteItemList* numSpikesDisplayed;
    BooleanItem* suppressionEnabled;
    BooleanItem* artifactsShown;
    IntRangeItem* suppressionThreshold;

    // Spike detection threshold setting options
    BooleanItem *absoluteThresholdsEnabled;
    IntRangeItem *absoluteThreshold;
    DoubleRangeItem *rmsMultipleThreshold;
    BooleanItem *negativeRelativeThreshold;

    // Configure tab
    BooleanItem* manualFastSettleEnabled;
    BooleanItem* externalFastSettleEnabled;
    IntRangeItem* externalFastSettleChannel;

    // Stimulation only
    DoubleRangeItem *desiredLowerSettleBandwidth;
    DoubleRangeItem *actualLowerSettleBandwidth;
    BooleanItem *useFastSettle;
    BooleanItem *headstageGlobalSettle;
    BooleanItem *chargeRecoveryMode;
    DiscreteItemList *chargeRecoveryCurrentLimit;
    DoubleRangeItem *chargeRecoveryTargetVoltage;
    bool stimParamsHaveChanged;

    SingleItemList globalItems;
    FilenameItemList stateFilenameItems;

signals:
    void stateChanged();
    void headstagesChanged();
    void spikeReportSignal(QString names);
    void spikeTimerTick();

protected:
    void timerEvent(QTimerEvent *event) override;

private:
    void setupLog(); // Checks QSettings to see if file should actually be created, and writes first message

    bool holdMode;
    bool updateNeeded;
    bool pendingStateChangedSignal;

    bool reportSpikes;
    double decayTime;

    int timerId;

    XMLInterface* globalSettingsInterface;

    void queueStateChangedSignal();

    QElapsedTimer logTimer;
};

#endif // SYSTEMSTATE_H

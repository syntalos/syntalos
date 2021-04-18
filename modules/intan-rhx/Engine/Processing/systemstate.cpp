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
#include "xmlinterface.h"
#include "signalsources.h"
#include "systemstate.h"

// Restrict functions for StateItem objects
bool RestrictAlways(const SystemState*) { return true; }
bool RestrictIfRunning(const SystemState* state) { return state->running; }
bool RestrictIfRecording(const SystemState* state) { return state->recording || state->triggerSet; }
bool RestrictIfNotStimController(const SystemState* state)
    { return (ControllerType) state->controllerType->getIndex() != ControllerStimRecordUSB2; }
bool RestrictIfNotStimControllerOrRunning(const SystemState* state)
    { return (ControllerType) state->controllerType->getIndex() != ControllerStimRecordUSB2 || state->running; }
bool RestrictIfStimController(const SystemState* state)
    { return (ControllerType) state->controllerType->getIndex() == ControllerStimRecordUSB2; }
bool RestrictIfStimControllerOrRunning(const SystemState* state)
    { return (ControllerType) state->controllerType->getIndex() == ControllerStimRecordUSB2 || state->running; }

SystemState::SystemState(const AbstractRHXController* controller_, StimStepSize stimStepSize_, int numSPIPorts_,
                         bool expanderConnected_) :
    numSPIPorts(numSPIPorts_),
    logErrors(false),
    reportSpikes(false),
    decayTime(1.0),
    globalSettingsInterface(nullptr)
{
    setupLog();

    writeToLog("Entered SystemState ctor");
    // Private variables
    holdMode = false;
    updateNeeded = false;
    pendingStateChangedSignal = false;

    highDPIScaleFactor = 1;

    controllerType = new DiscreteItemList("Type", globalItems, this, XMLGroupReadOnly);
    controllerType->addItem("ControllerRecordUSB2", "ControllerRecordUSB2", 0);
    controllerType->addItem("ControllerRecordUSB3", "ControllerRecordUSB3", 1);
    controllerType->addItem("ControllerStimRecordUSB2", "ControllerStimRecordUSB2", 2);
    controllerType->setIndex((int) controller_->getType());

    expanderConnected = new BooleanItem("ExpanderConnected", globalItems, this, expanderConnected_, XMLGroupNone);
    expanderConnected->setRestricted(RestrictAlways, ReadOnlyErrorMessage);

    // Intrinsic variables that shouldn't be changed solely through software (e.g. hardware-related, or set in software upon startup)
    signalSources = new SignalSources(this);
    synthetic = new BooleanItem("Synthetic", globalItems, this, controller_->isSynthetic(), XMLGroupNone);
    synthetic->setRestricted(RestrictAlways, ReadOnlyErrorMessage);
    playback = new BooleanItem("Playback", globalItems, this, controller_->isPlayback(), XMLGroupNone);
    playback->setRestricted(RestrictAlways, ReadOnlyErrorMessage);

    writeToLog("Created intrinsic variables");

    // Streaming data from the board
    running = false;

    int numDigitalInputs = AbstractRHXController::numDigitalIO(getControllerTypeEnum(), expanderConnected_);
    int numAnalogInputs = AbstractRHXController::numAnalogIO(getControllerTypeEnum(), expanderConnected_);

    // Read-only variables
    softwareVersion = new StringItem("Version", globalItems, this, SoftwareVersion, XMLGroupReadOnly);
    uploadInProgress = new BooleanItem("UploadInProgress", globalItems, this, false, XMLGroupNone);
    headstagePresent = new BooleanItem("HeadstagePresent", globalItems, this, false, XMLGroupNone);

    stimStepSize = new DiscreteItemList("StimStepSizeMicroAmps", globalItems, this, XMLGroupReadOnly, TypeDependencyStim);
    stimStepSize->addItem("Min", "Min", 0);
    stimStepSize->addItem("0.01", "10 nA", 1);
    stimStepSize->addItem("0.02", "20 nA", 2);
    stimStepSize->addItem("0.05", "50 nA", 3);
    stimStepSize->addItem("0.1", "100 nA", 4);
    stimStepSize->addItem("0.2", "200 nA", 5);
    stimStepSize->addItem("0.5", "500 nA", 6);
    stimStepSize->addItem("1", "1 " + MuSymbol + "A", 7);
    stimStepSize->addItem("2", "2 " + MuSymbol + "A", 8);
    stimStepSize->addItem("5", "5 " + MuSymbol + "A", 9);
    stimStepSize->addItem("10", "10 " + MuSymbol + "A", 10);
    stimStepSize->addItem("Max", "Max", 11);
    stimStepSize->setIndex((int) stimStepSize_);

    sampleRate = new DiscreteItemList("SampleRateHertz", globalItems, this, XMLGroupReadOnly);
    if (getControllerTypeEnum() != ControllerStimRecordUSB2) {
        sampleRate->addItem("1000", QString::fromStdString(AbstractRHXController::getSampleRateString(SampleRate1000Hz)), 1000.0);
        sampleRate->addItem("1250", QString::fromStdString(AbstractRHXController::getSampleRateString(SampleRate1250Hz)), 1250.0);
        sampleRate->addItem("1500", QString::fromStdString(AbstractRHXController::getSampleRateString(SampleRate1500Hz)), 1500.0);
        sampleRate->addItem("2000", QString::fromStdString(AbstractRHXController::getSampleRateString(SampleRate2000Hz)), 2000.0);
        sampleRate->addItem("2500", QString::fromStdString(AbstractRHXController::getSampleRateString(SampleRate2500Hz)), 2500.0);
        sampleRate->addItem("3000", QString::fromStdString(AbstractRHXController::getSampleRateString(SampleRate3000Hz)), 3000.0);
        sampleRate->addItem("3333", QString::fromStdString(AbstractRHXController::getSampleRateString(SampleRate3333Hz)), 10000.0 / 3.0);
        sampleRate->addItem("4000", QString::fromStdString(AbstractRHXController::getSampleRateString(SampleRate4000Hz)), 4000.0);
        sampleRate->addItem("5000", QString::fromStdString(AbstractRHXController::getSampleRateString(SampleRate5000Hz)), 5000.0);
        sampleRate->addItem("6250", QString::fromStdString(AbstractRHXController::getSampleRateString(SampleRate6250Hz)), 6250.0);
        sampleRate->addItem("8000", QString::fromStdString(AbstractRHXController::getSampleRateString(SampleRate8000Hz)), 8000.0);
        sampleRate->addItem("10000", QString::fromStdString(AbstractRHXController::getSampleRateString(SampleRate10000Hz)), 10000.0);
        sampleRate->addItem("12500", QString::fromStdString(AbstractRHXController::getSampleRateString(SampleRate12500Hz)), 12500.0);
        sampleRate->addItem("15000", QString::fromStdString(AbstractRHXController::getSampleRateString(SampleRate15000Hz)), 15000.0);
    }
    sampleRate->addItem("20000", QString::fromStdString(AbstractRHXController::getSampleRateString(SampleRate20000Hz)), 20000.0);
    sampleRate->addItem("25000", QString::fromStdString(AbstractRHXController::getSampleRateString(SampleRate25000Hz)), 25000.0);
    sampleRate->addItem("30000", QString::fromStdString(AbstractRHXController::getSampleRateString(SampleRate30000Hz)), 30000.0);

    sampleRate->setValue(QString::number(round(controller_->getSampleRate())));

    writeToLog("Created read-only variables");

    // Saving data
    fileFormat = new DiscreteItemList("FileFormat", globalItems, this);
    fileFormat->setRestricted(RestrictIfRunning, RunningErrorMessage);
    fileFormat->addItem("Traditional", "Traditional");
    fileFormat->addItem("OneFilePerSignalType", "OneFilePerSignalType");
    fileFormat->addItem("OneFilePerChannel", "OneFilePerChannel");
    fileFormat->setValue("Traditional");

    saveAuxInWithAmpWaveforms = new BooleanItem("SaveAuxInWithAmplifierWaveforms", globalItems, this, false);
    saveAuxInWithAmpWaveforms->setRestricted(RestrictIfRunning, RunningErrorMessage);

    saveWidebandAmplifierWaveforms = new BooleanItem("SaveWidebandAmplifierWaveforms", globalItems, this, true);
    saveWidebandAmplifierWaveforms->setRestricted(RestrictIfRunning, RunningErrorMessage);

    saveLowpassAmplifierWaveforms = new BooleanItem("SaveLowpassAmplifierWaveforms", globalItems, this, false);
    saveLowpassAmplifierWaveforms->setRestricted(RestrictIfRunning, RunningErrorMessage);

    lowpassWaveformDownsampleRate = new DiscreteItemList("LowpassWaveformDownsampleRate", globalItems, this);
    lowpassWaveformDownsampleRate->setRestricted(RestrictIfRunning, RunningErrorMessage);
    lowpassWaveformDownsampleRate->addItem("1", "none", 1.0);
    lowpassWaveformDownsampleRate->addItem("2", "2X", 2.0);
    lowpassWaveformDownsampleRate->addItem("4", "4X", 4.0);
    lowpassWaveformDownsampleRate->addItem("8", "8X", 8.0);
    lowpassWaveformDownsampleRate->addItem("16", "16X", 16.0);
    lowpassWaveformDownsampleRate->addItem("32", "32X", 32.0);
    lowpassWaveformDownsampleRate->addItem("64", "64X", 64.0);
    lowpassWaveformDownsampleRate->addItem("128", "128X", 128.0);
    lowpassWaveformDownsampleRate->setValue("1");

    saveHighpassAmplifierWaveforms = new BooleanItem("SaveHighpassAmplifierWaveforms", globalItems, this, false);
    saveHighpassAmplifierWaveforms->setRestricted(RestrictIfRunning, RunningErrorMessage);

    saveSpikeData = new BooleanItem("SaveSpikeData", globalItems, this, false);
    saveSpikeData->setRestricted(RestrictIfRunning, RunningErrorMessage);
    saveSpikeSnapshots = new BooleanItem("SaveSpikeSnapshots", globalItems, this, false);
    saveSpikeSnapshots->setRestricted(RestrictIfRunning, RunningErrorMessage);

    spikeSnapshotPreDetect = new IntRangeItem("SpikeSnapshotPreDetectMilliseconds", globalItems, this, -3, 0, -1);
    spikeSnapshotPostDetect = new IntRangeItem("SpikeSnapshotPostDetectMilliseconds", globalItems, this, 1, 6, 2);

    saveDCAmplifierWaveforms = new BooleanItem("SaveDCAmplifierWaveforms", globalItems, this, false, XMLGroupGeneral,
                                               TypeDependencyStim);
    saveDCAmplifierWaveforms->setRestricted(RestrictIfNotStimControllerOrRunning, NonStimRunningErrorMessage);

    newSaveFilePeriodMinutes = new IntRangeItem("NewSaveFilePeriodMinutes", globalItems, this, 1, 999, 1);
    newSaveFilePeriodMinutes->setRestricted(RestrictIfRunning, RunningErrorMessage);

    filename = new StateFilenameItem("Filename", &stateFilenameItems, this);
    filename->setRestricted(RestrictIfRunning, RunningErrorMessage);
    recording = false;
    triggerSet = false;
    triggered = false;
    sweeping = false;

    impedanceFilename = new StateFilenameItem("ImpedanceFilename", &stateFilenameItems, this);

    triggerSource = new DiscreteItemList("TriggerSource", globalItems, this);
    triggerSource->setRestricted(RestrictIfRunning, RunningErrorMessage);
    for (int i = 0; i < numDigitalInputs; ++i) {
        QString name = QString::fromStdString(AbstractRHXController::getDigitalInputChannelName(getControllerTypeEnum(), i));
        triggerSource->addItem(name, name, i);
    }
    for (int i = 0; i < numAnalogInputs; ++i) {
        QString name = QString::fromStdString(AbstractRHXController::getAnalogInputChannelName(getControllerTypeEnum(), i));
        triggerSource->addItem(name, name, i);
    }
    triggerSource->setIndex(0);

    triggerAnalogVoltageThreshold = new DoubleRangeItem("TriggerAnalogThresholdVolts", globalItems, this,
                                                        -10.24, 10.24, 1.65);
    triggerAnalogVoltageThreshold->setRestricted(RestrictIfRunning, RunningErrorMessage);

    triggerPolarity = new DiscreteItemList("TriggerPolarity", globalItems, this);
    triggerPolarity->setRestricted(RestrictIfRunning, RunningErrorMessage);
    triggerPolarity->addItem("High", "Trigger on Logic High");
    triggerPolarity->addItem("Low", "Trigger on Logic Low");
    triggerPolarity->setValue("High");

    preTriggerBuffer = new IntRangeItem("PreTriggerBufferSeconds", globalItems, this, 1, 30, 1);
    preTriggerBuffer->setRestricted(RestrictIfRunning, RunningErrorMessage);

    postTriggerBuffer = new IntRangeItem("PostTriggerBufferSeconds", globalItems, this, 1, 9999, 1);
    postTriggerBuffer->setRestricted(RestrictIfRunning, RunningErrorMessage);

    saveTriggerSource = new BooleanItem("TriggerSave", globalItems, this, true);
    saveTriggerSource->setRestricted(RestrictIfRunning, RunningErrorMessage);

    writeToLog("Created saving data variables");

    // TCP
    tcpNumDataBlocksWrite = new IntRangeItem("TCPNumberDataBlocksPerWrite", globalItems, this, 1, 100, 10, XMLGroupNone);
    tcpNumDataBlocksWrite->setRestricted(RestrictIfRunning, RunningErrorMessage);

    writeToLog("Created TCP variables");

    // Audio
    audioEnabled = new BooleanItem("AudioEnabled", globalItems, this, false);
    audioFilter = new DiscreteItemList("AudioFilter", globalItems, this);
    audioFilter->addItem("Wide", "WIDE", 0);
    audioFilter->addItem("Low", "LOW", 1);
    audioFilter->addItem("High", "HIGH", 2);
    audioFilter->setValue("Wide");
    audioVolume = new IntRangeItem("AudioVolume", globalItems, this, 0, 100, 50);
    audioThreshold = new IntRangeItem("AudioThresholdMicroVolts", globalItems, this, 0, 200, 0);

    writeToLog("Created audio variables");

    // Hardware Audio/Analog Out
    analogOutGainIndex = new IntRangeItem("AnalogOutGainIndex", globalItems, this, 0, 7, 0);
    analogOutNoiseSlicerIndex = new IntRangeItem("AnalogOutNoiseSlicerIndex", globalItems, this, 0, 64, 0);
    analogOut1LockToSelected = new BooleanItem("AnalogOut1LockToSelected", globalItems, this, false);
    analogOutHighpassFilterEnabled = new BooleanItem("AnalogOutHighpassFilterEnabled", globalItems, this, false);
    analogOutHighpassFilterFrequency = new DoubleRangeItem("AnalogOutHighpassFilterFreqHertz", globalItems, this, 0.01, 9999.99, 250.0);
    analogOut1Channel = new StringItem("AnalogOut1Channel", globalItems, this, "Off");
    analogOut2Channel = new StringItem("AnalogOut2Channel", globalItems, this, "Off");
    analogOut3Channel = new StringItem("AnalogOut3Channel", globalItems, this, "Off");
    analogOut4Channel = new StringItem("AnalogOut4Channel", globalItems, this, "Off");
    analogOut5Channel = new StringItem("AnalogOut5Channel", globalItems, this, "Off");
    analogOut6Channel = new StringItem("AnalogOut6Channel", globalItems, this, "Off");
    analogOut7Channel = new StringItem("AnalogOut7Channel", globalItems, this, "Off");
    analogOut8Channel = new StringItem("AnalogOut8Channel", globalItems, this, "Off");
    analogOutRefChannel = new StringItem("AnalogOutRefChannel", globalItems, this, "Hardware");
    analogOut1Threshold = new IntRangeItem("AnalogOut1ThresholdMicroVolts", globalItems, this, -6000, 6000, 0);
    analogOut2Threshold = new IntRangeItem("AnalogOut2ThresholdMicroVolts", globalItems, this, -6000, 6000, 0);
    analogOut3Threshold = new IntRangeItem("AnalogOut3ThresholdMicroVolts", globalItems, this, -6000, 6000, 0);
    analogOut4Threshold = new IntRangeItem("AnalogOut4ThresholdMicroVolts", globalItems, this, -6000, 6000, 0);
    analogOut5Threshold = new IntRangeItem("AnalogOut5ThresholdMicroVolts", globalItems, this, -6000, 6000, 0);
    analogOut6Threshold = new IntRangeItem("AnalogOut6ThresholdMicroVolts", globalItems, this, -6000, 6000, 0);
    analogOut7Threshold = new IntRangeItem("AnalogOut7ThresholdMicroVolts", globalItems, this, -6000, 6000, 0);
    analogOut8Threshold = new IntRangeItem("AnalogOut8ThresholdMicroVolts", globalItems, this, -6000, 6000, 0);
    bool defaultThresholdEnable = getControllerTypeEnum() != ControllerStimRecordUSB2;
    analogOut1ThresholdEnabled = new BooleanItem("AnalogOut1ThresholdEnabled", globalItems, this, defaultThresholdEnable);
    analogOut2ThresholdEnabled = new BooleanItem("AnalogOut2ThresholdEnabled", globalItems, this, defaultThresholdEnable);
    analogOut3ThresholdEnabled = new BooleanItem("AnalogOut3ThresholdEnabled", globalItems, this, defaultThresholdEnable);
    analogOut4ThresholdEnabled = new BooleanItem("AnalogOut4ThresholdEnabled", globalItems, this, defaultThresholdEnable);
    analogOut5ThresholdEnabled = new BooleanItem("AnalogOut5ThresholdEnabled", globalItems, this, defaultThresholdEnable);
    analogOut6ThresholdEnabled = new BooleanItem("AnalogOut6ThresholdEnabled", globalItems, this, defaultThresholdEnable);
    analogOut7ThresholdEnabled = new BooleanItem("AnalogOut7ThresholdEnabled", globalItems, this, defaultThresholdEnable);
    analogOut8ThresholdEnabled = new BooleanItem("AnalogOut8ThresholdEnabled", globalItems, this, defaultThresholdEnable);

    writeToLog("Created hardware audio/analog out variables");

    // TCP communication
    tcpCommandCommunicator = new TCPCommunicator();
    tcpWaveformDataCommunicator = new TCPCommunicator("127.0.0.1", 5001);
    tcpSpikeDataCommunicator = new TCPCommunicator("127.0.0.1", 5002);

    writeToLog("Created tcp communication variables");

    // Impedance testing
    impedancesHaveBeenMeasured = new BooleanItem("ImpedancesHaveBeenMeasured", globalItems, this, false, XMLGroupReadOnly);
    impedanceFreqValid = new BooleanItem("ImpedanceFreqValid", globalItems, this, false, XMLGroupNone);
    desiredImpedanceFreq = new DoubleRangeItem("DesiredImpedanceFreqHertz", globalItems, this, 0.0, 7500.0, 1000.0);
    actualImpedanceFreq = new DoubleRangeItem("ActualImpedanceFreqHertz", globalItems, this, 0.0, 7500.0, 1000.0, XMLGroupReadOnly);

    writeToLog("Created impedance testing variables");

    // Filtering

    // Note: 'actual' parameter sometimes has a wider range than 'desired' parameter because the closest-fitting achievable
    // (actual) parameter might be slightly outside of the desired range.

    desiredDspCutoffFreq = new DoubleRangeItem("DesiredDSPCutoffFreqHertz", globalItems, this, 0.0, 4000.0, 1.0);
    desiredDspCutoffFreq->setRestricted(RestrictIfRunning, RunningErrorMessage);

    actualDspCutoffFreq = new DoubleRangeItem("ActualDSPCutoffFreqHertz", globalItems, this, 0.0, 8000.0, 1.0, XMLGroupReadOnly);

    dspEnabled = new BooleanItem("DSPEnabled", globalItems, this, true);
    dspEnabled->setRestricted(RestrictIfRunning, RunningErrorMessage);

    desiredLowerBandwidth = new DoubleRangeItem("DesiredLowerBandwidthHertz", globalItems, this, 0.1, 500.0, 0.1);
    desiredLowerBandwidth->setRestricted(RestrictIfRunning, RunningErrorMessage);

    actualLowerBandwidth = new DoubleRangeItem("ActualLowerBandwidthHertz", globalItems, this, 0.05, 1000.0, 0.1, XMLGroupReadOnly);

    desiredUpperBandwidth = new DoubleRangeItem("DesiredUpperBandwidthHertz", globalItems, this, 100.0, 20000.0, 7500.0);
    desiredUpperBandwidth->setRestricted(RestrictIfRunning, RunningErrorMessage);

    actualUpperBandwidth = new DoubleRangeItem("ActualUpperBandwidthHertz", globalItems, this, 50.0, 25000.0, 7500.0, XMLGroupReadOnly);

    desiredLowerSettleBandwidth = new DoubleRangeItem("DesiredLowerSettleBandwidthHertz", globalItems, this, 0.1, 1000.0, 1000.0,
                                                      XMLGroupGeneral, TypeDependencyStim);
    desiredLowerSettleBandwidth->setRestricted(RestrictIfRunning, RunningErrorMessage);

    actualLowerSettleBandwidth = new DoubleRangeItem("ActualLowerSettleBandwidthHertz", globalItems, this, 0.05, 1500.0, 1000.0,
                                                     XMLGroupReadOnly, TypeDependencyStim);

    desiredLower3dBCutoff = new DoubleRangeItem("DesiredLower3dBCutoffFreqHertz", globalItems, this, 0.1, 500.0, 1.0);  // Do not make XMLGroupReadOnly
    actualLower3dBCutoff = new DoubleRangeItem("ActualLower3dBCutoffFreqHertz", globalItems, this, 0.05, 1000.0, 1.0, XMLGroupReadOnly);

    notchFreq = new DiscreteItemList("NotchFilterFreqHertz", globalItems, this);
    notchFreq->addItem("None", "None", -1.0);
    notchFreq->addItem("50", "50 Hz", 50.0);
    notchFreq->addItem("60", "60 Hz", 60.0);
    notchFreq->setRestricted(RestrictIfRecording, RecordingErrorMessage);

    lowOrder = new IntRangeItem("LowpassFilterOrder", globalItems, this, 1, 8, 2);
    lowOrder->setRestricted(RestrictIfRecording, RecordingErrorMessage);
    lowType = new DiscreteItemList("LowpassFilterType", globalItems, this);
    lowType->addItem("Bessel", "Bessel");
    lowType->addItem("Butterworth", "Butterworth");
    lowType->setValue("Bessel");
    lowType->setRestricted(RestrictIfRecording, RecordingErrorMessage);
    lowSWCutoffFreq = new DoubleRangeItem("LowpassFilterCutoffFreqHertz", globalItems, this, 1.0, 5000.0, 250.0);
    lowSWCutoffFreq->setRestricted(RestrictIfRecording, RecordingErrorMessage);
    highOrder = new IntRangeItem("HighpassFilterOrder", globalItems, this, 1, 8, 2);
    highOrder->setRestricted(RestrictIfRecording, RecordingErrorMessage);
    highType = new DiscreteItemList("HighpassFilterType", globalItems, this);
    highType->addItem("Bessel", "Bessel");
    highType->addItem("Butterworth", "Butterworth");
    highType->setValue("Bessel");
    highType->setRestricted(RestrictIfRecording, RecordingErrorMessage);
    highSWCutoffFreq = new DoubleRangeItem("HighpassFilterCutoffFreqHertz", globalItems, this, 1.0, 5000.0, 250.0);
    highSWCutoffFreq->setRestricted(RestrictIfRecording, RecordingErrorMessage);

    writeToLog("Created filtering variables");

    // Display options
    filterDisplay1 = new DiscreteItemList("FilterDisplay1", globalItems, this);
    filterDisplay1->addItem("None", "NONE", 0);
    filterDisplay1->addItem("Wide", "WIDE", 1);
    filterDisplay1->addItem("Low", "LOW", 2);
    filterDisplay1->addItem("High", "HIGH", 3);
    filterDisplay1->addItem("Spk", "SPK", 4);
    filterDisplay1->addItem("Dc", "DC", 5);
    filterDisplay1->setValue("Wide");

    filterDisplay2 = new DiscreteItemList("FilterDisplay2", globalItems, this);
    filterDisplay2->addItem("None", "NONE", 0);
    filterDisplay2->addItem("Wide", "WIDE", 1);
    filterDisplay2->addItem("Low", "LOW", 2);
    filterDisplay2->addItem("High", "HIGH", 3);
    filterDisplay2->addItem("Spk", "SPK", 4);
    filterDisplay2->addItem("Dc", "DC", 5);
    filterDisplay2->setValue("None");

    filterDisplay3 = new DiscreteItemList("FilterDisplay3", globalItems, this);
    filterDisplay3->addItem("None", "NONE", 0);
    filterDisplay3->addItem("Wide", "WIDE", 1);
    filterDisplay3->addItem("Low", "LOW", 2);
    filterDisplay3->addItem("High", "HIGH", 3);
    filterDisplay3->addItem("Spk", "SPK", 4);
    filterDisplay3->addItem("Dc", "DC", 5);
    filterDisplay3->setValue("None");

    filterDisplay4 = new DiscreteItemList("FilterDisplay4", globalItems, this);
    filterDisplay4->addItem("None", "NONE", 0);
    filterDisplay4->addItem("Wide", "WIDE", 1);
    filterDisplay4->addItem("Low", "LOW", 2);
    filterDisplay4->addItem("High", "HIGH", 3);
    filterDisplay4->addItem("Spk", "SPK", 4);
    filterDisplay4->addItem("Dc", "DC", 5);
    filterDisplay4->setValue("None");

    arrangeBy = new DiscreteItemList("ArrangeBy", globalItems, this);
    arrangeBy->addItem("Channel", "Arrange by Channel", 0);
    arrangeBy->addItem("Filter", "Arrange by Filter", 0);
    arrangeBy->setValue("Channel");

    showDisabledChannels = new BooleanItem("ShowDisabledChannels", globalItems, this, true);
    showAuxInputs = new BooleanItem("ShowAuxInputs", globalItems, this, true, XMLGroupGeneral, TypeDependencyNonStim);
    showSupplyVoltages = new BooleanItem("ShowSupplyVoltages", globalItems, this, false, XMLGroupGeneral, TypeDependencyNonStim);
    backgroundColor = new StringItem("BackgroundColor", globalItems, this, "#000000", XMLGroupGeneral);  // default to black
    displaySettings = new StringItem("DisplaySettings", globalItems, this, "", XMLGroupGeneral);

    note1 = new StringItem("Note1", globalItems, this, "");
    note1->setRestricted(RestrictIfRunning, RunningErrorMessage);
    note2 = new StringItem("Note2", globalItems, this, "");
    note2->setRestricted(RestrictIfRunning, RunningErrorMessage);
    note3 = new StringItem("Note3", globalItems, this, "");
    note3->setRestricted(RestrictIfRunning, RunningErrorMessage);

    writeToLog("Created display option variables");

    // Playback options
    runAfterJumpToPosition = new BooleanItem("RunAfterJumpToPosition", globalItems, this, false);

    writeToLog("Created playback option variables");

    // Display trigger
    triggerModeDisplay = new BooleanItem("DisplayTriggerMode", globalItems, this, false);
    triggerSourceDisplay = new DiscreteItemList("DisplayDigitalTrigger", globalItems, this);
    for (int i = 0; i < numDigitalInputs; ++i) {
        QString name = QString::fromStdString(AbstractRHXController::getDigitalInputChannelName(getControllerTypeEnum(), i));
        triggerSourceDisplay->addItem(name, name, i);
    }
    for (int i = 0; i < numAnalogInputs; ++i) {
        QString name = QString::fromStdString(AbstractRHXController::getAnalogInputChannelName(getControllerTypeEnum(), i));
        triggerSourceDisplay->addItem(name, name, i);
    }
    triggerSourceDisplay->setIndex(0);

    triggerPolarityDisplay = new DiscreteItemList("DisplayTriggerPolarity", globalItems, this);
    triggerPolarityDisplay->addItem("Rising", "Rising Edge", +1);
    triggerPolarityDisplay->addItem("Falling", "Falling Edge", -1);
    triggerPolarityDisplay->setValue("Rising");

    triggerPositionDisplay = new DiscreteItemList("DisplayTriggerPosition", globalItems, this);
    triggerPositionDisplay->addItem("1/10", "1/10", 0.10);
    triggerPositionDisplay->addItem("1/4", "1/4", 0.25);
    triggerPositionDisplay->addItem("1/2", "1/2", 0.50);
    triggerPositionDisplay->addItem("3/4", "3/4", 0.75);
    triggerPositionDisplay->addItem("9/10", "9/10", 0.90);
    triggerPositionDisplay->setValue("1/4");

    writeToLog("Created display trigger variables");

    // Plotting waveforms
    rollMode = new BooleanItem("RollMode", globalItems, this, false);
    clipWaveforms = new BooleanItem("ClipWaveforms", globalItems, this, false);
    tScale = new DiscreteItemList("TimeScaleMilliseconds", globalItems, this);
    tScale->addItem("10", "10 ms", 10.0);
    tScale->addItem("20", "20 ms", 20.0);
    tScale->addItem("40", "40 ms", 40.0);
    tScale->addItem("100", "100 ms", 100.0);
    tScale->addItem("200", "200 ms", 200.0);
    tScale->addItem("400", "400 ms", 400.0);
    tScale->addItem("1000", "1000 ms", 1000.0);
    tScale->addItem("2000", "2000 ms", 2000.0);
    tScale->addItem("4000", "4000 ms", 4000.0);
    tScale->setValue("2000");

    yScaleWide = new DiscreteItemList("WideScaleMicroVolts", globalItems, this);
    yScaleWide->addItem("50", "50 " + MicroVoltsSymbol, 50.0 );
    yScaleWide->addItem("100", "100 " + MicroVoltsSymbol, 100.0 );
    yScaleWide->addItem("200", "200 " + MicroVoltsSymbol, 200.0 );
    yScaleWide->addItem("500", "500 " + MicroVoltsSymbol, 500.0 );
    yScaleWide->addItem("1000", "1000 " + MicroVoltsSymbol, 1000.0 );
    yScaleWide->addItem("2000", "2000 " + MicroVoltsSymbol, 2000.0 );
    yScaleWide->addItem("5000", "5000 " + MicroVoltsSymbol, 5000.0 );
    yScaleWide->setValue("500");

    yScaleLow = new DiscreteItemList("LowScaleMicroVolts", globalItems, this);
    yScaleLow->addItem("50", "50 " + MicroVoltsSymbol, 50.0 );
    yScaleLow->addItem("100", "100 " + MicroVoltsSymbol, 100.0 );
    yScaleLow->addItem("200", "200 " + MicroVoltsSymbol, 200.0 );
    yScaleLow->addItem("500", "500 " + MicroVoltsSymbol, 500.0 );
    yScaleLow->addItem("1000", "1000 " + MicroVoltsSymbol, 1000.0 );
    yScaleLow->addItem("2000", "2000 " + MicroVoltsSymbol, 2000.0 );
    yScaleLow->addItem("5000", "5000 " + MicroVoltsSymbol, 5000.0 );
    yScaleLow->setValue("500");

    yScaleHigh = new DiscreteItemList("HighScaleMicroVolts", globalItems, this);
    yScaleHigh->addItem("50", "50 " + MicroVoltsSymbol, 50.0 );
    yScaleHigh->addItem("100", "100 " + MicroVoltsSymbol, 100.0 );
    yScaleHigh->addItem("200", "200 " + MicroVoltsSymbol, 200.0 );
    yScaleHigh->addItem("500", "500 " + MicroVoltsSymbol, 500.0 );
    yScaleHigh->addItem("1000", "1000 " + MicroVoltsSymbol, 1000.0 );
    yScaleHigh->addItem("2000", "2000 " + MicroVoltsSymbol, 2000.0 );
    yScaleHigh->addItem("5000", "5000 " + MicroVoltsSymbol, 5000.0 );
    yScaleHigh->setValue("500");

    yScaleAux = new DiscreteItemList("AuxScaleVolts", globalItems, this, XMLGroupGeneral, TypeDependencyNonStim);
    yScaleAux->addItem("0.1", "0.1 V", 0.1);
    yScaleAux->addItem("0.2", "0.2 V", 0.2);
    yScaleAux->addItem("0.5", "0.5 V", 0.5);
    yScaleAux->addItem("1.0", "1 V", 1.0);
    yScaleAux->addAlternateValueName("1.0", "1");
    yScaleAux->addItem("2.0", "2 V", 2.0);
    yScaleAux->addAlternateValueName("2.0", "2");
    yScaleAux->setValue("1.0");

    yScaleAnalog = new DiscreteItemList("AnalogScaleVolts", globalItems, this);
    yScaleAnalog->addItem("0.1", "0.1 V", 0.1);
    yScaleAnalog->addItem("0.2", "0.2 V", 0.2);
    yScaleAnalog->addItem("0.5", "0.5 V", 0.5);
    yScaleAnalog->addItem("1.0", "1 V", 1.0);
    yScaleAnalog->addAlternateValueName("1.0", "1");
    yScaleAnalog->addItem("2.0", "2 V", 2.0);
    yScaleAnalog->addAlternateValueName("2.0", "2");
    yScaleAnalog->addItem("5.0", "5 V", 5.0);
    yScaleAnalog->addAlternateValueName("5.0", "5");
    yScaleAnalog->addItem("10.0", "10 V", 10.0);
    yScaleAnalog->addAlternateValueName("10.0", "10");
    yScaleAnalog->setValue("1.0");

    yScaleDC = new DiscreteItemList("DCScaleVolts", globalItems, this, XMLGroupGeneral, TypeDependencyStim);
    yScaleDC->addItem("0.5", "0.5 V", 0.5);
    yScaleDC->addItem("1.0", "1 V", 1.0);
    yScaleDC->addAlternateValueName("1.0", "1");
    yScaleDC->addItem("2.0", "2 V", 2.0);
    yScaleDC->addAlternateValueName("2.0", "2");
    yScaleDC->addItem("5.0", "5 V", 5.0);
    yScaleDC->addAlternateValueName("5.0", "5");
    yScaleDC->addItem("10.0", "10 V", 10.0);
    yScaleDC->addAlternateValueName("10.0", "10");
    yScaleDC->setValue("1.0");

    displayLabelText = new DiscreteItemList("DisplayLabelText", globalItems, this);
    displayLabelText->addItem("CustomName", "Custom Name", 0);
    displayLabelText->addItem("NativeName", "Native Name", 1);
    displayLabelText->addItem("ImpedanceMagnitude", "Impedance Magnitude", 2);
    displayLabelText->addItem("ImpedancePhase", "Impedance Phase", 3);
    displayLabelText->addItem("Reference", "Reference", 4);
    displayLabelText->setValue("CustomName");

    labelWidth = new DiscreteItemList("LabelWidth", globalItems, this);
    labelWidth->addItem("Hide", "Hide Tags", 0);
    labelWidth->addItem("Narrow", "Narrow Tags", 1);
    labelWidth->addItem("Wide", "Wide Tags", 2);
    labelWidth->setValue("Wide");

    writeToLog("Created waveform plotting variables");

    // ISI
    isiChannel = new ChannelNameItem("ISIChannel", globalItems, this, "N/A");

    tSpanISI = new DiscreteItemList("ISITimeSpanMilliseconds", globalItems, this);
    tSpanISI->addItem("50", "50 ms", 50);
    tSpanISI->addItem("100", "100 ms", 100);
    tSpanISI->addItem("200", "200 ms", 200);
    tSpanISI->addItem("500", "500 ms", 500);
    tSpanISI->addItem("1000", "1000 ms", 1000);
    tSpanISI->setValue("200");

    binSizeISI = new DiscreteItemList("ISIBinSizeMilliseconds", globalItems, this);
    binSizeISI->addItem("1", "1 ms", 1);
    binSizeISI->addItem("2", "2 ms", 2);
    binSizeISI->addItem("5", "5 ms", 5);
    binSizeISI->addItem("10", "10 ms", 10);
    binSizeISI->addItem("20", "20 ms", 20);
    binSizeISI->setValue("5");

    yAxisLogISI = new BooleanItem("ISIYAxisLog", globalItems, this, false);

    saveCsvFileISI = new BooleanItem("ISISaveCsvFile", globalItems, this, true);
    saveMatFileISI = new BooleanItem("ISISaveMatFile", globalItems, this, true);
    savePngFileISI = new BooleanItem("ISISavePngFile", globalItems, this, true);

    writeToLog("Created ISI variables");

    // PSTH
    psthChannel = new ChannelNameItem("PSTHChannel", globalItems, this, "N/A");

    tSpanPreTriggerPSTH = new DiscreteItemList("PSTHPreTriggerSpanMilliseconds", globalItems, this);
    tSpanPreTriggerPSTH->addItem("50", "50 ms", 50);
    tSpanPreTriggerPSTH->addItem("100", "100 ms", 100);
    tSpanPreTriggerPSTH->addItem("200", "200 ms", 200);
    tSpanPreTriggerPSTH->addItem("500", "500 ms", 500);
    tSpanPreTriggerPSTH->addItem("1000", "1 s", 1000);
    tSpanPreTriggerPSTH->addItem("2000", "2 s", 2000);
    tSpanPreTriggerPSTH->setValue("100");

    tSpanPostTriggerPSTH = new DiscreteItemList("PSTHPostTriggerSpanMilliseconds", globalItems, this);
    tSpanPostTriggerPSTH->addItem("50", "50 ms", 50);
    tSpanPostTriggerPSTH->addItem("100", "100 ms", 100);
    tSpanPostTriggerPSTH->addItem("200", "200 ms", 200);
    tSpanPostTriggerPSTH->addItem("500", "500 ms", 500);
    tSpanPostTriggerPSTH->addItem("1000", "1 s", 1000);
    tSpanPostTriggerPSTH->addItem("2000", "2 s", 2000);
    tSpanPostTriggerPSTH->addItem("5000", "5 s", 5000);
    tSpanPostTriggerPSTH->addItem("10000", "10 s", 10000);
    tSpanPostTriggerPSTH->addItem("20000", "20 s", 20000);
    tSpanPostTriggerPSTH->setValue("500");

    binSizePSTH = new DiscreteItemList("PSTHBinSizeMilliseconds", globalItems, this);
    binSizePSTH->addItem("1", "1 ms", 1);
    binSizePSTH->addItem("2", "2 ms", 2);
    binSizePSTH->addItem("5", "5 ms", 5);
    binSizePSTH->addItem("10", "10 ms", 10);
    binSizePSTH->addItem("20", "20 ms", 20);
    binSizePSTH->addItem("50", "50 ms", 50);
    binSizePSTH->addItem("100", "100 ms", 100);
    binSizePSTH->setValue("5");

    maxNumTrialsPSTH = new DiscreteItemList("PSTHMaxNumTrials", globalItems, this);
    maxNumTrialsPSTH->addItem("10", "10", 10);
    maxNumTrialsPSTH->addItem("20", "20", 20);
    maxNumTrialsPSTH->addItem("50", "50", 50);
    maxNumTrialsPSTH->addItem("100", "100", 100);
    maxNumTrialsPSTH->addItem("200", "200", 200);
    maxNumTrialsPSTH->addItem("500", "500", 500);
    maxNumTrialsPSTH->setValue("50");

    digitalTriggerPSTH = new DiscreteItemList("PSTHDigitalTrigger", globalItems, this);
    for (int i = 0; i < numDigitalInputs; ++i) {
        QString name = QString::fromStdString(AbstractRHXController::getDigitalInputChannelName(getControllerTypeEnum(), i));
        digitalTriggerPSTH->addItem(name, name, i);
    }
    for (int i = 0; i < numAnalogInputs; ++i) {
        QString name = QString::fromStdString(AbstractRHXController::getAnalogInputChannelName(getControllerTypeEnum(), i));
        digitalTriggerPSTH->addItem(name, name, i);
    }
    digitalTriggerPSTH->setIndex(0);

    triggerPolarityPSTH = new DiscreteItemList("PSTHTriggerPolarity", globalItems, this);
    triggerPolarityPSTH->addItem("Rising", "Rising Edge", +1);
    triggerPolarityPSTH->addItem("Falling", "Falling Edge", -1);
    triggerPolarityPSTH->setValue("Rising");

    saveCsvFilePSTH = new BooleanItem("PSTHSaveCsvFile", globalItems, this, true);
    saveMatFilePSTH = new BooleanItem("PSTHSaveMatFile", globalItems, this, true);
    savePngFilePSTH = new BooleanItem("PSTHSavePngFile", globalItems, this, true);

    writeToLog("Created PSTH variables");

    // Spectrogram
    spectrogramChannel = new ChannelNameItem("SpectrogramChannel", globalItems, this, "N/A");

    displayModeSpectrogram = new DiscreteItemList("SpectrogramDisplayMode", globalItems, this);
    displayModeSpectrogram->addItem("Spectrogram", "Spectrogram", 0);
    displayModeSpectrogram->addItem("Spectrum", "Spectrum", 1);
    displayModeSpectrogram->setValue("Spectrogram");

    tScaleSpectrogram = new DiscreteItemList("SpectrogramTimeScaleSeconds", globalItems, this);
    tScaleSpectrogram->addItem("2", "2 s", 2.0);
    tScaleSpectrogram->addItem("5", "5 s", 5.0);
    tScaleSpectrogram->addItem("10", "10 s", 10.0);
    tScaleSpectrogram->setValue("5");

    fftSizeSpectrogram = new DiscreteItemList("SpectrogramFFTSize", globalItems, this);
    fftSizeSpectrogram->addItem("256", "256", 256);
    fftSizeSpectrogram->addItem("512", "512", 512);
    fftSizeSpectrogram->addItem("1024", "1024", 1024);
    fftSizeSpectrogram->addItem("2048", "2048", 2048);
    fftSizeSpectrogram->addItem("4096", "4096", 4096);
    fftSizeSpectrogram->addItem("8192", "8192", 8192);
    fftSizeSpectrogram->addItem("16384", "16384", 16384);
    fftSizeSpectrogram->setValue("2048");

    int fNyquist = qFloor(sampleRate->getNumericValue() / 2.0);
    fMinSpectrogram = new IntRangeItem("SpectrogramFreqMinHertz", globalItems, this, 0, fNyquist - 10, 0);
    fMaxSpectrogram = new IntRangeItem("SpectrogramFreqMaxHertz", globalItems, this, 10, fNyquist, qMin(fNyquist, 200));
    fMarkerSpectrogram = new IntRangeItem("SpectrogramFreqMarkerHertz", globalItems, this, 0, fNyquist, 60);
    showFMarkerSpectrogram = new BooleanItem("SpectrogramShowFreqMarker", globalItems, this, true);
    numHarmonicsSpectrogram = new IntRangeItem("SpectrogramFreqMarkerNumHarmonics", globalItems, this, 0, 9, 0);

    digitalDisplaySpectrogram = new DiscreteItemList("SpectrogramDigitalDisplay", globalItems, this);
    digitalDisplaySpectrogram->addItem("None", "None", -1);
    for (int i = 0; i < numDigitalInputs; ++i) {
        QString name = QString::fromStdString(AbstractRHXController::getDigitalInputChannelName(getControllerTypeEnum(), i));
        digitalDisplaySpectrogram->addItem(name, name, i);
    }
    for (int i = 0; i < numAnalogInputs; ++i) {
        QString name = QString::fromStdString(AbstractRHXController::getAnalogInputChannelName(getControllerTypeEnum(), i));
        digitalDisplaySpectrogram->addItem(name, name, i);
    }
    digitalDisplaySpectrogram->setValue("None");

    saveCsvFileSpectrogram = new BooleanItem("SpectrogramSaveCsvFile", globalItems, this, true);
    saveMatFileSpectrogram = new BooleanItem("SpectrogramSaveMatFile", globalItems, this, true);
    savePngFileSpectrogram = new BooleanItem("SpectrogramSavePngFile", globalItems, this, true);

    writeToLog("Created spectrogram variables");

    // Spike scope
    spikeScopeChannel = new ChannelNameItem("SpikeScopeChannel", globalItems, this, "N/A");
    yScaleSpikeScope = new DiscreteItemList("SpikeScopeScaleMicroVolts", globalItems, this);
    yScaleSpikeScope->addItem("50", "50 " + MicroVoltsSymbol, 50.0);
    yScaleSpikeScope->addItem("100", "100 " + MicroVoltsSymbol, 100.0);
    yScaleSpikeScope->addItem("200", "200 " + MicroVoltsSymbol, 200.0);
    yScaleSpikeScope->addItem("500", "500 " + MicroVoltsSymbol, 500.0);
    yScaleSpikeScope->addItem("1000", "1000 " + MicroVoltsSymbol, 1000.0);
    yScaleSpikeScope->addItem("2000", "2000 " + MicroVoltsSymbol, 2000.0);
    yScaleSpikeScope->addItem("5000", "5000 " + MicroVoltsSymbol, 5000.0);
    yScaleSpikeScope->setValue("500");
    tScaleSpikeScope = new DiscreteItemList("SpikeScopeTimeScaleMilliseconds", globalItems, this);
    tScaleSpikeScope->addItem("2", "2 ms", 2.0);
    tScaleSpikeScope->addItem("4", "4 ms", 4.0);
    tScaleSpikeScope->addItem("6", "6 ms", 6.0);
    tScaleSpikeScope->setValue("2");
    numSpikesDisplayed = new DiscreteItemList("SpikeScopeNumSpikes", globalItems, this);
    numSpikesDisplayed->addItem("10", "10", 10);
    numSpikesDisplayed->addItem("20", "20", 20);
    numSpikesDisplayed->addItem("30", "30", 30);
    numSpikesDisplayed->addItem("50", "50", 50);
    numSpikesDisplayed->addItem("100", "100", 100);
    numSpikesDisplayed->addItem("200", "200", 200);
    numSpikesDisplayed->addItem("500", "500", 500);
    numSpikesDisplayed->setValue("20");
    suppressionEnabled = new BooleanItem("ArtifactSuppressionEnabled", globalItems, this, true);
    artifactsShown = new BooleanItem("ArtifactsShown", globalItems, this, true);
    suppressionThreshold = new IntRangeItem("ArtifactSuppressionThresholdMicroVolts", globalItems, this, 0, 5000, 2500);

    writeToLog("Created spike scope variables");

    // Spike detection threshold setting options
    absoluteThresholdsEnabled = new BooleanItem("AbsoluteThresholdsEnabled", globalItems, this, true);
    absoluteThreshold = new IntRangeItem("AbsoluteThresholdMicroVolts", globalItems, this, -5000, 5000, -70);
    rmsMultipleThreshold = new DoubleRangeItem("RmsMultipleThreshold", globalItems, this, 3.0, 20.0, 4.0);
    negativeRelativeThreshold = new BooleanItem("NegativeRelativeThreshold", globalItems, this, true);

    writeToLog("Created spike detection threshold setting option variables");

    // Configure tab
    manualFastSettleEnabled = new BooleanItem("ManualFastSettleEnabled", globalItems, this, false,
                                              XMLGroupGeneral, TypeDependencyNonStim);
    externalFastSettleEnabled = new BooleanItem("ExternalFastSettleEnabled", globalItems, this, false,
                                                XMLGroupGeneral, TypeDependencyNonStim);
    externalFastSettleChannel = new IntRangeItem("ExternalFastSettleChannel", globalItems, this, 1, 16, 1,
                                                 XMLGroupGeneral, TypeDependencyNonStim);

    writeToLog("Created configure tab variables");

    // Stimulation only
    useFastSettle = new BooleanItem("UseFastSettle", globalItems, this, false, XMLGroupGeneral, TypeDependencyStim);
    useFastSettle->setRestricted(RestrictIfRunning, RunningErrorMessage);

    headstageGlobalSettle = new BooleanItem("HeadstageGlobalSettle", globalItems, this, false, XMLGroupGeneral, TypeDependencyStim);
    headstageGlobalSettle->setRestricted(RestrictIfRunning, RunningErrorMessage);

    chargeRecoveryMode = new BooleanItem("ChargeRecovery", globalItems, this, false, XMLGroupGeneral, TypeDependencyStim);
    chargeRecoveryMode->setRestricted(RestrictIfRunning, RunningErrorMessage);

    chargeRecoveryCurrentLimit = new DiscreteItemList("ChargeRecoveryCurrentLimitMicroAmps", globalItems, this, XMLGroupGeneral, TypeDependencyStim);
    chargeRecoveryCurrentLimit->addItem("Min", "Min", 0);
    chargeRecoveryCurrentLimit->addItem("0.001", "1 nA", 1);
    chargeRecoveryCurrentLimit->addItem("0.002", "2 nA", 2);
    chargeRecoveryCurrentLimit->addItem("0.005", "5 nA", 3);
    chargeRecoveryCurrentLimit->addItem("0.01", "10 nA", 4);
    chargeRecoveryCurrentLimit->addItem("0.02", "20 nA", 5);
    chargeRecoveryCurrentLimit->addItem("0.05", "50 nA", 6);
    chargeRecoveryCurrentLimit->addItem("0.1", "100 nA", 7);
    chargeRecoveryCurrentLimit->addItem("0.2", "200 nA", 8);
    chargeRecoveryCurrentLimit->addItem("0.5", "500 nA", 9);
    chargeRecoveryCurrentLimit->addItem("1", "1 " + MuSymbol + "A", 10);
    chargeRecoveryCurrentLimit->setValue("0.01");
    chargeRecoveryCurrentLimit->setRestricted(RestrictIfRunning, RunningErrorMessage);

    chargeRecoveryTargetVoltage = new DoubleRangeItem("ChargeRecoveryTargetVoltageVolts", globalItems, this, -1.225, 1.215, 0.0,
                                                      XMLGroupGeneral, TypeDependencyStim);
    chargeRecoveryTargetVoltage->setRestricted(RestrictIfRunning, RunningErrorMessage);

    stimParamsHaveChanged = false;

    writeToLog("Created stim only variables");

    // Start timer
    timerId = startTimer(20);  // Minimum time between two stateChanged() signals, in milliseconds.
    writeToLog("Started timer. End of SystemState ctor");
}

SystemState::~SystemState()
{
    delete signalSources;
    delete globalSettingsInterface;

    for (SingleItemList::const_iterator p = globalItems.begin(); p != globalItems.end(); ++p) {
        delete p->second;
    }
    for (FilenameItemList::const_iterator p = stateFilenameItems.begin(); p != stateFilenameItems.end(); ++p) {
        delete p->second;
    }
}

AmplifierSampleRate SystemState::getSampleRateEnum() const
{
    if (getControllerTypeEnum() != ControllerStimRecordUSB2) {
        return (AmplifierSampleRate) sampleRate->getIndex();
    } else {
        if (sampleRate->getValue() == "20000") {
            return SampleRate20000Hz;
        } else if (sampleRate->getValue() == "25000") {
            return SampleRate25000Hz;
        } else if (sampleRate->getValue() == "30000") {
            return SampleRate30000Hz;
        } else {
            qDebug() << "Error... getSampleRateEnum() failed for some reason";
            return SampleRate20000Hz;
        }
    }
}

FileFormat SystemState::getFileFormatEnum() const
{
    return (FileFormat) fileFormat->getIndex();
}

ControllerType SystemState::getControllerTypeEnum() const
{
    return (ControllerType) controllerType->getIndex();
}

StimStepSize SystemState::getStimStepSizeEnum() const
{
    if (getControllerTypeEnum() != ControllerStimRecordUSB2) {
        return StimStepSizeMin;
    } else {
        return (StimStepSize) stimStepSize->getIndex();
    }
}

RHXRegisters::ChargeRecoveryCurrentLimit SystemState::getChargeRecoveryCurrentLimitEnum() const
{
    if (getControllerTypeEnum() != ControllerStimRecordUSB2) {
        return RHXRegisters::CurrentLimitMin;
    } else {
        return (RHXRegisters::ChargeRecoveryCurrentLimit) chargeRecoveryCurrentLimit->getIndex();
    }
}

void SystemState::addStateSingleItem(SingleItemList &hList, StateSingleItem* item) const
{
    hList[item->getParameterName().toLower().toStdString()] = item;
}

StateSingleItem* SystemState::locateStateSingleItem(SingleItemList &hList, const QString& parameterName) const
{
    SingleItemList::const_iterator p = hList.find(parameterName.toLower().toStdString());
    if (p == hList.end()) return nullptr;
    else return p->second;
}

void SystemState::addStateFilenameItem(FilenameItemList &hList, StateFilenameItem* item) const
{
    hList[item->getParameterName().toLower().toStdString()] = item;
}

StateFilenameItem* SystemState::locateStateFilenameItem(FilenameItemList &hList, const QString& fullParameterName,
                                                        QString &pathOrBase_) const
{
    // Divide fullParameterName into parameterName and pathOrBase.
    QString parameterName = fullParameterName.section('.', 0, 0);
    QString pathOrBase = fullParameterName.section('.', 1, 1);

    // Find the item, returning nullptr if not found.
    FilenameItemList::const_iterator p = hList.find(parameterName.toStdString());
    if (p == hList.end()) return nullptr;

    // Return pathOrBase by reference (returning nullptr if pathOrBase isn't path or baseFilename)
    if (pathOrBase.toLower() != p->second->getPathParameterName().toLower() && pathOrBase.toLower() != p->second->getBaseFilenameParameterName().toLower()) {
        return nullptr;
    } else {
        pathOrBase_ = pathOrBase;
        return p->second;
    }
}

int SystemState::getSerialIndex(const QString& channelName) const
{
    if (channelName == "N/A" || channelName.isEmpty()) {
        return -1;
    }
    int channelsPerStream = RHXDataBlock::channelsPerStream((ControllerType) controllerType->getIndex());
    Channel* thisChannel = signalSources->channelByName(channelName);
    if (thisChannel)
        return thisChannel->getBoardStream() * channelsPerStream + thisChannel->getChipChannel();
    else
        return -1;
}

void SystemState::holdUpdate()
{
    holdMode = true;
    updateNeeded = false;
}

void SystemState::releaseUpdate()
{
    if (updateNeeded) {
        queueStateChangedSignal();
        updateNeeded = false;
    }
    holdMode = false;
}

int SystemState::usedXPUIndex() const
{
    if (cpuInfo.used)
        return 0;
    for (int gpu = 0; gpu < gpuList.size(); gpu++) {
        if (gpuList[gpu].used)
            return gpu + 1;
    }
    return -1;
}

QStringList SystemState::getAttributes(XMLGroup xmlGroup) const
{
    // Create a QStringList from globalItems and stateFilenameItems in the given XMLGroup
    QStringList attributeList;

    QString thisAttribute;
    // Add globalItems to attributeList
    for (SingleItemList::const_iterator p = globalItems.begin(); p != globalItems.end(); ++p) {
        if (p->second->getXMLGroup() == xmlGroup) {
            bool addAttribute = false;
            switch (p->second->getTypeDependency()) {
            case TypeDependencyNone:
                addAttribute = true;
                break;
            case TypeDependencyNonStim:
                if (getControllerTypeEnum() != ControllerStimRecordUSB2) {
                    addAttribute = true;
                }
                break;
            case TypeDependencyStim:
                if (getControllerTypeEnum() == ControllerStimRecordUSB2) {
                    addAttribute = true;
                }
                break;
            }

            if (addAttribute) {
                thisAttribute = p->second->getParameterName() + ":_:" + p->second->getValueString();
                attributeList.append(thisAttribute);
            }
        }
    }

    // Add stateFilenameItems to attributeList
    QString thisPath;
    QString thisBaseFilename;
    for (FilenameItemList::const_iterator p = stateFilenameItems.begin(); p != stateFilenameItems.end(); ++p) {
        if (p->second->getXMLGroup() == xmlGroup) {
            bool addAttribute = false;
            switch (p->second->getTypeDependency()) {
            case TypeDependencyNone:
                addAttribute = true;
                break;
            case TypeDependencyNonStim:
                if (getControllerTypeEnum() != ControllerStimRecordUSB2) {
                    addAttribute = true;
                }
                break;
            case TypeDependencyStim:
                if (getControllerTypeEnum() == ControllerStimRecordUSB2) {
                    addAttribute = true;
                }
                break;
            }

            if (addAttribute) {
                thisPath = p->second->getParameterName() + "." + p->second->getPathParameterName() + ":_:" + p->second->getPath();
                attributeList.append(thisPath);
                thisBaseFilename = p->second->getParameterName() + "." + p->second->getBaseFilenameParameterName() + ":_:" + p->second->getBaseFilename();
                attributeList.append(thisBaseFilename);
            }
        }
    }

    return attributeList;
}

// Get the Read-Only StateItems that are hard-wired to be attributes of the IntanRHX element: SampleRate, Type, StimStepSize (if Stim), Version
QVector<StateSingleItem*> SystemState::getHeaderStateItems() const
{
    QVector<StateSingleItem*> returnVector;

    returnVector.append(sampleRate);
    returnVector.append(controllerType);
    if (getControllerTypeEnum() == ControllerStimRecordUSB2) returnVector.append(stimStepSize);
    returnVector.append(softwareVersion);

    return returnVector;
}

QStringList SystemState::getTCPDataOutputChannels() const
{
    QStringList channelList;
    // Go through all signal groups.
    for (int group = 0; group < signalSources->numGroups(); group++) {
        SignalGroup* thisGroup = signalSources->groupByIndex(group);
        // Go through all channels in this group.
        for (int channel = 0; channel < thisGroup->numChannels(); channel++) {
            Channel* thisChannel = thisGroup->channelByIndex(channel);
            // If any band of this channel is output to tcp, then add it's native channel name to the string.
            if (thisChannel->getOutputToTcp()) {
                channelList.append(thisChannel->getNativeName());
            } else if (thisChannel->getSignalType() == AmplifierSignal) {
                if (thisChannel->getOutputToTcpLow() || thisChannel->getOutputToTcpHigh() || thisChannel->getOutputToTcpSpike()) {
                    channelList.append(thisChannel->getNativeName());
                } else if (getControllerTypeEnum() == ControllerStimRecordUSB2) {
                    if (thisChannel->getOutputToTcpDc() || thisChannel->getOutputToTcpStim()) {
                        channelList.append(thisChannel->getNativeName());
                    }
                }
            }
        }
    }
    return channelList;
}

void SystemState::clearProbeMapSettings()
{
    probeMapSettings.backgroundColor = "";
    probeMapSettings.siteOutlineColor = "";
    probeMapSettings.lineColor = "";
    probeMapSettings.fontHeight = 0.0;
    probeMapSettings.fontColor = "";
    probeMapSettings.textAlignment = "";
    probeMapSettings.siteShape = "";
    probeMapSettings.siteWidth = 0.0;
    probeMapSettings.siteHeight = 0.0;
    probeMapSettings.pages.clear();
}

void SystemState::printProbeMapSettings() const
{
    qDebug() << "GLOBAL LEVEL...";
    qDebug() << "background color: " << probeMapSettings.backgroundColor;
    qDebug() << "site outline color: " << probeMapSettings.siteOutlineColor;
    qDebug() << "line color: " << probeMapSettings.lineColor;
    qDebug() << "font height: " << probeMapSettings.fontHeight;
    qDebug() << "font color: " << probeMapSettings.fontColor;
    qDebug() << "text alignment: " << probeMapSettings.textAlignment;
    qDebug() << "site shape: " << probeMapSettings.siteShape;
    qDebug() << "site width: " << probeMapSettings.siteWidth;
    qDebug() << "site height: " << probeMapSettings.siteHeight;
    qDebug() << "number of pages: " << probeMapSettings.pages.size();
    for (int i = 0; i < probeMapSettings.pages.size(); ++i) {
        Page thisPage = probeMapSettings.pages[i];
        qDebug() << "PAGE LEVEL... #: " << i;
        qDebug() << "name: " << thisPage.name;
        qDebug() << "background color: " << thisPage.backgroundColor;
        qDebug() << "site outline color: " << thisPage.siteOutlineColor;
        qDebug() << "line color: " << thisPage.lineColor;
        qDebug() << "font height: " << thisPage.fontHeight;
        qDebug() << "font color: " << thisPage.fontColor;
        qDebug() << "text alignment: " << thisPage.textAlignment;
        qDebug() << "site shape: " << thisPage.siteShape;
        qDebug() << "site width: " << thisPage.siteWidth;
        qDebug() << "site height: " << thisPage.siteHeight;
        qDebug() << "number of texts: " << thisPage.texts.size();
        for (int j = 0; j < thisPage.texts.size(); ++j) {
            Text thisText = thisPage.texts[j];
            qDebug() << "TEXT LEVEL... #: " << j;
            qDebug() << "x: " << thisText.x;
            qDebug() << "y: " << thisText.y;
            qDebug() << "text: " << thisText.text;
            qDebug() << "fontHeight: " << thisText.fontHeight;
            qDebug() << "fontColor: " << thisText.fontColor;
            qDebug() << "textAlignment: " << thisText.textAlignment;
            qDebug() << "rotation: " << thisText.rotation;
        }
    }
}

// Public (but should only be used by BooleanItem, DiscreteItemList, etc.): All state changes should go through this function.
void SystemState::forceUpdate()
{
    if (holdMode) {
        updateNeeded = true;
    } else {
        queueStateChangedSignal();
    }
}

void SystemState::queueStateChangedSignal()
{
    pendingStateChangedSignal = true;
}

void SystemState::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == timerId) {  // Restrict the frequency at which we send stateChanged() signals.
        if (pendingStateChangedSignal) {
            pendingStateChangedSignal = false;
            emit stateChanged();
        }
    } else {
        QObject::timerEvent(event);
    }
}

void SystemState::setupGlobalSettingsLoadSave(ControllerInterface* controllerInterface)
{
    // XML interface for saving global settings.
    globalSettingsInterface = new XMLInterface(this, controllerInterface, XMLIncludeGlobalParameters);
}

bool SystemState::loadGlobalSettings(const QString& filename, QString &errorMessage) const
{
    if (!globalSettingsInterface) {
        cerr << "SystemState::loadGlobalSettings: Must run setupGlobalSettingsLoadSave first." << '\n';
        return false;
    }
    return globalSettingsInterface->loadFile(filename, errorMessage);
}

bool SystemState::saveGlobalSettings(const QString& filename) const
{
    if (!globalSettingsInterface) {
        cerr << "SystemState::loadGlobalSettings: Must run setupGlobalSettingsLoadSave first." << '\n';
        return false;
    }
    return globalSettingsInterface->saveFile(filename);
}

void SystemState::updateForChangeHeadstages()
{
    // If a single channel is selected, then initialize the isi/psth/spectrogram/spikesorting to that channel.
    QString channelNativeName = signalSources->singleSelectedAmplifierChannelName();
    if (channelNativeName.isEmpty()) {
        // If no single channel is selected, initialize to first channel in signalSources.
        channelNativeName = signalSources->firstChannelName();
    }
    isiChannel->setValue(channelNativeName);
    psthChannel->setValue(channelNativeName);
    spectrogramChannel->setValue(channelNativeName);
    spikeScopeChannel->setValue(channelNativeName);

    emit headstagesChanged();
}

// Should be called when logging is enabled or disabled through ControlWindow
void SystemState::enableLogging(bool enable)
{
    // Set QSettings bool
    QSettings settings;
    settings.setValue("generateLogFile", enable);

    setupLog();
}

// Called from State ctor and enableLogging(). Checks QSettings to see if file should actually be created, and writes first message
void SystemState::setupLog()
{
    // Check QSettings to see if logging should happen
    QSettings settings;
    logErrors = settings.value("generateLogFile", false).toBool();

    if (!logErrors) return;

    // If logging should occur, initialize file and write first message
    logFileName = settings.value("logFileName", "IntanRHXErrorLog.txt").toString();
    QFile *file = new QFile(logFileName);
    if (!file->open(QIODevice::WriteOnly)) {
        QMessageBox::warning(nullptr, "Problem Saving IntanRHX Error Log", "Cannot open text file for saving errors to disk. Is the file being used, or located in an administrator-only directory?");
        logErrors = false;
        if (file) delete file;
        return;
    }
    file->write("Log successfully opened\n");
    file->close();
    delete file;
    return;
}

void SystemState::writeToLog(QString message)
{
    if (!logErrors) return;

    QFile *file = new QFile(logFileName);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Append)) {
        QMessageBox::warning(nullptr, "Saving IntanRHX Error Message Error", "Cannot open text file for saving errors to disk (writeonly, append, SystemState)");
        if (file) delete file;
        return;
    }
    file->write(message.toLocal8Bit());
    file->write(" ... seconds: ");
    file->write(QString::number(((double) logTimer.elapsed()) / 1000.0).toLocal8Bit());
    file->write("\n");
    file->close();
    delete file;
    return;
}

void SystemState::setReportSpikes(bool enable)
{
    reportSpikes = enable;
}

bool SystemState::getReportSpikes()
{
    return reportSpikes;
}

void SystemState::setDecayTime(double time)
{
    decayTime = time;
}

double SystemState::getDecayTime()
{
    return decayTime;
}

void SystemState::spikeReport(QString names)
{
    emit spikeReportSignal(names);
}

void SystemState::advanceSpikeTimer()
{
    emit spikeTimerTick();
}

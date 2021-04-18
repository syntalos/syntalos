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

#ifndef ABSTRACTRHXCONTROLLER_H
#define ABSTRACTRHXCONTROLLER_H

#include "rhxglobals.h"
#include "rhxdatablock.h"
#include <string>
#include <vector>
#include <deque>
#include <mutex>

using namespace std;

enum AcquisitionMode {
    LiveMode,
    SyntheticMode,
    PlaybackMode
};

struct StreamChannelPair
{
    int stream;
    int channel;
};

bool operator ==(const StreamChannelPair& a, const StreamChannelPair& b);
bool operator !=(const StreamChannelPair& a, const StreamChannelPair& b);
bool operator <(const StreamChannelPair& a, const StreamChannelPair& b);
bool operator >(const StreamChannelPair& a, const StreamChannelPair& b);


class AbstractRHXController
{
public:
    // Stimulation sequencer register addresses
    enum StimRegister {
        TriggerParams = 0,
        StimParams = 1,
        EventAmpSettleOn = 2,
        EventAmpSettleOff = 3,
        EventStartStim = 4,
        EventStimPhase2 = 5,
        EventStimPhase3 = 6,
        EventEndStim = 7,
        EventRepeatStim = 8,
        EventChargeRecovOn = 9,
        EventChargeRecovOff = 10,
        EventAmpSettleOnRepeat = 11,
        EventAmpSettleOffRepeat = 12,
        EventEnd = 13,
        DacBaseline = 9,
        DacPositive = 10,
        DacNegative = 11
    };

    enum AuxCmdSlot {
        AuxCmd1 = 0,
        AuxCmd2 = 1,
        AuxCmd3 = 2,
        AuxCmd4 = 3  // AuxCmd4 used only with stim/record controller
    };

    AbstractRHXController(ControllerType type_, AmplifierSampleRate sampleRate_);
    virtual ~AbstractRHXController();

    void initialize();

    static int maxNumDataStreams(ControllerType type_);
    int maxNumDataStreams() const;

    static int maxNumSPIPorts(ControllerType type_);
    int maxNumSPIPorts() const;

    static int boardMode(ControllerType type_);
    int boardMode() const;

    ControllerType getType() const;
    AmplifierSampleRate getSampleRateEnum() const;
    double getSampleRate() const;
    static double getSampleRate(AmplifierSampleRate sampleRate_);
    static int numAnalogIO(ControllerType type_, bool expanderConnected_);
    static int numDigitalIO(ControllerType type_, bool expanderConnected_);
    static string getAnalogInputChannelName(ControllerType type_, int channel_);
    static string getAnalogOutputChannelName(ControllerType type_, int channel_);
    static string getDigitalInputChannelName(ControllerType type_, int channel_);
    static string getDigitalOutputChannelName(ControllerType type_, int channel_);
    static string getAnalogIOChannelNumber(ControllerType type_, int channel_);
    static string getDigitalIOChannelNumber(ControllerType type_, int channel_);

    static string getBoardTypeString(ControllerType type_);
    static string getSampleRateString(AmplifierSampleRate sampleRate);
    static string getStimStepSizeString(StimStepSize stepSize);
    static AmplifierSampleRate nearestSampleRate(double rate, double percentTolerance = 1.0);
    static StimStepSize nearestStimStepSize(double step, double percentTolerance = 1.0);

    unsigned int getNumWordsInFifo();
    unsigned int getLastNumWordsInFifo();
    unsigned int getLastNumWordsInFifo(bool& hasBeenUpdated);
    unsigned int fifoCapacityInWords();

    void printCommandList(const vector<unsigned int> &commandList) const;

    void setCableLengthMeters(BoardPort port, double lengthInMeters);
    void setCableLengthFeet(BoardPort port, double lengthInFeet);
    double estimateCableLengthMeters(int delay) const;
    double estimateCableLengthFeet(int delay) const;

    int getNumEnabledDataStreams() const;
    int getCableDelay(BoardPort port) const;
    void getCableDelay(vector<int> &delays) const;

    void setAllDacsToZero();

    void configureStimTrigger(int stream, int channel, int triggerSource, bool triggerEnabled, bool edgeTriggered, bool triggerOnLow);
    void configureStimPulses(int stream, int channel, int numPulses, StimShape shape, bool negStimFirst);

    StreamChannelPair streamChannelFromWaveName(const string& waveName) const;

    virtual bool isSynthetic() const = 0;
    virtual bool isPlayback() const = 0;
    virtual AcquisitionMode acquisitionMode() const = 0;
    virtual int open(const string& boardSerialNumber) = 0;
    virtual bool uploadFPGABitfile(const string& filename) = 0;
    virtual void resetBoard() = 0;

    virtual void run() = 0;
    virtual bool isRunning() = 0;
    virtual void flush() = 0;
    virtual void resetFpga() = 0;

    virtual bool readDataBlock(RHXDataBlock *dataBlock) = 0;
    virtual bool readDataBlocks(int numBlocks, deque<RHXDataBlock*> &dataQueue) = 0;
    virtual long readDataBlocksRaw(int numBlocks, uint8_t* buffer) = 0;

    virtual void setContinuousRunMode(bool continuousMode) = 0;
    virtual void setMaxTimeStep(unsigned int maxTimeStep) = 0;
    virtual void setCableDelay(BoardPort port, int delay) = 0;
    virtual void setDspSettle(bool enabled) = 0;
    virtual void setDataSource(int stream, BoardDataSource dataSource) = 0;
    virtual void setTtlOut(const int* ttlOutArray) = 0;
    virtual void setDacManual(int value) = 0;
    virtual void setLedDisplay(const int* ledArray) = 0;
    virtual void setSpiLedDisplay(const int* ledArray) = 0;
    virtual void setDacGain(int gain) = 0;
    virtual void setAudioNoiseSuppress(int noiseSuppress) = 0;
    virtual void setExternalFastSettleChannel(int channel) = 0;
    virtual void setExternalDigOutChannel(BoardPort port, int channel) = 0;
    virtual void setDacHighpassFilter(double cutoff) = 0;
    virtual void setDacThreshold(int dacChannel, int threshold, bool trigPolarity) = 0;
    virtual void setTtlMode(int mode) = 0;
    virtual void setDacRerefSource(int stream, int channel) = 0;
    virtual void setExtraStates(unsigned int extraStates) = 0;
    virtual void setStimCmdMode(bool enabled) = 0;
    virtual void setAnalogInTriggerThreshold(double voltageThreshold) = 0;
    virtual void setManualStimTrigger(int trigger, bool triggerOn) = 0;
    virtual void setGlobalSettlePolicy(bool settleWholeHeadstageA, bool settleWholeHeadstageB, bool settleWholeHeadstageC, bool settleWholeHeadstageD, bool settleAllHeadstages) = 0;
    virtual void setTtlOutMode(bool mode1, bool mode2, bool mode3, bool mode4, bool mode5, bool mode6, bool mode7, bool mode8) = 0;
    virtual void setAmpSettleMode(bool useFastSettle) = 0;
    virtual void setChargeRecoveryMode(bool useSwitch) = 0;
    virtual bool setSampleRate(AmplifierSampleRate newSampleRate) = 0;

    virtual void enableDataStream(int stream, bool enabled) = 0;
    virtual void enableDac(int dacChannel, bool enabled) = 0;
    virtual void enableExternalFastSettle(bool enable) = 0;
    virtual void enableExternalDigOut(BoardPort port, bool enabled) = 0;
    virtual void enableDacHighpassFilter(bool enable) = 0;
    virtual void enableDacReref(bool enabled) = 0;
    virtual void enableDcAmpConvert(bool enable) = 0;
    virtual void enableAuxCommandsOnAllStreams() = 0;
    virtual void enableAuxCommandsOnOneStream(int stream) = 0;

    virtual void selectDacDataStream(int dacChannel, int stream) = 0;
    virtual void selectDacDataChannel(int dacChannel, int dataChannel) = 0;
    virtual void selectAuxCommandLength(AuxCmdSlot auxCommandSlot, int loopIndex, int endIndex) = 0;
    virtual void selectAuxCommandBank(BoardPort port, AuxCmdSlot auxCommandSlot, int bank) = 0;
    void selectAuxCommandBankAllPorts(AuxCmdSlot auxCommandSlot, int bank);

    virtual int getBoardMode() = 0;
    virtual int getNumSPIPorts(bool& expanderBoardDetected) = 0;

    virtual void clearTtlOut() = 0;
    virtual void resetSequencers() = 0;
    virtual void programStimReg(int stream, int channel, StimRegister reg, int value) = 0;
    virtual void uploadCommandList(const vector<unsigned int> &commandList, AuxCmdSlot auxCommandSlot, int bank) = 0;

    virtual int findConnectedChips(vector<ChipType> &chipType, vector<int> &portIndex, vector<int> &commandStream,
                                   vector<int> &numChannelsOnPort) = 0;

protected:
    ControllerType type;
    AmplifierSampleRate sampleRate;
    unsigned int usbBufferSize;
    int numDataStreams; // total number of data streams currently enabled
    vector<bool> dataStreamEnabled;
    vector<BoardDataSource> boardDataSources; // used by ControllerRecordUSB2 only
    vector<int> cableDelay;

    // Methods in this class are designed to be thread-safe.  This variable is used to ensure that.
    mutex okMutex;

    unsigned int lastNumWordsInFifo;
    bool numWordsHasBeenUpdated;

    // Buffer for reading bytes from USB interface
    uint8_t* usbBuffer;

    // Buffers for writing bytes to command RAM (ControllerStimRecordUSB2 only)
    uint8_t commandBufferMsw[65536];
    uint8_t commandBufferLsw[65536];

    virtual unsigned int numWordsInFifo() = 0;
    virtual bool isDcmProgDone() const = 0;
    virtual bool isDataClockLocked() const = 0;
    virtual void forceAllDataStreamsOff() = 0;

private:
    static bool approximatelyEqual(double a, double b, double percentTolerance);
};

#endif // ABSTRACTRHXCONTROLLER_H

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

#include "rhxregisters.h"
#include "abstractrhxcontroller.h"

#include <iostream>
#include <iomanip>
#include <cmath>

bool operator ==(const StreamChannelPair& a, const StreamChannelPair& b)
{
    return (a.stream == b.stream) && (a.channel == b.channel);
}

bool operator !=(const StreamChannelPair& a, const StreamChannelPair& b)
{
    return (a.stream != b.stream) || (a.channel != b.channel);
}

bool operator <(const StreamChannelPair& a, const StreamChannelPair& b)
{
    return ((a.stream << 5) + a.channel) < ((b.stream << 5) + b.channel);
}

bool operator >(const StreamChannelPair& a, const StreamChannelPair& b)
{
    return ((a.stream << 5) + a.channel) > ((b.stream << 5) + b.channel);
}


AbstractRHXController::AbstractRHXController(ControllerType type_, AmplifierSampleRate sampleRate_) :
    type(type_),
    sampleRate(sampleRate_)
{
    usbBufferSize = MaxNumBlocksToRead * BytesPerWord * RHXDataBlock::dataBlockSizeInWords(type, maxNumDataStreams());
    cout << "RHXController: Allocating " << usbBufferSize / 1.0e6 << " MBytes for USB buffer.\n";
    usbBuffer = nullptr;
    usbBuffer = new uint8_t [usbBufferSize];
    numDataStreams = 0;
    dataStreamEnabled.resize(maxNumDataStreams(), false);
    boardDataSources.resize(8, Unassigned); // used by ControllerRecordUSB2 only
    cableDelay.resize(maxNumSPIPorts(), -1);

    lastNumWordsInFifo = 0;
    numWordsHasBeenUpdated = false;
}

AbstractRHXController::~AbstractRHXController()
{
    delete [] usbBuffer;
}

// Initialize Rhythm FPGA to default starting values.
void AbstractRHXController::initialize()
{
    resetBoard();
    if (type == ControllerStimRecordUSB2) {
        enableAuxCommandsOnAllStreams();
        setGlobalSettlePolicy(false, false, false, false, false);
        setTtlOutMode(false, false, false, false, false, false, false, false);
    }
    setSampleRate(SampleRate30000Hz);  // By default, initialize to highest sampling rate.
    if (type == ControllerRecordUSB2 || type == ControllerRecordUSB3) {
        selectAuxCommandBankAllPorts(AuxCmd1, 0);
        selectAuxCommandBankAllPorts(AuxCmd2, 0);
        selectAuxCommandBankAllPorts(AuxCmd3, 0);
    }

    selectAuxCommandLength(AuxCmd1, 0, 0);
    selectAuxCommandLength(AuxCmd2, 0, 0);
    selectAuxCommandLength(AuxCmd3, 0, 0);
    if (type == ControllerStimRecordUSB2) {
        selectAuxCommandLength(AuxCmd4, 0, 0);
        setStimCmdMode(false);
    }
    setContinuousRunMode(true);
    setMaxTimeStep(4294967295);  // 4294967295 == (2^32 - 1)

    setCableLengthFeet(PortA, 3.0);  // Assume 3 ft cables.
    setCableLengthFeet(PortB, 3.0);
    setCableLengthFeet(PortC, 3.0);
    setCableLengthFeet(PortD, 3.0);
    if (type == ControllerRecordUSB3) {
        setCableLengthFeet(PortE, 3.0);
        setCableLengthFeet(PortF, 3.0);
        setCableLengthFeet(PortG, 3.0);
        setCableLengthFeet(PortH, 3.0);
    }

    setDspSettle(false);

    if (type == ControllerRecordUSB2) {
        setDataSource(0, PortA1);
        setDataSource(1, PortB1);
        setDataSource(2, PortC1);
        setDataSource(3, PortD1);
        setDataSource(4, PortA2);
        setDataSource(5, PortB2);
        setDataSource(6, PortC2);
        setDataSource(7, PortD2);
    }

    // Must first force all data streams off
    forceAllDataStreamsOff();

    enableDataStream(0, true);        // Start with only one data stream enabled.
    for (int i = 1; i < maxNumDataStreams(); i++) {
        enableDataStream(i, false);
    }

    if (type == ControllerStimRecordUSB2) {
        enableDcAmpConvert(true);
        setExtraStates(0);
    } else {
        clearTtlOut();
    }

    enableDac(0, false);
    enableDac(1, false);
    enableDac(2, false);
    enableDac(3, false);
    enableDac(4, false);
    enableDac(5, false);
    enableDac(6, false);
    enableDac(7, false);
    selectDacDataStream(0, 0);
    selectDacDataStream(1, 0);
    selectDacDataStream(2, 0);
    selectDacDataStream(3, 0);
    selectDacDataStream(4, 0);
    selectDacDataStream(5, 0);
    selectDacDataStream(6, 0);
    selectDacDataStream(7, 0);
    selectDacDataChannel(0, 0);
    selectDacDataChannel(1, 0);
    selectDacDataChannel(2, 0);
    selectDacDataChannel(3, 0);
    selectDacDataChannel(4, 0);
    selectDacDataChannel(5, 0);
    selectDacDataChannel(6, 0);
    selectDacDataChannel(7, 0);

    setDacManual(32768);    // midrange value = 0 V

    setDacGain(0);
    setAudioNoiseSuppress(0);

    if (type == ControllerRecordUSB2 || type == ControllerRecordUSB3) {
        setTtlMode(1);      // Digital outputs 0-7 are DAC comparators; 8-15 under manual control.
        enableExternalFastSettle(false);
        setExternalFastSettleChannel(0);
    }

    setDacThreshold(0, 32768, true);
    setDacThreshold(1, 32768, true);
    setDacThreshold(2, 32768, true);
    setDacThreshold(3, 32768, true);
    setDacThreshold(4, 32768, true);
    setDacThreshold(5, 32768, true);
    setDacThreshold(6, 32768, true);
    setDacThreshold(7, 32768, true);

    if (type == ControllerStimRecordUSB2 || type == ControllerRecordUSB3) {
        enableDacReref(false);
    }

    if (type == ControllerRecordUSB2 || type == ControllerRecordUSB3) {
        enableExternalDigOut(PortA, false);
        enableExternalDigOut(PortB, false);
        enableExternalDigOut(PortC, false);
        enableExternalDigOut(PortD, false);
        setExternalDigOutChannel(PortA, 0);
        setExternalDigOutChannel(PortB, 0);
        setExternalDigOutChannel(PortC, 0);
        setExternalDigOutChannel(PortD, 0);
    }
    if (type == ControllerRecordUSB3) {
        enableExternalDigOut(PortE, false);
        enableExternalDigOut(PortF, false);
        enableExternalDigOut(PortG, false);
        enableExternalDigOut(PortH, false);
        setExternalDigOutChannel(PortE, 0);
        setExternalDigOutChannel(PortF, 0);
        setExternalDigOutChannel(PortG, 0);
        setExternalDigOutChannel(PortH, 0);
    }

    if (type == ControllerStimRecordUSB2) {
        setAnalogInTriggerThreshold(1.65); // +1.65 V

        const int NEVER = 65535;
        int stream = 0;
        int channel = 0;

        for (stream = 0; stream < maxNumDataStreams(); stream++) {
            for (channel = 0; channel < RHXDataBlock::channelsPerStream(type); channel++) {
                configureStimTrigger(stream, channel, 0, false, true, false);
                configureStimPulses(stream, channel, 1, Biphasic, true);
                programStimReg(stream, channel, EventAmpSettleOn, NEVER);
                programStimReg(stream, channel, EventStartStim, NEVER);
                programStimReg(stream, channel, EventStimPhase2, NEVER);
                programStimReg(stream, channel, EventStimPhase3, NEVER);
                programStimReg(stream, channel, EventEndStim, NEVER);
                programStimReg(stream, channel, EventRepeatStim, NEVER);
                programStimReg(stream, channel, EventAmpSettleOff, NEVER);
                programStimReg(stream, channel, EventChargeRecovOn, NEVER);
                programStimReg(stream, channel, EventChargeRecovOff, NEVER);
                programStimReg(stream, channel, EventAmpSettleOnRepeat, NEVER);
                programStimReg(stream, channel, EventAmpSettleOffRepeat, NEVER);
                programStimReg(stream, channel, EventEnd, 65534);
            }
        }

        // Configure analog output sequencers.
        for (stream = 8; stream < 16; stream++) {
            configureStimTrigger(stream, 0, 0, false, true, false);
            configureStimPulses(stream, 0, 1, Monophasic, true);
            programStimReg(stream, 0, EventStartStim, 0);
            programStimReg(stream, 0, EventStimPhase2, NEVER);
            programStimReg(stream, 0, EventStimPhase3, NEVER);
            programStimReg(stream, 0, EventEndStim, 200);
            programStimReg(stream, 0, EventRepeatStim, NEVER);
            programStimReg(stream, 0, EventEnd, 240);
            programStimReg(stream, 0, DacBaseline, 32768);
            programStimReg(stream, 0, DacPositive, 32768 + 3200);
            programStimReg(stream, 0, DacNegative, 32768 - 3200);
        }

        // Configure digital output sequencers.
        stream = 16;
        for (channel = 0; channel < 16; channel++) {
            configureStimTrigger(stream, channel, 0, false, true, false);
            configureStimPulses(stream, channel, 3, Biphasic, true);
            programStimReg(stream, channel, EventStartStim, NEVER);
            programStimReg(stream, channel, EventEndStim, NEVER);
            programStimReg(stream, channel, EventRepeatStim, NEVER);
            programStimReg(stream, channel, EventEnd, 65534);
        }

        setAmpSettleMode(false); // set amp_settle_mode (false = amplifier low frequency cutoff select; true = amplifier fast settle)
        setChargeRecoveryMode(false); // set charge_recov_mode (false = current-limited charge recovery drivers; true = charge recovery switch)
    }
}

// Return the maximum number of data streams for a controller of the given type.
int AbstractRHXController::maxNumDataStreams(ControllerType type_)
{
    switch (type_) {
    case ControllerRecordUSB2:
    case ControllerStimRecordUSB2:
        return 8;   // USB 2 bus is limited to 8 data streams
    case ControllerRecordUSB3:
        return 32;   // USB 3 bus can handle 32 data streams
    default:
        return 0;
    }
}

// Return the maximum number of data streams for a controller of this controller's type.
int AbstractRHXController::maxNumDataStreams() const
{
    return maxNumDataStreams(type);
}

// Return the maximum number of SPI ports for a controller of the given type.
int AbstractRHXController::maxNumSPIPorts(ControllerType type_)
{
    switch (type_) {
    case ControllerRecordUSB2:
    case ControllerStimRecordUSB2:
        return 4;
    case ControllerRecordUSB3:
        return 8;
    default:
        return 0;
    }
}

// Return the maximum number of SPI ports for a controller of this controller's type.
int AbstractRHXController::maxNumSPIPorts() const
{
    return maxNumSPIPorts(type);
}

// Return the board mode for a controller of the given type.
int AbstractRHXController::boardMode(ControllerType type_)
{
    switch (type_) {
    case ControllerRecordUSB2:
        return 0;
    case ControllerRecordUSB3:
        return 13;
    case ControllerStimRecordUSB2:
        return 14;
    default:
        return -1;
    }
}

// Return the board mode for a controller of this controller's type.
int AbstractRHXController::boardMode() const
{
    return boardMode(type);
}

// Return the type of this controller.
ControllerType AbstractRHXController::getType() const
{
    return type;
}

// Return the sample rate (as an enum) of this controller.
AmplifierSampleRate AbstractRHXController::getSampleRateEnum() const
{
    return sampleRate;
}

// Return the current per-channel sampling rate (in Hz) as a floating-point number.
double AbstractRHXController::getSampleRate() const
{
    return getSampleRate(sampleRate);
}

// Return the given sample rate enum as a floating-point number.
double AbstractRHXController::getSampleRate(AmplifierSampleRate sampleRate_)
{
    switch (sampleRate_) {
    case SampleRate1000Hz:
        return 1000.0;
        break;
    case SampleRate1250Hz:
        return 1250.0;
        break;
    case SampleRate1500Hz:
        return 1500.0;
        break;
    case SampleRate2000Hz:
        return 2000.0;
        break;
    case SampleRate2500Hz:
        return 2500.0;
        break;
    case SampleRate3000Hz:
        return 3000.0;
        break;
    case SampleRate3333Hz:
        return (10000.0 / 3.0);
        break;
    case SampleRate4000Hz:
        return 4000.0;
        break;
    case SampleRate5000Hz:
        return 5000.0;
        break;
    case SampleRate6250Hz:
        return 6250.0;
        break;
    case SampleRate8000Hz:
        return 8000.0;
        break;
    case SampleRate10000Hz:
        return 10000.0;
        break;
    case SampleRate12500Hz:
        return 12500.0;
        break;
    case SampleRate15000Hz:
        return 15000.0;
        break;
    case SampleRate20000Hz:
        return 20000.0;
        break;
    case SampleRate25000Hz:
        return 25000.0;
        break;
    case SampleRate30000Hz:
        return 30000.0;
        break;
    default:
        return -1.0;
    }
}

int AbstractRHXController::numAnalogIO(ControllerType type_, bool expanderConnected_)
{
    int numAnalogInputs = 8;
    if ((type_ != ControllerRecordUSB2) && !expanderConnected_) numAnalogInputs = 2;
    return numAnalogInputs;
}

int AbstractRHXController::numDigitalIO(ControllerType type_, bool expanderConnected_)
{
    int numDigitalInputs = 16;
    if ((type_ != ControllerRecordUSB2) && !expanderConnected_) numDigitalInputs = 2;
    return numDigitalInputs;
}

string AbstractRHXController::getAnalogInputChannelName(ControllerType type_, int channel_)
{
    return "ANALOG-IN-" + getAnalogIOChannelNumber(type_, channel_);
}

string AbstractRHXController::getAnalogOutputChannelName(ControllerType type_, int channel_)
{
    return "ANALOG_OUT-" + getAnalogIOChannelNumber(type_, channel_);
}

string AbstractRHXController::getDigitalInputChannelName(ControllerType type_, int channel_)
{
    return "DIGITAL-IN-" + getDigitalIOChannelNumber(type_, channel_);
}

string AbstractRHXController::getDigitalOutputChannelName(ControllerType type_, int channel_)
{
    return "DIGITAL-OUT-" + getDigitalIOChannelNumber(type_, channel_);
}

string AbstractRHXController::getAnalogIOChannelNumber(ControllerType type_, int channel_)
{
    int channelWithOffset = (type_ == ControllerRecordUSB2) ? channel_ : channel_ + 1;
    string channelNumber = to_string(channelWithOffset);
    int numChars = (type_ == ControllerRecordUSB2) ? 2 : 1;
    if (numChars == 2 && channelWithOffset < 10) channelNumber = "0" + channelNumber;
    return channelNumber;
}

string AbstractRHXController::getDigitalIOChannelNumber(ControllerType type_, int channel_)
{
    int channelWithOffset = (type_ == ControllerRecordUSB2) ? channel_ : channel_ + 1;
    string channelNumber = to_string(channelWithOffset);
    int numChars = 2;
    if (numChars == 2 && channelWithOffset < 10) channelNumber = "0" + channelNumber;
    return channelNumber;
}

string AbstractRHXController::getBoardTypeString(ControllerType type_)
{
    string typeString;
    switch (type_) {
    case ControllerRecordUSB2:
        typeString = "ControllerRecordUSB2"; break;
    case ControllerRecordUSB3:
        typeString = "ControllerRecordUSB3"; break;
    case ControllerStimRecordUSB2:
        typeString = "ControllerStimRecordUSB2"; break;
    default:
        typeString = "unknown"; break;
    }
    return typeString;
}

string AbstractRHXController::getSampleRateString(AmplifierSampleRate sampleRate)
{
    string sampleRateString;
    switch (sampleRate) {
    case SampleRate30000Hz:
        sampleRateString = "30 kHz"; break;
    case SampleRate25000Hz:
        sampleRateString = "25 kHz"; break;
    case SampleRate20000Hz:
        sampleRateString = "20 kHz"; break;
    case SampleRate15000Hz:
        sampleRateString = "15 kHz"; break;
    case SampleRate12500Hz:
        sampleRateString = "12.5 kHz"; break;
    case SampleRate10000Hz:
        sampleRateString = "10 kHz"; break;
    case SampleRate8000Hz:
        sampleRateString = "8 kHz"; break;
    case SampleRate6250Hz:
        sampleRateString = "6.25 kHz"; break;
    case SampleRate5000Hz:
        sampleRateString = "5 kHz"; break;
    case SampleRate4000Hz:
        sampleRateString = "4 kHz"; break;
    case SampleRate3333Hz:
        sampleRateString = "3.33 kHz"; break;
    case SampleRate3000Hz:
        sampleRateString = "3 kHz"; break;
    case SampleRate2500Hz:
        sampleRateString = "2.5 kHz"; break;
    case SampleRate2000Hz:
        sampleRateString = "2 kHz"; break;
    case SampleRate1500Hz:
        sampleRateString = "1.5 kHz"; break;
    case SampleRate1250Hz:
        sampleRateString = "1.25 kHz"; break;
    case SampleRate1000Hz:
        sampleRateString = "1 kHz"; break;
    default:
        sampleRateString = "unknown"; break;
    }
    return sampleRateString;
}

string AbstractRHXController::getStimStepSizeString(StimStepSize stepSize)
{
    string stimStepSizeString;
    switch (stepSize) {
    case StimStepSize10nA:
        stimStepSizeString = "10 nA"; break;
    case StimStepSize20nA:
        stimStepSizeString = "20 nA"; break;
    case StimStepSize50nA:
        stimStepSizeString = "50 nA"; break;
    case StimStepSize100nA:
        stimStepSizeString = "100 nA"; break;
    case StimStepSize200nA:
        stimStepSizeString = "200 nA"; break;
    case StimStepSize500nA:
        stimStepSizeString = "500 nA"; break;
    case StimStepSize1uA:
        stimStepSizeString = "1 uA"; break;
    case StimStepSize2uA:
        stimStepSizeString = "2 uA"; break;
    case StimStepSize5uA:
        stimStepSizeString = "5 uA"; break;
    case StimStepSize10uA:
        stimStepSizeString = "10 uA"; break;
    default:
        stimStepSizeString = "unknown"; break;
    }
    return stimStepSizeString;
}

bool AbstractRHXController::approximatelyEqual(double a, double b, double percentTolerance)
{
    double error = a / b;
    if (error < 1.0) error = 1.0 / error;
    double percentError = 100.0 * (error - 1.0);
    return percentError <= percentTolerance;
}

AmplifierSampleRate AbstractRHXController::nearestSampleRate(double rate, double percentTolerance)
{
    if (approximatelyEqual(rate, getSampleRate(SampleRate30000Hz), percentTolerance))
        return SampleRate30000Hz;
    if (approximatelyEqual(rate, getSampleRate(SampleRate25000Hz), percentTolerance))
        return SampleRate25000Hz;
    if (approximatelyEqual(rate, getSampleRate(SampleRate20000Hz), percentTolerance))
        return SampleRate20000Hz;
    if (approximatelyEqual(rate, getSampleRate(SampleRate15000Hz), percentTolerance))
        return SampleRate15000Hz;
    if (approximatelyEqual(rate, getSampleRate(SampleRate12500Hz), percentTolerance))
        return SampleRate12500Hz;
    if (approximatelyEqual(rate, getSampleRate(SampleRate10000Hz), percentTolerance))
        return SampleRate10000Hz;
    if (approximatelyEqual(rate, getSampleRate(SampleRate8000Hz), percentTolerance))
        return SampleRate8000Hz;
    if (approximatelyEqual(rate, getSampleRate(SampleRate6250Hz), percentTolerance))
        return SampleRate6250Hz;
    if (approximatelyEqual(rate, getSampleRate(SampleRate5000Hz), percentTolerance))
        return SampleRate5000Hz;
    if (approximatelyEqual(rate, getSampleRate(SampleRate4000Hz), percentTolerance))
        return SampleRate4000Hz;
    if (approximatelyEqual(rate, getSampleRate(SampleRate3333Hz), percentTolerance))
        return SampleRate3333Hz;
    if (approximatelyEqual(rate, getSampleRate(SampleRate3000Hz), percentTolerance))
        return SampleRate3000Hz;
    if (approximatelyEqual(rate, getSampleRate(SampleRate2500Hz), percentTolerance))
        return SampleRate2500Hz;
    if (approximatelyEqual(rate, getSampleRate(SampleRate2000Hz), percentTolerance))
        return SampleRate2000Hz;
    if (approximatelyEqual(rate, getSampleRate(SampleRate1500Hz), percentTolerance))
        return SampleRate1500Hz;
    if (approximatelyEqual(rate, getSampleRate(SampleRate1250Hz), percentTolerance))
        return SampleRate1250Hz;
    if (approximatelyEqual(rate, getSampleRate(SampleRate1000Hz), percentTolerance))
        return SampleRate1000Hz;
    return (AmplifierSampleRate) -1;
}

StimStepSize AbstractRHXController::nearestStimStepSize(double step, double percentTolerance)
{
    if (approximatelyEqual(step, RHXRegisters::stimStepSizeToDouble(StimStepSize10nA), percentTolerance))
        return StimStepSize10nA;
    if (approximatelyEqual(step, RHXRegisters::stimStepSizeToDouble(StimStepSize20nA), percentTolerance))
        return StimStepSize20nA;
    if (approximatelyEqual(step, RHXRegisters::stimStepSizeToDouble(StimStepSize50nA), percentTolerance))
        return StimStepSize50nA;
    if (approximatelyEqual(step, RHXRegisters::stimStepSizeToDouble(StimStepSize100nA), percentTolerance))
        return StimStepSize100nA;
    if (approximatelyEqual(step, RHXRegisters::stimStepSizeToDouble(StimStepSize200nA), percentTolerance))
        return StimStepSize200nA;
    if (approximatelyEqual(step, RHXRegisters::stimStepSizeToDouble(StimStepSize500nA), percentTolerance))
        return StimStepSize500nA;
    if (approximatelyEqual(step, RHXRegisters::stimStepSizeToDouble(StimStepSize1uA), percentTolerance))
        return StimStepSize1uA;
    if (approximatelyEqual(step, RHXRegisters::stimStepSizeToDouble(StimStepSize2uA), percentTolerance))
        return StimStepSize2uA;
    if (approximatelyEqual(step, RHXRegisters::stimStepSizeToDouble(StimStepSize5uA), percentTolerance))
        return StimStepSize5uA;
    if (approximatelyEqual(step, RHXRegisters::stimStepSizeToDouble(StimStepSize10uA), percentTolerance))
        return StimStepSize10uA;
    return (StimStepSize) -1;
}

// Return the number of 16-bit words in the USB FIFO.  The user should never attempt to read
// more data than the FIFO currently contains, as it is not protected against underflow.
// (Public, threadsafe method.)
unsigned int AbstractRHXController::getNumWordsInFifo()
{
    lock_guard<mutex> lockOk(okMutex);

    return numWordsInFifo();
}

// Return the most recently measured number of 16-bit words in the USB FIFO.  Does not directly
// read this value from the USB port, and so may be out of date, but does not have to wait on
// other USB access to finish in order to execute.
unsigned int AbstractRHXController::getLastNumWordsInFifo()
{
    numWordsHasBeenUpdated = false;
    return lastNumWordsInFifo;
}

// Return the most recently measured number of 16-bit words in the USB FIFO.  Does not directly
// read this value from the USB port, and so may be out of date, but does not have to wait on
// other USB access to finish in order to execute.  The boolean variable hasBeenUpdated indicates
// if this value has been updated since the last time this function was called.
unsigned int AbstractRHXController::getLastNumWordsInFifo(bool& hasBeenUpdated)
{
    hasBeenUpdated = numWordsHasBeenUpdated;
    numWordsHasBeenUpdated = false;
    return lastNumWordsInFifo;
}

// Return the number of 16-bit words the USB SDRAM FIFO can hold.  The FIFO can actually hold a few
// thousand words more than the number returned by this method due to FPGA "mini-FIFOs" interfacing
// with the SDRAM, but this provides a conservative estimate of FIFO capacity.
unsigned int AbstractRHXController::fifoCapacityInWords()
{
    return FIFOCapacityInWords;
}

// Print a command list to the console in readable form.
void AbstractRHXController::printCommandList(const vector<unsigned int> &commandList) const
{
    unsigned int i, cmd;
    int channel, reg, data, uFlag, mFlag, dFlag, hFlag;

    cout << '\n';
    for (i = 0; i < commandList.size(); ++i) {
        cmd = commandList[i];
        if (type != ControllerStimRecordUSB2) {
            if (cmd < 0 || cmd > 0xffff) {
                cout << "  command[" << i << "] = INVALID COMMAND: " << cmd << '\n';
            } else if ((cmd & 0xc000) == 0x0000) {
                channel = (cmd & 0x3f00) >> 8;
                cout << "  command[" << i << "] = CONVERT(" << channel << ")\n";
            } else if ((cmd & 0xc000) == 0xc000) {
                reg = (cmd & 0x3f00) >> 8;
                cout << "  command[" << i << "] = READ(" << reg << ")\n";
            } else if ((cmd & 0xc000) == 0x8000) {
                reg = (cmd & 0x3f00) >> 8;
                data = (cmd & 0x00ff);
                cout << "  command[" << i << "] = WRITE(" << reg << ",";
                cout << hex << uppercase << internal << setfill('0') << setw(2) << data << nouppercase << dec;
                cout << ")\n";
            } else if (cmd == 0x5500) {
                cout << "  command[" << i << "] = CALIBRATE\n";
            } else if (cmd == 0x6a00) {
                cout << "  command[" << i << "] = CLEAR\n";
            } else {
                cout << "  command[" << i << "] = INVALID COMMAND: ";
                cout << hex << uppercase << internal << setfill('0') << setw(4) << cmd << nouppercase << dec << '\n';
            }
        } else {
            channel = (cmd & 0x003f0000) >> 16;
            uFlag = (cmd & 0x20000000) >> 29;
            mFlag = (cmd & 0x10000000) >> 28;
            dFlag = (cmd & 0x08000000) >> 27;
            hFlag = (cmd & 0x04000000) >> 26;
            reg = (cmd & 0x00ff0000) >> 16;
            data = (cmd & 0x0000ffff);

            if ((cmd & 0xc0000000) == 0x00000000) {
                cout << "  command[" << i << "] = CONVERT(" << channel << "), UMDH=" << uFlag << mFlag << dFlag << hFlag << '\n';
            } else if ((cmd & 0xc0000000) == 0xc0000000) {
                cout << "  command[" << i << "] = READ(" << reg << "), UM=" << uFlag << mFlag << '\n';
            } else if ((cmd & 0xc0000000) == 0x80000000) {
                cout << "  command[" << i << "] = WRITE(" << reg << ",";
                cout << hex << uppercase << internal << setfill('0') << setw(4) << data << nouppercase << dec;
                cout << "), UM=" << uFlag << mFlag << '\n';
            } else if (cmd == 0x55000000) {
                cout << "  command[" << i << "] = CALIBRATE\n";
            } else if (cmd == 0x6a000000) {
                cout << "  command[" << i << "] = CLEAR\n";
            } else {
                cout << "  command[" << i << "] = INVALID COMMAND: ";
                cout << hex << uppercase << internal << setfill('0') << setw(8) << cmd << nouppercase << dec << '\n';
            }
        }
    }
    cout << '\n';
}

// Set the delay for sampling the MISO line on a particular SPI port (PortA - PortH) based on the length
// of the cable between the FPGA and the Intan RHD/RHS chip (in meters).
// Note: Cable delay must be updated after sampleRate is changed, since cable delay calculations are
// based on the clock frequency!
void AbstractRHXController::setCableLengthMeters(BoardPort port, double lengthInMeters)
{
    int delay;
    double tStep, cableVelocity, distance, timeDelay;
    const double SpeedOfLight = 299792458.0;  // units = meters per second
    const double XilinxLvdsOutputDelay = 1.9e-9;    // 1.9 ns Xilinx LVDS output pin delay
    const double XilinxLvdsInputDelay = 1.4e-9;     // 1.4 ns Xilinx LVDS input pin delay
    const double Rhd2000Delay = 9.0e-9;             // 9.0 ns RHD2000 SCLK-to-MISO delay
    const double MisoSettleTime = 6.7e-9;           // 6.7 ns delay after MISO changes, before we sample it

    tStep = 1.0 / (2800.0 * getSampleRate());  // data clock that samples MISO has a rate 35 x 80 = 2800x higher than the sampling rate
    // cableVelocity = 0.67 * speedOfLight;  // propogation velocity on cable: version 1.3 and earlier
    cableVelocity = 0.555 * SpeedOfLight;  // propogation velocity on cable: version 1.4 improvement based on cable measurements
    distance = 2.0 * lengthInMeters;      // round trip distance data must travel on cable
    timeDelay = (distance / cableVelocity) + XilinxLvdsOutputDelay + Rhd2000Delay + XilinxLvdsInputDelay + MisoSettleTime;

    delay = (int) floor(((timeDelay / tStep) + 1.0) + 0.5);

    if (delay < 1) delay = 1;   // delay of zero is too short (due to I/O delays), even for zero-length cables

    setCableDelay(port, delay);
}

// Same function as above, but accept lengths in feet instead of meters
void AbstractRHXController::setCableLengthFeet(BoardPort port, double lengthInFeet)
{
    setCableLengthMeters(port, 0.3048 * lengthInFeet);   // convert feet to meters
}

// Estimate cable length based on a particular delay used in setCableDelay.
// (Note: Depends on sample rate.)
double AbstractRHXController::estimateCableLengthMeters(int delay) const
{
    double tStep, cableVelocity, distance;
    const double speedOfLight = 299792458.0;  // units = meters per second
    const double xilinxLvdsOutputDelay = 1.9e-9;    // 1.9 ns Xilinx LVDS output pin delay
    const double xilinxLvdsInputDelay = 1.4e-9;     // 1.4 ns Xilinx LVDS input pin delay
    const double rhd2000Delay = 9.0e-9;             // 9.0 ns RHD2000 SCLK-to-MISO delay
    const double misoSettleTime = 6.7e-9;           // 6.7 ns delay after MISO changes, before we sample it

    tStep = 1.0 / (2800.0 * getSampleRate());  // data clock that samples MISO has a rate 35 x 80 = 2800x higher than the sampling rate
    // cableVelocity = 0.67 * speedOfLight;  // propogation velocity on cable: version 1.3 and earlier
    cableVelocity = 0.555 * speedOfLight;  // propogation velocity on cable: version 1.4 improvement based on cable measurements

    // distance = cableVelocity * (delay * tStep - (xilinxLvdsOutputDelay + rhd2000Delay + xilinxLvdsInputDelay));  // version 1.3 and earlier
    distance = cableVelocity * ((((double) delay) - 1.0) * tStep - (xilinxLvdsOutputDelay + rhd2000Delay + xilinxLvdsInputDelay + misoSettleTime));  // version 1.4 improvement
    if (distance < 0.0) distance = 0.0;

    return (distance / 2.0);
}

// Same function as above, but return length in feet instead of meters.
double AbstractRHXController::estimateCableLengthFeet(int delay) const
{
    return 3.2808 * estimateCableLengthMeters(delay);
}

// Return the number of enabled data streams.
int AbstractRHXController::getNumEnabledDataStreams() const
{
    return numDataStreams;
}

// Return FPGA cable delay for selected SPI port.
int AbstractRHXController::getCableDelay(BoardPort port) const
{
    switch (port) {
    case PortA:
        return cableDelay[0];
    case PortB:
        return cableDelay[1];
    case PortC:
        return cableDelay[2];
    case PortD:
        return cableDelay[3];
    case PortE:
        return cableDelay[4];
    case PortF:
        return cableDelay[5];
    case PortG:
        return cableDelay[6];
    case PortH:
        return cableDelay[7];
    default:
        cerr << "Error in RHXController::getCableDelay: unknown port.\n";
        return -1;
    }
}

// Return FPGA cable delays for all SPI ports.
void AbstractRHXController::getCableDelay(vector<int> &delays) const
{
    if ((int) delays.size() != maxNumSPIPorts()) {
        delays.resize(maxNumSPIPorts());
    }
    for (int i = 0; i < maxNumSPIPorts(); ++i) {
        delays[i] = cableDelay[i];
    }
}

// Set all DACs to midline value (zero).  Must run SPI commands for this to take effect.
void AbstractRHXController::setAllDacsToZero()
{
    setDacManual(32768);    // midrange value = 0 V
    int dacManualStream = (type == ControllerRecordUSB3) ? 32 : 8;
    for (int i = 0; i < 8; i++) {
        selectDacDataStream(i, dacManualStream);
    }
}

// Configure a particular stimulation trigger.
void AbstractRHXController::configureStimTrigger(int stream, int channel, int triggerSource, bool triggerEnabled, bool edgeTriggered, bool triggerOnLow)
{
    if (type != ControllerStimRecordUSB2) return;
    int value = (triggerEnabled ? (1 << 7) : 0) + (triggerOnLow ? (1 << 6) : 0) + (edgeTriggered ? (1 << 5) : 0) + triggerSource;
    programStimReg(stream, channel, TriggerParams, value);
}

// Configure the shape, polarity, and number of pulses for a particular stimulation control unit.
void AbstractRHXController::configureStimPulses(int stream, int channel, int numPulses, StimShape shape, bool negStimFirst)
{
    if (type != ControllerStimRecordUSB2) return;
    if (numPulses < 1) {
        cerr << "Error in RHXController::configureStimPulses: numPulses out of range.\n";
        return;
    }

    int shapeInt = (int)shape;
    int value = (negStimFirst ? (1 << 10) : 0) + (shapeInt << 8) + (numPulses - 1);
    programStimReg(stream, channel, StimParams, value);
}

// Return the stream and channel number within that stream required to access an amplifier or auxiliary analog input
// specified by the string waveName.  Amplifier channels are specified as follows: "A-000", "C-127", "H-031", etc.
// The first letter is a port designation (A-H in the 1024-channel Recording Controller; A-D in all others), followed
// by a hyphen, followed by a three-digit channel number ranging from 000-127 in RHD systems; 000-031 in RHS systems.
// Auxiliary input channels (RHD systems only) are specified as follows: "A-AUX1", "C-AUX6", "H-AUX2", etc.  The first
// letter is a port designation, followed by a hyphen, followed by "AUX" and a number ranging from 1-6.  Auxiliary
// waveNames return a valid stream number, but channel = -1 since three auxiliary inputs share the same channel.
// Invalid waveNames return stream = -1, channel = -1.
StreamChannelPair AbstractRHXController::streamChannelFromWaveName(const string& waveName) const
{
    StreamChannelPair streamChannelPair;
    streamChannelPair.stream = -1;
    streamChannelPair.channel = -1;

    char portLetter = waveName[0];
    BoardPort port;
    if      (portLetter == 'A') port = PortA;
    else if (portLetter == 'B') port = PortB;
    else if (portLetter == 'C') port = PortC;
    else if (portLetter == 'D') port = PortD;
    else if (portLetter == 'E' && type == ControllerRecordUSB3) port = PortE;
    else if (portLetter == 'F' && type == ControllerRecordUSB3) port = PortF;
    else if (portLetter == 'G' && type == ControllerRecordUSB3) port = PortG;
    else if (portLetter == 'H' && type == ControllerRecordUSB3) port = PortH;
    else return streamChannelPair;

    if (waveName[1] != '-') return streamChannelPair;

    bool auxChannel = false;
    if (waveName.substr(2,3) == "AUX") auxChannel = true;
    if (auxChannel && type == ControllerStimRecordUSB2) return streamChannelPair;

    int channelNumber;
    if (auxChannel) {
        channelNumber = stoi(waveName.substr(5,1));
        if (channelNumber < 1 || channelNumber > 6) return streamChannelPair;
    } else {
        channelNumber = stoi(waveName.substr(2,3));
        if (channelNumber < 0 || channelNumber > 127) return streamChannelPair;
    }

    if (type == ControllerStimRecordUSB2) {
        int streamBase = 2 * (int)port;
        if (!dataStreamEnabled[streamBase] && dataStreamEnabled[streamBase+1]) {
            // Unlikely case where a single RHS2116 chip is plugged into MISO2 on a port
            streamChannelPair.stream = streamBase + 1;
            streamChannelPair.channel = channelNumber;
        } else {
            bool miso2 = channelNumber > 15;
            streamChannelPair.stream = streamBase + (miso2 ? 1 : 0);
            streamChannelPair.channel = channelNumber - (miso2 ? 16 : 0);
        }
        int stream = streamChannelPair.stream;
        for (int u = 0; u < stream; ++u) {  // Adjust for unused data streams before this one.
            if (!dataStreamEnabled[u]) streamChannelPair.stream--;
        }
        return streamChannelPair;
    }

    if (type == ControllerRecordUSB3) {
        int streamBase = 4 * (int)port;
        if (auxChannel) {
            streamChannelPair.stream = streamBase + (channelNumber > 3 ? 2 : 0);
            return streamChannelPair;
        }
        int channelTemp = channelNumber;
        for (int i = 0; i < 4; ++i) {
            if (dataStreamEnabled[streamBase + i]) {
                if (channelTemp > 31) {
                    channelTemp -= 32;
                } else {
                    streamChannelPair.stream = streamBase + i;
                    for (int u = 0; u < streamBase + i; ++u) {  // Adjust for unused data streams before this one.
                        if (!dataStreamEnabled[u]) streamChannelPair.stream--;
                    }
                    streamChannelPair.channel = channelTemp;
                    return streamChannelPair;
                }
            }
        }
        return streamChannelPair;
    }

    if (type == ControllerRecordUSB2) {
        // This code assumes that data streams have been assigned *in order* from A to D, MISO1 to MISO2,
        // non-DDR to DDR, across the eight available data streams.

        // Find beginning of designated port in list of data streams.
        int sBegin = 0;
        for (int s = 0; s < 8; ++s) {
            if (dataStreamEnabled[s]) {
                if (port == PortA) {
                    if (boardDataSources[s] == PortA1 || boardDataSources[s] == PortA2 ||
                        boardDataSources[s] == PortA1Ddr || boardDataSources[s] == PortA2Ddr) break;
                } else if (port == PortB) {
                    if (boardDataSources[s] == PortB1 || boardDataSources[s] == PortB2 ||
                        boardDataSources[s] == PortB1Ddr || boardDataSources[s] == PortB2Ddr) break;
                } else if (port == PortC) {
                    if (boardDataSources[s] == PortC1 || boardDataSources[s] == PortC2 ||
                        boardDataSources[s] == PortC1Ddr || boardDataSources[s] == PortC2Ddr) break;
                } else if (port == PortD) {
                    if (boardDataSources[s] == PortD1 || boardDataSources[s] == PortD2 ||
                        boardDataSources[s] == PortD1Ddr || boardDataSources[s] == PortD2Ddr) break;
                }
                ++sBegin;
            }
        }
        if (sBegin == 8) return streamChannelPair;

        if (auxChannel) {
            if (channelNumber < 4) {
                streamChannelPair.stream = sBegin;
                return streamChannelPair;
            } else {
                if (sBegin < 7) {
                    if (boardDataSources[sBegin + 1] == (BoardDataSource)((int)boardDataSources[sBegin] + 1)) {
                        streamChannelPair.stream = sBegin + 1;
                        return streamChannelPair;
                    }
                }
                if (sBegin < 6) {
                    if (boardDataSources[sBegin + 2] == (BoardDataSource)((int)boardDataSources[sBegin] + 1)) {
                        streamChannelPair.stream = sBegin + 2;
                        return streamChannelPair;
                    }
                }
                return streamChannelPair;
            }
        }

        // Find end of designated port in list of data streams.
        int sEnd = sBegin;
        for (int s = sBegin + 1; s < 8; ++s) {
            if (dataStreamEnabled[s]) {
                if (port == PortA) {
                    if (boardDataSources[s] != PortA1 && boardDataSources[s] != PortA2 &&
                        boardDataSources[s] != PortA1Ddr && boardDataSources[s] != PortA2Ddr) break;
                } else if (port == PortB) {
                    if (boardDataSources[s] != PortB1 && boardDataSources[s] != PortB2 &&
                        boardDataSources[s] != PortB1Ddr && boardDataSources[s] != PortB2Ddr) break;
                } else if (port == PortC) {
                    if (boardDataSources[s] != PortC1 && boardDataSources[s] != PortC2 &&
                        boardDataSources[s] != PortC1Ddr && boardDataSources[s] != PortC2Ddr) break;
                } else if (port == PortD) {
                    if (boardDataSources[s] != PortD1 && boardDataSources[s] != PortD2 &&
                        boardDataSources[s] != PortD1Ddr && boardDataSources[s] != PortD2Ddr) break;
                }
                ++sEnd;
            }
        }
        if (sEnd - sBegin > 3) return streamChannelPair;

        int channelTemp = channelNumber;
        for (int s = sBegin; s < sEnd + 1; ++s) {
            if (dataStreamEnabled[s]) {
                if (channelTemp > 31) {
                    channelTemp -= 32;
                } else {
                    streamChannelPair.stream = s;
                    for (int u = 0; u < s; ++u) {  // Adjust for unused data streams before this one.
                        if (!dataStreamEnabled[u]) streamChannelPair.stream--;
                    }
                    streamChannelPair.channel = channelTemp;
                    return streamChannelPair;
                }
            }
        }
        return streamChannelPair;
    }
    return streamChannelPair;
}

void AbstractRHXController::selectAuxCommandBankAllPorts(AuxCmdSlot auxCommandSlot, int bank)
{
    selectAuxCommandBank(PortA, auxCommandSlot, bank);
    selectAuxCommandBank(PortB, auxCommandSlot, bank);
    selectAuxCommandBank(PortC, auxCommandSlot, bank);
    selectAuxCommandBank(PortD, auxCommandSlot, bank);
    if (type == ControllerRecordUSB3) {
        selectAuxCommandBank(PortE, auxCommandSlot, bank);
        selectAuxCommandBank(PortF, auxCommandSlot, bank);
        selectAuxCommandBank(PortG, auxCommandSlot, bank);
        selectAuxCommandBank(PortH, auxCommandSlot, bank);
    }
}

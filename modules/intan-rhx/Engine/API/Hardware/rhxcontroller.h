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

#ifndef RHXCONTROLLER_H
#define RHXCONTROLLER_H

#include "abstractrhxcontroller.h"
#include "rhxglobals.h"
#include "rhxdatablock.h"
#include "okFrontPanel.h"

const int USB3BlockSize	= 1024;
const int RAMBurstSize = 32;

class RHXController : public AbstractRHXController
{

public:
    RHXController(ControllerType type_, AmplifierSampleRate sampleRate_, bool is7310_ = false);
    ~RHXController();

    bool isSynthetic() const override { return false; }
    bool isPlayback() const override { return false; }
    AcquisitionMode acquisitionMode() const override { return LiveMode; }

    std::vector<std::string> listAvailableDeviceSerials();

    int open(const std::string& boardSerialNumber) override;
    int open();
    bool uploadFPGABitfile(const std::string& filename) override;
    void resetBoard() override;

    void run() override;
    bool isRunning() override;
    void flush() override;
    void resetFpga() override;

    bool readDataBlock(RHXDataBlock *dataBlock) override;
    bool readDataBlocks(int numBlocks, std::deque<RHXDataBlock*> &dataQueue) override;
    long readDataBlocksRaw(int numBlocks, uint8_t* buffer) override;

    int queueToFile(std::deque<RHXDataBlock*> &dataQueue, std::ofstream &saveOut);

    void setContinuousRunMode(bool continuousMode) override;
    void setMaxTimeStep(unsigned int maxTimeStep) override;
    void setCableDelay(BoardPort port, int delay) override;
    void setDspSettle(bool enabled) override;
    void setDataSource(int stream, BoardDataSource dataSource) override;  // used only with ControllerRecordUSB2
    void setTtlOut(const int* ttlOutArray) override;  // not used with ControllerStimRecord
    void setDacManual(int value) override;
    void setLedDisplay(const int* ledArray) override;
    void setSpiLedDisplay(const int* ledArray) override;  // not used with ControllerRecordUSB2
    void setDacGain(int gain) override;
    void setAudioNoiseSuppress(int noiseSuppress) override;
    void setExternalFastSettleChannel(int channel) override;             // not used with ControllerStimRecord
    void setExternalDigOutChannel(BoardPort port, int channel) override; // not used with ControllerStimRecord
    void setDacHighpassFilter(double cutoff) override;
    void setDacThreshold(int dacChannel, int threshold, bool trigPolarity) override;
    void setTtlMode(int mode) override;      // not used with ControllerStimRecord
    void setDacRerefSource(int stream, int channel) override;  // not used with ControllerRecordUSB2
    void setExtraStates(unsigned int extraStates) override;
    void setStimCmdMode(bool enabled) override;
    void setAnalogInTriggerThreshold(double voltageThreshold) override;
    void setManualStimTrigger(int trigger, bool triggerOn) override;
    void setGlobalSettlePolicy(bool settleWholeHeadstageA, bool settleWholeHeadstageB, bool settleWholeHeadstageC, bool settleWholeHeadstageD, bool settleAllHeadstages) override;
    void setTtlOutMode(bool mode1, bool mode2, bool mode3, bool mode4, bool mode5, bool mode6, bool mode7, bool mode8) override;
    void setAmpSettleMode(bool useFastSettle) override;
    void setChargeRecoveryMode(bool useSwitch) override;
    bool setSampleRate(AmplifierSampleRate newSampleRate) override;

    void enableDataStream(int stream, bool enabled) override;
    void enableDac(int dacChannel, bool enabled) override;
    void enableExternalFastSettle(bool enable) override;                 // not used with ControllerStimRecord
    void enableExternalDigOut(BoardPort port, bool enable) override;     // not used with ControllerStimRecord
    void enableDacHighpassFilter(bool enable) override;
    void enableDacReref(bool enabled) override;  // not used with ControllerRecordUSB2
    void enableDcAmpConvert(bool enable) override;
    void enableAuxCommandsOnAllStreams() override;
    void enableAuxCommandsOnOneStream(int stream) override;

    void selectDacDataStream(int dacChannel, int stream) override;
    void selectDacDataChannel(int dacChannel, int dataChannel) override;
    void selectAuxCommandLength(AuxCmdSlot auxCommandSlot, int loopIndex, int endIndex) override;
    void selectAuxCommandBank(BoardPort port, AuxCmdSlot auxCommandSlot, int bank) override; // not used with ControllerStimRecord

    int getBoardMode() override;
    int getNumSPIPorts(bool& expanderBoardDetected) override;

    void clearTtlOut() override;                 // not used with ControllerStimRecord
    void resetSequencers() override;
    void programStimReg(int stream, int channel, StimRegister reg, int value) override;
    void uploadCommandList(const std::vector<unsigned int> &commandList, AuxCmdSlot auxCommandSlot, int bank = 0) override;

    int findConnectedChips(std::vector<ChipType> &chipType, std::vector<int> &portIndex, std::vector<int> &commandStream,
                           std::vector<int> &numChannelsOnPort, bool /*synthMaxChannels = false*/, bool returnToFastSettle = false,
                           bool usePreviousDelay = false, int selectedPort = 0, int lastDetectedChip = -1,
                           int lastDetectedNumStreams = -1) override;

    // Physical board only
    static void resetBoard(okCFrontPanel* dev_);
    static int getBoardMode(okCFrontPanel* dev_);
    static int getNumSPIPorts(okCFrontPanel *dev_, bool isUSB3, bool& expanderBoardDetected);

private:
    // Objects of this class should not be copied.  Disable copy and assignment operators.
    RHXController(const RHXController&);            // declaration only
    RHXController& operator=(const RHXController&); // declaration only

    okCFrontPanel *dev;
    bool is7310;

    // Opal Kelly module USB interface endpoint addresses common to all controller types
    enum EndPoint {
        WireInResetRun = 0x00,
        WireInDataFreqPll = 0x03,
        WireInMisoDelay = 0x04,
        WireInDataStreamEn = 0x14,
        WireInDacSource1 = 0x16,
        WireInDacSource2 = 0x17,
        WireInDacSource3 = 0x18,
        WireInDacSource4 = 0x19,
        WireInDacSource5 = 0x1a,
        WireInDacSource6 = 0x1b,
        WireInDacSource7 = 0x1c,
        WireInDacSource8 = 0x1d,
        WireInDacManual = 0x1e,
        WireInMultiUse = 0x1f,

        TrigInSpiStart = 0x41,

        WireOutSpiRunning = 0x22,
        WireOutTtlIn = 0x23,
        WireOutDataClkLocked = 0x24,
        WireOutBoardMode = 0x25,
        WireOutBoardId = 0x3e,
        WireOutBoardVersion = 0x3f,

        PipeOutData = 0xa0
    };

    // Opal Kelly module USB interface endpoint addresses common to recorder controller types
    enum EndPointRecord {
        WireInCmdRamAddr_R = 0x05,
        WireInCmdRamBank_R = 0x06,
        WireInCmdRamData_R = 0x07,
        WireInAuxCmdBank1_R = 0x08,
        WireInAuxCmdBank2_R = 0x09,
        WireInAuxCmdBank3_R = 0x0a,
        WireInTtlOut_R = 0x15
    };

    // Opal Kelly module USB interface endpoint addresses common to USB2 controller types
    enum EndPointUSB2 {
        WireInMaxTimeStepLsb_USB2 = 0x01,
        WireInMaxTimeStepMsb_USB2 = 0x02,

        TrigInDcmProg_USB2 = 0x40,
        TrigInDacThresh_USB2 = 0x43,
        TrigInDacHpf_USB2 = 0x44,

        WireOutNumWordsLsb_USB2 = 0x20,
        WireOutNumWordsMsb_USB2 = 0x21
    };

    // Opal Kelly module USB interface endpoint addresses common to USB3 controller types
    enum EndPointUSB3 {
        WireInMaxTimeStep_USB3 = 0x01,

        TrigInConfig_USB3 = 0x40,
        TrigInDacConfig_USB3 = 0x42,

        WireOutNumWords_USB3 = 0x20
    };

    // Opal Kelly module USB interface endpoint addresses unique to ControllerRecordUSB2 type
    enum EndPointRecordUSB2 {
        WireInAuxCmdLength1_R_USB2 = 0x0b,
        WireInAuxCmdLength2_R_USB2 = 0x0c,
        WireInAuxCmdLength3_R_USB2 = 0x0d,
        WireInAuxCmdLoop1_R_USB2 = 0x0e,
        WireInAuxCmdLoop2_R_USB2 = 0x0f,
        WireInAuxCmdLoop3_R_USB2 = 0x10,
        WireInLedDisplay_R_USB2 = 0x11,
        WireInDataStreamSel1234_R_USB2 = 0x12,
        WireInDataStreamSel5678_R_USB2 = 0x13,

        TrigInRamWrite_R_USB2 = 0x42,
        TrigInExtFastSettle_R_USB2 = 0x45,
        TrigInExtDigOut_R_USB2 = 0x46,
    };

    // Opal Kelly module USB interface endpoint addresses unique to ControllerRecordUSB3 type
    enum EndPointRecordUSB3 {
        WireInSerialDigitalInCntl_R_USB3 = 0x02,
        WireInAuxCmdLength_R_USB3 = 0x0b,
        WireInAuxCmdLoop_R_USB3 = 0x0c,
        WireInLedDisplay_R_USB3 = 0x0d,
        WireInDacReref_R_USB3 = 0x0e,

        WireOutSerialDigitalIn_R_USB3 = 0x21
    };

    // Opal Kelly module USB interface endpoint addresses unique to ControllerStimRecord
    enum EndPointStimRecordUSB2 {
        WireInStimCmdMode_S_USB2 = 0x05,
        WireInStimRegAddr_S_USB2 = 0x06,
        WireInStimRegWord_S_USB2 = 0x07,
        WireInDcAmpConvert_S_USB2 = 0x08,
        WireInExtraStates_S_USB2 = 0x09,
        WireInDacReref_S_USB2 = 0x0a,
        WireInAuxEnable_S_USB2 = 0x0c,
        WireInGlobalSettleSelect_S_USB2 = 0x0d,
        WireInAdcThreshold_S_USB2 = 0x0f,
        WireInSerialDigitalInCntl_S_USB2 = 0x10,
        WireInLedDisplay_S_USB2 = 0x11,
        WireInManualTriggers_S_USB2 = 0x12,
        WireInTtlOutMode_S_USB2 = 0x13,

        TrigInRamAddrReset_S_USB2 = 0x42,
        TrigInAuxCmdLength_S_USB2 = 0x45,

        WireOutSerialDigitalIn_S_USB2 = 0x26,

        PipeInAuxCmd1Msw_S_USB2 = 0x80,
        PipeInAuxCmd1Lsw_S_USB2 = 0x81,
        PipeInAuxCmd2Msw_S_USB2 = 0x82,
        PipeInAuxCmd2Lsw_S_USB2 = 0x83,
        PipeInAuxCmd3Msw_S_USB2 = 0x84,
        PipeInAuxCmd3Lsw_S_USB2 = 0x85,
        PipeInAuxCmd4Msw_S_USB2 = 0x86,
        PipeInAuxCmd4Lsw_S_USB2 = 0x87
    };

    unsigned int numWordsInFifo() override;
    bool isDcmProgDone() const override;
    bool isDataClockLocked() const override;
    void forceAllDataStreamsOff() override;

    // Physical board only
    static void pulseWireIn(okCFrontPanel* dev_, int wireIn, unsigned int value);
    static int endPointWireInResetRun() { return (int)WireInResetRun; }
    static int endPointWireInSerialDigitalInCntl(bool isUSB3);
    static int endPointWireOutSerialDigitalIn(bool isUSB3);
    static int endPointWireOutBoardMode() { return (int)WireOutBoardMode; }

    int previousDelay;
};

#endif // RHXCONTROLLER_H

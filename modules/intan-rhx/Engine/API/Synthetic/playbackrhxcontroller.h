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

#ifndef PLAYBACKRHXCONTROLLER_H
#define PLAYBACKRHXCONTROLLER_H

#include "datafilereader.h"
#include "abstractrhxcontroller.h"

class PlaybackRHXController : public AbstractRHXController
{
public:
    PlaybackRHXController(ControllerType type_, AmplifierSampleRate sampleRate_, DataFileReader* dataFileReader_);
    ~PlaybackRHXController();

    bool isSynthetic() const override { return false; }
    bool isPlayback() const override { return true; }
    AcquisitionMode acquisitionMode() const override { return PlaybackMode; }
    int open(const string& /* boardSerialNumber */) override { return 1; }  // Always return 1 to emulate a successful opening.
    bool uploadFPGABitfile(const string& /* filename */) override { return true; }
    void resetBoard() override {}

    void run() override {}
    bool isRunning() override { return false; }
    void flush() override {}
    void resetFpga() override {}

    bool readDataBlock(RHXDataBlock *dataBlock) override;
    bool readDataBlocks(int numBlocks, deque<RHXDataBlock*> &dataQueue) override;
    long readDataBlocksRaw(int numBlocks, uint8_t *buffer) override;

    void setContinuousRunMode(bool) override {}
    void setMaxTimeStep(unsigned int) override {}
    void setCableDelay(BoardPort port, int delay) override;
    void setDspSettle(bool) override {}
    void setDataSource(int stream, BoardDataSource dataSource) override;
    void setTtlOut(const int*) override {}
    void setDacManual(int) override {}
    void setLedDisplay(const int*) override {}
    void setSpiLedDisplay(const int*) override {}
    void setDacGain(int) override {}
    void setAudioNoiseSuppress(int) override {}
    void setExternalFastSettleChannel(int) override {}
    void setExternalDigOutChannel(BoardPort, int) override {}
    void setDacHighpassFilter(double) override {}
    void setDacThreshold(int, int, bool) override {}
    void setTtlMode(int) override {}
    void setDacRerefSource(int, int) override {}
    void setExtraStates(unsigned int) override {}
    void setStimCmdMode(bool) override {}
    void setAnalogInTriggerThreshold(double) override {}
    void setManualStimTrigger(int, bool) override {}
    void setGlobalSettlePolicy(bool, bool, bool, bool, bool) override {}
    void setTtlOutMode(bool, bool, bool, bool, bool, bool, bool, bool) override {}
    void setAmpSettleMode(bool) override {}
    void setChargeRecoveryMode(bool) override {}
    bool setSampleRate(AmplifierSampleRate newSampleRate) override;

    void enableDataStream(int stream, bool enabled) override;
    void enableDac(int, bool) override {}
    void enableExternalFastSettle(bool) override {}
    void enableExternalDigOut(BoardPort, bool) override {}
    void enableDacHighpassFilter(bool) override {}
    void enableDacReref(bool) override {}
    void enableDcAmpConvert(bool) override {}
    void enableAuxCommandsOnAllStreams() override {}
    void enableAuxCommandsOnOneStream(int) override {}

    void selectDacDataStream(int, int) override {}
    void selectDacDataChannel(int, int) override {}
    void selectAuxCommandLength(AuxCmdSlot, int, int) override {}
    void selectAuxCommandBank(BoardPort, AuxCmdSlot, int) override {}

    int getBoardMode() override;
    int getNumSPIPorts(bool& expanderBoardDetected) override;

    void clearTtlOut() override {}
    void resetSequencers() override {}
    void programStimReg(int, int, StimRegister, int) override {}
    void uploadCommandList(const vector<unsigned int>&, AuxCmdSlot, int) override {}

    int findConnectedChips(vector<ChipType> &chipType, vector<int> &portIndex, vector<int> &commandStream,
                           vector<int> &numChannelsOnPort) override;

private:
    unsigned int numWordsInFifo() override;
    bool isDcmProgDone() const override { return true; }
    bool isDataClockLocked() const override { return true; }
    void forceAllDataStreamsOff() override {} // Used in FPGA initialization; no analog for playback.

    DataFileReader* dataFileReader;
};

#endif // PLAYBACKRHXCONTROLLER_H

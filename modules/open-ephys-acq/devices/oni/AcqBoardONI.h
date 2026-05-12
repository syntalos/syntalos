/*
    ------------------------------------------------------------------
    Copyright (C) 2024 Open Ephys
    Copyright (C) 2026 Syntalos Project

    ------------------------------------------------------------------

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "../AcquisitionBoard.h"
#include "HeadstageONI.h"
#include "ImpedanceMeterONI.h"

#include "rhythm-api/rhd2000ONIboard.h"
#include "rhythm-api/rhd2000ONIdatablock.h"
#include "rhythm-api/rhd2000ONIregisters.h"

#include "fabric/logging.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#define CHIP_ID_RHD2132     1
#define CHIP_ID_RHD2216     2
#define CHIP_ID_RHD2164     4
#define CHIP_ID_RHD2164_B   1000
#define REGISTER_59_MISO_A  53
#define REGISTER_59_MISO_B  58
#define RHD2132_16CH_OFFSET 8

#define MAX_NUM_CHANNELS (MAX_NUM_DATA_STREAMS_USB3 * 35 + 16)

/**
 * Open Ephys Acquisition Board with the Open Ephys ONI FPGA bitstream.
 *
 * https://open-ephys.org/acq-board
 */
class AcqBoardONI : public AcquisitionBoard
{
    friend class ImpedanceMeterONI;

public:
    AcqBoardONI();
    ~AcqBoardONI() override;

    bool detectBoard() override;
    bool initializeBoard() override;
    bool foundInputSource() const override;

    std::vector<const Headstage *> getHeadstages() override;
    std::vector<int> getAvailableSampleRates() override;

    void setSampleRate(int sampleRateHz) override;
    float getSampleRate() const override;

    void scanPorts() override;

    /** After a sample-rate change, re-evaluate optimum cable delays. */
    void checkAllCableDelays();

    void enableAuxChannels(bool enabled) override;
    bool areAuxChannelsEnabled() const override;
    void enableAdcChannels(bool enabled) override;
    bool areAdcChannelsEnabled() const override;

    float getBitVolts(ChannelKind kind) const override;

    void measureImpedances() override;
    void impedanceMeasurementFinished() override;

    void setNamingScheme(ChannelNamingScheme scheme) override;
    ChannelNamingScheme getNamingScheme() override;

    bool isReady() override;
    bool startAcquisition() override;
    bool stopAcquisition() override;

    bool pumpSamples(std::span<AcqSampleChunk> sinks) override;

    double setUpperBandwidth(double upperBandwidth) override;
    double setLowerBandwidth(double lowerBandwidth) override;
    double setDspCutoffFreq(double freq) override;
    double getDspCutoffFreq() const override;
    void setDspOffset(bool enabled) override;
    void setTTLOutputMode(bool enabled) override;
    void setDAChpf(float cutoff, bool enabled) override;
    void setFastTTLSettle(bool state, int channel) override;
    int setNoiseSlicerLevel(int level) override;
    void enableBoardLeds(bool enabled) override;
    int setClockDivider(int divide_ratio) override;
    void connectHeadstageChannelToDAC(int headstageChannelIndex, int dacChannelIndex) override;
    void setDACTriggerThreshold(int dacChannelIndex, float threshold) override;

    bool isHeadstageEnabled(int hsNum) const override;
    void setNumHeadstageChannels(int headstageIndex, int channelCount) override;
    int getActiveChannelsInHeadstage(int hsNum) const override;
    int getChannelsInHeadstage(int hsNum) const override;
    int getNumDataOutputs(ChannelKind kind) override;
    int getAuxChannelsPerHeadstage() const override
    {
        return 3;
    }

    int getNumAdcChannels() const override
    {
        return 8;
    }

    /** Number of BNO IMU devices currently attached. */
    int getNumBnos() const;

    /** True if the board firmware reports memory-monitor support. */
    bool getMemoryMonitorSupport() const;

private:
    void setSampleRate(int sampleRateHz, bool reScanDelays);
    bool checkBoardMem() const;

    void scanPortsInThread();
    bool initializeBoardInThread();

    void updateRegisters();
    void updateBoardStreams();

    int getIntanChipId(Rhd2000ONIDataBlock *dataBlock, int stream, int &register59Value);
    void setCableLength(int hsNum, float length);
    bool enableHeadstage(int hsNum, bool enabled, int nStr = 1, int strChans = 32, bool hasBno = false);

    int getChannelFromHeadstage(int headstageIndex, int channelIndex);
    int getHeadstageChannel(int &headstageIndex, int channelIndex) const;

    static bool CheckSemVer(int major, int minor, int patch, int targetMajor, int targetMinor, int targetPatch);

    /** Rhythm API objects. */
    std::unique_ptr<Rhd2000ONIBoard> evalBoard;
    Rhd2000ONIRegisters chipRegisters;
    std::unique_ptr<Rhd2000ONIDataBlock> dataBlock;
    std::vector<Rhd2000ONIBoard::BoardDataSource> enabledStreams;

    int numUsbBlocksToRead = 0;
    unsigned int blockSize = 0;

    /** Holds incoming sample data while interleaving into chunks. */
    float thisSample[MAX_NUM_CHANNELS];
    float auxBuffer[MAX_NUM_CHANNELS];
    float auxSamples[MAX_NUM_DATA_STREAMS_USB3][3];

    std::vector<std::unique_ptr<HeadstageONI>> headstages;

    std::vector<int> numChannelsPerDataStream;

    int MAX_NUM_HEADSTAGES = 0;
    int MAX_NUM_DATA_STREAMS = 0;

    std::vector<int> dacChannels;
    std::vector<int> dacStream;
    std::vector<int> dacThresholds;
    std::vector<bool> dacChannelsToUpdate;

    std::vector<int> chipId;

    int TTL_OUTPUT_STATE[16] = {0};

    bool deviceFound = false;
    bool isTransmitting = false;

    std::mutex oniLock;

    int deviceId = 0;

    static constexpr int DEVICE_ID_V2 = 0x0100;
    static constexpr int DEVICE_ID_V3 = 0x0102;
    static constexpr double
        v3AdcBitVal = ((((1.25 * (1 + 84.5 / 51)) / (1 << 12)) * (10 / (1.25 * (1 + 84.5 / 51)))) / 16);

    static constexpr int NUMBER_OF_PORTS = 4;
    static constexpr int BNO_CHANNELS = 3 + 3 + 4 + 3 + 1 + 4;
    static constexpr int MEMORY_MONITOR_FS = 100;

    static constexpr double eulerAngleScale = 1.0 / 16;
    static constexpr double quaternionScale = 1.0 / (1 << 14);
    static constexpr double accelerationScale = 1.0 / 100;

    int regOffset = 0;
    bool varSampleRateCapable = false;
    bool commonCommandsSet = false;
    bool initialScan = true;

    std::array<bool, NUMBER_OF_PORTS> hasBNO{};
    std::array<bool, NUMBER_OF_PORTS> hasI2c{};
    std::array<uint32_t, NUMBER_OF_PORTS> headstageId{};
    bool hasI2cSupport = false;
    bool hasMemoryMonitorSupport = false;

    uint32_t acquisitionClockHz = 0;
    uint32_t totalMemory = 0;

    int64_t dataSampleNumber = 0;
    int64_t memorySampleNumber = 0;
    std::array<int64_t, NUMBER_OF_PORTS> bnoSampleNumbers{0, 0, 0, 0};

    /** Last memory-monitor percentage we reported up to the module, to avoid spamming setStatusMessage. */
    float m_lastReportedMemPct = -1.0f;

    Syntalos::QuillLogger *m_log;
};

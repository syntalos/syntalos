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
#include "HeadstageSim.h"
#include "ImpedanceMeterSim.h"
#include "SimulatedData.h"
#include "logging.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

class AcqBoardSim : public AcquisitionBoard
{
public:
    AcqBoardSim();
    ~AcqBoardSim() override = default;

    bool detectBoard() override;
    bool initializeBoard() override;
    bool foundInputSource() const override;

    std::vector<const Headstage *> getHeadstages() override;
    std::vector<int> getAvailableSampleRates() override;

    void setSampleRate(int sampleRateHz) override;
    float getSampleRate() const override;

    void scanPorts() override;

    void enableAuxChannels(bool enabled) override;
    bool areAuxChannelsEnabled() const override;
    void enableAdcChannels(bool enabled) override;
    bool areAdcChannelsEnabled() const override;

    float getBitVolts(ChannelKind kind) const override;

    void measureImpedances() override;
    void impedanceMeasurementFinished() override;
    void saveImpedances(const std::filesystem::path &file) override;

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

    static constexpr int MAX_NUM_HEADSTAGES = 8;
    static constexpr int SAMPLES_PER_PUMP = 128;

private:
    bool enableHeadstage(int hsNum, bool enabled, int nStr = 1, int strChans = 32);

    /** Convert a microvolt sample to offset-binary uint16 (1 LSB = 0.195 µV). */
    static uint16_t uvToCount(float uv)
    {
        constexpr float lsb = 0.195f;
        const float counts = uv / lsb + 32768.0f;
        if (counts <= 0.0f)
            return 0;
        if (counts >= 65535.0f)
            return 65535;
        return static_cast<uint16_t>(counts + 0.5f);
    }

    Syntalos::QuillLogger *m_log;

    SimulatedData data;
    std::vector<std::unique_ptr<HeadstageSim>> headstages;

    bool isAcquiring = false;
    uint64_t sampleNumber = 0;
    std::chrono::steady_clock::time_point acqStart;
};

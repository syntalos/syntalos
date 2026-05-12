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

#include "AcqBoardSim.h"

#include "fabric/logging.h"

#include <thread>

AcqBoardSim::AcqBoardSim()
    : AcquisitionBoard()
{
    m_log = Syntalos::getLogger("oeacq.board-sim");

    boardType = BoardType::Simulated;
    impedanceMeter = std::make_unique<ImpedanceMeterSim>();

    headstages.reserve(MAX_NUM_HEADSTAGES);
    for (int i = 0; i < MAX_NUM_HEADSTAGES; ++i)
        headstages.push_back(std::make_unique<HeadstageSim>(i));
}

bool AcqBoardSim::detectBoard()
{
    return true;
}

bool AcqBoardSim::initializeBoard()
{
    return true;
}

bool AcqBoardSim::foundInputSource() const
{
    return true;
}

std::vector<const Headstage *> AcqBoardSim::getHeadstages()
{
    std::vector<const Headstage *> connected;
    connected.reserve(headstages.size());
    for (const auto &hs : headstages) {
        if (hs->isConnected())
            connected.push_back(hs.get());
    }
    return connected;
}

std::vector<int> AcqBoardSim::getAvailableSampleRates()
{
    return {1000, 15000, 30000};
}

void AcqBoardSim::setSampleRate(int sampleRateHz)
{
    LOG_DEBUG(m_log, "Simulated acquisition board setting sample rate to {} Hz.", sampleRateHz);
    settings.boardSampleRate = static_cast<float>(sampleRateHz);
}

float AcqBoardSim::getSampleRate() const
{
    return settings.boardSampleRate;
}

void AcqBoardSim::scanPorts()
{
    for (int i = 0; i < MAX_NUM_HEADSTAGES; ++i)
        enableHeadstage(i, i == 0);
}

bool AcqBoardSim::enableHeadstage(int hsNum, bool enabled, int nStr, int strChans)
{
    LOG_DEBUG(m_log, "Headstage {}, enabled: {}, num streams: {}, stream channels: {}", hsNum, enabled, nStr, strChans);

    if (enabled) {
        headstages[hsNum]->setFirstChannel(getNumDataOutputs(ChannelKind::Electrode));
        headstages[hsNum]->setChannelCount(nStr * strChans);
    } else {
        headstages[hsNum]->setFirstChannel(-1);
        headstages[hsNum]->setChannelCount(0);
    }
    return true;
}

void AcqBoardSim::enableAuxChannels(bool enabled)
{
    settings.acquireAux = enabled;
}

bool AcqBoardSim::areAuxChannelsEnabled() const
{
    return settings.acquireAux;
}

void AcqBoardSim::enableAdcChannels(bool enabled)
{
    settings.acquireAdc = enabled;
}

bool AcqBoardSim::areAdcChannelsEnabled() const
{
    return settings.acquireAdc;
}

float AcqBoardSim::getBitVolts(ChannelKind kind) const
{
    switch (kind) {
    case ChannelKind::Electrode:
        return 0.195f;
    case ChannelKind::Aux:
        return 0.0000374f;
    case ChannelKind::Adc:
        return 0.00015258789f;
    }
    return 1.0f;
}

void AcqBoardSim::measureImpedances()
{
    if (impedanceMeter) {
        impedanceMeter->resetStopRequest();
        impedanceMeter->runImpedanceMeasurement(impedances);
    }
}

void AcqBoardSim::impedanceMeasurementFinished() {}

void AcqBoardSim::setNamingScheme(ChannelNamingScheme scheme)
{
    channelNamingScheme = scheme;
    for (const auto &hs : headstages)
        hs->setNamingScheme(scheme);
}

ChannelNamingScheme AcqBoardSim::getNamingScheme()
{
    return channelNamingScheme;
}

bool AcqBoardSim::isReady()
{
    return true;
}

bool AcqBoardSim::startAcquisition()
{
    isAcquiring = true;
    sampleNumber = 0;
    acqStart = std::chrono::steady_clock::now();
    return true;
}

bool AcqBoardSim::stopAcquisition()
{
    isAcquiring = false;
    return true;
}

bool AcqBoardSim::pumpSamples(std::span<AcqSampleChunk> sinks)
{
    if (!isAcquiring)
        return false;

    const float sampleRate = settings.boardSampleRate > 0.0f ? settings.boardSampleRate : 30000.0f;
    constexpr int N = SAMPLES_PER_PUMP;

    const int availableSpikeSamples = static_cast<int>(data.spikes.size());
    const int availableSineSamples = static_cast<int>(data.sine_wave.size());
    const int availableAdcSamples = static_cast<int>(data.adc.size());
    const int skip = std::max(1, static_cast<int>(30000.0f / sampleRate));

    // Decide once per pump whether the optional channel kinds will produce
    // data, so disabled chunks can be left empty.
    const auto wantAux = settings.acquireAux;
    const auto wantAdc = settings.acquireAdc;

    for (auto &sink : sinks) {
        const bool active = (sink.kind == ChannelKind::Electrode) || (sink.kind == ChannelKind::Aux && wantAux)
                            || (sink.kind == ChannelKind::Adc && wantAdc);
        if (!active) {
            sink.numSamples = 0;
            continue;
        }
        sink.numSamples = N;
        const std::size_t needed = static_cast<std::size_t>(N) * static_cast<std::size_t>(sink.channelsPerSample);
        if (sink.samples.size() < needed)
            sink.samples.resize(needed);
        if (sink.sampleIndices.size() < static_cast<std::size_t>(N))
            sink.sampleIndices.resize(N);
    }

    for (int s = 0; s < N; ++s) {
        const uint64_t idx = sampleNumber + static_cast<uint64_t>(s);

        // Pre-pull one value per source for this sample tick.
        const float spikeUv = data.spikes[(idx * skip) % availableSpikeSamples];
        const uint16_t spikeCount = uvToCount(spikeUv);
        const float sineSample = data.sine_wave[(idx * skip) % availableSineSamples] * 0.01f;
        const uint16_t sineCount = uvToCount(sineSample);
        const float adcSample = data.adc[(idx * skip) % availableAdcSamples];
        const uint16_t adcCount = uvToCount(adcSample);

        for (auto &sink : sinks) {
            if (sink.numSamples != N)
                continue;
            sink.sampleIndices[s] = idx;
            uint16_t *row = sink.samples.data() + static_cast<std::size_t>(s) * sink.channelsPerSample;
            const int cps = sink.channelsPerSample;
            switch (sink.kind) {
            case ChannelKind::Electrode:
                for (int c = 0; c < cps; ++c)
                    row[c] = spikeCount;
                break;
            case ChannelKind::Aux:
                for (int c = 0; c < cps; ++c)
                    row[c] = sineCount;
                break;
            case ChannelKind::Adc:
                for (int c = 0; c < cps; ++c)
                    row[c] = adcCount;
                break;
            }
        }
    }
    sampleNumber += N;

    // Throttle to roughly real-time so downstream subscribers aren't drowned.
    const auto target = acqStart
                        + std::chrono::microseconds(
                            static_cast<int64_t>((sampleNumber * 1'000'000ULL) / static_cast<uint64_t>(sampleRate)));
    const auto now = std::chrono::steady_clock::now();
    if (target > now)
        std::this_thread::sleep_for(target - now);

    drainElapsedDigitalDeadlines();
    // Sim board ignores actual digital output state; just drain the queue.
    while (!m_digitalOutputCommands.empty())
        m_digitalOutputCommands.pop();

    return true;
}

double AcqBoardSim::setUpperBandwidth(double upperBandwidth)
{
    settings.analogFilter.upperBandwidth = upperBandwidth;
    return upperBandwidth;
}

double AcqBoardSim::setLowerBandwidth(double lowerBandwidth)
{
    settings.analogFilter.lowerBandwidth = lowerBandwidth;
    return lowerBandwidth;
}

double AcqBoardSim::setDspCutoffFreq(double freq)
{
    settings.dsp.cutoffFreq = freq;
    return freq;
}

double AcqBoardSim::getDspCutoffFreq() const
{
    return settings.dsp.cutoffFreq;
}

void AcqBoardSim::setDspOffset(bool enabled)
{
    settings.dsp.enabled = enabled;
}

void AcqBoardSim::setTTLOutputMode(bool enabled)
{
    settings.ttlOutputMode = enabled;
}

void AcqBoardSim::setDAChpf(float cutoff, bool /*enabled*/)
{
    settings.desiredDAChpf = cutoff;
}

void AcqBoardSim::setFastTTLSettle(bool state, int channel)
{
    settings.fastSettleEnabled = state;
    settings.fastSettleTTLChannel = channel;
}

int AcqBoardSim::setNoiseSlicerLevel(int level)
{
    settings.noiseSlicerLevel = level;
    return level;
}

void AcqBoardSim::enableBoardLeds(bool enabled)
{
    settings.ledsEnabled = enabled;
}

int AcqBoardSim::setClockDivider(int /*divide_ratio*/)
{
    return 1;
}

void AcqBoardSim::connectHeadstageChannelToDAC(int /*hsCh*/, int /*dacCh*/) {}
void AcqBoardSim::setDACTriggerThreshold(int /*dacCh*/, float /*threshold*/) {}

bool AcqBoardSim::isHeadstageEnabled(int hsNum) const
{
    return headstages[hsNum]->isConnected();
}

int AcqBoardSim::getActiveChannelsInHeadstage(int hsNum) const
{
    return headstages[hsNum]->getNumActiveChannels();
}

int AcqBoardSim::getChannelsInHeadstage(int hsNum) const
{
    return headstages[hsNum]->isConnected() ? 32 : 0;
}

int AcqBoardSim::getNumDataOutputs(ChannelKind kind)
{
    switch (kind) {
    case ChannelKind::Electrode: {
        int total = 0;
        for (const auto &hs : headstages) {
            if (hs->isConnected())
                total += hs->getNumActiveChannels();
        }
        return total;
    }
    case ChannelKind::Aux: {
        if (!settings.acquireAux)
            return 0;
        int total = 0;
        for (const auto &hs : headstages) {
            if (hs->isConnected())
                total += 3;
        }
        return total;
    }
    case ChannelKind::Adc:
        return settings.acquireAdc ? 8 : 0;
    }
    return 0;
}

void AcqBoardSim::setNumHeadstageChannels(int hsNum, int numChannels)
{
    headstages[hsNum]->setChannelCount(numChannels);
}

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

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <span>
#include <string>
#include <vector>

#include "Headstage.h"
#include "ImpedanceMeter.h"

#include "datactl/timesync.h"

using namespace Syntalos;

/**
 * Type of continuous channel.
 */
enum class ChannelKind {
    Electrode,
    Aux,
    Adc,
    TtlIn
};

/** Instructions for setting digital output. */
struct DigitalOutputCommand {
    int ttlLine = -1;
    bool state = false;
};

/**
 * Per-headstage / per-stream sink that the board fills with raw uint16 samples
 * during acquisition. The owning module hands a span of these to
 * @ref AcquisitionBoard::pumpSamples and consumes the buffers afterwards.
 *
 * Layout: @c samples is a row-major matrix of size @c numSamples * @c
 * channelsPerSample (column-major would be cheaper for downstream Eigen, but
 * the ONI rhythm-api emits row-major so we stay with that and let the module
 * transpose into its SignalBlockU16 if needed).
 */
struct AcqSampleChunk {
    int groupIndex = -1;                       ///< headstage / stream index, or -1 for board-wide kinds
    ChannelKind kind = ChannelKind::Electrode; ///< what kind of channels this chunk carries
    int channelsPerSample = 0;                 ///< columns in @c samples
    int numSamples = 0;                        ///< rows in @c samples (set by the board)
    std::vector<uint16_t> samples;
    std::vector<uint64_t> sampleIndices; ///< per-sample monotonic index from board
};

/**
 * Abstract interface for any type of Open Ephys Acquisition Board.
 *
 * https://open-ephys.org/acq-board
 */
class AcquisitionBoard
{
public:
    enum class BoardType {
        None = 0,
        Simulated = 1,
        ONI = 3
    };

    /** Severity hint for messages bubbled up to the host module. */
    enum class MessageSeverity {
        Info,    ///< informational; can be shown as status / a friendly notification
        Warning, ///< user attention warranted (e.g. firmware should be updated)
        Error,   ///< operation failed or will fail; module should raiseError()
    };

    /**
     * Callback the module installs so the board can surface human-facing
     * messages (firmware notes, status updates, errors). The string may
     * contain a URL on a separate line. Always invoked from the thread that
     * triggered the underlying board call — the module is responsible for
     * marshaling onto the GUI thread if needed.
     */
    using MessageCallback = std::function<void(MessageSeverity, const std::string &)>;

    void setMessageCallback(MessageCallback cb)
    {
        m_messageCb = std::move(cb);
    }

    AcquisitionBoard() = default;
    virtual ~AcquisitionBoard() = default;

    AcquisitionBoard(const AcquisitionBoard &) = delete;
    AcquisitionBoard &operator=(const AcquisitionBoard &) = delete;

    // ---------------- detection / lifetime ----------------

    /** Detect whether a board is present. */
    virtual bool detectBoard() = 0;

    /** Initialize the board after successful detection (synchronous). */
    virtual bool initializeBoard() = 0;

    /** True if the device is connected. */
    virtual bool foundInputSource() const = 0;

    /** Connected headstages for this board. */
    virtual std::vector<const Headstage *> getHeadstages() = 0;

    /** Available sample rates (Hz). */
    virtual std::vector<int> getAvailableSampleRates() = 0;

    virtual void setSampleRate(int sampleRateHz) = 0;
    virtual float getSampleRate() const = 0;

    /** Synchronously scan ports; updates the headstage list. */
    virtual void scanPorts() = 0;

    virtual void enableAuxChannels(bool enabled) = 0;
    virtual bool areAuxChannelsEnabled() const = 0;

    virtual void enableAdcChannels(bool enabled) = 0;
    virtual bool areAdcChannelsEnabled() const = 0;

    virtual void enableTtlInChannels(bool enabled) = 0;
    virtual bool areTtlInChannelsEnabled() const = 0;

    /** bitVolts scaling (microvolts per LSB) for each channel kind. */
    virtual float getBitVolts(ChannelKind kind) const = 0;

    /**
     * Affine mapping from a stored raw offset-binary uint16 sample to its physical
     * value: physical = scale * raw + offset (this is exactly how consumers such as
     * the time-series plot and writers reconstruct the signal).
     */
    struct DataScaling {
        double scale;
        double offset;
    };

    /**
     * Scale/offset that for physical conversion for @p kind.
     *
     * The samples we store are raw offset-binary counts (mid-scale 32768). The original
     * code converts electrode/aux as (raw - 32768) * bitVolts, which is the default here.
     * Boards whose ADC (or other) conversion has extra affine terms override this.
     */
    virtual DataScaling getDataScaling(ChannelKind kind) const
    {
        // TTL inputs are raw 0/1 line states; the stored value is already the
        // physical value, so no offset-binary mid-scale conversion applies.
        if (kind == ChannelKind::TtlIn)
            return {1.0, 0.0};

        const double bitVolts = getBitVolts(kind);
        return {bitVolts, -32768.0 * bitVolts};
    }

    // ---------------- impedances ----------------

    virtual void measureImpedances() = 0;
    virtual void impedanceMeasurementFinished() = 0;

    /** Forward a progress callback to the underlying impedance meter, if any. */
    void setImpedanceProgressCallback(ImpedanceMeter::ProgressCallback cb)
    {
        if (impedanceMeter)
            impedanceMeter->setProgressCallback(std::move(cb));
    }

    /** Ask any in-progress impedance scan to abort at the next safe point. */
    void requestImpedanceStop()
    {
        if (impedanceMeter)
            impedanceMeter->requestStop();
    }

    /** Latest impedance measurement result (only meaningful after a successful run). */
    const Impedances &getImpedances() const
    {
        return impedances;
    }

    virtual void setNamingScheme(ChannelNamingScheme scheme) = 0;
    virtual ChannelNamingScheme getNamingScheme() = 0;

    // ---------------- acquisition ----------------

    virtual bool isReady() = 0;

    void setSyncTimer(std::shared_ptr<SyncTimer> syTimer)
    {
        m_syTimer = std::move(syTimer);
    }

    /**
     * Start streaming samples from the board.
     *
     * @p acqStartTimestamp is set to the master-clock time captured as close as
     * possible to the instant acquisition is actually commanded to start (i.e.
     * immediately before the hardware "run" command).
     */
    virtual bool startAcquisition(microseconds_t &acqStartTimestamp) = 0;

    /** Stop streaming. */
    virtual bool stopAcquisition() = 0;

    /**
     * Pull one chunk of samples from the board into the supplied per-group
     * sinks. Implementations may block until a USB datablock arrives. Each
     * span entry corresponds to one headstage / stream; the board fills @c
     * numSamples and @c samples in place.
     *
     * The returned @p blockAcqTimestamp is the best guesstimate of the master-clock
     * time at which the *last* sample of the chunk was physically acquired by the
     * hardware (i.e. on-board buffer latency has been subtracted out where the
     * board can observe it).
     *
     * @return true if a chunk was produced, false on stop / error.
     */
    virtual bool pumpSamples(std::span<AcqSampleChunk> sinks, microseconds_t &blockAcqTimestamp) = 0;

    // ---------------- analog / DSP / DAC settings ----------------

    virtual double setUpperBandwidth(double upperBandwidth) = 0;
    virtual double setLowerBandwidth(double lowerBandwidth) = 0;

    virtual double setDspCutoffFreq(double freq) = 0;
    virtual double getDspCutoffFreq() const = 0;
    virtual void setDspOffset(bool enabled) = 0;

    virtual void setTTLOutputMode(bool enabled) = 0;
    virtual void setDAChpf(float cutoff, bool enabled) = 0;
    virtual void setFastTTLSettle(bool state, int channel) = 0;

    virtual int setNoiseSlicerLevel(int level) = 0;
    virtual void enableBoardLeds(bool enabled) = 0;
    virtual int setClockDivider(int divide_ratio) = 0;

    virtual void connectHeadstageChannelToDAC(int headstageChannelIndex, int dacChannelIndex) = 0;
    virtual void setDACTriggerThreshold(int dacChannelIndex, float threshold) = 0;

    // ---------------- headstage layout ----------------

    virtual bool isHeadstageEnabled(int hsNum) const = 0;
    virtual void setNumHeadstageChannels(int headstageIndex, int channelCount) = 0;
    virtual int getActiveChannelsInHeadstage(int hsNum) const = 0;
    virtual int getChannelsInHeadstage(int hsNum) const = 0;

    /** Total number of continuous channels of the given kind across all headstages. */
    virtual int getNumDataOutputs(ChannelKind kind) = 0;

    /** Hardware-fixed AUX channel count per headstage. */
    virtual int getAuxChannelsPerHeadstage() const = 0;

    /** Hardware-fixed board-wide ADC channel count. */
    virtual int getNumAdcChannels() const = 0;

    /** Hardware-fixed board-wide TTL input line count. */
    virtual int getNumTtlInChannels() const = 0;

    int getNumChannels()
    {
        return getNumDataOutputs(ChannelKind::Electrode) + getNumDataOutputs(ChannelKind::Aux)
               + getNumDataOutputs(ChannelKind::Adc);
    }

    // ---------------- digital output (timer-free) ----------------

    /**
     * Enqueue a digital output pulse: the line is set high immediately and
     * scheduled to drop low after @p eventDurationMs has elapsed. The board
     * implementation drains the rise queue from inside @ref pumpSamples and
     * the fall queue from @ref drainElapsedDigitalDeadlines.
     */
    void triggerDigitalOutput(int ttlLine, int eventDurationMs)
    {
        const std::lock_guard guard(m_digitalMutex);
        m_digitalOutputCommands.push({ttlLine, true});
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(eventDurationMs);
        m_digitalDeadlines.push_back({ttlLine, deadline});
    }

    /**
     * Pop any pending digital deadlines that have elapsed and append matching
     * "line low" commands. Called by the module's run loop alongside
     * @ref pumpSamples.
     */
    void drainElapsedDigitalDeadlines()
    {
        const auto now = std::chrono::steady_clock::now();
        const std::lock_guard guard(m_digitalMutex);
        for (auto it = m_digitalDeadlines.begin(); it != m_digitalDeadlines.end();) {
            if (it->deadline <= now) {
                m_digitalOutputCommands.push({it->ttlLine, false});
                it = m_digitalDeadlines.erase(it);
            } else {
                ++it;
            }
        }
    }

    BoardType getBoardType() const
    {
        return boardType;
    }

protected:
    /** Board settings. */
    struct OptimumDelay {
        float portA = -1, portB = -1, portC = -1, portD = -1;
        float portE = -1, portF = -1, portG = -1, portH = -1;
    };

    struct CableLength {
        float portA = 0.914f, portB = 0.914f, portC = 0.914f, portD = 0.914f;
        float portE = 0.914f, portF = 0.914f, portG = 0.914f, portH = 0.914f;
    };

    struct Dsp {
        bool enabled = true;
        double cutoffFreq = 0.5;
    };

    struct AnalogFilter {
        double upperBandwidth = 7500.0;
        double lowerBandwidth = 1.0;
    };

    struct Settings {
        bool acquireAux = false;
        bool acquireAdc = false;
        bool acquireTtlIn = false;

        bool fastSettleEnabled = false;
        bool fastTTLSettleEnabled = false;
        int fastSettleTTLChannel = -1;
        bool ttlOutputMode = false;

        Dsp dsp;
        AnalogFilter analogFilter;

        int noiseSlicerLevel = 0;

        bool desiredDAChpfState = false;
        double desiredDAChpf = 0.0;
        float boardSampleRate = 30000.0f;

        CableLength cableLength;
        OptimumDelay optimumDelay;

        int audioOutputL = -1;
        int audioOutputR = -1;
        bool ledsEnabled = true;
        bool newScan = true;
        int numberingScheme = 1;
        std::uint16_t clockDivideFactor = 0;
    } settings;

    /** Impedance meter. */
    std::unique_ptr<ImpedanceMeter> impedanceMeter;

    /** Impedance results. */
    Impedances impedances;

    /** Channel naming scheme. */
    ChannelNamingScheme channelNamingScheme = GLOBAL_INDEX;

    /** Pending digital-output commands consumed by the board impl. */
    std::queue<DigitalOutputCommand> m_digitalOutputCommands;

    /** True if a settings change is pending while acquisition is active. */
    bool updateSettingsDuringAcquisition = false;

    BoardType boardType = BoardType::None;

    std::shared_ptr<SyncTimer> m_syTimer;

    void emitMessage(MessageSeverity sev, const std::string &message) const
    {
        if (m_messageCb)
            m_messageCb(sev, message);
    }

private:
    struct DigitalDeadline {
        int ttlLine;
        std::chrono::steady_clock::time_point deadline;
    };

    std::mutex m_digitalMutex;
    std::vector<DigitalDeadline> m_digitalDeadlines;

    MessageCallback m_messageCb;
};

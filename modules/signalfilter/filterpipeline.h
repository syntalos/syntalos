/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

/**
 * @file filterpipeline.h
 * @brief IIR filtering core for the signalfilter module.
 *
 * This is intentionally free of any Qt or Syntalos dependencies so that all
 * of the (heavily templated) iir1 instantiations live in one translation unit
 * and the DSP can be unit-tested in isolation.
 */

/// Maximum filter order any single pole-filter stage may use.
/// Band-pass/-stop stages internally reserve twice this many biquads.
constexpr int MAX_FILTER_ORDER = 12;

/// Maximum number of second-order-sections a Custom (SOS) stage may hold.
constexpr int MAX_SOS_SECTIONS = 16;

/**
 * @brief Filter design family.
 */
enum class FilterFamily {
    Butterworth = 0, ///< maximally flat passband, the ephys default
    ChebyshevI,      ///< steeper rolloff, passband ripple
    ChebyshevII,     ///< steeper rolloff, flat passband, stopband ripple
    RbjNotch,        ///< 2nd-order Q-based notch (mains hum removal)
    CustomSOS        ///< raw scipy second-order-sections, pasted by the user
};

/**
 * @brief Frequency response of a (pole-) filter family.
 *
 * Ignored for RbjNotch (always a notch) and CustomSOS (defined by coefficients).
 */
enum class FilterResponse {
    LowPass = 0,
    HighPass,
    BandPass,
    BandStop
};

/**
 * @brief Serializable description of a single filter stage.
 *
 * Only the fields relevant to the chosen @ref family / @ref response are used;
 * the rest keep sensible defaults so the GUI can switch types without losing
 * previously entered values.
 */
struct FilterStage {
    FilterFamily family = FilterFamily::Butterworth;
    FilterResponse response = FilterResponse::HighPass;

    int order = 2; ///< pole-filter order (1..MAX_FILTER_ORDER)

    double freq1 = 300.0; ///< cutoff frequency (Hz), or band center for band types
    double freq2 = 10.0;  ///< band width (Hz), BandPass/BandStop only

    double rippleDb = 1.0;    ///< passband ripple (ChebyshevI)
    double stopbandDb = 60.0; ///< stopband attenuation (ChebyshevII)
    double qFactor = 20.0;    ///< notch quality factor (RbjNotch)

    /// Second-order-sections for CustomSOS, scipy order {b0,b1,b2,a0,a1,a2}.
    std::vector<std::array<double, 6>> sos;

    /// For CustomSOS: whether the user's coefficient input parsed cleanly
    bool sosValid = true;

    /// True if this stage needs a sample rate (everything except CustomSOS).
    [[nodiscard]] bool needsSampleRate() const
    {
        return family != FilterFamily::CustomSOS;
    }
};

/**
 * @brief Type-erased single-channel IIR filter with internal state.
 *
 * iir1 keeps its delay-line state inside the filter object, so one instance is
 * required per channel and per stage.
 */
class IStageFilter
{
public:
    virtual ~IStageFilter();
    /// Process a single sample, advancing the internal state.
    virtual double process(double x) = 0;
    /// Zero the delay lines, keeping the coefficients.
    virtual void reset() = 0;
};

/**
 * @brief Build a single-channel filter realising @p stage at @p sampleRate.
 * @throws std::invalid_argument on invalid parameters (e.g. order too high).
 * @return never null on success.
 */
std::unique_ptr<IStageFilter> makeStageFilter(const FilterStage &stage, double sampleRate);

/**
 * @brief A chain of filter stages with independent per-channel state.
 *
 * Configure once with the stage list and sample rate, then @ref build for a
 * concrete channel count and selection mask. @ref processSample applies the
 * whole chain to channels that are selected and passes others through
 * unchanged.
 */
class FilterPipeline
{
public:
    FilterPipeline() = default;

    void setStages(std::vector<FilterStage> stages)
    {
        m_stages = std::move(stages);
        m_channelCount = -1; // force rebuild
    }

    const std::vector<FilterStage> &stages() const
    {
        return m_stages;
    }

    void setSampleRate(double sampleRate)
    {
        m_sampleRate = sampleRate;
        m_channelCount = -1;
    }

    /// True if any configured stage requires a valid sample rate.
    [[nodiscard]] bool needsSampleRate() const;

    /// True if the pipeline is currently built for exactly @p channelCount channels.
    [[nodiscard]] bool isBuiltFor(int channelCount) const
    {
        return m_channelCount == channelCount;
    }

    /**
     * @brief (Re)build per-channel filter instances for @p channelCount channels.
     * @param channelMask one bool per channel; channels with false are bypassed.
     *        An empty mask means "all channels".
     * @param error optional, receives a human-readable message on failure.
     * @return false on failure (the pipeline is then left unbuilt).
     */
    bool build(int channelCount, const std::vector<bool> &channelMask, std::string *error = nullptr);

    /// Reset all delay lines without rebuilding the coefficients.
    void reset();

    /**
     * @brief Filter a single sample of a given channel.
     *
     * Hot path: called once per sample per channel. If @p ch is unselected (or
     * out of range), the input is returned unchanged.
     */
    inline double processSample(int ch, double x)
    {
        if (ch < 0 || ch >= static_cast<int>(m_channelFilters.size()))
            return x;
        auto &chain = m_channelFilters[static_cast<size_t>(ch)];
        for (auto &f : chain)
            x = f->process(x);

        return x;
    }

private:
    std::vector<FilterStage> m_stages;
    double m_sampleRate = -1.0;
    int m_channelCount = -1;
    // [channel][stage]; an empty inner vector means the channel is bypassed.
    std::vector<std::vector<std::unique_ptr<IStageFilter>>> m_channelFilters;
};

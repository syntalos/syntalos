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

#include "filterpipeline.h"

#include <Iir.h>
#include <stdexcept>

IStageFilter::~IStageFilter() = default;

// Transposed Direct Form II is numerically better behaved than the default
// DirectFormII at the low normalised cutoffs common in electrophysiology.
using StateT = Iir::TransposedDirectFormII;

/**
 * @brief Concrete wrapper holding one iir1 filter object (with its state).
 */
template<typename IirT>
class StageFilterT final : public IStageFilter
{
public:
    IirT inner;

    double process(double x) override
    {
        return inner.filter(x);
    }

    void reset() override
    {
        inner.reset();
    }
};

template<typename IirT>
std::unique_ptr<StageFilterT<IirT>> makeWrapper()
{
    return std::make_unique<StageFilterT<IirT>>();
}

static std::unique_ptr<IStageFilter> makeButterworth(const FilterStage &st, double fs, int order)
{
    using namespace Iir::Butterworth;
    switch (st.response) {
    case FilterResponse::LowPass: {
        auto f = makeWrapper<LowPass<MAX_FILTER_ORDER, StateT>>();
        f->inner.setup(order, fs, st.freq1);
        return f;
    }
    case FilterResponse::HighPass: {
        auto f = makeWrapper<HighPass<MAX_FILTER_ORDER, StateT>>();
        f->inner.setup(order, fs, st.freq1);
        return f;
    }
    case FilterResponse::BandPass: {
        auto f = makeWrapper<BandPass<MAX_FILTER_ORDER, StateT>>();
        f->inner.setup(order, fs, st.freq1, st.freq2);
        return f;
    }
    case FilterResponse::BandStop: {
        auto f = makeWrapper<BandStop<MAX_FILTER_ORDER, StateT>>();
        f->inner.setup(order, fs, st.freq1, st.freq2);
        return f;
    }
    }

    throw std::invalid_argument("unknown filter response");
}

static std::unique_ptr<IStageFilter> makeChebyshevI(const FilterStage &st, double fs, int order)
{
    using namespace Iir::ChebyshevI;
    switch (st.response) {
    case FilterResponse::LowPass: {
        auto f = makeWrapper<LowPass<MAX_FILTER_ORDER, StateT>>();
        f->inner.setup(order, fs, st.freq1, st.rippleDb);
        return f;
    }
    case FilterResponse::HighPass: {
        auto f = makeWrapper<HighPass<MAX_FILTER_ORDER, StateT>>();
        f->inner.setup(order, fs, st.freq1, st.rippleDb);
        return f;
    }
    case FilterResponse::BandPass: {
        auto f = makeWrapper<BandPass<MAX_FILTER_ORDER, StateT>>();
        f->inner.setup(order, fs, st.freq1, st.freq2, st.rippleDb);
        return f;
    }
    case FilterResponse::BandStop: {
        auto f = makeWrapper<BandStop<MAX_FILTER_ORDER, StateT>>();
        f->inner.setup(order, fs, st.freq1, st.freq2, st.rippleDb);
        return f;
    }
    }

    throw std::invalid_argument("unknown filter response");
}

static std::unique_ptr<IStageFilter> makeChebyshevII(const FilterStage &st, double fs, int order)
{
    using namespace Iir::ChebyshevII;
    switch (st.response) {
    case FilterResponse::LowPass: {
        auto f = makeWrapper<LowPass<MAX_FILTER_ORDER, StateT>>();
        f->inner.setup(order, fs, st.freq1, st.stopbandDb);
        return f;
    }
    case FilterResponse::HighPass: {
        auto f = makeWrapper<HighPass<MAX_FILTER_ORDER, StateT>>();
        f->inner.setup(order, fs, st.freq1, st.stopbandDb);
        return f;
    }
    case FilterResponse::BandPass: {
        auto f = makeWrapper<BandPass<MAX_FILTER_ORDER, StateT>>();
        f->inner.setup(order, fs, st.freq1, st.freq2, st.stopbandDb);
        return f;
    }
    case FilterResponse::BandStop: {
        auto f = makeWrapper<BandStop<MAX_FILTER_ORDER, StateT>>();
        f->inner.setup(order, fs, st.freq1, st.freq2, st.stopbandDb);
        return f;
    }
    }

    throw std::invalid_argument("unknown filter response");
}

static std::unique_ptr<IStageFilter> makeCustomSos(const FilterStage &st)
{
    if (st.sos.empty())
        throw std::invalid_argument(
            "Custom (SOS) filter has no valid coefficients (expected lines of 6 numbers: b0 b1 b2 a0 a1 a2)");

    // The number of sections is a compile-time template parameter, so we always
    // instantiate the maximum and pad unused sections with the identity biquad
    // {b0,b1,b2,a0,a1,a2} = {1,0,0,1,0,0} (transfer function H(z) = 1).
    double coeffs[MAX_SOS_SECTIONS][6];
    for (int i = 0; i < MAX_SOS_SECTIONS; ++i) {
        coeffs[i][0] = 1.0;
        coeffs[i][1] = 0.0;
        coeffs[i][2] = 0.0;
        coeffs[i][3] = 1.0;
        coeffs[i][4] = 0.0;
        coeffs[i][5] = 0.0;
    }

    if (static_cast<int>(st.sos.size()) > MAX_SOS_SECTIONS)
        throw std::invalid_argument("too many second-order-sections (max " + std::to_string(MAX_SOS_SECTIONS) + ")");

    const int n = static_cast<int>(st.sos.size());
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < 6; ++j)
            coeffs[i][j] = st.sos[static_cast<size_t>(i)][static_cast<size_t>(j)];

    auto f = makeWrapper<Iir::Custom::SOSCascade<MAX_SOS_SECTIONS, StateT>>();
    f->inner.setup(coeffs);

    return f;
}

std::unique_ptr<IStageFilter> makeStageFilter(const FilterStage &stage, double sampleRate)
{
    if (stage.needsSampleRate() && !(sampleRate > 0.0))
        throw std::invalid_argument("a valid sample rate is required for this filter");

    int order = stage.order;
    if (order < 1)
        order = 1;
    if (order > MAX_FILTER_ORDER)
        throw std::invalid_argument("filter order exceeds maximum of " + std::to_string(MAX_FILTER_ORDER));

    switch (stage.family) {
    case FilterFamily::Butterworth:
        return makeButterworth(stage, sampleRate, order);
    case FilterFamily::ChebyshevI:
        return makeChebyshevI(stage, sampleRate, order);
    case FilterFamily::ChebyshevII:
        return makeChebyshevII(stage, sampleRate, order);
    case FilterFamily::RbjNotch: {
        auto f = makeWrapper<Iir::RBJ::IIRNotch>();
        f->inner.setup(sampleRate, stage.freq1, stage.qFactor);
        return f;
    }
    case FilterFamily::CustomSOS:
        return makeCustomSos(stage);
    }

    throw std::invalid_argument("unknown filter family");
}

bool FilterPipeline::needsSampleRate() const
{
    for (const auto &st : m_stages)
        if (st.needsSampleRate())
            return true;
    return false;
}

bool FilterPipeline::build(int channelCount, const std::vector<bool> &channelMask, std::string *error)
{
    m_channelCount = -1;
    m_channelFilters.clear();

    if (channelCount <= 0) {
        if (error)
            *error = "invalid channel count";
        return false;
    }

    try {
        m_channelFilters.resize(static_cast<size_t>(channelCount));
        for (int ch = 0; ch < channelCount; ++ch) {
            const bool selected = channelMask.empty()
                                  || (ch < static_cast<int>(channelMask.size())
                                      && channelMask[static_cast<size_t>(ch)]);
            if (!selected)
                continue; // leave an empty chain -> bypass

            auto &chain = m_channelFilters[static_cast<size_t>(ch)];
            chain.reserve(m_stages.size());
            for (const auto &st : m_stages)
                chain.push_back(makeStageFilter(st, m_sampleRate));
        }
    } catch (const std::exception &e) {
        m_channelFilters.clear();
        if (error)
            *error = e.what();
        return false;
    }

    m_channelCount = channelCount;
    return true;
}

void FilterPipeline::reset()
{
    for (auto &chain : m_channelFilters)
        for (auto &f : chain)
            f->reset();
}

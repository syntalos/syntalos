/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dimension.h"

#include <algorithm>

namespace SyBench
{

/**
 * @brief How many electrophysiology channels can be filtered and stored at 30 kHz.
 *
 * One source emits float signal blocks at 100 blocks per second which are filtered,
 * written to a Zarr store and counted by a flow meter.
 */
class SignalProcessingDimension : public Dimension
{
public:
    static constexpr int kSampleRate = 30000;
    static constexpr int kBlocksPerSec = 100;
    /// channels per amplifier; more channels are spread over parallel amplifiers,
    /// so the data generator never limits the measurement
    static constexpr int kChannelsPerSource = 1024;

    QString id() const override
    {
        return QStringLiteral("signal-processing");
    }

    QString title() const override
    {
        return QStringLiteral("Signal Processing");
    }

    QString description() const override
    {
        return QStringLiteral(
            "Number of 30 kHz signal channels (in amplifiers of up to 1024 channels) that can be "
            "filtered at full rate. Writing signals to disk is covered by the disk write dimension.");
    }

    QString levelUnit(const QString &) const override
    {
        return QStringLiteral("channels");
    }

    QList<DimensionProfile> profiles() const override
    {
        return {
            DimensionProfile{QStringLiteral("30khz-filter"), QStringLiteral("30 kHz, IIR filter")}
        };
    }

    int startLevel(int, const QString &) const override
    {
        return 64;
    }

    int maxLevel(const QString &) const override
    {
        return 65536;
    }

    static int sourceCount(int level)
    {
        return (level + kChannelsPerSource - 1) / kChannelsPerSource;
    }

    ProjectSpec buildProject(const QString &profileId, int level) const override
    {
        ProjectSpec spec;
        spec.experimentId = QStringLiteral("bench-%1-%2-%3").arg(id(), profileId).arg(level);
        int remaining = level;
        for (int i = 1; i <= sourceCount(level); ++i) {
            const int channels = std::min(remaining, kChannelsPerSource);
            remaining -= channels;
            const auto amp = QStringLiteral("Amplifier %1").arg(i);
            const auto filt = QStringLiteral("Filter %1").arg(i);
            spec.addModule(Modules::dataSourceSignals(amp, 320, 240, kBlocksPerSec, kSampleRate, channels));
            spec.addModule(
                Modules::flowMeter(
                    QStringLiteral("Source Meter %1").arg(i),
                    QStringLiteral("SignalBlockF32"),
                    amp,
                    QStringLiteral("float-out")));
            spec.addModule(Modules::signalFilterLowPass(filt, 6000.0, amp, QStringLiteral("float-out")));
            spec.addModule(
                Modules::flowMeter(
                    QStringLiteral("Meter %1").arg(i),
                    QStringLiteral("SignalBlockF32"),
                    filt,
                    QStringLiteral("signals-out")));
        }
        return spec;
    }

    StepVerdict evaluate(const QString &, int level, const StepResult &result) const override
    {
        QList<RateCheck> checks;
        for (int i = 1; i <= sourceCount(level); ++i) {
            checks.append(
                RateCheck{
                    .moduleName = QStringLiteral("Source Meter %1").arg(i),
                    .expectedRate = kBlocksPerSec,
                    .isSource = true});
            checks.append(RateCheck{.moduleName = QStringLiteral("Meter %1").arg(i), .expectedRate = kBlocksPerSec});
        }
        return evaluateRates(result, checks);
    }
};

std::unique_ptr<Dimension> createSignalProcessingDimension()
{
    return std::make_unique<SignalProcessingDimension>();
}

} // namespace SyBench

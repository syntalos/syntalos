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

#include "dimensions-common.h"

#include <algorithm>

namespace SyBench
{

/**
 * @brief How many independent processing chains of mixed modules can run side by side.
 *
 * Each chain is one rig: a source producing a 720p camera stream and an amplifier's worth
 * of channels, a video transform, a Python tracker script working on the scaled frames and
 * emitting one position row per frame, and a signal filter. No single module is expensive,
 * so the limit comes from running many modules and worker processes at once rather than
 * from one heavy module. The chain is not made cheaper than this on purpose: every chain
 * is a worker process, and a machine reaches its CPU limit with a few dozen of them rather
 * than with hundreds. Encoding and display modules are left out, they have their own
 * dimensions and would dominate this one.
 */
class MixedTasksDimension : public Dimension
{
public:
    static constexpr int kFps = 30;
    static constexpr int kWidth = 1280;
    static constexpr int kHeight = 720;
    static constexpr int kSignalChannels = 128;
    /// every chain is a worker process, and more of them than this per core can not be a
    /// CPU limit anymore but only a process pile-up that takes the machine down
    static constexpr int kMaxChainsPerCore = 8;

    QString id() const override
    {
        return QStringLiteral("mixed-tasks");
    }

    QString title() const override
    {
        return QStringLiteral("Mixed Tasks");
    }

    QString description() const override
    {
        return QStringLiteral(
            "Number of independent processing chains that can run side by side at full rate. "
            "Each chain scales a 720p @ 30 fps video stream, tracks a target in it with a "
            "Python script and filters 128 channels of 30 kHz signals. No single module is "
            "expensive, so this measures how many modules and worker processes the machine "
            "can keep going at once.");
    }

    QString levelUnit(const QString &) const override
    {
        return QStringLiteral("chains");
    }

    int startLevel(int cpuCores, const QString &) const override
    {
        return std::max(2, cpuCores / 2);
    }

    int maxLevel(int cpuCores, const QString &) const override
    {
        return std::max(4, cpuCores * kMaxChainsPerCore);
    }

    QList<DimensionProfile> profiles() const override
    {
        return {
            DimensionProfile{QStringLiteral("mixed-chain"), QStringLiteral("Mixed processing chain")}
        };
    }

    static QString filterMeterName(int i)
    {
        return QStringLiteral("Filter Meter %1").arg(i);
    }

    static ModuleSpec source(const QString &name)
    {
        return Modules::dataSourceSignals(name, kWidth, kHeight, kFps, Signals::kSampleRate, kSignalChannels);
    }

    ProjectSpec buildProject(const QString &, int level) const override
    {
        ProjectSpec spec;
        for (int i = 1; i <= level; ++i) {
            const auto src = QStringLiteral("Source %1").arg(i);
            const auto scale = QStringLiteral("Scale %1").arg(i);
            const auto tracker = QStringLiteral("Tracker %1").arg(i);
            const auto filter = QStringLiteral("Filter %1").arg(i);
            spec.addModule(source(src));
            spec.addModule(
                Modules::flowMeter(sourceMeterName(i), QStringLiteral("Frame"), src, QStringLiteral("frames-out")));
            spec.addModule(Modules::videoTransformScale(scale, 0.5, src, QStringLiteral("frames-out")));
            spec.addModule(Modules::pyScriptFrameTracker(tracker, scale, QStringLiteral("frames-out")));
            spec.addModule(
                Modules::flowMeter(meterName(i), QStringLiteral("TableRow"), tracker, QStringLiteral("rows-out")));
            spec.addModule(Modules::signalFilterLowPass(filter, 6000.0, src, QStringLiteral("float-out")));
            spec.addModule(
                Modules::flowMeter(
                    filterMeterName(i),
                    QStringLiteral("SignalBlockF32"),
                    filter,
                    QStringLiteral("signals-out")));
        }
        // the tracker applies backpressure to its source, so a free-running source
        // tells whether a source could deliver the rate at all
        spec.addModule(source(QStringLiteral("Reference Source")));
        spec.addModule(
            Modules::flowMeter(
                QStringLiteral("Reference Meter"),
                QStringLiteral("Frame"),
                QStringLiteral("Reference Source"),
                QStringLiteral("frames-out")));
        return spec;
    }

    StepVerdict evaluate(const QString &, int level, const StepResult &result) const override
    {
        QList<RateCheck> checks;
        checks.append(
            RateCheck{.moduleName = QStringLiteral("Reference Meter"), .expectedRate = kFps, .isSource = true});
        for (int i = 1; i <= level; ++i) {
            checks.append(RateCheck{.moduleName = sourceMeterName(i), .expectedRate = kFps});
            checks.append(RateCheck{.moduleName = meterName(i), .expectedRate = kFps});
            checks.append(RateCheck{.moduleName = filterMeterName(i), .expectedRate = kFps});
        }
        return evaluateRates(result, checks);
    }
};

std::unique_ptr<Dimension> createMixedTasksDimension()
{
    return std::make_unique<MixedTasksDimension>();
}

} // namespace SyBench

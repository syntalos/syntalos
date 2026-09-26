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

namespace SyBench
{

/**
 * @brief At which frame rate 1080p frames can be passed through an out-of-process module.
 */
class OutOfProcessDimension : public Dimension
{
public:
    /// frames per second one test card source can comfortably deliver at 1080p; higher rates
    /// are spread over several source/worker pairs so the sources never limit the measurement
    static constexpr int kMaxFpsPerSource = 480;

    QString id() const override
    {
        return QStringLiteral("out-of-process");
    }

    QString title() const override
    {
        return QStringLiteral("Out-of-Process Modules");
    }

    QString description() const override
    {
        return QStringLiteral(
            "Total frame rate of 1080p frames that can be handed to Python scripts or C++ worker processes "
            "and back without loss, in streams of up to 480 fps each. A worker that falls behind slows "
            "down its source through backpressure.");
    }

    QString levelUnit(const QString &) const override
    {
        return QStringLiteral("fps");
    }

    QList<DimensionProfile> profiles() const override
    {
        return {
            DimensionProfile{QStringLiteral("python"), QStringLiteral("Python script")   },
            DimensionProfile{QStringLiteral("cpp"),    QStringLiteral("C++ MLink module")},
        };
    }

    int startLevel(int, const QString &) const override
    {
        return 30;
    }

    int maxLevel(const QString &) const override
    {
        return 15360;
    }

    static int pairCount(int level)
    {
        return (level + kMaxFpsPerSource - 1) / kMaxFpsPerSource;
    }

    static int pairFps(int level)
    {
        return level / pairCount(level);
    }

    ProjectSpec buildProject(const QString &profileId, int level) const override
    {
        ProjectSpec spec;
        const int fps = pairFps(level);
        for (int i = 1; i <= pairCount(level); ++i) {
            const auto cam = QStringLiteral("Camera %1").arg(i);
            const auto worker = QStringLiteral("Worker %1").arg(i);
            spec.addModule(Modules::dataSourceTestCard(cam, 1920, 1080, fps));
            spec.addModule(
                Modules::flowMeter(sourceMeterName(i), QStringLiteral("Frame"), cam, QStringLiteral("frames-out")));
            if (profileId == QLatin1String("cpp"))
                spec.addModule(Modules::mlinkExampleFramePassthrough(worker, cam, QStringLiteral("frames-out")));
            else
                spec.addModule(Modules::pyScriptFramePassthrough(worker, cam, QStringLiteral("frames-out")));
            spec.addModule(
                Modules::flowMeter(meterName(i), QStringLiteral("Frame"), worker, QStringLiteral("frames-out")));
        }
        // the IPC path applies backpressure to its source, so a free-running camera
        // tells whether a source could deliver the rate at all
        spec.addModule(Modules::dataSourceTestCard(QStringLiteral("Reference Camera"), 1920, 1080, fps));
        spec.addModule(
            Modules::flowMeter(
                QStringLiteral("Reference Meter"),
                QStringLiteral("Frame"),
                QStringLiteral("Reference Camera"),
                QStringLiteral("frames-out")));
        return spec;
    }

    StepVerdict evaluate(const QString &, int level, const StepResult &result) const override
    {
        const auto rate = static_cast<double>(pairFps(level));
        QList<RateCheck> checks;
        checks.append(
            RateCheck{.moduleName = QStringLiteral("Reference Meter"), .expectedRate = rate, .isSource = true});
        for (int i = 1; i <= pairCount(level); ++i) {
            checks.append(RateCheck{.moduleName = sourceMeterName(i), .expectedRate = rate});
            checks.append(RateCheck{.moduleName = meterName(i), .expectedRate = rate});
        }
        return evaluateRates(result, checks);
    }
};

std::unique_ptr<Dimension> createOutOfProcessDimension()
{
    return std::make_unique<OutOfProcessDimension>();
}

} // namespace SyBench

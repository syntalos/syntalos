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
 * @brief How much data can be passed through an out-of-process module and back.
 *
 * Large items (1080p frames) show the bandwidth of the shared-memory path, small items
 * (table rows) show the per-item overhead of the IPC and the worker runtime. Frames only
 * go through the C++ worker: the copy chain is the limit, a Python worker measured the same.
 */
class OutOfProcessDimension : public Dimension
{
public:
    enum class Language {
        Python,
        Cpp
    };
    enum class Workload {
        Frames, /// 1080p test card frames, level is the total frame rate
        Rows    /// three-column table rows, level is the total row rate
    };

    struct Profile {
        QString id;
        QString title;
        Language language;
        Workload workload;
    };

    /// frames per second one source can comfortably deliver; higher rates are spread over
    /// several source/worker pairs so the sources never limit the measurement
    static constexpr int kMaxFpsPerSource = 480;
    /// rows always go through a single worker, as the per-item overhead is what is measured;
    /// they are emitted in bursts on a fixed tick, so row rates are multiples of this
    static constexpr int kRowTicksPerSec = 1000;

    static const QList<Profile> &profileList()
    {
        static const QList<Profile> profiles = {
            {QStringLiteral("cpp-frames"),
             QStringLiteral("C++ MLink module, 1080p frames"),
             Language::Cpp,
             Workload::Frames                                                                                            },
            {QStringLiteral("python-rows"),
             QStringLiteral("Python script, table rows"),
             Language::Python,
             Workload::Rows                                                                                              },
            {QStringLiteral("cpp-rows"),    QStringLiteral("C++ MLink module, table rows"), Language::Cpp, Workload::Rows},
        };
        return profiles;
    }

    static const Profile &profile(const QString &id)
    {
        const auto &list = profileList();
        const auto it = std::ranges::find(list, id, &Profile::id);
        return it != list.end() ? *it : list.first();
    }

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
            "Rate at which data can be handed to worker processes and back without loss: "
            "1080p frames through C++ workers for the aggregate bandwidth of the shared-memory path, "
            "spread over several workers as no single source can saturate it, and small table rows "
            "through a single Python or C++ worker for the per-item overhead. "
            "A worker that falls behind slows down its source through backpressure.");
    }

    QString levelUnit(const QString &profileId) const override
    {
        return profile(profileId).workload == Workload::Frames ? QStringLiteral("fps") : QStringLiteral("rows/s");
    }

    QList<DimensionProfile> profiles() const override
    {
        QList<DimensionProfile> res;
        for (const auto &p : profileList())
            res.append(DimensionProfile{p.id, p.title});
        return res;
    }

    int startLevel(int, const QString &profileId) const override
    {
        return profile(profileId).workload == Workload::Frames ? 30 : 16000;
    }

    int maxLevel(const QString &profileId) const override
    {
        // the row limit is what the data source can emit per tick
        return profile(profileId).workload == Workload::Frames ? 15360 : 10000000;
    }

    static int pairCount(Workload workload, int level)
    {
        if (workload == Workload::Rows)
            return 1;
        return (level + kMaxFpsPerSource - 1) / kMaxFpsPerSource;
    }

    /// items per second of each source, row rates rounded down to whole rows per tick
    static int pairRate(Workload workload, int level)
    {
        const int rate = level / pairCount(workload, level);
        if (workload == Workload::Frames)
            return rate;
        return std::max(rate / kRowTicksPerSec, 1) * kRowTicksPerSec;
    }

    ProjectSpec buildProject(const QString &profileId, int level) const override
    {
        const auto &p = profile(profileId);
        const bool frames = p.workload == Workload::Frames;
        const auto dataType = frames ? QStringLiteral("Frame") : QStringLiteral("TableRow");
        const auto sourcePort = frames ? QStringLiteral("frames-out") : QStringLiteral("rows-out");
        const int rate = pairRate(p.workload, level);

        ProjectSpec spec;
        const auto source = [&](const QString &name) {
            return frames ? Modules::dataSourceTestCard(name, 1920, 1080, rate)
                          : Modules::dataSourceRows(name, kRowTicksPerSec, rate / kRowTicksPerSec);
        };
        for (int i = 1; i <= pairCount(p.workload, level); ++i) {
            const auto src = QStringLiteral("Source %1").arg(i);
            const auto worker = QStringLiteral("Worker %1").arg(i);
            spec.addModule(source(src));
            spec.addModule(Modules::flowMeter(sourceMeterName(i), dataType, src, sourcePort));
            QString workerPort;
            if (p.language == Language::Cpp) {
                spec.addModule(
                    frames ? Modules::mlinkExampleFramePassthrough(worker, src, sourcePort)
                           : Modules::mlinkExampleRowPassthrough(worker, src, sourcePort));
                workerPort = frames ? QStringLiteral("frames-out") : QStringLiteral("table-out");
            } else {
                spec.addModule(Modules::pyScriptRowPassthrough(worker, src, sourcePort));
                workerPort = QStringLiteral("rows-out");
            }
            spec.addModule(Modules::flowMeter(meterName(i), dataType, worker, workerPort));
        }
        // the IPC path applies backpressure to its source, so a free-running source
        // tells whether a source could deliver the rate at all
        spec.addModule(source(QStringLiteral("Reference Source")));
        spec.addModule(
            Modules::flowMeter(
                QStringLiteral("Reference Meter"),
                dataType,
                QStringLiteral("Reference Source"),
                sourcePort));
        return spec;
    }

    StepVerdict evaluate(const QString &profileId, int level, const StepResult &result) const override
    {
        const auto &p = profile(profileId);
        const auto rate = static_cast<double>(pairRate(p.workload, level));
        QList<RateCheck> checks;
        checks.append(
            RateCheck{.moduleName = QStringLiteral("Reference Meter"), .expectedRate = rate, .isSource = true});
        for (int i = 1; i <= pairCount(p.workload, level); ++i) {
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

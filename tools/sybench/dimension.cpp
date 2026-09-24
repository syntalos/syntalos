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
#include <cmath>

namespace SyBench
{

int Dimension::startLevel(int cpuCores) const
{
    return std::max(1, cpuCores / 2);
}

int Dimension::maxLevel() const
{
    return 512;
}

bool Dimension::writesData() const
{
    return false;
}

StepVerdict evaluateRates(const StepResult &result, const QList<RateCheck> &checks, double minRateFraction)
{
    StepVerdict v;
    if (!result.success) {
        v.summary = result.failureReason.isEmpty() ? QStringLiteral("run failed") : result.failureReason;
        return v;
    }
    if (!result.stats || result.stats->durationSec <= 0) {
        v.summary = QStringLiteral("no run duration recorded");
        return v;
    }
    const auto &stats = *result.stats;

    double maxRate = 0;
    v.minRateFraction = 1.0;
    for (const auto &c : checks) {
        maxRate = std::max(maxRate, c.expectedRate);
        const auto m = result.meter(c.meterName);
        if (!m) {
            v.minRateFraction = 0;
            v.summary = QStringLiteral("no statistics from meter '%1'").arg(c.meterName);
            return v;
        }
        const double expected = c.expectedRate * stats.durationSec;
        if (expected > 0)
            v.minRateFraction = std::min(v.minRateFraction, m->items / expected);
    }

    // a backlog of more than half a second of data means the consumer can not keep up
    const qint64 backlogLimit = std::max<qint64>(4, std::llround(maxRate / 2.0));
    for (const auto &c : stats.connections) {
        if (c.directIpc)
            continue;
        v.maxPeakBacklog = std::max<qint64>(v.maxPeakBacklog, c.peakPending);
        v.maxBacklogAtStop = std::max<qint64>(v.maxBacklogAtStop, c.pendingAtStop);
    }

    const bool rateOk = v.minRateFraction >= minRateFraction;
    const bool backlogOk = v.maxPeakBacklog <= backlogLimit && v.maxBacklogAtStop <= 2;
    v.passed = rateOk && backlogOk;
    v.summary = QStringLiteral("min rate %1 %, peak backlog %2, backlog at stop %3")
                    .arg(v.minRateFraction * 100.0, 0, 'f', 1)
                    .arg(v.maxPeakBacklog)
                    .arg(v.maxBacklogAtStop);
    return v;
}

namespace Modules
{

ModuleSpec dataSourceCamera(const QString &name, int width, int height, int fps)
{
    ModuleSpec m;
    m.id = QStringLiteral("devel.datasource");
    m.name = name;
    m.settings.insert(QStringLiteral("fps"), fps);
    m.settings.insert(QStringLiteral("frame_width"), width);
    m.settings.insert(QStringLiteral("frame_height"), height);
    m.settings.insert(QStringLiteral("frame_content"), QStringLiteral("camera"));
    m.settings.insert(QStringLiteral("color_video"), true);
    return m;
}

ModuleSpec videoTransformScale(
    const QString &name,
    double scaleFactor,
    const QString &srcModule,
    const QString &srcPort)
{
    ModuleSpec m;
    m.id = QStringLiteral("videotransform");
    m.name = name;
    QVariantHash tf;
    tf.insert(QStringLiteral("type"), QStringLiteral("ScaleTransform"));
    tf.insert(QStringLiteral("scale_factor"), scaleFactor);
    m.settings.insert(QStringLiteral("video_transform"), QVariantList{tf});
    m.subscribe(QStringLiteral("frames-in"), srcModule, srcPort);
    return m;
}

ModuleSpec flowMeter(const QString &name, const QString &dataType, const QString &srcModule, const QString &srcPort)
{
    ModuleSpec m;
    m.id = QStringLiteral("flowmeter");
    m.name = name;
    m.settings.insert(QStringLiteral("data_type"), dataType);
    m.subscribe(QStringLiteral("data-in"), srcModule, srcPort);
    return m;
}

} // namespace Modules

std::vector<std::unique_ptr<Dimension>> createAllDimensions()
{
    std::vector<std::unique_ptr<Dimension>> dims;
    dims.push_back(createCameraCapacityDimension());
    return dims;
}

std::unique_ptr<Dimension> createDimension(const QString &id)
{
    for (auto &d : createAllDimensions()) {
        if (d->id() == id)
            return std::move(d);
    }
    return nullptr;
}

} // namespace SyBench

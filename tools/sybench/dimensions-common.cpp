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

const VideoProfile &VideoDimension::videoProfile(const QString &id) const
{
    const auto &list = videoProfiles();
    const auto it = std::ranges::find(list, id, &VideoProfile::id);
    return it != list.end() ? *it : list.first();
}

QList<DimensionProfile> VideoDimension::profiles() const
{
    QList<DimensionProfile> res;
    for (const auto &p : videoProfiles())
        res.append(DimensionProfile{p.id, p.title});
    return res;
}

QString meterName(int i)
{
    return QStringLiteral("Meter %1").arg(i);
}

QString sourceMeterName(int i)
{
    return QStringLiteral("Source Meter %1").arg(i);
}

namespace Signals
{

int amplifierCount(int channels)
{
    return (channels + kChannelsPerAmplifier - 1) / kChannelsPerAmplifier;
}

QStringList addAmplifiers(ProjectSpec &spec, int channels)
{
    QStringList names;
    int remaining = channels;
    for (int i = 1; i <= amplifierCount(channels); ++i) {
        const int ampChannels = std::min(remaining, kChannelsPerAmplifier);
        remaining -= ampChannels;
        const auto amp = QStringLiteral("Amplifier %1").arg(i);
        spec.addModule(Modules::dataSourceSignals(amp, 320, 240, kBlocksPerSec, kSampleRate, ampChannels));
        spec.addModule(
            Modules::flowMeter(sourceMeterName(i), QStringLiteral("SignalBlockF32"), amp, QStringLiteral("float-out")));
        names.append(amp);
    }
    return names;
}

QList<RateCheck> amplifierSourceChecks(int channels)
{
    QList<RateCheck> checks;
    for (int i = 1; i <= amplifierCount(channels); ++i)
        checks.append(RateCheck{.moduleName = sourceMeterName(i), .expectedRate = kBlocksPerSec, .isSource = true});
    return checks;
}

} // namespace Signals

} // namespace SyBench

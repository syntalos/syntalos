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

#pragma once

#include <QStringList>

#include "dimension.h"

/*
 * Building blocks shared by the dimension implementations.
 */

namespace SyBench
{

/**
 * @brief A camera-like video stream, optionally recorded with a codec
 */
struct VideoProfile {
    QString id;
    QString title;
    int width;
    int height;
    int fps;
    Modules::Codec codec = Modules::Codec::Raw;
};

/**
 * @brief A dimension whose profiles are video streams
 */
class VideoDimension : public Dimension
{
public:
    virtual const QList<VideoProfile> &videoProfiles() const = 0;

    /// the profile with the given id, or the first one if there is none
    const VideoProfile &videoProfile(const QString &id) const;

    QList<DimensionProfile> profiles() const override;
};

/// flow meter counting the output of the i-th chain
QString meterName(int i);
/// flow meter counting what the i-th data source produced
QString sourceMeterName(int i);

namespace Signals
{
constexpr int kSampleRate = 30000;
/// one signal block per source tick
constexpr int kBlocksPerSec = 100;
/// more channels are spread over several amplifiers, so the data generator never limits the measurement
constexpr int kChannelsPerAmplifier = 1024;

/// number of amplifiers needed for the given channel count
int amplifierCount(int channels);

/**
 * @brief Add amplifiers carrying the given number of 30 kHz float channels, each with a source meter.
 * @return The names of the amplifier modules, their signals are on port "float-out".
 */
QStringList addAmplifiers(ProjectSpec &spec, int channels);

/// source rate checks for the amplifiers addAmplifiers() created
QList<RateCheck> amplifierSourceChecks(int channels);
} // namespace Signals

} // namespace SyBench

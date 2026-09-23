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

#include <QList>
#include <QString>
#include <memory>
#include <vector>

#include "projectgen.h"
#include "runner.h"

namespace SyBench
{

struct DimensionProfile {
    QString id;
    QString title;
};

/**
 * @brief Result of judging one step
 */
struct StepVerdict {
    bool passed = false;
    QString summary;            /// short human-readable reason, e.g. "min rate 97.1 %, backlog at stop 40"
    double minRateFraction = 0; /// worst meter rate relative to the expected rate
    qint64 maxPeakBacklog = 0;
    qint64 maxBacklogAtStop = 0;
};

/**
 * @brief Expected data rate on a flow meter
 */
struct RateCheck {
    QString meterName;
    double expectedRate = 0; /// items per second
};

/**
 * @brief One benchmark dimension, e.g. "how many camera streams can be handled"
 */
class Dimension
{
public:
    virtual ~Dimension() = default;

    virtual QString id() const = 0;
    virtual QString title() const = 0;
    virtual QString description() const = 0;
    virtual QString levelUnit() const = 0; /// e.g. "streams", "channels"

    virtual QList<DimensionProfile> profiles() const = 0;

    /**
     * @brief Level to start the ladder from, on a machine with the given number of physical cores.
     */
    virtual int startLevel(int cpuCores) const;
    virtual int maxLevel() const;

    /**
     * @brief Whether the runs of this dimension need a real export directory (disk tests).
     */
    virtual bool writesData() const;

    virtual ProjectSpec buildProject(const QString &profileId, int level) const = 0;
    virtual StepVerdict evaluate(const QString &profileId, int level, const StepResult &result) const = 0;
};

/**
 * @brief Judge a step by the flow meter rates and connection backlogs.
 *
 * A step passes when the run succeeded, every meter received at least minRateFraction of the
 * expected items, no connection had more than a few items left at stop, and the peak backlog
 * stayed below half a second worth of data.
 */
StepVerdict evaluateRates(const StepResult &result, const QList<RateCheck> &checks, double minRateFraction = 0.98);

/** Module specs for the modules benchmark projects are built from. */
namespace Modules
{
ModuleSpec dataSourceCamera(const QString &name, int width, int height, int fps);
ModuleSpec videoTransformScale(
    const QString &name,
    double scaleFactor,
    const QString &srcModule,
    const QString &srcPort);
ModuleSpec flowMeter(const QString &name, const QString &dataType, const QString &srcModule, const QString &srcPort);
} // namespace Modules

std::unique_ptr<Dimension> createCameraCapacityDimension();

std::vector<std::unique_ptr<Dimension>> createAllDimensions();
std::unique_ptr<Dimension> createDimension(const QString &id);

} // namespace SyBench

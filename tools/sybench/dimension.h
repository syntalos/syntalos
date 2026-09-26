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
    bool sourceLimited = false; /// a data source itself missed its rate, the step can not judge the chain
    QString summary;            /// short human-readable reason, e.g. "min rate 97.1 %, backlog at stop 40"
    double minRateFraction = 0; /// worst checked rate relative to the expected rate
    qint64 maxPeakBacklog = 0;
    qint64 maxBacklogAtStop = 0;
};

/**
 * @brief Expected item rate reported by a module in its run statistics
 */
struct RateCheck {
    QString moduleName;
    double expectedRate = 0;                   /// items per second
    QString statKey = QStringLiteral("items"); /// module statistic holding the item count (flow meter default)
    bool isSource = false; /// a reference meter directly on a data source: a shortfall there is a source limit
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
    virtual QString levelUnit(const QString &profileId) const = 0; /// e.g. "streams", "channels"

    virtual QList<DimensionProfile> profiles() const = 0;
    QString profileTitle(const QString &profileId) const;

    /**
     * @brief Level to start the ladder from, on a machine with the given number of physical cores.
     */
    virtual int startLevel(int cpuCores, const QString &profileId) const;
    virtual int maxLevel(const QString &profileId) const;

    /**
     * @brief Whether runs of this profile need a real export directory (they record data).
     */
    virtual bool writesData(const QString &profileId) const;

    virtual ProjectSpec buildProject(const QString &profileId, int level) const = 0;
    virtual StepVerdict evaluate(const QString &profileId, int level, const StepResult &result) const = 0;
};

/**
 * @brief Judge a step by the reported item rates and connection backlogs.
 *
 * A step passes when the run succeeded, every checked module reported at least minRateFraction
 * of the expected items, no connection had more than a few items left at stop, and the peak
 * backlog stayed below two seconds worth of data.
 */
StepVerdict evaluateRates(const StepResult &result, const QList<RateCheck> &checks, double minRateFraction = 0.98);

/** Module specs for the modules benchmark projects are built from. */
namespace Modules
{
/// data source producing camera-like frames (its signal ports stay unconnected)
ModuleSpec dataSourceCamera(const QString &name, int width, int height, int fps);
/// data source producing test card frames and signals with the given channel count;
/// one signal block per frame tick, so the block rate equals fps
ModuleSpec dataSourceSignals(const QString &name, int width, int height, int fps, double sampleRate, int channels);
/// canvas display module, input port frames-in
ModuleSpec canvas(const QString &name, const QString &srcModule, const QString &srcPort);
ModuleSpec videoTransformScale(
    const QString &name,
    double scaleFactor,
    const QString &srcModule,
    const QString &srcPort);
ModuleSpec flowMeter(const QString &name, const QString &dataType, const QString &srcModule, const QString &srcPort);

enum class Codec {
    Raw,
    FFV1,
    AV1
};
/// video recorder encoding live (no deferred encoding), input port frames-in
ModuleSpec videoRecorder(const QString &name, Codec codec, const QString &srcModule, const QString &srcPort);
/// signal filter with one Butterworth low-pass stage on all channels, ports signals-in / signals-out
ModuleSpec signalFilterLowPass(const QString &name, double cutoffHz, const QString &srcModule, const QString &srcPort);
/// Zarr writer for float signal blocks, input port f32sig-in
ModuleSpec zarrWriterSignals(const QString &name, const QString &srcModule, const QString &srcPort);
/// Python script forwarding frames unchanged, ports frames-in / frames-out
ModuleSpec pyScriptFramePassthrough(const QString &name, const QString &srcModule, const QString &srcPort);
/// the C++ MLink example module forwarding frames, ports frames-in / frames-out
ModuleSpec mlinkExampleFramePassthrough(const QString &name, const QString &srcModule, const QString &srcPort);
} // namespace Modules

std::unique_ptr<Dimension> createCameraCapacityDimension();
std::unique_ptr<Dimension> createEncodingDimension();
std::unique_ptr<Dimension> createDiskWriteDimension();
std::unique_ptr<Dimension> createSignalProcessingDimension();
std::unique_ptr<Dimension> createOutOfProcessDimension();

std::vector<std::unique_ptr<Dimension>> createAllDimensions();
std::unique_ptr<Dimension> createDimension(const QString &id);

} // namespace SyBench

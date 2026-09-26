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
#include <optional>

namespace SyBench
{

QString Dimension::profileTitle(const QString &profileId) const
{
    const auto list = profiles();
    const auto it = std::ranges::find(list, profileId, &DimensionProfile::id);
    return it != list.end() ? it->title : QString();
}

int Dimension::startLevel(int cpuCores, const QString &) const
{
    return std::max(1, cpuCores / 2);
}

int Dimension::maxLevel(const QString &) const
{
    return 512;
}

bool Dimension::writesData(const QString &) const
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
        std::optional<qint64> items;
        for (const auto &mod : stats.modules) {
            if (mod.name == c.moduleName && mod.moduleStats.contains(c.statKey))
                items = mod.moduleStats.value(c.statKey).toLongLong();
        }
        if (!items) {
            v.minRateFraction = 0;
            v.summary = QStringLiteral("no '%1' statistic from '%2'").arg(c.statKey, c.moduleName);
            return v;
        }
        const double expected = c.expectedRate * stats.durationSec;
        const double fraction = expected > 0 ? *items / expected : 1.0;
        if (c.isSource) {
            if (fraction < minRateFraction) {
                // a source starved of CPU on a saturated machine is the machine's limit; a source
                // that falls short on an idle machine is its own limit and nothing downstream can be judged
                const double load = result.processLoad();
                const bool overloaded = stats.cpuCoreCount > 0 && load > 0.75 * stats.cpuCoreCount;
                if (overloaded) {
                    v.summary = QStringLiteral("overloaded: '%1' produced only %2 % of its rate at a load of %3 cores")
                                    .arg(c.moduleName)
                                    .arg(fraction * 100.0, 0, 'f', 1)
                                    .arg(load, 0, 'f', 1);
                    return v;
                }
                v.sourceLimited = true;
                v.summary = QStringLiteral("source limit: '%1' produced only %2 % of its rate")
                                .arg(c.moduleName)
                                .arg(fraction * 100.0, 0, 'f', 1);
                return v;
            }
            continue;
        }
        maxRate = std::max(maxRate, c.expectedRate);
        v.minRateFraction = std::min(v.minRateFraction, fraction);
    }

    // short hiccups (encoder start-up, pipeline builds) may queue a second or so of data;
    // more than two seconds worth means the consumer can not keep up
    const qint64 backlogLimit = std::max<qint64>(4, std::llround(maxRate * 2.0));
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

ModuleSpec dataSourceSignals(const QString &name, int width, int height, int fps, double sampleRate, int channels)
{
    ModuleSpec m;
    m.id = QStringLiteral("devel.datasource");
    m.name = name;
    m.settings.insert(QStringLiteral("fps"), fps);
    m.settings.insert(QStringLiteral("frame_width"), width);
    m.settings.insert(QStringLiteral("frame_height"), height);
    m.settings.insert(QStringLiteral("frame_content"), QStringLiteral("testcard"));
    m.settings.insert(QStringLiteral("color_video"), true);
    m.settings.insert(QStringLiteral("sample_rate"), sampleRate);
    m.settings.insert(QStringLiteral("signal_channels"), channels);
    return m;
}

ModuleSpec canvas(const QString &name, const QString &srcModule, const QString &srcPort)
{
    ModuleSpec m;
    m.id = QStringLiteral("canvas");
    m.name = name;
    m.subscribe(QStringLiteral("frames-in"), srcModule, srcPort);
    return m;
}

ModuleSpec videoRecorder(const QString &name, Codec codec, const QString &srcModule, const QString &srcPort)
{
    // numeric codec ids as persisted by the video recorder module (VideoCodec enum)
    int codecId = 2;
    bool lossless = true;
    int quality = 0;
    switch (codec) {
    case Codec::Raw:
        codecId = 1;
        break;
    case Codec::FFV1:
        codecId = 2;
        break;
    case Codec::AV1:
        codecId = 3;
        lossless = false;
        quality = 24;
        break;
    }

    ModuleSpec m;
    m.id = QStringLiteral("videorecorder");
    m.name = name;
    m.settings.insert(QStringLiteral("video_codec"), codecId);
    m.settings.insert(QStringLiteral("video_container"), 1); // Matroska
    m.settings.insert(QStringLiteral("lossless"), lossless);
    m.settings.insert(QStringLiteral("exact_colors"), false);
    m.settings.insert(QStringLiteral("vaapi_enabled"), false);
    m.settings.insert(QStringLiteral("mode"), QStringLiteral("constant-quality"));
    m.settings.insert(QStringLiteral("quality"), quality);
    m.settings.insert(QStringLiteral("video_name_from_source"), true);
    m.settings.insert(QStringLiteral("save_timestamps"), true);
    m.settings.insert(QStringLiteral("slices_enabled"), false);
    m.settings.insert(QStringLiteral("deferred_encode_enabled"), false);
    m.subscribe(QStringLiteral("frames-in"), srcModule, srcPort);
    return m;
}

ModuleSpec signalFilterLowPass(const QString &name, double cutoffHz, const QString &srcModule, const QString &srcPort)
{
    ModuleSpec m;
    m.id = QStringLiteral("signalfilter");
    m.name = name;
    m.settings.insert(QStringLiteral("input_type"), QStringLiteral("SignalBlockF32"));
    m.settings.insert(QStringLiteral("use_all_channels"), true);
    QVariantHash stage;
    stage.insert(QStringLiteral("family"), 0);   // Butterworth
    stage.insert(QStringLiteral("response"), 0); // low-pass
    stage.insert(QStringLiteral("order"), 4);
    stage.insert(QStringLiteral("freq1"), cutoffHz);
    m.settings.insert(QStringLiteral("stages"), QVariantList{stage});
    m.subscribe(QStringLiteral("signals-in"), srcModule, srcPort);
    return m;
}

ModuleSpec zarrWriterSignals(const QString &name, const QString &srcModule, const QString &srcPort)
{
    ModuleSpec m;
    m.id = QStringLiteral("zarrwriter");
    m.name = name;
    m.settings.insert(QStringLiteral("input_type"), QStringLiteral("SignalBlockF32"));
    m.settings.insert(QStringLiteral("use_name_from_source"), true);
    m.subscribe(QStringLiteral("f32sig-in"), srcModule, srcPort);
    return m;
}

static QVariantList framePortList(const QString &id, const QString &title)
{
    QVariantHash port;
    port.insert(QStringLiteral("id"), id);
    port.insert(QStringLiteral("title"), title);
    port.insert(QStringLiteral("data_type"), QStringLiteral("Frame"));
    return QVariantList{port};
}

ModuleSpec pyScriptFramePassthrough(const QString &name, const QString &srcModule, const QString &srcPort)
{
    ModuleSpec m;
    m.id = QStringLiteral("pyscript");
    m.name = name;
    m.settings.insert(
        QStringLiteral("ports_in"),
        framePortList(QStringLiteral("frames-in"), QStringLiteral("Frames In")));
    m.settings.insert(
        QStringLiteral("ports_out"),
        framePortList(QStringLiteral("frames-out"), QStringLiteral("Frames Out")));
    m.extraData = QByteArrayLiteral(
        "import syntalos_mlink as syl\n"
        "\n"
        "iport = syl.get_input_port('frames-in')\n"
        "oport = syl.get_output_port('frames-out')\n"
        "\n"
        "\n"
        "def on_frame(frame) -> None:\n"
        "    oport.submit(frame)\n"
        "\n"
        "\n"
        "def prepare() -> bool:\n"
        "    iport.on_data = on_frame\n"
        "    oport.set_metadata_value('framerate', iport.metadata['framerate'])\n"
        "    oport.set_metadata_value_size('size', iport.metadata['size'])\n"
        "    return True\n"
        "\n"
        "\n"
        "def run():\n"
        "    while syl.is_running():\n"
        "        syl.await_data()\n");
    m.subscribe(QStringLiteral("frames-in"), srcModule, srcPort);
    return m;
}

ModuleSpec mlinkExampleFramePassthrough(const QString &name, const QString &srcModule, const QString &srcPort)
{
    ModuleSpec m;
    m.id = QStringLiteral("example-mlink");
    m.name = name;
    m.subscribe(QStringLiteral("frames-in"), srcModule, srcPort);
    return m;
}

} // namespace Modules

std::vector<std::unique_ptr<Dimension>> createAllDimensions()
{
    std::vector<std::unique_ptr<Dimension>> dims;
    dims.push_back(createCameraCapacityDimension());
    dims.push_back(createEncodingDimension());
    dims.push_back(createDiskWriteDimension());
    dims.push_back(createSignalProcessingDimension());
    dims.push_back(createOutOfProcessDimension());
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

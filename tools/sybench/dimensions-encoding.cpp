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

#include "utils/misc.h"

#include <algorithm>

namespace SyBench
{

/**
 * Camera-like sources recorded by video recorder modules, shared by the encoding and the disk dimension.
 */
class RecordingDimensionBase : public Dimension
{
public:
    struct Profile {
        QString id;
        QString title;
        int width;
        int height;
        int fps;
        Modules::Codec codec;
    };

    virtual const QList<Profile> &profileList() const = 0;

    const Profile &profile(const QString &id) const
    {
        for (const auto &p : profileList()) {
            if (p.id == id)
                return p;
        }
        return profileList().first();
    }

    QString levelUnit(const QString &) const override
    {
        return QStringLiteral("streams");
    }

    bool writesData(const QString &) const override
    {
        return true;
    }

    QList<DimensionProfile> profiles() const override
    {
        QList<DimensionProfile> res;
        for (const auto &p : profileList())
            res.append(DimensionProfile{p.id, p.title});
        return res;
    }

    static QString recorderName(int i)
    {
        return QStringLiteral("Recorder %1").arg(i);
    }

    static QString sourceMeterName(int i)
    {
        return QStringLiteral("Source Meter %1").arg(i);
    }

    ProjectSpec buildProject(const QString &profileId, int level) const override
    {
        const auto &p = profile(profileId);
        ProjectSpec spec;
        spec.experimentId = QStringLiteral("bench-%1-%2-%3").arg(id(), p.id).arg(level);
        for (int i = 1; i <= level; ++i) {
            const auto camName = QStringLiteral("Camera %1").arg(i);
            spec.addModule(Modules::dataSourceCamera(camName, p.width, p.height, p.fps));
            spec.addModule(
                Modules::flowMeter(sourceMeterName(i), QStringLiteral("Frame"), camName, QStringLiteral("frames-out")));
            spec.addModule(Modules::videoRecorder(recorderName(i), p.codec, camName, QStringLiteral("frames-out")));
        }
        return spec;
    }

    StepVerdict evaluate(const QString &profileId, int level, const StepResult &result) const override
    {
        const auto &p = profile(profileId);
        QList<RateCheck> checks;
        for (int i = 1; i <= level; ++i) {
            checks.append(
                RateCheck{
                    .moduleName = sourceMeterName(i),
                    .expectedRate = static_cast<double>(p.fps),
                    .isSource = true});
            checks.append(
                RateCheck{
                    .moduleName = recorderName(i),
                    .expectedRate = static_cast<double>(p.fps),
                    .statKey = QStringLiteral("frames_encoded")});
        }
        return evaluateRates(result, checks);
    }
};

/**
 * @brief How many camera streams can be encoded live with a given codec.
 */
class EncodingDimension : public RecordingDimensionBase
{
public:
    const QList<Profile> &profileList() const override
    {
        static const QList<Profile> profiles = {
            {QStringLiteral("1080p30-ffv1"),
             QStringLiteral("1080p @ 30 fps, FFV1"),
             1920,                                                                        1080,
             30,                                                                                     Modules::Codec::FFV1},
            {QStringLiteral("1080p30-av1"),  QStringLiteral("1080p @ 30 fps, AV1"), 1920, 1080, 30,  Modules::Codec::AV1 },
            {QStringLiteral("720p120-ffv1"),
             QStringLiteral("720p @ 120 fps, FFV1"),
             1280,                                                                        720,
             120,                                                                                    Modules::Codec::FFV1},
            {QStringLiteral("720p120-av1"),  QStringLiteral("720p @ 120 fps, AV1"), 1280, 720,  120, Modules::Codec::AV1 },
        };
        return profiles;
    }

    QString id() const override
    {
        return QStringLiteral("encoding");
    }

    QString title() const override
    {
        return QStringLiteral("Video Encoding");
    }

    QString description() const override
    {
        return QStringLiteral(
            "Number of camera streams that can be encoded live at their full frame rate, "
            "with the lossless FFV1 codec and with AV1.");
    }
};

/**
 * @brief How much data the storage can absorb: uncompressed video streams, and 30 kHz
 * signal channels written to Zarr stores.
 */
class DiskWriteDimension : public RecordingDimensionBase
{
public:
    static constexpr int kSampleRate = 30000;
    static constexpr int kBlocksPerSec = 100;
    static constexpr int kChannelsPerSource = 1024;
    static const QString &zarrProfileId()
    {
        static const QString id = QStringLiteral("zarr-30khz");
        return id;
    }

    const QList<Profile> &profileList() const override
    {
        static const QList<Profile> profiles = {
            {QStringLiteral("raw1080p30"),
             QStringLiteral("Raw 1080p @ 30 fps video"),
             1920, 1080,
             30, Modules::Codec::Raw},
        };
        return profiles;
    }

    QString id() const override
    {
        return QStringLiteral("disk-write");
    }

    QString title() const override
    {
        return QStringLiteral("Disk Write");
    }

    QString description() const override
    {
        return QStringLiteral(
            "How much data can be written to the data directory at full rate: uncompressed 1080p video "
            "streams (4:2:0, as Syntalos stores raw video), and 30 kHz signal channels stored as Zarr.");
    }

    QList<DimensionProfile> profiles() const override
    {
        auto res = RecordingDimensionBase::profiles();
        res.append(DimensionProfile{zarrProfileId(), QStringLiteral("Zarr, 30 kHz signals")});
        return res;
    }

    QString levelUnit(const QString &profileId) const override
    {
        return profileId == zarrProfileId() ? QStringLiteral("channels") : QStringLiteral("streams");
    }

    int startLevel(int cpuCores, const QString &profileId) const override
    {
        return profileId == zarrProfileId() ? 64 : RecordingDimensionBase::startLevel(cpuCores, profileId);
    }

    int maxLevel(const QString &profileId) const override
    {
        return profileId == zarrProfileId() ? 65536 : RecordingDimensionBase::maxLevel(profileId);
    }

    static int sourceCount(int channels)
    {
        return (channels + kChannelsPerSource - 1) / kChannelsPerSource;
    }

    ProjectSpec buildProject(const QString &profileId, int level) const override
    {
        if (profileId != zarrProfileId())
            return RecordingDimensionBase::buildProject(profileId, level);

        ProjectSpec spec;
        spec.experimentId = QStringLiteral("bench-%1-%2-%3").arg(id(), profileId).arg(level);
        int remaining = level;
        for (int i = 1; i <= sourceCount(level); ++i) {
            const int channels = std::min(remaining, kChannelsPerSource);
            remaining -= channels;
            const auto amp = QStringLiteral("Amplifier %1").arg(i);
            spec.addModule(Modules::dataSourceSignals(amp, 320, 240, kBlocksPerSec, kSampleRate, channels));
            spec.addModule(
                Modules::flowMeter(
                    QStringLiteral("Source Meter %1").arg(i),
                    QStringLiteral("SignalBlockF32"),
                    amp,
                    QStringLiteral("float-out")));
            spec.addModule(
                Modules::zarrWriterSignals(QStringLiteral("Zarr Writer %1").arg(i), amp, QStringLiteral("float-out")));
        }
        return spec;
    }

    StepVerdict evaluate(const QString &profileId, int level, const StepResult &result) const override
    {
        StepVerdict v;
        if (profileId != zarrProfileId()) {
            v = RecordingDimensionBase::evaluate(profileId, level, result);
        } else {
            QList<RateCheck> checks;
            for (int i = 1; i <= sourceCount(level); ++i) {
                checks.append(
                    RateCheck{
                        .moduleName = QStringLiteral("Source Meter %1").arg(i),
                        .expectedRate = kBlocksPerSec,
                        .isSource = true});
                checks.append(
                    RateCheck{
                        .moduleName = QStringLiteral("Zarr Writer %1").arg(i),
                        .expectedRate = kBlocksPerSec,
                        .statKey = QStringLiteral("items_written")});
            }
            v = evaluateRates(result, checks);
        }
        if (result.stats && result.stats->bytesWritten > 0 && result.stats->durationSec > 0) {
            const double bytesPerSec = result.stats->bytesWritten / result.stats->durationSec;
            v.summary += QStringLiteral(", %1/s written").arg(Syntalos::formatByteSize(std::llround(bytesPerSec)));
        }
        return v;
    }
};

std::unique_ptr<Dimension> createEncodingDimension()
{
    return std::make_unique<EncodingDimension>();
}

std::unique_ptr<Dimension> createDiskWriteDimension()
{
    return std::make_unique<DiskWriteDimension>();
}

} // namespace SyBench

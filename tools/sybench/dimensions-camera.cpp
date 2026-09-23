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

namespace SyBench
{

/**
 * @brief How many camera streams can be received, transformed and handed on at full rate.
 *
 * Each stream is a camera-like data source, scaled down by a video transform module
 * and counted by a flow meter.
 */
class CameraCapacityDimension : public Dimension
{
public:
    struct Profile {
        QString id;
        QString title;
        int width;
        int height;
        int fps;
    };

    static const QList<Profile> &profileList()
    {
        static const QList<Profile> profiles = {
            {QStringLiteral("1080p30"), QStringLiteral("1080p @ 30 fps"), 1920, 1080, 30 },
            {QStringLiteral("720p120"), QStringLiteral("720p @ 120 fps"), 1280, 720,  120},
        };
        return profiles;
    }

    static const Profile &profile(const QString &id)
    {
        for (const auto &p : profileList()) {
            if (p.id == id)
                return p;
        }
        return profileList().first();
    }

    QString id() const override
    {
        return QStringLiteral("camera-capacity");
    }

    QString title() const override
    {
        return QStringLiteral("Camera Capacity");
    }

    QString description() const override
    {
        return QStringLiteral(
            "Number of camera streams that can be received, transformed and passed on "
            "at their full frame rate, without encoding.");
    }

    QString levelUnit() const override
    {
        return QStringLiteral("streams");
    }

    QList<DimensionProfile> profiles() const override
    {
        QList<DimensionProfile> res;
        for (const auto &p : profileList())
            res.append(DimensionProfile{p.id, p.title});
        return res;
    }

    static QString meterName(int i)
    {
        return QStringLiteral("Meter %1").arg(i);
    }

    ProjectSpec buildProject(const QString &profileId, int level) const override
    {
        const auto &p = profile(profileId);
        ProjectSpec spec;
        spec.experimentId = QStringLiteral("bench-%1-%2-%3").arg(id(), p.id).arg(level);
        for (int i = 1; i <= level; ++i) {
            const auto camName = QStringLiteral("Camera %1").arg(i);
            const auto tfName = QStringLiteral("Scale %1").arg(i);
            spec.addModule(Modules::dataSourceCamera(camName, p.width, p.height, p.fps));
            spec.addModule(Modules::videoTransformScale(tfName, 0.5, camName, QStringLiteral("frames-out")));
            spec.addModule(
                Modules::flowMeter(meterName(i), QStringLiteral("Frame"), tfName, QStringLiteral("frames-out")));
        }
        return spec;
    }

    StepVerdict evaluate(const QString &profileId, int level, const StepResult &result) const override
    {
        const auto &p = profile(profileId);
        QList<RateCheck> checks;
        for (int i = 1; i <= level; ++i)
            checks.append(RateCheck{meterName(i), static_cast<double>(p.fps)});
        return evaluateRates(result, checks);
    }
};

std::unique_ptr<Dimension> createCameraCapacityDimension()
{
    return std::make_unique<CameraCapacityDimension>();
}

} // namespace SyBench

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

#include "score.h"

#include <QJsonArray>
#include <cmath>

namespace SyBench
{

namespace
{

struct ReferenceEntry {
    const char *dimensionId;
    const char *profileId;
    int level;
};

/*
 * Reference machine (1000 points): development workstation, benchmark run of 2026-09-25.
 * AMD Ryzen 9 7900X (12 cores), 30 GiB RAM, NVMe SSD.
 */
constexpr ReferenceEntry kReference[] = {
    {"camera-capacity",   "1080p30",      36    },
    {"camera-capacity",   "720p120",      20    },
    {"encoding",          "1080p30-ffv1", 9     },
    {"encoding",          "1080p30-av1",  8     },
    {"disk-write",        "raw1080p30",   18    },
    {"disk-write",        "zarr-30khz",   5120  },
    {"signal-processing", "30khz-filter", 4096  },
    {"out-of-process",    "cpp-frames",   600   },
    {"out-of-process",    "python-rows",  448000},
    {"out-of-process",    "cpp-rows",     640000},
    {"mixed-tasks",       "mixed-chain",  62    },
};

int geometricMeanScore(const QList<double> &ratios)
{
    if (ratios.isEmpty())
        return 0;
    double logSum = 0;
    for (const double r : ratios)
        logSum += std::log(r);
    return static_cast<int>(std::lround(1000.0 * std::exp(logSum / ratios.size())));
}

} // namespace

int referenceLevel(const QString &dimensionId, const QString &profileId)
{
    for (const auto &e : kReference) {
        if (dimensionId == QLatin1String(e.dimensionId) && profileId == QLatin1String(e.profileId))
            return e.level;
    }
    return 0;
}

BenchmarkScore computeScore(const QList<LadderRecord> &ladders, const SessionConfig &config)
{
    BenchmarkScore score;
    score.quick = config.quick;

    // ratios per dimension, in the order of the reference table
    QList<QString> dimOrder;
    QHash<QString, QList<double>> ratiosByDim;
    QHash<QString, QString> titleByDim;
    QHash<QString, int> totalByDim;
    for (const auto &e : kReference) {
        const auto dim = QString::fromLatin1(e.dimensionId);
        if (!dimOrder.contains(dim))
            dimOrder.append(dim);
        totalByDim[dim]++;
    }

    QList<double> allRatios;
    for (const auto &lr : ladders) {
        if (lr.outcome.cancelled)
            continue;
        const int ref = referenceLevel(lr.dimensionId, lr.profileId);
        if (ref <= 0)
            continue;
        // a machine that sustains nothing still needs a finite ratio
        const double ratio = std::max(static_cast<double>(lr.outcome.sustained), 0.5) / ref;
        ratiosByDim[lr.dimensionId].append(ratio);
        titleByDim[lr.dimensionId] = lr.dimensionTitle;
        allRatios.append(ratio);
    }

    score.profilesTotal = static_cast<int>(std::size(kReference));
    score.profilesScored = allRatios.size();
    score.partial = score.profilesScored < score.profilesTotal;
    score.overall = geometricMeanScore(allRatios);

    for (const auto &dimId : dimOrder) {
        if (!ratiosByDim.contains(dimId))
            continue;
        DimensionScore ds;
        ds.dimensionId = dimId;
        ds.title = titleByDim.value(dimId);
        ds.profilesScored = ratiosByDim[dimId].size();
        ds.profilesTotal = totalByDim[dimId];
        ds.score = geometricMeanScore(ratiosByDim[dimId]);
        score.dimensions.append(ds);
    }
    return score;
}

QJsonObject scoreToJson(const BenchmarkScore &score)
{
    QJsonObject o;
    o.insert(QStringLiteral("overall"), score.overall);
    o.insert(QStringLiteral("partial"), score.partial);
    o.insert(QStringLiteral("quick"), score.quick);
    o.insert(QStringLiteral("profiles_scored"), score.profilesScored);
    o.insert(QStringLiteral("profiles_total"), score.profilesTotal);
    QJsonArray dims;
    for (const auto &d : score.dimensions) {
        QJsonObject dj;
        dj.insert(QStringLiteral("dimension"), d.dimensionId);
        dj.insert(QStringLiteral("score"), d.score);
        dj.insert(QStringLiteral("profiles_scored"), d.profilesScored);
        dj.insert(QStringLiteral("profiles_total"), d.profilesTotal);
        dims.append(dj);
    }
    o.insert(QStringLiteral("dimensions"), dims);
    return o;
}

QString scoreHeadline(const BenchmarkScore &score)
{
    if (!score.valid())
        return QStringLiteral("No score");
    QStringList notes;
    if (score.quick)
        notes.append(QStringLiteral("quick mode"));
    if (score.partial)
        notes.append(
            QStringLiteral("partial run, %1 of %2 profiles").arg(score.profilesScored).arg(score.profilesTotal));
    auto text = QStringLiteral("%1%2 points").arg(score.quick ? QStringLiteral("~ ") : QString()).arg(score.overall);
    if (!notes.isEmpty())
        text += QStringLiteral(" (%1)").arg(notes.join(QStringLiteral(", ")));
    return text;
}

} // namespace SyBench

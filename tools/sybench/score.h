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

#include <QJsonObject>
#include <QList>
#include <QString>

#include "benchsession.h"

namespace SyBench
{

struct DimensionScore {
    QString dimensionId;
    QString title;
    int score = 0;
    int profilesScored = 0;
    int profilesTotal = 0; /// profiles the reference table knows for this dimension
};

/**
 * @brief A single comparable number for a benchmark run
 *
 * The score is 1000 times the geometric mean of (sustained level / reference level)
 * over all profiles that ran, so a machine that sustains twice as much as the
 * reference everywhere scores 2000, and one weak dimension pulls the score down
 * proportionally. We use a rather powerful machine as reference.
 */
struct BenchmarkScore {
    int overall = 0;
    bool partial = false; /// not every reference profile ran, the score covers only part of the benchmark
    bool quick = false;   /// computed from a quick-mode run
    int profilesScored = 0;
    int profilesTotal = 0;
    QList<DimensionScore> dimensions;

    [[nodiscard]] bool valid() const
    {
        return profilesScored > 0;
    }
};

BenchmarkScore computeScore(const QList<LadderRecord> &ladders, const SessionConfig &config);

/**
 * @brief Reference level for a profile, or 0 if the profile is not part of the score.
 */
int referenceLevel(const QString &dimensionId, const QString &profileId);

QJsonObject scoreToJson(const BenchmarkScore &score);

/**
 * @brief Headline such as "1050 points" or "~ 980 points (quick mode, partial run)"
 */
QString scoreHeadline(const BenchmarkScore &score);

} // namespace SyBench

/*
 * Copyright (C) 2016-2017 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "utils.h"

#include <chrono>

using namespace std::chrono;

QString
ExperimentKind::toHumanString(ExperimentKind::Kind kind)
{
    switch (kind) {
    case ExperimentKind::KindMaze:
        return QStringLiteral("Maze");
    case ExperimentKind::KindRestingBox:
        return QStringLiteral("Resting Box");
    default:
        return QStringLiteral("Unknown");
    }
}

QString
ExperimentKind::toString(ExperimentKind::Kind kind)
{
    switch (kind) {
    case ExperimentKind::KindMaze:
        return QStringLiteral("maze");
    case ExperimentKind::KindRestingBox:
        return QStringLiteral("resting-box");
    default:
        return QStringLiteral("unknown");
    }
}

ExperimentKind::Kind
ExperimentKind::fromString(const QString& str)
{
    if (str == "maze")
        return ExperimentKind::KindMaze;
    else if (str == "resting-box")
        return ExperimentKind::KindRestingBox;
    else
        return ExperimentKind::KindUnknown;
}

time_t getMsecEpoch()
{
    auto ms = duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch());
    return ms.count();
}

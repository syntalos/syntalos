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

#include "ladder.h"

#include <algorithm>

namespace SyBench
{

LadderOutcome runLadder(const LadderConfig &cfg, const TryLevelFn &tryLevel)
{
    LadderOutcome out;
    const int maxLevel = std::max(1, cfg.maxLevel);
    int lo = 0; // highest level known to pass
    int hi = 0; // lowest level known to fail (0: none yet)

    const auto attempt = [&](int level) -> std::optional<bool> {
        const auto res = tryLevel(level);
        if (!res.has_value()) {
            out.cancelled = true;
            return std::nullopt;
        }
        out.steps.append(LadderStep{level, *res});
        if (*res)
            lo = std::max(lo, level);
        else
            hi = (hi == 0) ? level : std::min(hi, level);
        return res;
    };

    // doubling phase
    int level = std::clamp(cfg.startLevel, 1, maxLevel);
    while (true) {
        const auto res = attempt(level);
        if (!res.has_value())
            return out;
        if (!*res)
            break;
        if (level >= maxLevel) {
            out.reachedMax = true;
            break;
        }
        level = std::min(level * 2, maxLevel);
    }

    // halving phase, if even the start level failed
    while (lo == 0 && hi > 1) {
        const auto res = attempt(hi / 2);
        if (!res.has_value())
            return out;
        if (*res)
            break;
    }

    // refinement phase
    if (hi != 0) {
        for (int i = 0; i < cfg.bisections; ++i) {
            const int mid = (lo + hi) / 2;
            if (mid <= lo || mid >= hi)
                break;
            if (!attempt(mid).has_value())
                return out;
        }
    }

    out.sustained = lo;
    return out;
}

} // namespace SyBench

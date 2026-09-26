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
#include <cmath>

namespace SyBench
{

int nextLadderLevel(int level, const LevelOutcome &outcome)
{
    // a little backlog is noise, a growing one means the limit is near: a step that used half
    // of its allowance only creeps up by one unit, and the increase shrinks with the cube of
    // the used share on the way there (a tenth used still grows by 51 %, a fifth by 22 %)
    const double headroom = std::clamp(1.0 - 2.0 * outcome.backlogUse, 0.0, 1.0);
    double factor = 1.0 + headroom * headroom * headroom;

    // Queues stay empty as long as spare CPU absorbs the load, so the backlog gives no warning
    // until the machine is nearly full. The load itself does: the next level may need about
    // what the machine can sustain, a slight overload the memory guard can still stop, but
    // not multiples of it.
    if (outcome.cpuUse > 0)
        factor = std::min(factor, 1.0 / outcome.cpuUse);
    // memory grows with the level as well, mostly through worker processes; leave a margin
    if (outcome.memoryUse > 0)
        factor = std::min(factor, 0.8 / outcome.memoryUse);

    // always move on, in steps of at least a tenth: near the limit the backlog rule takes over,
    // and a tenth of overload is what the memory guard handles comfortably
    const int minStep = std::max(1, level / 10);
    const auto next = static_cast<int>(std::lround(level * factor));
    return std::max(next, level + minStep);
}

LadderOutcome runLadder(const LadderConfig &cfg, const TryLevelFn &tryLevel)
{
    LadderOutcome out;
    const int maxLevel = std::max(1, cfg.maxLevel);
    int lo = 0;           // highest level known to pass
    int hi = 0;           // lowest level known to fail (0: none yet)
    int lowestFailed = 0; // lowest level that failed for real (not source-limited)

    // returns false if the search has to stop
    LevelOutcome lastOutcome{LevelResult::Failed};
    const auto attempt = [&](int level, bool &passed) {
        lastOutcome = tryLevel(level);
        const auto res = lastOutcome.result;
        if (res == LevelResult::Cancelled) {
            out.cancelled = true;
            out.sustained = lo;
            return false;
        }
        // an inconclusive step (the data source itself could not keep up) bounds the search
        // from above like a failure, so we still refine towards the highest level that works
        passed = res == LevelResult::Passed;
        if (passed) {
            lo = std::max(lo, level);
        } else {
            hi = (hi == 0) ? level : std::min(hi, level);
            if (res == LevelResult::Failed)
                lowestFailed = (lowestFailed == 0) ? level : std::min(lowestFailed, level);
        }
        // the result is only source-limited if no real failure bounds it from above
        out.inconclusive = hi != 0 && lowestFailed != hi;
        return true;
    };

    // growing phase
    int level = std::clamp(cfg.startLevel, 1, maxLevel);
    while (true) {
        bool passed = false;
        if (!attempt(level, passed))
            return out;
        if (!passed)
            break;
        if (level >= maxLevel) {
            out.reachedMax = true;
            break;
        }
        level = std::min(nextLadderLevel(level, lastOutcome), maxLevel);
    }

    // halving phase, if even the start level failed
    while (lo == 0 && hi > 1) {
        bool passed = false;
        if (!attempt(hi / 2, passed))
            return out;
        if (passed)
            break;
    }

    // refinement phase
    if (hi != 0) {
        for (int i = 0; i < cfg.bisections; ++i) {
            const int mid = (lo + hi) / 2;
            if (mid <= lo || mid >= hi)
                break;
            bool passed = false;
            if (!attempt(mid, passed))
                return out;
        }
    }

    out.sustained = lo;
    return out;
}

} // namespace SyBench

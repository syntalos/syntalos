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

#include <functional>

namespace SyBench
{

/**
 * @brief Search strategy for the highest sustainable level of a dimension
 */
struct LadderConfig {
    int startLevel = 1;
    int maxLevel = 4096;
    int bisections = 4;      /// at most this many refinement steps between the last pass and the first failure
    double targetGap = 0.05; /// stop refining once the first failure is within this share above the last pass
};

struct LadderOutcome {
    int sustained = 0; /// highest level that passed, 0 if even the lowest tried level failed (partial if cancelled)
    bool cancelled = false;
    bool inconclusive = false; /// the boundary was a source limit, not a failure: the result is a lower bound
    bool reachedMax = false;   /// the maximum level passed, the true limit is higher
};

/**
 * @brief What trying one level yielded
 */
enum class LevelResult {
    Passed,
    Failed,
    Inconclusive, /// the step could not be judged (source limit); treated as the upper boundary
    Cancelled
};

/**
 * @brief What trying one level yielded, with hints at how close to the limit it was
 *
 * Each share is 0 when the resource was untouched and 1 when it was used up.
 */
struct LevelOutcome {
    LevelResult result;
    double backlogUse = 0; /// share of the allowed backlog the step used
    double cpuUse = 0;     /// CPU load relative to what the machine can sustain
    double memoryUse = 0;  /// peak memory relative to what the machine has available

    LevelOutcome(LevelResult r, double backlog = 0, double cpu = 0, double memory = 0)
        : result(r),
          backlogUse(backlog),
          cpuUse(cpu),
          memoryUse(memory)
    {
    }
};

using TryLevelFn = std::function<LevelOutcome(int level)>;

/**
 * @brief Level to try after the given one passed with the given outcome.
 *
 * A step with no backlog doubles the level; the more of the allowance a step used, the smaller
 * the next increase. Queues stay empty while spare CPU absorbs the load, so the CPU load and
 * memory of the step are extrapolated as well, and the next level may need about what the
 * machine has but not more. The smallest of the three increases wins, but the level always
 * grows by at least a tenth.
 */
int nextLadderLevel(int level, const LevelOutcome &outcome);

/**
 * @brief Run a growing search from the start level, then bisect between the last
 * pass and the first failure.
 *
 * If the start level already fails, the level is halved until one passes (or level 1 fails).
 */
LadderOutcome runLadder(const LadderConfig &cfg, const TryLevelFn &tryLevel);

} // namespace SyBench

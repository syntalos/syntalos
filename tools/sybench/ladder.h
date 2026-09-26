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
    int bisections = 2; /// refinement steps between the last pass and the first failure
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
 * Each share is 0 when the resource was untouched and 1 when it was used up. The search
 * uses them to pick the next level: a step that came close to a limit is followed by a
 * small increase, so the first failure lands near the limit instead of far beyond it.
 */
struct LevelOutcome {
    LevelResult result;
    /// share of the allowed backlog the step used up; a step that needed part of its
    /// allowance is close to the limit
    double backlogUse = 0;
    /// CPU load of the step relative to what the machine can sustain; load grows with the
    /// level, and a level that would need far more than that takes the machine down before
    /// the step can fail
    double cpuUse = 0;
    /// peak memory of the step relative to what the machine has available for it
    double memoryUse = 0;

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
 * the next increase, down to a single unit. The CPU load and memory of the step are extrapolated
 * to the next level, which may not need much more than the machine has: whichever of the three
 * allows the smallest increase wins.
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

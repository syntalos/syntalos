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
 * @brief What trying one level yielded, with a hint at how close to the limit it was
 */
struct LevelOutcome {
    LevelResult result;
    /// share of the allowed backlog the step used up (0 = none, 1 = at the limit); a step
    /// that needed part of its allowance is close to the limit, and the search slows down
    double backlogUse = 0;

    LevelOutcome(LevelResult r, double use = 0)
        : result(r),
          backlogUse(use)
    {
    }
};

using TryLevelFn = std::function<LevelOutcome(int level)>;

/**
 * @brief Level to try after the given one passed with the given share of its backlog allowance used.
 *
 * A step with no backlog doubles the level; the more of the allowance a step used, the smaller
 * the next increase, down to a single unit. This keeps the first failure close to the limit,
 * instead of overshooting into a level that overwhelms the machine.
 */
int nextLadderLevel(int level, double backlogUse);

/**
 * @brief Run a growing search from the start level, then bisect between the last
 * pass and the first failure.
 *
 * If the start level already fails, the level is halved until one passes (or level 1 fails).
 */
LadderOutcome runLadder(const LadderConfig &cfg, const TryLevelFn &tryLevel);

} // namespace SyBench

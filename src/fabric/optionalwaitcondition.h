/*
 * Copyright (C) 2019-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QSharedPointer>

namespace Syntalos
{

#if !defined(DOXYGEN_SHOULD_SKIP_THIS)
class AbstractModule;
#endif // DOXYGEN_SHOULD_SKIP_THIS

/**
 * @brief The OptionalWaitCondition class
 *
 * Create a thread barrier to synchronize a
 * set of threads to run at once.
 *
 * Threads park in wait() until the owner releases the barrier via wakeAll().
 * The barrier is "optional" in the sense that a thread which arrives after
 * the barrier was already released passes through immediately instead of
 * blocking forever.
 *
 * The module-aware wait(AbstractModule*) variant flags the module as READY
 * as part of parking the thread: once a module is seen in the READY state,
 * its thread is guaranteed to receive the next wakeAll() (or has already
 * passed the barrier). This is the property the engine relies on to decide
 * when it is safe to start a run.
 */
class OptionalWaitCondition
{
    friend class Engine;
    friend class TestWaitCondition;

public:
    OptionalWaitCondition();

    void wait();
    void wait(AbstractModule *mod);

    /**
     * @brief Number of threads that parked on this barrier so far (diagnostic).
     */
    uint waitingCount() const;

    /**
     * @brief Whether the barrier has already been released via wakeAll().
     */
    bool isReleased() const;

private:
    class OWCData;
    std::shared_ptr<OWCData> d;
    Q_DISABLE_COPY(OptionalWaitCondition)

    /**
     * @brief Release the barrier, waking all parked threads.
     * Only the owner of the barrier (usually the engine) should call this.
     */
    void wakeAll();

    /**
     * @brief Re-arm the barrier so it can be waited on again.
     * Only valid while no thread is parked on the barrier.
     */
    void reset();
};

} // namespace Syntalos

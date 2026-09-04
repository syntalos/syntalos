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

#include <QMutex>
#include <QWaitCondition>

#include "moduleapi.h"
#include "optionalwaitcondition.h"

using namespace Syntalos;

class OptionalWaitCondition::OWCData
{
public:
    OWCData()
        : ready(false),
          count(0)
    {
    }

    // Both fields are only ever modified while holding the mutex. The barrier
    // state is read under the mutex as well; the waiter count is atomic so that
    // it can be inspected for diagnostics without contending the lock.
    bool ready;
    std::atomic_uint count;
    QMutex mutex;
    QWaitCondition condition;

private:
    Q_DISABLE_COPY(OWCData)
};

OptionalWaitCondition::OptionalWaitCondition()
    : d(new OptionalWaitCondition::OWCData())
{
}

void OptionalWaitCondition::wait()
{
    QMutexLocker locker(&d->mutex);
    if (d->ready)
        return;

    d->count++;
    // loop guards against spurious wakeups
    while (!d->ready)
        d->condition.wait(&d->mutex);
}

void OptionalWaitCondition::wait(AbstractModule *mod)
{
    QMutexLocker locker(&d->mutex);
    if (d->ready) {
        // we are late to the party - the barrier was already released.
        // Still flag the module as ready, this is mandatory.
        mod->setStateReady();
        return;
    }

    // Mark the module as ready *while holding the mutex*: wakeAll() needs the same
    // mutex, so by the time the engine can release the barrier this thread is
    // guaranteed to be parked in condition.wait() (which releases the mutex atomically)
    // and will therefore receive the wakeup. This is what allows the engine to treat
    // the READY state as "this thread will be woken by the next wakeAll()".
    d->count++;
    mod->setStateReady();
    while (!d->ready)
        d->condition.wait(&d->mutex);
}

uint OptionalWaitCondition::waitingCount() const
{
    return d->count.load();
}

bool OptionalWaitCondition::isReleased() const
{
    QMutexLocker locker(&d->mutex);
    return d->ready;
}

void OptionalWaitCondition::wakeAll()
{
    QMutexLocker locker(&d->mutex);
    d->ready = true;
    d->condition.wakeAll();
}

void OptionalWaitCondition::reset()
{
    QMutexLocker locker(&d->mutex);
    d->ready = false;
    d->count = 0;
}

/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include "optionalwaitcondition.h"
#include "moduleapi.h"

using namespace Syntalos;

class OptionalWaitCondition::OWCData
{
public:
    OWCData() {}

    std::atomic_bool ready;
    uint count;
    QMutex mutex;
    QWaitCondition condition;

private:
    Q_DISABLE_COPY(OWCData)
};

OptionalWaitCondition::OptionalWaitCondition()
    : d(new OptionalWaitCondition::OWCData())
{
    d->ready = false;
}

void OptionalWaitCondition::wait()
{
    if (d->ready)
        return;
    d->mutex.lock();
    d->count++;
    d->condition.wait(&d->mutex);
    d->mutex.unlock();
}

void OptionalWaitCondition::wait(AbstractModule *mod)
{
    mod->setStateReady();
    wait();
}

uint OptionalWaitCondition::waitingCount() const
{
    return d->count;
}

void OptionalWaitCondition::wakeAll()
{
    d->condition.wakeAll();
    d->ready = true;
}

void OptionalWaitCondition::reset()
{
    d->ready = false;
    d->count = 0;
}

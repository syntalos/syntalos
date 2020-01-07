/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include <QMutex>
#include <QWaitCondition>

#include "optionalwaitcondition.h"
#include "moduleapi.h"

class OptionalWaitCondition::OWCData
{
public:
    OWCData() {}

    uint count;
    QMutex mutex;
    QWaitCondition condition;

private:
    Q_DISABLE_COPY(OWCData)
};

OptionalWaitCondition::OptionalWaitCondition()
    : d(new OptionalWaitCondition::OWCData())
{}

void OptionalWaitCondition::wait()
{
    d->mutex.lock();
    d->count++;
    d->condition.wait(&d->mutex);
    d->mutex.unlock();
}

void OptionalWaitCondition::wait(AbstractModule *mod)
{
    mod->setState(ModuleState::READY);
    wait();
}

uint OptionalWaitCondition::waitingCount() const
{
    return d->count;
}

void OptionalWaitCondition::wakeAll()
{
    d->condition.wakeAll();
}

void OptionalWaitCondition::reset()
{
    d->count = 0;
}

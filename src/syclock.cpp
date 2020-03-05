/*
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include "syclock.h"

#include <time.h>
#include <QDebug>

using namespace Syntalos;

#ifdef SYNTALOS_USE_RAW_MONOTONIC_TIME
#define STEADY_CLOCK_ID CLOCK_MONOTONIC_RAW
#else
#define STEADY_CLOCK_ID CLOCK_MONOTONIC
#endif

symaster_clock::time_point symaster_clock::now() noexcept
{
    ::timespec tp;
    // -EINVAL, -EFAULT

    ::clock_gettime(STEADY_CLOCK_ID, &tp);
    return time_point(duration(std::chrono::seconds(tp.tv_sec)
             + std::chrono::nanoseconds(tp.tv_nsec)));
}

SyncTimer::SyncTimer()
    : m_started(false)
{}

void SyncTimer::start() noexcept
{
    // we should probably crash here, but let's show a warning for now
    if (m_started)
        qCritical().noquote() << "The master sync timer was restarted after it was already running! This must never happen.";

    m_startTime = symaster_clock::now();
    m_started = true;
}

void SyncTimer::startAt(const Syntalos::symaster_timepoint &startTimePoint) noexcept
{
    if (m_started)
        qCritical().noquote() << "The master sync timer was restarted after it was already running! This must never happen.";

    m_startTime = startTimePoint;
    m_started = true;
}

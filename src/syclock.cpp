/*
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include "syclock.h"

#include <time.h>
#include <QDebug>

Q_LOGGING_CATEGORY(logTimeClock, "time.clock")

using namespace Syntalos;
using namespace Eigen;

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
{
    // Synchronizers and other timer-dependent classes may need to send these types
    // in queued connections, so we ensure metatypes are registered for them
    qRegisterMetaType<std::chrono::milliseconds>();
    qRegisterMetaType<std::chrono::microseconds>();
}

void SyncTimer::start() noexcept
{
    // we should probably crash here, but let's show a warning for now
    if (m_started)
        qCCritical(logTimeClock).noquote() << "The master sync timer was restarted after it was already running! This must never happen.";

    m_startTime = symaster_clock::now();
    m_started = true;
}

void SyncTimer::startAt(const Syntalos::symaster_timepoint &startTimePoint) noexcept
{
    if (m_started)
        qCCritical(logTimeClock).noquote() << "The master sync timer was restarted after it was already running! This must never happen.";

    m_startTime = startTimePoint;
    m_started = true;
}

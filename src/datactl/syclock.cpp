/*
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include <ctime>

#include "eigenaux.h"
#include "loginternal.h"

using namespace Syntalos;
using namespace Eigen;

#ifdef SYNTALOS_USE_RAW_MONOTONIC_TIME
#define STEADY_CLOCK_ID CLOCK_MONOTONIC_RAW
#else
#define STEADY_CLOCK_ID CLOCK_MONOTONIC
#endif

SY_DEFINE_LOG_CATEGORY(logTimeClock, "time.clock");

symaster_clock::time_point symaster_clock::now() noexcept
{
    ::timespec tp;
    // -EINVAL, -EFAULT

    ::clock_gettime(STEADY_CLOCK_ID, &tp);
    return time_point(duration(std::chrono::seconds(tp.tv_sec) + std::chrono::nanoseconds(tp.tv_nsec)));
}

SyncTimer::SyncTimer()
    : m_started(false)
{
}

void SyncTimer::start() noexcept
{
    // we should probably crash here, but let's show a warning for now
    if (m_started)
        SY_LOG_CRITICAL(
            logTimeClock, "The master sync timer was restarted after it was already running! This must never happen.");

    // capture both clocks as close together as possible so startWallTime() is a
    // reliable absolute reference for cross-device alignment
    m_startTime = symaster_clock::now();
    m_startWallTime = std::chrono::system_clock::now();
    m_started = true;
}

void SyncTimer::startAt(const Syntalos::symaster_timepoint &startTimePoint) noexcept
{
    if (m_started)
        SY_LOG_CRITICAL(
            logTimeClock, "The master sync timer was restarted after it was already running! This must never happen.");

    // Back-compute the wall-clock start from how long ago the master clock started.
    // Both clocks tick at nanosecond resolution with (hopefully!) negligible relative drift,
    // so the same elapsed duration serves as a ruler for both.
    const auto elapsed = symaster_clock::now() - startTimePoint;
    m_startWallTime = std::chrono::system_clock::now() - elapsed;
    m_startTime = startTimePoint;
    m_started = true;
}

void SyncTimer::startAtWallTime(const std::chrono::system_clock::time_point &startWallTime) noexcept
{
    if (m_started)
        SY_LOG_CRITICAL(
            logTimeClock, "The master sync timer was restarted after it was already running! This must never happen.");

    // Back-compute the master-clock start from how long ago the wall clock started.
    // This allows multiple devices in a networked system to share a common t=0.
    const auto elapsed = std::chrono::system_clock::now() - startWallTime;
    m_startTime = symaster_clock::now() - std::chrono::duration_cast<symaster_clock::duration>(elapsed);
    m_startWallTime = startWallTime;
    m_started = true;
}

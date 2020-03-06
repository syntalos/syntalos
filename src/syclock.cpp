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
#include <iostream>

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

FreqCounterSynchronizer::FreqCounterSynchronizer()
    : m_valid(false)
{}

FreqCounterSynchronizer::FreqCounterSynchronizer(std::shared_ptr<SyncTimer> masterTimer, double frequencyHz)
    : m_valid(true),
      m_syTimer(masterTimer),
      m_freq(frequencyHz),
      m_lastBaseTime(0)
{}

bool FreqCounterSynchronizer::isValid() const
{
    return m_valid;
}

void FreqCounterSynchronizer::adjustTimestamps(const milliseconds_t &recvTimestamp, const double &devLatencyMs, VectorXu &idxTimestamps)
{
    // we want the device latency in microseconds
    auto deviceLatency = std::chrono::microseconds(static_cast<int>(devLatencyMs * 1000));
    adjustTimestamps(recvTimestamp, deviceLatency, idxTimestamps);
}

void FreqCounterSynchronizer::adjustTimestamps(const milliseconds_t &recvTimestamp, const std::chrono::microseconds &deviceLatency, VectorXu &idxTimestamps)
{
    // timestamp when (as good as we can tell...) the data was actually acquired, in milliseconds
    const auto assumedAcqTS = std::chrono::duration_cast<milliseconds_t>(recvTimestamp - deviceLatency);
    if (m_lastBaseTime.count() == 0)
        m_lastBaseTime = assumedAcqTS;

    // guess the actual timestamps in relation to the received timestamp in milliseconds for the given index vector
    VectorXd times = (idxTimestamps.cast<double>() / m_freq) * 1000.0;
    times += m_lastBaseTime.count() * VectorXd::Ones(times.rows());

    // calculate current offset
    const auto lastTimestamp = std::chrono::microseconds(static_cast<int64_t>(std::round(times[times.rows() - 1] * 1000.0)));
    const auto timeOffsetUsec = (std::chrono::duration_cast<std::chrono::microseconds>(lastTimestamp - assumedAcqTS)).count();


    if (std::abs(timeOffsetUsec) < SECONDARY_CLOCK_TOLERANCE_C)
        return;

    qDebug().nospace() << "Timer offset of " << timeOffsetUsec / 1000 << "ms";

#if 0
    qDebug().nospace() << "Freq: " << m_freq << "Hz "
                       << "Timer offset of " << timeOffsetUsec / 1000 << "ms "
                       << "LastECTS: " << lastTimestamp.count() << "µs "
                       << "RecvTS: " << recvTimestamp.count() << "ms "
                       << "AssumedAcqTS: " << assumedAcqTS.count() << "ms ";

    //qDebug() << "TIMES:" << times[0] << times[times.rows() - 1];
    //qDebug() << "IDX:  " << idxTimestamps[0] << idxTimestamps[idxTimestamps.rows() - 1];
#endif
}

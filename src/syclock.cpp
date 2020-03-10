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
    : m_valid(false),
      m_checkInterval(DEFAULT_CLOCKSYNC_CHECK_INTERVAL)
{}

FreqCounterSynchronizer::FreqCounterSynchronizer(std::shared_ptr<SyncTimer> masterTimer, double frequencyHz)
    : m_valid(true),
      m_isFirstInterval(true),
      m_syTimer(masterTimer),
      m_checkInterval(DEFAULT_CLOCKSYNC_CHECK_INTERVAL),
      m_lastUpdateTime(milliseconds_t(DEFAULT_CLOCKSYNC_CHECK_INTERVAL * -1)),
      m_freq(frequencyHz),
      m_baseTime(0),
      m_indexOffset(0)
{
    m_checkInterval = std::chrono::seconds(1);
}

milliseconds_t FreqCounterSynchronizer::timeBase() const
{
    return m_baseTime;
}

int FreqCounterSynchronizer::indexOffset() const
{
    return m_indexOffset;
}

void FreqCounterSynchronizer::setCheckInterval(const std::chrono::seconds &intervalSec)
{
    m_checkInterval = intervalSec;
}

void FreqCounterSynchronizer::adjustTimestamps(const milliseconds_t &recvTimestamp, const double &devLatencyMs, VectorXl &idxTimestamps)
{
    // we want the device latency in microseconds
    auto deviceLatency = std::chrono::microseconds(static_cast<int>(devLatencyMs * 1000));
    adjustTimestamps(recvTimestamp, deviceLatency, idxTimestamps);
}

void FreqCounterSynchronizer::adjustTimestamps(const milliseconds_t &recvTimestamp, const std::chrono::microseconds &deviceLatency, VectorXl &idxTimestamps)
{
    // adjust timestamp based on our current offset
    if (m_indexOffset != 0)
        idxTimestamps += m_indexOffset * VectorXl::Ones(idxTimestamps.rows());

    // do nothing if we aren't checking the timestamp for validity yet
    const auto currentTimestamp = m_syTimer->timeSinceStartMsec();
    if (!m_isFirstInterval && ((currentTimestamp - m_lastUpdateTime) < m_checkInterval))
        return;
    m_lastUpdateTime = currentTimestamp;

    // timestamp when (as far and well as we can tell...) the data was actually acquired, in milliseconds
    const auto assumedAcqTS = std::chrono::duration_cast<milliseconds_t>(recvTimestamp - deviceLatency);

    // set initial device timebase, if we don't have one yet
    if (m_baseTime.count() == 0)
        m_baseTime = assumedAcqTS;

    // guess the actual timestamps in relation to the received timestamp in milliseconds for the given index vector
    VectorXd times = (idxTimestamps.cast<double>() / m_freq) * 1000.0;
    times += m_baseTime.count() * VectorXd::Ones(times.rows());

    // calculate current offset
    const auto lastTimestamp = std::chrono::microseconds(static_cast<int64_t>(std::round(times[times.rows() - 1] * 1000.0)));
    const auto timeOffsetUsec = (std::chrono::duration_cast<std::chrono::microseconds>(assumedAcqTS - lastTimestamp)).count();

    if (std::abs(timeOffsetUsec) < SECONDARY_CLOCK_TOLERANCE_US) {
        // everything is within tolerance range, no adjustments needed
        m_isFirstInterval = false;
        return;
    }

    qDebug().nospace() << "Freq: " << m_freq << "Hz "
                       << "Timer offset of " << timeOffsetUsec / 1000 << "ms "
                       << "LastECTS: " << lastTimestamp.count() << "µs "
                       << "RecvTS: " << recvTimestamp.count() << "ms "
                       << "AssumedAcqTS: " << assumedAcqTS.count() << "ms ";

    qDebug() << "TIMES:" << times[0] << times[times.rows() - 1];
    qDebug() << "IDX:  " << idxTimestamps[0] << idxTimestamps[idxTimestamps.rows() - 1];

    // offset the device time index by a much smaller amount of what is needed to sync up the clocks
    // if this doesn't bring us back within tolerance, we'll adjust the index offset again
    // the next time this function is run
    // how slowly the external timestamps ares adjusted depends on the DAQ frequency - the slower it runs,
    // the faster we adjust it.
    int change = std::floor(((timeOffsetUsec / 1000.0 / 1000.0) * m_freq) / (m_freq / 20000 + 1));

    if (timeOffsetUsec > 0) {
        // the external device is running too slow

        m_indexOffset += change;
        qWarning().nospace() << "Index offset changed by " << change << " to " << m_indexOffset << " (in raw idx: " << (timeOffsetUsec / 1000.0 / 1000.0) * m_freq << ")";
    } else {
        qWarning() << "External device is too fast!";

        if (m_isFirstInterval) {
            change = change * 1.5;
            m_indexOffset += change;
            qWarning().nospace() << "Index offset changed by " << change << " to " << m_indexOffset << " (in raw idx: " << (timeOffsetUsec / 1000.0 / 1000.0) * m_freq << ")";
        } else {
            qWarning().nospace() << "Would change index offset by " << change << " to " << m_indexOffset << " (in raw idx: " << (timeOffsetUsec / 1000.0 / 1000.0) * m_freq << "), but change not possible";
        }
    }

    // adjust time indices again based on the current change
    if (change != 0)
        idxTimestamps += change * VectorXl::Ones(idxTimestamps.rows());

    m_isFirstInterval = false;
    qDebug().noquote() << "";
}

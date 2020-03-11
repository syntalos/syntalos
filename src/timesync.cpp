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

#include "timesync.h"

#include <QDebug>
#include "moduleapi.h"

#include "utils.h"

using namespace Syntalos;
using namespace Eigen;

FreqCounterSynchronizer::FreqCounterSynchronizer(std::shared_ptr<SyncTimer> masterTimer, AbstractModule *mod, double frequencyHz, const QString &id)
    : m_mod(mod),
      m_id(id),
      m_isFirstInterval(true),
      m_syTimer(masterTimer),
      m_toleranceUsec(SECONDARY_CLOCK_TOLERANCE.count()),
      m_checkInterval(DEFAULT_CLOCKSYNC_CHECK_INTERVAL),
      m_lastUpdateTime(milliseconds_t(DEFAULT_CLOCKSYNC_CHECK_INTERVAL * -1)),
      m_freq(frequencyHz),
      m_baseTime(0),
      m_indexOffset(0)
{
    if (m_id.isEmpty())
        m_id = createRandomString(4);
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
    if (!m_isFirstInterval) {
        qWarning().noquote() << "Rejected check-interval change on active FreqCounter Synchronizer for" << m_mod->name();
        return;
    }
    m_checkInterval = intervalSec;
    emit m_mod->synchronizerDetailsChanged(m_id, std::chrono::microseconds(m_toleranceUsec), m_checkInterval);
}

void FreqCounterSynchronizer::setTolerance(const std::chrono::microseconds &tolerance)
{
    if (!m_isFirstInterval) {
        qWarning().noquote() << "Rejected tolerance change on active FreqCounter Synchronizer for" << m_mod->name();
        return;
    }
    m_toleranceUsec = tolerance.count();
    emit m_mod->synchronizerDetailsChanged(m_id, std::chrono::microseconds(m_toleranceUsec), m_checkInterval);
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
        idxTimestamps += VectorXl::LinSpaced(idxTimestamps.rows(), 0, m_indexOffset);

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
    const auto timeOffset = (std::chrono::duration_cast<std::chrono::microseconds>(assumedAcqTS - lastTimestamp));
    const auto timeOffsetUsec = timeOffset.count();

    // TODO: Emit current offset here, occasionally
    emit m_mod->synchronizerOffsetChanged(m_id, timeOffset);

    if (std::abs(timeOffsetUsec) < m_toleranceUsec) {
        // everything is within tolerance range, no adjustments needed
        m_isFirstInterval = false;
        return;
    }

    qDebug().nospace() << "Freq: " << m_freq / 1000 << "kHz "
                       << "Timer offset of " << timeOffsetUsec / 1000 << "ms "
                       << "LastECTS: " << lastTimestamp.count() << "µs "
                       << "RecvTS: " << recvTimestamp.count() << "ms "
                       << "AssumedAcqTS: " << assumedAcqTS.count() << "ms ";

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
            // we change the initial index to match the master clock exactly
            change = std::round((timeOffsetUsec / 1000.0 / 1000.0) * m_freq);
            m_indexOffset += change;
            qWarning().nospace() << "Index offset changed by " << change << " to " << m_indexOffset;
        } else {
            qWarning().nospace() << "Would change index offset by " << change << " to " << m_indexOffset << " (in raw idx: " << (timeOffsetUsec / 1000.0 / 1000.0) * m_freq << "), but change not possible";
        }
    }

    // adjust time indices again based on the current change
    if (change != 0)
        idxTimestamps += VectorXl::LinSpaced(idxTimestamps.rows(), 0, change);

    m_isFirstInterval = false;
    qDebug().noquote() << "";
}

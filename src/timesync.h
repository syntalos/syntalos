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

#pragma once

#include <memory>
#include <QString>
#include <QMetaType>
#include <QDataStream>

#include "syclock.h"
#include "eigenaux.h"

namespace Syntalos {

class AbstractModule;

/**
 * @brief The time synchronization strategy
 */
enum class TimeSyncStrategy {
    INVALID = 0,
    SHIFT_TIMESTAMPS_FWD = 1 << 0, /// Move timestamps forward to match the master clock
    SHIFT_TIMESTAMPS_BWD = 1 << 1, /// Move timestamps backward to match the master clock
    ADJUST_CLOCK         = 1 << 2, /// Do not change timestamps by adjust the secondary clocks to match the master clock
    WRITE_TSYNCFILE      = 1 << 3  /// Do not directly adjust timestamps, but write a time-sync file to correct for errors in postprocessing
};
Q_DECLARE_FLAGS(TimeSyncStrategies, TimeSyncStrategy)
Q_DECLARE_OPERATORS_FOR_FLAGS(TimeSyncStrategies)

const QString timeSyncStrategyToHString(const TimeSyncStrategy &strategy);
const QString timeSyncStrategiesToHString(const TimeSyncStrategies &strategies);

} // end of namespace

Q_DECLARE_METATYPE(Syntalos::TimeSyncStrategies);

namespace Syntalos {

class TimeSyncFileWriter
{
public:
    explicit TimeSyncFileWriter();

private:
    QDataStream m_stream;
};

/**
 * @brief Synchronizer for a monotonic counter, given a frequency
 *
 * This synchronizer helps synchronizing the counting of a monotonic counter
 * (e.g. adding an increasing index number to signals/frames/etc. from a starting point)
 * to the master clock if we know a sampling frequency for the counter.
 *
 * The adjusted counter is guaranteed to never move backwards, but gaps and identical timestamps
 * (depending on the settings) may occur.
 */
class FreqCounterSynchronizer
{
public:
    explicit FreqCounterSynchronizer(std::shared_ptr<SyncTimer> masterTimer, AbstractModule *mod, double frequencyHz, const QString &id = nullptr);

    milliseconds_t timeBase() const;
    int indexOffset() const;

    void setStrategies(const TimeSyncStrategies &strategies);
    void setCheckInterval(const std::chrono::seconds &intervalSec);
    void setTolerance(const std::chrono::microseconds &tolerance);
    void setTimeSyncFilename(const QString &fname);

    void adjustTimestamps(const milliseconds_t &recvTimestamp, const double &devLatencyMs, VectorXl &idxTimestamps);
    void adjustTimestamps(const milliseconds_t &recvTimestamp, const std::chrono::microseconds &deviceLatency, VectorXl &idxTimestamps);

private:
    AbstractModule *m_mod;
    QString m_id;
    TimeSyncStrategies m_strategies;
    bool m_isFirstInterval;
    std::shared_ptr<SyncTimer> m_syTimer;

    uint m_toleranceUsec;
    std::chrono::seconds m_checkInterval;
    milliseconds_t m_lastUpdateTime;

    double m_freq;
    milliseconds_t m_baseTime;
    int m_indexOffset;
};

class SecondaryClockSynchronizer {

};

} // end of namespace

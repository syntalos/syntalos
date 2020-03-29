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

class QFile;

namespace Syntalos {

class AbstractModule;

/**
 * @brief The amount of time a secondary clock is allowed to deviate from the master.
 *
 * Since Syntalos uses millisecond time resolution, permitting (slightly more than)
 * half a millisecond deviation for secondary clocks from the master clock is sensible.
 *
 * IMPORTANT: Modules may override this value for their synchronizers to one that fits their
 * device better. This is just a default for modules which do not change the default setting.
 */
static constexpr auto SECONDARY_CLOCK_TOLERANCE = microseconds_t(1000);

/**
 * @brief Interval at which we check for external clock synchronization
 *
 * IMPORTANT: This is just a default value for modules which do not explicitly define a check
 * interval. Individual modules may choose a different value that fits the device they are
 * communicating with best.
 */
static constexpr auto DEFAULT_CLOCKSYNC_CHECK_INTERVAL = milliseconds_t(4000);

/**
 * @brief The time synchronization strategy
 */
enum class TimeSyncStrategy {
    NONE = 0,
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

/**
 * @brief Unit types for time representation in a TSync file
 */
enum class TimeSyncFileTimeUnit
{
    INDEX,
    MICROSECONDS,
    MILLISECONDS,
    SECONDS
};

QString timeSyncFileTimeUnitToString(const TimeSyncFileTimeUnit &tsftunit);

/**
 * @brief Write a timestamp synchronization file
 *
 * Helper class to write a timestamp synchronization file to adjust
 * timestamps in a recording post-hoc. This is commonly used if the
 * format data is stored in does not support timestamp adjustments, or
 * as additional set of datapoints to ensure timestamps are really
 * synchronized.
 */
class TimeSyncFileWriter
{
public:
    explicit TimeSyncFileWriter();
    ~TimeSyncFileWriter();

    QString lastError() const;

    void setTimeNames(QPair<QString, QString> pair);
    void setTimeUnits(QPair<TimeSyncFileTimeUnit, TimeSyncFileTimeUnit> pair);
    void setFileName(const QString &fname);
    bool open(const microseconds_t &checkInterval, const microseconds_t &tolerance, const QString &modName);
    void flush();
    void close();

    void writeTimes(const microseconds_t &deviceTime, const microseconds_t &masterTime);
    void writeTimes(const long long &timeIndex, const long long &masterTime);

private:
    QFile *m_file;
    QDataStream m_stream;
    int m_index;
    QString m_lastError;
    QPair<QString, QString> m_timeNames;
    QPair<TimeSyncFileTimeUnit, TimeSyncFileTimeUnit> m_timeUnits;
};

/**
 * @brief Read a time-sync (.tsync) file
 *
 * Simple helper class to read the contents of a .tsync file,
 * for adjustments of the source timestamps or simply conversion
 * into a non-binary format.
 */
class TimeSyncFileReader
{
public:
    explicit TimeSyncFileReader();

    bool open(const QString &fname);
    QString lastError() const;

    QString moduleName() const;
    time_t creationTime() const;
    microseconds_t checkInterval() const;
    microseconds_t tolerance() const;
    QPair<QString, QString> timeNames() const;
    QPair<TimeSyncFileTimeUnit, TimeSyncFileTimeUnit> timeUnits() const;

    QList<QPair<long long, long long>> times() const;

private:
    QString m_lastError;
    QString m_moduleName;
    qint64 m_creationTime;
    microseconds_t m_checkInterval;
    microseconds_t m_tolerance;
    QList<QPair<long long, long long>> m_times;
    QPair<QString, QString> m_timeNames;
    QPair<TimeSyncFileTimeUnit, TimeSyncFileTimeUnit> m_timeUnits;
};

/**
 * @brief Synchronizer for a monotonic counter, given a frequency
 *
 * This synchronizer helps synchronizing the counting of a monotonic counter
 * (e.g. adding an increasing index number to signals/frames/etc. from a starting point)
 * to the master clock if we know a sampling frequency for the counter.
 *
 * Depending on the permitted strategies, the counter may never move forward or backward,
 * but gaps may always occur unless the sole active sync strategy is to write a TSync file.
 */
class FreqCounterSynchronizer
{
public:
    explicit FreqCounterSynchronizer(std::shared_ptr<SyncTimer> masterTimer, AbstractModule *mod, double frequencyHz, const QString &id = nullptr);

    milliseconds_t timeBase() const;
    int indexOffset() const;

    void setMinimumBaseTSCalibrationPoints(int count);
    void setStrategies(const TimeSyncStrategies &strategies);
    void setCheckInterval(const milliseconds_t &interval);
    void setTolerance(const std::chrono::microseconds &tolerance);
    void setTimeSyncBasename(const QString &fname);

    bool start();
    void stop();
    void processTimestamps(const milliseconds_t &recvTimestamp, const std::chrono::microseconds &deviceLatency, VectorXl &idxTimestamps);
    void processTimestamps(const milliseconds_t &recvTimestamp, const double &devLatencyMs, VectorXl &idxTimestamps);

private:
    void writeTsyncFileBlock(const VectorXl &timeIndices, const microseconds_t &lastOffset);

    AbstractModule *m_mod;
    QString m_id;
    TimeSyncStrategies m_strategies;
    milliseconds_t m_lastOffsetEmission;
    std::shared_ptr<SyncTimer> m_syTimer;
    bool m_lastOffsetWithinTolerance;

    uint m_toleranceUsec;
    milliseconds_t m_checkInterval;
    milliseconds_t m_lastUpdateTime;

    int m_minimumBaseTSCalibrationPoints;
    int m_baseTSCalibrationCount;
    double m_baseTimeMsec;

    double m_freq;
    int m_indexOffset;

    std::unique_ptr<TimeSyncFileWriter> m_tswriter;
};

/**
 * @brief Synchronizer for an external steady monotonic clock
 *
 * This synchronizer helps synchronizing a timestamp from an external
 * source with Syntalos' master clock.
 */
class SecondaryClockSynchronizer {
public:
    explicit SecondaryClockSynchronizer(std::shared_ptr<SyncTimer> masterTimer, AbstractModule *mod, double frequencyHz, const QString &id = nullptr);

    milliseconds_t clockCorrectionOffset() const;

    void setStrategies(const TimeSyncStrategies &strategies);
    void setCheckInterval(const milliseconds_t &interval);
    void setTolerance(const std::chrono::microseconds &tolerance);
    void setTimeSyncBasename(const QString &fname);

    bool start();
    void stop();
    void processTimestamp(milliseconds_t &masterTimestamp, const milliseconds_t &secondaryAcqTimestamp);

private:
    AbstractModule *m_mod;
    QString m_id;
    TimeSyncStrategies m_strategies;
    milliseconds_t m_lastOffsetEmission;
    std::shared_ptr<SyncTimer> m_syTimer;
    bool m_lastOffsetWithinTolerance;

    uint m_toleranceUsec;
    milliseconds_t m_checkInterval;
    milliseconds_t m_lastUpdateTime;

    bool m_firstTimestamp;
    double m_freqHz;
    microseconds_t m_acqInterval;
    milliseconds_t m_timeDiffToMaster;
    microseconds_t m_lastSecondaryAcqTimestamp;

    milliseconds_t m_clockCorrectionOffset;
    microseconds_t m_lastMasterTS;

    std::unique_ptr<TimeSyncFileWriter> m_tswriter;
};

} // end of namespace

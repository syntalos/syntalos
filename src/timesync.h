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

#pragma once

#include <memory>
#include <QLoggingCategory>
#include <QString>
#include <QMetaType>
#include <QDataStream>

#include "syclock.h"
#include "eigenaux.h"

class QFile;

namespace Syntalos {

Q_DECLARE_LOGGING_CATEGORY(logTimeSync)

#if !defined(DOXYGEN_SHOULD_SKIP_THIS)
class AbstractModule;
#endif // DOXYGEN_SHOULD_SKIP_THIS

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
    NANOSECONDS,
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
    bool open(const microseconds_t &tolerance, const QString &modName);
    void flush();
    void close();

    void writeTimes(const microseconds_t &deviceTime, const microseconds_t &masterTime);
    void writeTimes(const long long &timeIndex, const microseconds_t &masterTime);

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
    microseconds_t tolerance() const;
    QPair<QString, QString> timeNames() const;
    QPair<TimeSyncFileTimeUnit, TimeSyncFileTimeUnit> timeUnits() const;

    QList<QPair<long long, long long>> times() const;

private:
    QString m_lastError;
    QString m_moduleName;
    qint64 m_creationTime;
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
    ~FreqCounterSynchronizer();

    void setCalibrationBlocksCount(int count);
    void setStrategies(const TimeSyncStrategies &strategies);
    void setTolerance(const std::chrono::microseconds &tolerance);
    void setTimeSyncBasename(const QString &fname);

    bool isCalibrated() const;
    int indexOffset() const;

    bool start();
    void stop();
    void processTimestamps(const microseconds_t &blocksRecvTimestamp, const microseconds_t &deviceLatency,
                           int blockIndex, int blockCount, VectorXu &idxTimestamps);
    void processTimestamps(const microseconds_t &recvTimestamp, const double &devLatencyMs,
                           int blockIndex, int blockCount, VectorXu &idxTimestamps);

private:
    void writeTsyncFileBlock(const VectorXu &timeIndices, const microseconds_t &lastOffset);

    AbstractModule *m_mod;
    QString m_id;
    TimeSyncStrategies m_strategies;
    microseconds_t m_lastOffsetEmission;
    std::shared_ptr<SyncTimer> m_syTimer;

    uint m_toleranceUsec;
    bool m_lastOffsetWithinTolerance;

    uint m_calibrationMaxBlockN;
    uint m_calibrationIdx;
    VectorXl m_tsOffsetsUsec;

    bool m_haveExpectedOffset;
    uint m_expectedOffsetCalCount;
    microseconds_t m_expectedOffset;
    double m_expectedSD;

    uint m_offsetChangeWaitBlocks;
    microseconds_t m_timeCorrectionOffset;
    uint m_lastTimeIndex;

    double m_freq;
    double m_timePerPointUs;
    int m_indexOffset;

    std::unique_ptr<TimeSyncFileWriter> m_tswriter;
};

/**
 * @brief Synchronizer for an external steady monotonic clock
 *
 * This synchronizer helps synchronizing a timestamp from an external
 * source with Syntalos' master clock.
 */
class SecondaryClockSynchronizer
{
public:
    explicit SecondaryClockSynchronizer(std::shared_ptr<SyncTimer> masterTimer, AbstractModule *mod, const QString &id = nullptr);
    ~SecondaryClockSynchronizer();

    /**
     * @brief An adjustment offset to pring the secondary clock back to speed.
     *
     * negative values indicate the secondary clock running too slow, positive values mean it is
     * running too fast compared to the master clock.
     */
    microseconds_t clockCorrectionOffset() const;

    /**
     * @brief Set the amount of points needed to determine the average offset explicitly.
     */
    void setCalibrationPointsCount(int timepointCount);

    /**
     * @brief Automatically determine tolerance and needed calibration point count based on expected DAQ frequency.
     */
    void setExpectedClockFrequencyHz(double frequency);

    void setStrategies(const TimeSyncStrategies &strategies);
    void setTolerance(const microseconds_t &tolerance);
    void setTimeSyncBasename(const QString &fname);

    bool isCalibrated() const;
    microseconds_t expectedOffsetToMaster() const;

    bool start();
    void stop();
    void processTimestamp(microseconds_t &masterTimestamp, const microseconds_t &secondaryAcqTimestamp);

private:
    void emitSyncDetailsChanged();
    AbstractModule *m_mod;
    QString m_id;
    TimeSyncStrategies m_strategies;
    microseconds_t m_lastOffsetEmission;
    std::shared_ptr<SyncTimer> m_syTimer;

    uint m_toleranceUsec;
    bool m_lastOffsetWithinTolerance;

    uint m_calibrationMaxN;
    uint m_calibrationIdx;
    VectorXl m_clockOffsetsUsec;

    bool m_haveExpectedOffset;
    uint m_expectedOffsetCalCount;
    microseconds_t m_expectedOffset;
    double m_expectedSD;

    microseconds_t m_clockCorrectionOffset;
    microseconds_t m_lastMasterTS;

    std::unique_ptr<TimeSyncFileWriter> m_tswriter;
};

template<typename T>
void safeStopSynchronizer(const T &synchronizerSmartPtr)
{
    static_assert(std::is_base_of<SecondaryClockSynchronizer, typename T::element_type>::value ||
                  std::is_base_of<FreqCounterSynchronizer, typename T::element_type>::value,
                  "This function requires a smart pointer to a clock synchronizer.");
    if (synchronizerSmartPtr.get() != nullptr)
        synchronizerSmartPtr->stop();
}

} // end of namespace

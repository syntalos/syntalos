/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "timesync.h"

#include <QDateTime>
#include <QDebug>
#include <iostream>
#include <algorithm>

#include "utils/misc.h"

namespace Syntalos
{
Q_LOGGING_CATEGORY(logTimeSync, "time.synchronizer")
}

using namespace Syntalos;
using namespace Eigen;

const QString Syntalos::timeSyncStrategyToHString(const TimeSyncStrategy &strategy)
{
    switch (strategy) {
    case TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD:
        return QStringLiteral("shift timestamps (fwd)");
    case TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD:
        return QStringLiteral("shift timestamps (bwd)");
    case TimeSyncStrategy::ADJUST_CLOCK:
        return QStringLiteral("align secondary clock");
    case TimeSyncStrategy::WRITE_TSYNCFILE:
        return QStringLiteral("write time-sync file");
    default:
        return QStringLiteral("invalid");
    }
}

const QString Syntalos::timeSyncStrategiesToHString(const TimeSyncStrategies &strategies)
{
    QStringList sl;

    if (strategies.testFlag(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD)
        && strategies.testFlag(TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD)) {
        sl.append(QStringLiteral("shift timestamps"));
    } else {
        if (strategies.testFlag(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD))
            sl.append(timeSyncStrategyToHString(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD));
        if (strategies.testFlag(TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD))
            sl.append(timeSyncStrategyToHString(TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD));
    }
    if (strategies.testFlag(TimeSyncStrategy::ADJUST_CLOCK))
        sl.append(timeSyncStrategyToHString(TimeSyncStrategy::ADJUST_CLOCK));
    if (strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE))
        sl.append(timeSyncStrategyToHString(TimeSyncStrategy::WRITE_TSYNCFILE));

    return sl.join(" and ");
}

// -----------------------
// FreqCounterSynchronizer
// -----------------------

FreqCounterSynchronizer::FreqCounterSynchronizer(
    std::shared_ptr<SyncTimer> masterTimer,
    const QString &modName,
    double frequencyHz,
    const QString &id)
    : m_modName(modName),
      m_id(id),
      m_strategies(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD | TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD),
      m_lastOffsetEmission(0),
      m_syTimer(masterTimer),
      m_toleranceUsec(SECONDARY_CLOCK_TOLERANCE.count()),
      m_calibrationMaxBlockN(500),
      m_calibrationIdx(0),
      m_haveExpectedOffset(false),
      m_freq(frequencyHz),
      m_lastValidMasterTimestamp(0),
      m_tswriter(new TimeSyncFileWriter)
{
    if (m_id.isEmpty())
        m_id = createRandomString(4);

    // time one datapoint takes to acquire, if the frequency in Hz is accurate, in microseconds
    m_timePerPointUs = (1.0 / m_freq) * 1000.0 * 1000.0;
}

FreqCounterSynchronizer::~FreqCounterSynchronizer()
{
    stop();
}

void FreqCounterSynchronizer::setNotifyCallbacks(
    const SyncDetailsChangeNotifyFn &detailsChangeNotifyFn,
    const OffsetChangeNotifyFn &offsetChangeNotifyFn)
{
    m_detailsChangeNotifyFn = detailsChangeNotifyFn;
    m_offsetChangeNotifyFn = offsetChangeNotifyFn;

    // make our existence known to the system immediately
    if (m_detailsChangeNotifyFn)
        m_detailsChangeNotifyFn(m_id, m_strategies, std::chrono::microseconds(m_toleranceUsec));
}

int FreqCounterSynchronizer::indexOffset() const
{
    return m_indexOffset;
}

void FreqCounterSynchronizer::setCalibrationBlocksCount(int count)
{
    if (count <= 0)
        count = 24;
    m_calibrationMaxBlockN = count;
}

void FreqCounterSynchronizer::setTimeSyncBasename(const QString &fname, const QUuid &collectionId)
{
    m_collectionId = collectionId;
    m_tswriter->setFileName(fname);
    m_strategies = m_strategies.setFlag(TimeSyncStrategy::WRITE_TSYNCFILE, !fname.isEmpty());
}

/**
 * @brief Set the last known valid master timestamp.
 *
 * This is a hack in case the timestamp-generating module generates timestamps that
 * it may not write to disk.
 */
void FreqCounterSynchronizer::setLastValidMasterTimestamp(microseconds_t masterTimestamp)
{
    m_lastValidMasterTimestamp = masterTimestamp;
}

microseconds_t FreqCounterSynchronizer::lastMasterAssumedAcqTS() const
{
    return m_lastMasterAssumedAcqTS;
}

bool FreqCounterSynchronizer::isCalibrated() const
{
    return m_haveExpectedOffset;
}

void FreqCounterSynchronizer::setStrategies(const TimeSyncStrategies &strategies)
{
    if (m_haveExpectedOffset) {
        qCWarning(logTimeSync).noquote() << "Rejected strategy change on active FreqCounter Synchronizer for"
                                         << m_modName;
        return;
    }
    m_strategies = strategies;
    if (m_detailsChangeNotifyFn)
        m_detailsChangeNotifyFn(m_id, m_strategies, std::chrono::microseconds(m_toleranceUsec));
}

void FreqCounterSynchronizer::setTolerance(const std::chrono::microseconds &tolerance)
{
    if (m_haveExpectedOffset) {
        qCWarning(logTimeSync).noquote() << "Rejected tolerance change on active FreqCounter Synchronizer for"
                                         << m_modName;
        return;
    }
    m_toleranceUsec = tolerance.count();
    if (m_detailsChangeNotifyFn)
        m_detailsChangeNotifyFn(m_id, m_strategies, std::chrono::microseconds(m_toleranceUsec));
}

bool FreqCounterSynchronizer::start()
{
    if (m_haveExpectedOffset) {
        qCWarning(logTimeSync).noquote()
            << "Restarting a FreqCounter Synchronizer that has already been used is not permitted. This is an issue in"
            << m_modName;
        return false;
    }
    if (m_strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE)) {
        m_tswriter->setSyncMode(TSyncFileMode::SYNCPOINTS);
        m_tswriter->setTimeDataTypes(TSyncFileDataType::INT64, TSyncFileDataType::INT64);
        if (!m_tswriter->open(m_modName, m_collectionId, microseconds_t(m_toleranceUsec))) {
            qCCritical(logTimeSync).noquote().nospace()
                << "Unable to open timesync file for " << m_modName << "[" << m_id << "]: " << m_tswriter->lastError();
            return false;
        }
    }

    m_lastOffsetWithinTolerance = false;
    m_timeCorrectionOffset = microseconds_t(0);
    m_haveExpectedOffset = false;
    m_calibrationIdx = 0;
    m_expectedOffsetCalCount = 0;
    m_tsOffsetsUsec = VectorXsl::Zero(m_calibrationMaxBlockN);
    m_lastTimeIndex = 0;
    m_indexOffset = 0;
    m_offsetChangeWaitBlocks = 0;
    m_applyIndexOffset = false;

    m_lastSecondaryIdxUnandjusted = 0;
    m_lastMasterAssumedAcqTS = microseconds_t(0);

    return true;
}

void FreqCounterSynchronizer::stop()
{
    // Write the last timestamp, even if it was not out of tolerance.
    // This (for the most part) removes some guesswork and extrapolation in post-processing
    if (m_strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE)) {
        if (m_lastSecondaryIdxUnandjusted != 0 && m_lastMasterAssumedAcqTS.count() != 0) {
            auto offset = m_lastValidMasterTimestamp - m_lastMasterAssumedAcqTS;
            // we do not allow an to jump forward in time via the offset
            if (offset.count() > 0)
                offset = microseconds_t(0);
            else if (offset.count() != 0)
                qCDebug(logTimeSync).noquote()
                    << "Cutting off" << offset.count() << "µs from timesync file to align endpoint for" << m_modName;

            m_tswriter->writeTimes(
                microseconds_t(std::lround((m_lastSecondaryIdxUnandjusted + 1) * m_timePerPointUs)) + offset,
                m_lastMasterAssumedAcqTS + offset);
        }
        m_lastValidMasterTimestamp = microseconds_t(0);
        m_lastMasterAssumedAcqTS = microseconds_t(0);
    }

    m_tswriter->close();
}

void FreqCounterSynchronizer::processTimestamps(
    const microseconds_t &blocksRecvTimestamp,
    int blockIndex,
    int blockCount,
    VectorXul &idxTimestamps)
{
    // basic input value sanity checks
    assert(blockCount >= 1);
    assert(blockIndex >= 0);
    assert(blockIndex < blockCount);

    // get last index value of vector before we made any adjustments to it
    const auto secondaryLastIdxUnadjusted = idxTimestamps[idxTimestamps.rows() - 1];
    m_lastSecondaryIdxUnandjusted = secondaryLastIdxUnadjusted;

    // adjust timestamp based on our current offset
    if (m_applyIndexOffset && (m_indexOffset != 0))
        idxTimestamps -= VectorXul::Ones(idxTimestamps.rows()) * m_indexOffset;

    // timestamp when (as far and well as we can guess...) the current block was actually acquired, in microseconds
    // and based on the master clock timestamp generated upon data receival.
    const microseconds_t masterAssumedAcqTS =
        blocksRecvTimestamp - microseconds_t(std::lround(m_timePerPointUs * ((blockCount - 1) * idxTimestamps.rows())))
        + microseconds_t(std::lround(m_timePerPointUs * (blockIndex * idxTimestamps.rows())));
    m_lastMasterAssumedAcqTS = masterAssumedAcqTS;

    // value of the last entry of the current block
    const auto secondaryLastIdx = idxTimestamps[idxTimestamps.rows() - 1];

    // Timestamp, in microseconds, when according to the device frequency the last datapoint of this block was acquired
    // since we assume a zero-indexed time series, we need to add one to the secondary index
    // If the index offset has already been applied, take the value as-is, otherwise apply our current offset even if
    // modifications to the data are not permitted (we need the corrected last timestamp here, even if we don't apply
    // it to the output data and are just writing a tsync file)
    const auto secondaryLastTS = m_applyIndexOffset
                                     ? microseconds_t(std::lround((secondaryLastIdx + 1) * m_timePerPointUs))
                                     : microseconds_t(std::lround(
                                         (secondaryLastIdxUnadjusted + 1 - m_indexOffset) * m_timePerPointUs));

    // calculate time offset
    const int64_t curOffsetUsec = (secondaryLastTS - masterAssumedAcqTS).count();

    // add new datapoint to our "memory" vector
    m_tsOffsetsUsec[m_calibrationIdx++] = curOffsetUsec;
    if (m_calibrationIdx >= m_calibrationMaxBlockN)
        m_calibrationIdx = 0;

    // calculate offsets and offset expectation delta
    const int64_t avgOffsetUsec = m_tsOffsetsUsec.mean();
    const int64_t avgOffsetDeviationUsec = avgOffsetUsec - m_expectedOffset.count();

    // we do nothing more until we have enough measurements to estimate the "natural" timer offset
    // of the secondary clock and master clock
    if (!m_haveExpectedOffset) {
        m_expectedOffsetCalCount++;

        // we want a bit more values than needed for perpetual calibration, because the first
        // few values in the vector stem from the initialization phase of Syntalos and may have
        // a higher variance than actually expected during normal operation (as in the startup
        // phase, the system load is high and lots of external devices are starting up)
        if (m_expectedOffsetCalCount < (m_calibrationMaxBlockN * 2))
            return;

        m_expectedSD = sqrt(vectorVariance(m_tsOffsetsUsec));
        m_expectedOffset = microseconds_t(std::lround(vectorMedian(m_tsOffsetsUsec)));

        qCDebug(logTimeSync).noquote().nospace()
            << QTime::currentTime().toString() << "[" << m_id << "] "
            << "Determined expected time offset: " << m_expectedOffset.count() << "µs "
            << "SD: " << m_expectedSD;
        m_haveExpectedOffset = true;

        // send (possibly initial) offset info to the controller)
        if (m_offsetChangeNotifyFn)
            m_offsetChangeNotifyFn(m_id, microseconds_t(avgOffsetUsec - m_expectedOffset.count()));

        // If we are writing a timesync-file, always write the time when the very first
        // datapoint was acquired as first value.
        // We used the calibration phase to just guess the offset between the counting clock and master
        // clock by calculating backwards.
        if (m_strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE))
            m_tswriter->writeTimes(0, m_expectedOffset * -1);

        m_lastTimeIndex = secondaryLastIdx;
        return;
    }

    // we added a new block, so remove one from the wait counter that's used
    // to wait for new data after a time adjustment was made.
    if (m_offsetChangeWaitBlocks > 0)
        m_offsetChangeWaitBlocks--;

    // do nothing if we have not enough average deviation from the norm
    if (abs(avgOffsetDeviationUsec) < m_toleranceUsec) {
        // we are within tolerance range!
        // share the good news with the controller! (immediately on change, or every 30sec otherwise)
        if ((blockIndex == 0)
            && ((!m_lastOffsetWithinTolerance)
                || (blocksRecvTimestamp.count() > (m_lastOffsetEmission.count() + (30 * 1000 * 1000))))) {
            if (m_offsetChangeNotifyFn)
                m_offsetChangeNotifyFn(m_id, microseconds_t(avgOffsetDeviationUsec));
            m_lastOffsetEmission = blocksRecvTimestamp;
        }

        // check if we would still be within third-of-tolerance with the correction offset applied, and if that's
        // the case, consider gradually resetting it (as the external clock for some reason may be accurate again)
        if ((m_indexOffset != 0) && (abs(avgOffsetDeviationUsec) < (m_toleranceUsec / 3))) {
            m_indexOffset /= 2.0;

            if (m_indexOffset == 0)
                m_timeCorrectionOffset = microseconds_t(0);
            else
                m_timeCorrectionOffset = microseconds_t(static_cast<long>(floor(m_timeCorrectionOffset.count() / 2.0)));
        }

        m_lastOffsetWithinTolerance = true;
        m_lastTimeIndex = secondaryLastIdx;
        return;
    }
    m_lastOffsetWithinTolerance = false;

    const double offsetDiffToAvg = abs(avgOffsetUsec - curOffsetUsec);
    if (offsetDiffToAvg > m_expectedSD) {
        // "sane value threshold" is 1.5x the standard deviation of the offsets
        const int64_t offsetsSDThr = 1.5 * sqrt(vectorVariance(m_tsOffsetsUsec, avgOffsetUsec, true));
        if (offsetDiffToAvg > offsetsSDThr) {
            // the current offset diff to the moving average offset is not within standard deviation range.
            // This means the data point we just added is likely a fluke, potentially due to a context switch
            // or system load spike. We just ignore those events completely and don't make time adjustments
            // to index offsets based on them.
            m_lastTimeIndex = secondaryLastIdx;
            return;
        }
    }

    // Don't do even more adjustments until we have lived with the current one for a while.
    // Otherwise, the system will rapidly shift the index around, often not reaching a stable
    // state anymore.
    if (m_offsetChangeWaitBlocks > 0) {
        m_lastTimeIndex = secondaryLastIdx;
        return;
    }

    // Emit offset information to the main controller about every 10sec or slower
    // in case we run at slower speeds
    if ((blockIndex == 0) && (masterAssumedAcqTS.count() > (m_lastOffsetEmission.count() + (15 * 1000 * 1000)))) {
        if (m_offsetChangeNotifyFn)
            m_offsetChangeNotifyFn(m_id, microseconds_t(avgOffsetDeviationUsec));
        m_lastOffsetEmission = blocksRecvTimestamp;
    }

    // calculate time-based correction offset by changing the previous offset by 1/3 of the difference,
    // to get fairly smooth adjustments
    const auto corrOffsetDiff = avgOffsetDeviationUsec - m_timeCorrectionOffset.count();
    m_timeCorrectionOffset += microseconds_t((int64_t)std::ceil((double)corrOffsetDiff / 3.0));

    // sanity check: we need to correct by at least one datapoint for any synchronization to occur at all
    if (abs(m_timeCorrectionOffset.count()) <= m_timePerPointUs)
        m_timeCorrectionOffset = microseconds_t(static_cast<int64_t>(ceil(m_timePerPointUs)));

    // translate the clock update offset to indices. We round up here as we are already below threshold,
    // and overshooting slightly appears to be the better solution than being too conservative
    const bool initialOffset = m_indexOffset == 0;
    const int newIndexOffset = static_cast<int64_t>((m_timeCorrectionOffset.count() / 1000.0 / 1000.0) * m_freq);

    // only make adjustments (and potentially write to a tsync file) if we actually changed
    // the index offset and not just the time value associated with it
    // (this may result in less accurate, but also within-tolerance and less noisy tsync files)
    if (m_indexOffset == newIndexOffset) {
        m_lastTimeIndex = secondaryLastIdx;
        return;
    }
    m_indexOffset = std::lround(((newIndexOffset * 2) + m_indexOffset) / 3.0);

    if (m_indexOffset != 0) {
        m_offsetChangeWaitBlocks = m_calibrationMaxBlockN * 1.2;

        m_applyIndexOffset = false;
        if (m_strategies.testFlag(TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD)) {
            if (m_indexOffset > 0)
                m_applyIndexOffset = true;
        }
        if (m_strategies.testFlag(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD)) {
            if (m_indexOffset < 0)
                m_applyIndexOffset = true;
        }

        // already apply offset as gradient to the current vector, if we are permitted to make that change
        if (initialOffset && m_applyIndexOffset)
            idxTimestamps -= VectorXul::LinSpaced(idxTimestamps.rows(), 0, m_indexOffset);
    }

    // we're out of sync, record that fact to the tsync file if we are writing one
    // NOTE: we have to use the unadjusted time value for the device clock - since we didn't need that until now,
    // we calculate it here from the unadjusted last index value of the current block.
    // 1 is added to secondaryLastIdxUnadjusted becuse the timestamp reflects the time *after* a sample was acquired,
    // so the zero-index needs to be offset by one.
    if (m_strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE))
        m_tswriter->writeTimes(
            microseconds_t(std::lround((secondaryLastIdxUnadjusted + 1) * m_timePerPointUs)), masterAssumedAcqTS);

    m_lastTimeIndex = secondaryLastIdx;
}

// --------------------------
// SecondaryClockSynchronizer
// --------------------------

SecondaryClockSynchronizer::SecondaryClockSynchronizer(
    std::shared_ptr<SyncTimer> masterTimer,
    const QString &modName,
    const QString &id)
    : m_modName(modName),
      m_id(id),
      m_strategies(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD | TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD),
      m_lastOffsetEmission(0),
      m_syTimer(masterTimer),
      m_toleranceUsec(SECONDARY_CLOCK_TOLERANCE.count()),
      m_calibrationMaxN(400),
      m_calibrationIdx(0),
      m_haveExpectedOffset(false),
      m_tswriter(new TimeSyncFileWriter)
{
    if (m_id.isEmpty())
        m_id = createRandomString(4);

    // make our existence known to the system
    emitSyncDetailsChanged();
}

SecondaryClockSynchronizer::~SecondaryClockSynchronizer()
{
    stop();
}

void SecondaryClockSynchronizer::setNotifyCallbacks(
    const SyncDetailsChangeNotifyFn &detailsChangeNotifyFn,
    const OffsetChangeNotifyFn &offsetChangeNotifyFn)
{
    m_detailsChangeNotifyFn = detailsChangeNotifyFn;
    m_offsetChangeNotifyFn = offsetChangeNotifyFn;

    // make our existence known to the system immediately
    emitSyncDetailsChanged();
}

microseconds_t SecondaryClockSynchronizer::clockCorrectionOffset() const
{
    return m_clockCorrectionOffset;
}

void SecondaryClockSynchronizer::setCalibrationPointsCount(int timepointCount)
{
    if (m_haveExpectedOffset) {
        qCWarning(logTimeSync).noquote() << "Rejected calibration point count change on active Clock Synchronizer for"
                                         << m_modName;
        return;
    }

    m_calibrationMaxN = timepointCount > 24 ? timepointCount : 24;
}

void SecondaryClockSynchronizer::setExpectedClockFrequencyHz(double frequency)
{
    if (m_haveExpectedOffset) {
        qCWarning(logTimeSync).noquote() << "Rejected frequency change on active Clock Synchronizer for" << m_modName;
        return;
    }

    if (frequency <= 0) {
        qCWarning(logTimeSync).noquote() << "Rejected bogus frequency change to <= 0 for" << m_modName;
        return;
    }

    // the amount of datapoints needed is on a curve, approaching 10 sec (or minmal required time)
    // if we get a lot of points in a short time, we don't need to wait that long to calculate the
    // average offset, but with a low frequency of new points we need a bit more data to calculate
    // the averages and their SD reliably
    m_calibrationMaxN = std::ceil((frequency + (1.0 / (0.01 + std::pow(frequency / 250.0, 2)))) * 10.0);

    // limit the number of points to at least 24 and the time to a maximum of 90 seconds
    if (m_calibrationMaxN > (frequency * 90.0))
        m_calibrationMaxN = std::ceil((frequency * 90.0));
    m_calibrationMaxN = m_calibrationMaxN > 24 ? m_calibrationMaxN : 24;

    // set tolerance of half the time one sample takes to be acquired
    m_toleranceUsec = std::lround(((1000.0 / frequency) / 2) * 1000.0);
    emitSyncDetailsChanged();
}

void SecondaryClockSynchronizer::setTimeSyncBasename(const QString &fname, const QUuid &collectionId)
{
    m_collectionId = collectionId;
    m_tswriter->setFileName(fname);
    m_strategies = m_strategies.setFlag(TimeSyncStrategy::WRITE_TSYNCFILE, !fname.isEmpty());
}

bool SecondaryClockSynchronizer::isCalibrated() const
{
    return m_haveExpectedOffset;
}

microseconds_t SecondaryClockSynchronizer::expectedOffsetToMaster() const
{
    return m_expectedOffset;
}

void SecondaryClockSynchronizer::setStrategies(const TimeSyncStrategies &strategies)
{
    if (m_haveExpectedOffset) {
        qCWarning(logTimeSync).noquote() << "Rejected strategy change on active Clock Synchronizer for" << m_modName;
        return;
    }
    m_strategies = strategies;
    emitSyncDetailsChanged();
}

void SecondaryClockSynchronizer::setTolerance(const microseconds_t &tolerance)
{
    if (m_haveExpectedOffset) {
        qCWarning(logTimeSync).noquote() << "Rejected tolerance change on active Clock Synchronizer for" << m_modName;
        return;
    }
    m_toleranceUsec = tolerance.count();
    emitSyncDetailsChanged();
}

bool SecondaryClockSynchronizer::start()
{
    if (m_haveExpectedOffset) {
        qCWarning(logTimeSync).noquote()
            << "Restarting a Clock Synchronizer that has already been used is not permitted. This is an issue in "
            << m_modName;
        return false;
    }
    if (m_strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE)) {
        m_tswriter->setSyncMode(TSyncFileMode::SYNCPOINTS);
        m_tswriter->setTimeDataTypes(TSyncFileDataType::INT64, TSyncFileDataType::INT64);
        if (!m_tswriter->open(m_modName, m_collectionId, microseconds_t(m_toleranceUsec))) {
            qCCritical(logTimeSync).noquote().nospace()
                << "Unable to open timesync file for " << m_modName << "[" << m_id << "]: " << m_tswriter->lastError();
            return false;
        }
    }

    if (m_calibrationMaxN <= 4)
        qCCritical(logTimeSync).noquote().nospace()
            << "Clock synchronizer for " << m_modName << "[" << m_id << "] uses a tiny calibration array (length <= 4)";
    assert(m_calibrationMaxN > 0);

    m_lastOffsetWithinTolerance = false;
    m_clockCorrectionOffset = microseconds_t(0);
    m_haveExpectedOffset = false;
    m_calibrationIdx = 0;
    m_expectedOffsetCalCount = 0;
    m_expectedOffset = microseconds_t(0);
    m_clockOffsetsUsec = VectorXsl::Zero(m_calibrationMaxN);
    m_lastMasterTS = m_syTimer->timeSinceStartMsec();
    m_lastSecondaryAcqTS = microseconds_t(0);
    m_clockUpdateWaitPoints = 0;

    return true;
}

void SecondaryClockSynchronizer::stop()
{
    // write the last acquired timestamp pair, to simplify data post processing
    if (m_strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE)) {
        if (m_lastSecondaryAcqTS.count() != 0)
            m_tswriter->writeTimes(m_lastSecondaryAcqTS, m_lastMasterTS);
    }

    m_tswriter->close();
}

void SecondaryClockSynchronizer::processTimestamp(
    microseconds_t &masterTimestamp,
    const microseconds_t &secondaryAcqTimestamp)
{
    const int64_t curOffsetUsec = (secondaryAcqTimestamp - masterTimestamp).count();

    // calculate offsets without the new datapoint included
    const int64_t avgOffsetUsec = m_clockOffsetsUsec.mean();
    const int64_t avgOffsetDeviationUsec = avgOffsetUsec - m_expectedOffset.count();

    // add new datapoint to our "memory" vector
    m_clockOffsetsUsec[m_calibrationIdx++] = curOffsetUsec;
    if (m_calibrationIdx >= m_calibrationMaxN)
        m_calibrationIdx = 0;

    // update delay-after-adjustment counter
    if (m_clockUpdateWaitPoints > 0)
        m_clockUpdateWaitPoints--;

    // we do nothing more until we have enough measurements to estimate the "natural" timer offset
    // of the secondary clock and master clock
    if (!m_haveExpectedOffset) {
        m_expectedOffsetCalCount++;

        // we want a bit more values than needed for perpetual calibration, because the first
        // few values in the vector stem from the initialization phase of Syntalos and may have
        // a higher variance than actually expected during normal operation (as in the startup
        // phase, the system load is high and lots of external devices are starting up)
        if (m_expectedOffsetCalCount < (m_calibrationMaxN * 2))
            return;

        m_expectedSD = sqrt(vectorVariance(m_clockOffsetsUsec));
        m_expectedOffset = microseconds_t(std::lround(vectorMedian(m_clockOffsetsUsec)));

        qCDebug(logTimeSync).noquote().nospace()
            << QTime::currentTime().toString() << "[" << m_id << "] "
            << "Determined expected time offset: " << m_expectedOffset.count() << "µs "
            << "SD: " << m_expectedSD;
        m_haveExpectedOffset = true;

        // if we are writing a timesync-file, always write the initial secondary clock time
        // matched to the expected-offset adjusted time
        if (m_strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE))
            m_tswriter->writeTimes(microseconds_t(0), m_expectedOffset * -1);

        // remember the secondary clock timestamp & master timestamp for the next iteration
        m_lastSecondaryAcqTS = secondaryAcqTimestamp;
        m_lastMasterTS = masterTimestamp;
        return;
    }

    const double offsetDiffToAvg = abs(avgOffsetUsec - curOffsetUsec);
    if (offsetDiffToAvg > m_expectedSD) {
        const double offsetsSDThr = 2 * sqrt(vectorVariance(m_clockOffsetsUsec, avgOffsetUsec, true));
        if (offsetDiffToAvg > offsetsSDThr) {
            // the current offset diff to the moving average offset is not within the defined, standard-deviation-based
            // range. This means the data point we just added is likely a fluke, potentially due to a context switch
            // or system load spike.
            // In this case we derive a new master timestamp from the previous master timestamp, by adding the current
            // delta of the previous secondary clock time to current secondary clock time.
            // Assuming this issue does not appear too frequently and the secondary clock is not totally off, this
            // method should yield a good estimate for the correct timestamp.
            const auto masterTimestampFAdj = m_lastMasterTS + (secondaryAcqTimestamp - m_lastSecondaryAcqTS);

            // correct fluke unconditionally
            masterTimestamp = masterTimestampFAdj;

            // prevent any time-travel into the past (this should be impossible, but better be safe)
            if (masterTimestamp < m_lastMasterTS)
                masterTimestamp = m_lastMasterTS + microseconds_t(1);

            if (m_strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE))
                m_tswriter->writeTimes(secondaryAcqTimestamp, masterTimestamp);

            /*
            qCDebug(logTimeSync).noquote().nospace() << QTime::currentTime().toString() << "[" << m_id << "] "
                    << "Offset deviation diff not within SD. Adjusted for fluke offset by adding " << avgOffsetUsec*-1
            << "µs "
                    << "to secondary clock time " << secondaryAcqTimestamp.count() << "µs "
                    << " SD: " << offsetsSD;
            */

            // remember the original timestamps (master TS having been adjusted)
            m_lastSecondaryAcqTS = secondaryAcqTimestamp;
            m_lastMasterTS = masterTimestamp;

            // nothing left to do here, we dealt with the fluke
            return;
        }
    }

    // do nothing if we have not enough average deviation from the norm
    if (abs(avgOffsetDeviationUsec) < m_toleranceUsec) {
        // we are within tolerance range!
        // share the good news with the controller! (immediately on change, or every 30sec otherwise)
        if ((!m_lastOffsetWithinTolerance)
            || (masterTimestamp.count() > (m_lastOffsetEmission.count() + (30 * 1000 * 1000)))) {
            if (m_offsetChangeNotifyFn)
                m_offsetChangeNotifyFn(m_id, microseconds_t(avgOffsetDeviationUsec));
            m_lastOffsetEmission = masterTimestamp;
        }

        // check if we would still be within third-of-tolerance with the correction offset applied, and if that's
        // the case, gradually reset it (as the external clock may be accurate again)
        if (m_clockCorrectionOffset.count() != 0) {
            if (abs(avgOffsetDeviationUsec) < (m_toleranceUsec / 3))
                m_clockCorrectionOffset = microseconds_t(0);
            else
                m_clockCorrectionOffset = microseconds_t(
                    static_cast<int64_t>(std::ceil(m_clockCorrectionOffset.count() / 1.25)));

            // we still apply any corrective offset (most likely reduced in the previous step)
            if (m_strategies.testFlag(TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD) && m_clockCorrectionOffset.count() > 0)
                masterTimestamp = secondaryAcqTimestamp - m_expectedOffset - m_clockCorrectionOffset;
            if (m_strategies.testFlag(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD) && m_clockCorrectionOffset.count() < 0)
                masterTimestamp = secondaryAcqTimestamp - m_expectedOffset - m_clockCorrectionOffset;
            // prevent any time-travel into the past
            if (masterTimestamp < m_lastMasterTS)
                masterTimestamp = m_lastMasterTS + microseconds_t(1);
        }

        // remember the secondary clock timestamp & master timestamp for the next iteration
        m_lastSecondaryAcqTS = secondaryAcqTimestamp;
        m_lastMasterTS = masterTimestamp;
        m_lastOffsetWithinTolerance = true;
        return;
    }
    m_lastOffsetWithinTolerance = false;

    // Emit offset information to the main controller about every 15sec or slower
    // in case we run at slower speeds
    if (masterTimestamp.count() > (m_lastOffsetEmission.count() + (15 * 1000 * 1000))) {
        if (m_offsetChangeNotifyFn)
            m_offsetChangeNotifyFn(m_id, microseconds_t(avgOffsetDeviationUsec));
        m_lastOffsetEmission = masterTimestamp;
    }

    if (m_clockUpdateWaitPoints == 0
        && abs(avgOffsetDeviationUsec - m_clockCorrectionOffset.count()) > (m_toleranceUsec / 1.5)) {
        // try to smoothly adjust the offset to the new value
        const double offsetDiff = (double)avgOffsetDeviationUsec - (double)m_clockCorrectionOffset.count();
        auto delayFactor = (secondaryAcqTimestamp - m_lastSecondaryAcqTS).count() / 800.0;
        if (delayFactor < 1)
            delayFactor = 1;
        if (delayFactor >= abs(offsetDiff))
            delayFactor = abs(offsetDiff) / 4.0;

        auto adjValue = offsetDiff / delayFactor;
        m_clockCorrectionOffset += microseconds_t((int64_t)std::ceil(adjValue));

        // write timestamp correction to tsync file
        if (m_strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE))
            m_tswriter->writeTimes(
                secondaryAcqTimestamp, secondaryAcqTimestamp - m_expectedOffset - m_clockCorrectionOffset);

        // add a delay before we make any more changes, if we actually made a significant change
        if (abs(m_clockCorrectionOffset.count()) > 1)
            m_clockUpdateWaitPoints = std::ceil(0.65 * m_calibrationMaxN);
    }

    // apply any existing timestamp correction offsets
    if (m_strategies.testFlag(TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD) && m_clockCorrectionOffset.count() > 0)
        masterTimestamp = secondaryAcqTimestamp - m_expectedOffset - m_clockCorrectionOffset;
    if (m_strategies.testFlag(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD) && m_clockCorrectionOffset.count() < 0)
        masterTimestamp = secondaryAcqTimestamp - m_expectedOffset - m_clockCorrectionOffset;

    /*
    qCDebug(logTimeSync).noquote().nospace() << "[" << m_id << "] "
            << "Clocks out of sync. Mean offset deviation: " << avgOffsetDeviationUsec << "µs, "
            << "Current: " << (curOffsetUsec - m_expectedOffset.count()) << "µs, "
            << "Active SD: " << offsetsSD << " "
            << "Correction offset: " << m_clockCorrectionOffset.count() << "µs";
    */

    // ensure time doesn't run backwards - this really shouldn't happen at this
    // point, but we prevent this just in case
    if (masterTimestamp < m_lastMasterTS) {
        qCWarning(logTimeSync).noquote().nospace()
            << "[" << m_id << "] "
            << "Timestamp moved backwards when calculating adjusted new time: " << masterTimestamp.count() << " < "
            << m_lastMasterTS.count() << " (mitigated by reusing previous time)";
        masterTimestamp = m_lastMasterTS + microseconds_t(0);
    }

    // remember the secondary clock timestamp & master timestamp for the next iteration
    m_lastSecondaryAcqTS = secondaryAcqTimestamp;
    m_lastMasterTS = masterTimestamp;
}

void SecondaryClockSynchronizer::emitSyncDetailsChanged()
{
    if (m_detailsChangeNotifyFn)
        m_detailsChangeNotifyFn(m_id, m_strategies, microseconds_t(m_toleranceUsec));
}

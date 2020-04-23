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

#include "timesync.h"

#include <QDebug>
#include <QDateTime>
#include <QFile>
#include "moduleapi.h"

#include "utils.h"

using namespace Syntalos;
using namespace Eigen;

#define TSYNC_FILE_MAGIC 0xC6BBDFBC
#define TSYNC_FILE_FORMAT_VERSION  1

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

    if (strategies.testFlag(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD) && strategies.testFlag(TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD)) {
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

QString Syntalos::timeSyncFileTimeUnitToString(const TimeSyncFileTimeUnit &tsftunit)
{
    switch (tsftunit) {
    case TimeSyncFileTimeUnit::INDEX:
        return QStringLiteral("index");
    case TimeSyncFileTimeUnit::MICROSECONDS:
        return QStringLiteral("µs");
    case TimeSyncFileTimeUnit::MILLISECONDS:
        return QStringLiteral("ms");
    case TimeSyncFileTimeUnit::SECONDS:
        return QStringLiteral("sec");
    default:
        return QStringLiteral("?");
    }
}

// ------------------
// TimeSyncFileWriter
// ------------------

TimeSyncFileWriter::TimeSyncFileWriter()
    : m_file(new QFile()),
      m_index(0)
{
    m_timeNames = qMakePair(QStringLiteral("device-time"),
                            QStringLiteral("master-time"));
    m_timeUnits = qMakePair(TimeSyncFileTimeUnit::MICROSECONDS,
                            TimeSyncFileTimeUnit::MICROSECONDS);
    m_stream.setVersion(QDataStream::Qt_5_12);
    m_stream.setByteOrder(QDataStream::LittleEndian);
}

TimeSyncFileWriter::~TimeSyncFileWriter()
{
    flush();
    if (m_file->isOpen())
        m_file->close();
    delete m_file;
}

QString TimeSyncFileWriter::lastError() const
{
    return m_lastError;
}

void TimeSyncFileWriter::setTimeNames(QPair<QString, QString> pair)
{
    m_timeNames = pair;
}

void TimeSyncFileWriter::setTimeUnits(QPair<TimeSyncFileTimeUnit, TimeSyncFileTimeUnit> pair)
{
    m_timeUnits = pair;
}

void TimeSyncFileWriter::setFileName(const QString &fname)
{
    if (m_file->isOpen())
        m_file->close();

    auto tsyncFname = fname;
    if (!tsyncFname.endsWith(QStringLiteral(".tsync")))
        tsyncFname = tsyncFname + QStringLiteral(".tsync");
    m_file->setFileName(tsyncFname);
}

bool TimeSyncFileWriter::open(const microseconds_t &checkInterval, const microseconds_t &tolerance, const QString &modName)
{
    if (m_file->isOpen())
        m_file->close();

    if (!m_file->open(QIODevice::WriteOnly)) {
        m_lastError = m_file->errorString();
        return false;
    }

    m_index = 0;
    m_stream.setDevice(m_file);

    // write file header
    QDateTime currentTime(QDateTime::currentDateTime());

    m_stream << (quint32) TSYNC_FILE_MAGIC;
    m_stream << (quint32) TSYNC_FILE_FORMAT_VERSION;
    m_stream << (qint64) currentTime.toTime_t();
    m_stream << (quint32) checkInterval.count();
    m_stream << (quint32) tolerance.count();
    m_stream << modName;
    m_stream << m_timeNames.first;
    m_stream << m_timeNames.second;
    m_stream << (quint16) m_timeUnits.first;
    m_stream << (quint16) m_timeUnits.second;

    m_file->flush();
    return true;
}

void TimeSyncFileWriter::flush()
{
    if (m_file->isOpen())
        m_file->flush();
}

void TimeSyncFileWriter::close()
{
    if (m_file->isOpen()) {
        m_file->flush();
        m_file->close();
    }
}

void TimeSyncFileWriter::writeTimes(const microseconds_t &deviceTime, const microseconds_t &masterTime)
{
    m_stream << (quint32) m_index++;
    m_stream << (qint64) deviceTime.count();
    m_stream << (qint64) masterTime.count();
}

void TimeSyncFileWriter::writeTimes(const long long &timeIndex, const long long &masterTime)
{
    m_stream << (quint32) m_index++;
    m_stream << (qint64) timeIndex;
    m_stream << (qint64) masterTime;
}

TimeSyncFileReader::TimeSyncFileReader()
    : m_lastError(QString())
{}

bool TimeSyncFileReader::open(const QString &fname)
{
    QFile file(fname);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = file.errorString();
        return false;
    }
    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_5_12);
    in.setByteOrder(QDataStream::LittleEndian);

    // read the header
    quint32 magic;
    quint32 formatVersion;
    quint32 checkIntervalUsec;
    quint32 toleranceUsec;
    quint16 timeUnitDev;
    quint16 timeUnitMaster;

    in >> magic >> formatVersion;
    if ((magic != TSYNC_FILE_MAGIC) || (formatVersion != TSYNC_FILE_FORMAT_VERSION)) {
        m_lastError = QStringLiteral("Unable to read data: This file is not a valid timesync metadata file.");
        return false;
    }

    in >> m_creationTime
       >> checkIntervalUsec
       >> toleranceUsec
       >> m_moduleName
       >> m_timeNames.first
       >> m_timeNames.second
       >> timeUnitDev
       >> timeUnitMaster;
    m_checkInterval = microseconds_t(checkIntervalUsec);
    m_tolerance = microseconds_t(toleranceUsec);
    m_timeUnits.first = static_cast<TimeSyncFileTimeUnit>(timeUnitDev);
    m_timeUnits.second = static_cast<TimeSyncFileTimeUnit>(timeUnitMaster);

    // read the time data
    m_times.clear();
    quint64 expectedIndex = 0;
    while (!in.atEnd()) {
        quint32 index;
        qint64 deviceTime;
        qint64 masterTime;

        in >> index
           >> deviceTime
           >> masterTime;
        if (index != expectedIndex)
            qWarning().noquote() << "The timesync file has gaps: Expected index" << expectedIndex << "but got" << index;

        m_times.append(qMakePair(deviceTime, masterTime));
        expectedIndex++;
    }

    return true;
}

QString TimeSyncFileReader::lastError() const
{
    return m_lastError;
}

QString TimeSyncFileReader::moduleName() const
{
    return m_moduleName;
}

time_t TimeSyncFileReader::creationTime() const
{
    return m_creationTime;
}

microseconds_t TimeSyncFileReader::checkInterval() const
{
    return m_checkInterval;
}

microseconds_t TimeSyncFileReader::tolerance() const
{
    return m_tolerance;
}

QPair<QString, QString> TimeSyncFileReader::timeNames() const
{
    return m_timeNames;
}

QPair<TimeSyncFileTimeUnit, TimeSyncFileTimeUnit> TimeSyncFileReader::timeUnits() const
{
    return m_timeUnits;
}

QList<QPair<long long, long long> > TimeSyncFileReader::times() const
{
    return m_times;
}

// -----------------------
// FreqCounterSynchronizer
// -----------------------

FreqCounterSynchronizer::FreqCounterSynchronizer(std::shared_ptr<SyncTimer> masterTimer, AbstractModule *mod, double frequencyHz, const QString &id)
    : m_mod(mod),
      m_id(id),
      m_strategies(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD | TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD),
      m_lastOffsetEmission(0),
      m_syTimer(masterTimer),
      m_toleranceUsec(SECONDARY_CLOCK_TOLERANCE.count()),
      m_checkInterval(DEFAULT_CLOCKSYNC_CHECK_INTERVAL),
      m_lastUpdateTime(milliseconds_t(DEFAULT_CLOCKSYNC_CHECK_INTERVAL * -1)),
      m_minimumBaseTSCalibrationPoints(180), // 180 datapoints for base time calibration by default
      m_baseTSCalibrationCount(0),
      m_baseTimeMsec(0),
      m_freq(frequencyHz),
      m_indexOffset(0),
      m_tswriter(new TimeSyncFileWriter)
{
    if (m_id.isEmpty())
        m_id = createRandomString(4);

    // make our existence known to the system
    emit m_mod->synchronizerDetailsChanged(m_id, m_strategies, std::chrono::microseconds(m_toleranceUsec), m_checkInterval);
}

FreqCounterSynchronizer::~FreqCounterSynchronizer()
{
    stop();
}

milliseconds_t FreqCounterSynchronizer::timeBase() const
{
    return milliseconds_t(static_cast<int>(std::round(m_baseTimeMsec)));
}

int FreqCounterSynchronizer::indexOffset() const
{
    return m_indexOffset;
}

void FreqCounterSynchronizer::setMinimumBaseTSCalibrationPoints(int count)
{
    // impose some limits on the amount of timepoints we use for calibration
    if (count <= 0)
        count = 3;
    if (count > 1500)
        count = 1500;
    m_minimumBaseTSCalibrationPoints = count;
}

void FreqCounterSynchronizer::setTimeSyncBasename(const QString &fname)
{
    m_tswriter->setFileName(fname);
    m_strategies = m_strategies.setFlag(TimeSyncStrategy::WRITE_TSYNCFILE, !fname.isEmpty());
}

void FreqCounterSynchronizer::setStrategies(const TimeSyncStrategies &strategies)
{
    if (m_baseTSCalibrationCount != 0) {
        qWarning().noquote() << "Rejected strategy change on active FreqCounter Synchronizer for" << m_mod->name();
        return;
    }
    m_strategies = strategies;
    emit m_mod->synchronizerDetailsChanged(m_id, m_strategies, std::chrono::microseconds(m_toleranceUsec), m_checkInterval);
}

void FreqCounterSynchronizer::setCheckInterval(const milliseconds_t &interval)
{
    if (m_baseTSCalibrationCount != 0) {
        qWarning().noquote() << "Rejected check-interval change on active FreqCounter Synchronizer for" << m_mod->name();
        return;
    }
    m_checkInterval = interval;
    emit m_mod->synchronizerDetailsChanged(m_id, m_strategies, std::chrono::microseconds(m_toleranceUsec), m_checkInterval);
}

void FreqCounterSynchronizer::setTolerance(const std::chrono::microseconds &tolerance)
{
    if (m_baseTSCalibrationCount != 0) {
        qWarning().noquote() << "Rejected tolerance change on active FreqCounter Synchronizer for" << m_mod->name();
        return;
    }
    m_toleranceUsec = tolerance.count();
    emit m_mod->synchronizerDetailsChanged(m_id, m_strategies, std::chrono::microseconds(m_toleranceUsec), m_checkInterval);
}

bool FreqCounterSynchronizer::start()
{
    if (m_baseTSCalibrationCount != 0) {
        qWarning().noquote() << "Restarting a FreqCounter Synchronizer that has already been used is not permitted. This is an issue in " << m_mod->name();
        return false;
    }
    if (m_strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE)) {
        if (!m_tswriter->open(m_checkInterval, microseconds_t(m_toleranceUsec), m_mod->name())) {
            qCritical().noquote().nospace() << "Unable to open timesync file for " << m_mod->name() << "[" << m_id << "]: " << m_tswriter->lastError();
            return false;
        }
    }
    m_baseTimeMsec = 0;
    m_baseTSCalibrationCount = 0;
    m_lastOffsetWithinTolerance = false;
    return true;
}

void FreqCounterSynchronizer::stop()
{
    m_tswriter->close();
}

void FreqCounterSynchronizer::processTimestamps(const milliseconds_t &recvTimestamp, const std::chrono::microseconds &deviceLatency,
                                                int blockIndex, int blockCount, VectorXu &idxTimestamps)
{
    Q_UNUSED(blockCount)

    // adjust timestamp based on our current offset
    if (m_indexOffset != 0)
        idxTimestamps += VectorXu::LinSpaced(idxTimestamps.rows(), 0, m_indexOffset);

    // check if we are calibrating our timebase and - if not - see if we have to check for
    // timing offsets already
    const auto currentTimestamp = m_syTimer->timeSinceStartMsec();
    if (m_baseTSCalibrationCount >= m_minimumBaseTSCalibrationPoints) {
        // do nothing if we aren't checking the timestamp for validity yet
        if ((currentTimestamp - m_lastUpdateTime) < m_checkInterval)
            return;
    }

    m_lastUpdateTime = currentTimestamp;

    // timestamp when (as far and well as we can guess...) the data was actually acquired, in milliseconds
    const auto assumedAcqTS = std::chrono::duration_cast<milliseconds_t>(recvTimestamp
                                                                         - milliseconds_t(qRound(((idxTimestamps.rows() / m_freq) * 1000.0) * (blockIndex + 1)))
                                                                         - deviceLatency);

    if (m_baseTSCalibrationCount < m_minimumBaseTSCalibrationPoints) {
        // we are in the timebase calibration phase, so update our timebase

        // we make the bold assumption here that the assumedAcq timestamp is when the last timepoint was acquired
        m_baseTSCalibrationCount += idxTimestamps.rows();

        // guess the time when the first data point was acquired based on the current timestamp,
        // adjust our timebase based on that
        m_baseTimeMsec += (assumedAcqTS.count() - (m_baseTSCalibrationCount / m_freq * 1000.0)) / (m_minimumBaseTSCalibrationPoints / idxTimestamps.rows());

        if (m_baseTSCalibrationCount >= m_minimumBaseTSCalibrationPoints) {
            // always write down the starting time, if we are using a timesync-file
            if (m_strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE))
                 m_tswriter->writeTimes(static_cast<int>(std::round(m_baseTimeMsec * 1000)), 0);
        }

        // until calibration is done, we can't do any offset calculations
        return;
    }

    // guess the actual timestamps in relation to the received timestamp in milliseconds for the given index vector
    VectorXd times = (idxTimestamps.cast<double>() / m_freq) * 1000.0;
    times += m_baseTimeMsec * VectorXd::Ones(times.rows());

    // calculate current offset
    const auto lastTimestamp = std::chrono::microseconds(static_cast<int64_t>(std::round(times[times.rows() - 1] * 1000.0)));
    const auto timeOffset = (std::chrono::duration_cast<std::chrono::microseconds>(assumedAcqTS - lastTimestamp));
    const auto timeOffsetUsec = timeOffset.count();

    if (std::abs(timeOffsetUsec) < m_toleranceUsec) {
        // everything is within tolerance range, no adjustments needed
        // share the good news with the controller! (only once, for now)
        if (!m_lastOffsetWithinTolerance)
            emit m_mod->synchronizerOffsetChanged(m_id, timeOffset);
        m_lastOffsetWithinTolerance = true;
        return;
    }
    m_lastOffsetWithinTolerance = false;

    if (m_strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE))
        writeTsyncFileBlock(idxTimestamps, timeOffset);

    // Emit offset change information to the main controller every 2sec or slower
    // in case we run at slower speeds
    if ((m_lastOffsetEmission.count() + 2000) < recvTimestamp.count()) {
        emit m_mod->synchronizerOffsetChanged(m_id, timeOffset);
        m_lastOffsetEmission = currentTimestamp;
    }

    const bool shiftFwdAllowed = m_strategies.testFlag(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD);
    const bool shiftBwdAllowed = m_strategies.testFlag(TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD);

    if (!shiftFwdAllowed && !shiftBwdAllowed) {
        // no timeshifting is permitted, there is nothing left to do for us
        return;
    }

    if (timeOffsetUsec > 0) {
        // the external devive is running too slow

        if (!shiftFwdAllowed) {
            // we can not shift indices forward, but we would need to do that in order to adjust the timestamp
            // there is nothing we can do here anymore
            return;
        }
    } else {
        // the external device is running too fast

        if (!shiftBwdAllowed) {
            // we can not shift backwards, but we would need to do that in order to adjust the timestamp
            // there is nothing we can do here anymore
            return;
        }
    }

    // offset the device time index by a much smaller amount of what is needed to sync up the clocks
    // if this doesn't bring us back within tolerance, we'll adjust the index offset again
    // the next time this function is run
    // how slowly the external timestamps ares adjusted depends on the DAQ frequency - the slower it runs,
    // the faster we adjust it.
    const int changeInt = std::floor(((timeOffsetUsec / 1000.0 / 1000.0) * m_freq) / (m_freq / 20000 + 1));

    // adjust timestamps with an appropriate offset step
    const auto change = VectorXu::LinSpaced(idxTimestamps.rows(), 0, changeInt);
    m_indexOffset += change[change.rows() - 1];
    idxTimestamps += change;
}

void FreqCounterSynchronizer::processTimestamps(const milliseconds_t &recvTimestamp, const double &devLatencyMs, int blockIndex, int blockCount, VectorXu &idxTimestamps)
{
    // we want the device latency in microseconds
    auto deviceLatency = std::chrono::microseconds(static_cast<long>(devLatencyMs * 1000));
    processTimestamps(recvTimestamp, deviceLatency, blockIndex, blockCount, idxTimestamps);
}

inline
void FreqCounterSynchronizer::writeTsyncFileBlock(const VectorXu &timeIndices, const microseconds_t &lastOffset)
{
    // if we only have a very short vector, we don't also add the offset information to the first
    // datapoint
    const auto lastIdxTimeUsec = microseconds_t(static_cast<qint64>((static_cast<double>(timeIndices[timeIndices.rows() - 1]) / m_freq) * 1000.0 * 1000.0));
    if (timeIndices.size() <= 4) {
        m_tswriter->writeTimes(lastIdxTimeUsec, lastIdxTimeUsec + lastOffset);
        return;
    }

    const auto firstIdxTimeUsec = microseconds_t(static_cast<qint64>((static_cast<double>(timeIndices[0]) / m_freq) * 1000.0 * 1000.0));
    m_tswriter->writeTimes(firstIdxTimeUsec, firstIdxTimeUsec + (lastOffset - (lastIdxTimeUsec - firstIdxTimeUsec)));
    m_tswriter->writeTimes(lastIdxTimeUsec, lastIdxTimeUsec + lastOffset);
}

// --------------------------
// SecondaryClockSynchronizer
// --------------------------

SecondaryClockSynchronizer::SecondaryClockSynchronizer(std::shared_ptr<SyncTimer> masterTimer, AbstractModule *mod,
                                                       double frequencyHz, const QString &id)
    : m_mod(mod),
      m_id(id),
      m_strategies(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD | TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD),
      m_lastOffsetEmission(0),
      m_syTimer(masterTimer),
      m_toleranceUsec(SECONDARY_CLOCK_TOLERANCE.count()),
      m_checkInterval(DEFAULT_CLOCKSYNC_CHECK_INTERVAL),
      m_lastUpdateTime(milliseconds_t(DEFAULT_CLOCKSYNC_CHECK_INTERVAL * -1)),
      m_firstTimestamp(true),
      m_lastSecondaryAcqTimestamp(0),
      m_lastMasterTS(0),
      m_tswriter(new TimeSyncFileWriter)
{
    m_acqInterval = std::chrono::microseconds(static_cast<long>((1000.0 / frequencyHz) * 1000));

    if (m_id.isEmpty())
        m_id = createRandomString(4);

    // make our existence known to the system
    emit m_mod->synchronizerDetailsChanged(m_id, m_strategies, std::chrono::microseconds(m_toleranceUsec), m_checkInterval);
}

SecondaryClockSynchronizer::~SecondaryClockSynchronizer()
{
    stop();
}

milliseconds_t SecondaryClockSynchronizer::clockCorrectionOffset() const
{
    return m_clockCorrectionOffset;
}

void SecondaryClockSynchronizer::setTimeSyncBasename(const QString &fname)
{
    m_tswriter->setFileName(fname);
    m_strategies = m_strategies.setFlag(TimeSyncStrategy::WRITE_TSYNCFILE, !fname.isEmpty());
}

void SecondaryClockSynchronizer::setStrategies(const TimeSyncStrategies &strategies)
{
    if (!m_firstTimestamp) {
        qWarning().noquote() << "Rejected strategy change on active Clock Synchronizer for" << m_mod->name();
        return;
    }
    m_strategies = strategies;
    emit m_mod->synchronizerDetailsChanged(m_id, m_strategies, std::chrono::microseconds(m_toleranceUsec), m_checkInterval);
}

void SecondaryClockSynchronizer::setCheckInterval(const milliseconds_t &interval)
{
    if (!m_firstTimestamp) {
        qWarning().noquote() << "Rejected check-interval change on active Clock Synchronizer for" << m_mod->name();
        return;
    }
    m_checkInterval = interval;
    emit m_mod->synchronizerDetailsChanged(m_id, m_strategies, std::chrono::microseconds(m_toleranceUsec), m_checkInterval);
}

void SecondaryClockSynchronizer::setTolerance(const std::chrono::microseconds &tolerance)
{
    if (!m_firstTimestamp) {
        qWarning().noquote() << "Rejected tolerance change on active Clock Synchronizer for" << m_mod->name();
        return;
    }
    m_toleranceUsec = tolerance.count();
    emit m_mod->synchronizerDetailsChanged(m_id, m_strategies, std::chrono::microseconds(m_toleranceUsec), m_checkInterval);
}

bool SecondaryClockSynchronizer::start()
{
    if (!m_firstTimestamp) {
        qWarning().noquote() << "Restarting a Clock Synchronizer that has already been used is not permitted. This is an issue in " << m_mod->name();
        return false;
    }
    if (m_strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE)) {
        if (!m_tswriter->open(m_checkInterval, microseconds_t(m_toleranceUsec), m_mod->name())) {
            qCritical().noquote().nospace() << "Unable to open timesync file for " << m_mod->name() << "[" << m_id << "]: " << m_tswriter->lastError();
            return false;
        }
    }
    m_lastOffsetWithinTolerance = false;
    m_firstTimestamp = true;
    m_lastMasterTS = milliseconds_t(0);
    m_clockCorrectionOffset = milliseconds_t(0);

    return true;
}

void SecondaryClockSynchronizer::stop()
{
    m_tswriter->close();
}

void SecondaryClockSynchronizer::processTimestamp(milliseconds_t &masterTimestamp, const milliseconds_t &secondaryAcqTimestamp)
{
    if (m_firstTimestamp) {
        m_firstTimestamp = false;
        m_timeDiffToMaster = masterTimestamp - secondaryAcqTimestamp;
        m_lastSecondaryAcqTimestamp = secondaryAcqTimestamp - m_acqInterval;
    }

    // adjust time difference slowly, weighting the existing time higher than the newly added time difference
    m_timeDiffToMaster = milliseconds_t(static_cast<long>(((m_timeDiffToMaster.count() * m_freqHz * 2.0) + (masterTimestamp.count() - secondaryAcqTimestamp.count())) / (m_freqHz * 2.0 + 1)));

    // calculate expected timestamps, if the external device was running with the selected frequency
    // FIXME: Do we allow dynamic-frequency DAQ devices as well later?
    auto expectedSecondaryAcqTS = std::chrono::duration_cast<milliseconds_t>(m_lastSecondaryAcqTimestamp + m_acqInterval);
    const auto expectedMasterTS = expectedSecondaryAcqTS + m_timeDiffToMaster;
    m_lastSecondaryAcqTimestamp = secondaryAcqTimestamp;


    if (m_strategies.testFlag(TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD)) {
        if (expectedMasterTS < m_lastMasterTS)
            masterTimestamp = expectedMasterTS;
    }
    if (m_strategies.testFlag(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD)) {
        if (expectedMasterTS > m_lastMasterTS)
            masterTimestamp = expectedMasterTS;
    }

    const auto currentTimestamp = m_syTimer->timeSinceStartMsec();
    // do nothing if we aren't supposed to adjust the clock offset yet
    if ((currentTimestamp - m_lastUpdateTime) < m_checkInterval)
        return;
    m_lastUpdateTime = currentTimestamp;

    m_clockCorrectionOffset = std::chrono::duration_cast<milliseconds_t>((expectedSecondaryAcqTS - secondaryAcqTimestamp) / 2.5);

    if (std::abs(m_clockCorrectionOffset.count() * 1000) < m_toleranceUsec) {
        // everything is within tolerance range, no adjustments needed
        // share the good news with the controller! (only once, for now)
        if (!m_lastOffsetWithinTolerance)
            emit m_mod->synchronizerOffsetChanged(m_id, m_clockCorrectionOffset);
        m_clockCorrectionOffset = milliseconds_t(0);
        m_lastOffsetWithinTolerance = true;
        return;
    }

    // Emit offset change information to the main controller every 2sec or slower
    // in case we run at slower speeds
    if ((m_lastOffsetEmission.count() + 2000) < expectedMasterTS.count()) {
        emit m_mod->synchronizerOffsetChanged(m_id, m_clockCorrectionOffset);
        m_lastOffsetEmission = currentTimestamp;
    }

    if (m_strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE)) {
        m_tswriter->writeTimes(secondaryAcqTimestamp, masterTimestamp);
    }
}

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
        return QStringLiteral("shift timestamps (forward)");
    case TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD:
        return QStringLiteral("shift timestamps (backward)");
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

TimeSyncFileWriter::TimeSyncFileWriter()
    : m_file(nullptr),
      m_index(0)
{
    m_file = new QFile();
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

bool TimeSyncFileWriter::open(const QString &fname, const QString &modName, const microseconds_t &checkInterval, const microseconds_t &tolerance)
{
    if (m_file->isOpen())
        m_file->close();

    auto tsyncFname = fname;
    if (!tsyncFname.endsWith(QStringLiteral(".tsync")))
        tsyncFname = tsyncFname + QStringLiteral(".tsync");
    m_file->setFileName(tsyncFname);
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

    m_file->flush();
    return true;
}

void TimeSyncFileWriter::flush()
{
    if (m_file->isOpen())
        m_file->flush();
}

void TimeSyncFileWriter::writeTimeOffset(const microseconds_t &deviceTime, const microseconds_t &offset)
{
    m_stream << (quint64) m_index++;
    m_stream << (qint64) deviceTime.count();
    m_stream << (qint64) offset.count();

    // TODO: get rid of this
    m_file->flush();
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

    in >> magic >> formatVersion;
    if ((magic != TSYNC_FILE_MAGIC) || (formatVersion != TSYNC_FILE_FORMAT_VERSION)) {
        m_lastError = QStringLiteral("Unable to read data: This file is not a valid timesync metadata file.");
        return false;
    }

    in >> m_creationTime
       >> checkIntervalUsec
       >> toleranceUsec
       >> m_moduleName;
    m_checkInterval = microseconds_t(checkIntervalUsec);
    m_tolerance = microseconds_t(toleranceUsec);

    // read the offset data
    m_offsets.clear();
    quint64 expectedIndex = 0;
    while (!in.atEnd()) {
        quint64 index;
        qint64 deviceTimeUsec;
        qint64 offsetUsec;

        in >> index
           >> deviceTimeUsec
           >> offsetUsec;
        if (index != expectedIndex)
            qWarning().noquote() << "The timesync file has gaps: Expected index" << expectedIndex << "but got" << index;

        m_offsets.append(qMakePair(microseconds_t(deviceTimeUsec), microseconds_t(offsetUsec)));
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

QList<QPair<microseconds_t, microseconds_t> > TimeSyncFileReader::offsets() const
{
    return m_offsets;
}

FreqCounterSynchronizer::FreqCounterSynchronizer(std::shared_ptr<SyncTimer> masterTimer, AbstractModule *mod, double frequencyHz, const QString &id)
    : m_mod(mod),
      m_id(id),
      m_strategies(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD | TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD),
      m_isFirstInterval(true),
      m_syTimer(masterTimer),
      m_toleranceUsec(SECONDARY_CLOCK_TOLERANCE.count()),
      m_checkInterval(DEFAULT_CLOCKSYNC_CHECK_INTERVAL),
      m_lastUpdateTime(milliseconds_t(DEFAULT_CLOCKSYNC_CHECK_INTERVAL * -1)),
      m_freq(frequencyHz),
      m_baseTime(0),
      m_indexOffset(0),
      m_tswriter(new TimeSyncFileWriter)
{
    if (m_id.isEmpty())
        m_id = createRandomString(4);

    if (!m_tswriter->open("/tmp/testts", "mysimplemod!!", m_checkInterval, SECONDARY_CLOCK_TOLERANCE))
        qCritical() << "!!!!" << m_tswriter->lastError();

    // make our existence known to the system
    emit m_mod->synchronizerDetailsChanged(m_id, m_strategies, std::chrono::microseconds(m_toleranceUsec), m_checkInterval);
}

FreqCounterSynchronizer::~FreqCounterSynchronizer()
{
    delete m_tswriter;
}

milliseconds_t FreqCounterSynchronizer::timeBase() const
{
    return m_baseTime;
}

int FreqCounterSynchronizer::indexOffset() const
{
    return m_indexOffset;
}

void FreqCounterSynchronizer::setStrategies(const TimeSyncStrategies &strategies)
{
    if (!m_isFirstInterval) {
        qWarning().noquote() << "Rejected strategy change on active FreqCounter Synchronizer for" << m_mod->name();
        return;
    }
    m_strategies = strategies;
    emit m_mod->synchronizerDetailsChanged(m_id, m_strategies, std::chrono::microseconds(m_toleranceUsec), m_checkInterval);
}

void FreqCounterSynchronizer::setCheckInterval(const std::chrono::seconds &intervalSec)
{
    if (!m_isFirstInterval) {
        qWarning().noquote() << "Rejected check-interval change on active FreqCounter Synchronizer for" << m_mod->name();
        return;
    }
    m_checkInterval = intervalSec;
    emit m_mod->synchronizerDetailsChanged(m_id, m_strategies, std::chrono::microseconds(m_toleranceUsec), m_checkInterval);
}

void FreqCounterSynchronizer::setTolerance(const std::chrono::microseconds &tolerance)
{
    if (!m_isFirstInterval) {
        qWarning().noquote() << "Rejected tolerance change on active FreqCounter Synchronizer for" << m_mod->name();
        return;
    }
    m_toleranceUsec = tolerance.count();
    emit m_mod->synchronizerDetailsChanged(m_id, m_strategies, std::chrono::microseconds(m_toleranceUsec), m_checkInterval);
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
    int changeInt = std::floor(((timeOffsetUsec / 1000.0 / 1000.0) * m_freq) / (m_freq / 20000 + 1));
    VectorXl change;

    if (timeOffsetUsec > 0) {
        // the external device is running too slow

        change = VectorXl::LinSpaced(idxTimestamps.rows(), 0, changeInt);
        qWarning().nospace() << "Index offset changed by " << changeInt << " to " << m_indexOffset << " (in raw idx: " << (timeOffsetUsec / 1000.0 / 1000.0) * m_freq << ")";
    } else {
        qWarning() << "External device is too fast!";

        if (m_isFirstInterval) {
            // we change the initial index to match the master clock exactly
            changeInt = std::round((timeOffsetUsec / 1000.0 / 1000.0) * m_freq);
            change = VectorXl::LinSpaced(idxTimestamps.rows(), 0, changeInt);
            m_indexOffset += change[change.rows() - 1];

            qWarning().nospace() << "Index offset changed by " << changeInt << " to " << m_indexOffset;
        } else {
            qWarning().nospace() << "Would change index offset by " << changeInt << " to " << m_indexOffset << " (in raw idx: " << (timeOffsetUsec / 1000.0 / 1000.0) * m_freq << "), but change not possible";
        }
    }

    m_tswriter->writeTimeOffset(lastTimestamp, timeOffset);

    // adjust time indices again based on the current change
    if (changeInt != 0)
        idxTimestamps += change;

    m_isFirstInterval = false;
    qDebug().noquote() << "";
}

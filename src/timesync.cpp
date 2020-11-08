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
#include <QJsonObject>
#include <QJsonDocument>
#include <zlib.h>
#include "moduleapi.h"

#include "utils.h"

namespace Syntalos {
    Q_LOGGING_CATEGORY(logTimeSync, "time.synchronizer")
}

using namespace Syntalos;
using namespace Eigen;

// TSYNC file magic number (converted to LE): 8A T S Y N C ⏲
#define TSYNC_FILE_MAGIC 0xF223434E5953548A

#define TSYNC_FILE_VERSION_MAJOR  1
#define TSYNC_FILE_VERSION_MINOR  0

#define TSYNC_FILE_BLOCK_TERM 0x1126000000000000

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
      m_bIndex(0)
{
    m_timeNames = qMakePair(QStringLiteral("device-time"),
                            QStringLiteral("master-time"));
    m_timeUnits = qMakePair(TimeSyncFileTimeUnit::MICROSECONDS,
                            TimeSyncFileTimeUnit::MICROSECONDS);
    m_time1DType = TimeSyncFileDataType::UINT32;
    m_time2DType = TimeSyncFileDataType::UINT32;
    m_blockSize = 2400;

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

void TimeSyncFileWriter::setTimeNames(const QString &time1Name, const QString &time2Name)
{
    m_timeNames = qMakePair(time1Name, time2Name);
}

void TimeSyncFileWriter::setTimeUnits(TimeSyncFileTimeUnit time1Unit, TimeSyncFileTimeUnit time2Unit)
{
    m_timeUnits = qMakePair(time1Unit, time2Unit);
}

void TimeSyncFileWriter::setTimeDataTypes(TimeSyncFileDataType time1DType, TimeSyncFileDataType time2DType)
{
    m_time1DType = time1DType;
    m_time2DType = time2DType;
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

bool Syntalos::TimeSyncFileWriter::TimeSyncFileWriter::open(const QString &modName, const QUuid &collectionId, const QVariantHash &userData)
{
    if (m_file->isOpen())
        m_file->close();

    if (!m_file->open(QIODevice::WriteOnly)) {
        m_lastError = m_file->errorString();
        return false;
    }

    // ensure block size is not extremely small
    if ((m_blockSize >= 0) && (m_blockSize < 128))
        m_blockSize = 128;

    m_bIndex = 0;
    m_blockCRC = 0;
    m_stream.setDevice(m_file);

    // user-defined metadata
    QJsonDocument jdoc(QJsonObject::fromVariantHash(userData));
    const QString userDataJson(jdoc.toJson(QJsonDocument::Compact));

    // write file header
    QDateTime currentTime(QDateTime::currentDateTime());

    m_stream << (quint64) TSYNC_FILE_MAGIC;
    m_stream << (quint16) TSYNC_FILE_VERSION_MAJOR;
    m_stream << (quint16) TSYNC_FILE_VERSION_MINOR;
    m_stream << (qint64) currentTime.toTime_t();
    m_stream << modName.toUtf8();
    m_stream << collectionId.toString(QUuid::WithoutBraces).toUtf8();
    m_stream << userDataJson.toUtf8(); // custom JSON values

    m_stream << (qint32) m_blockSize;

    m_stream << m_timeNames.first.toUtf8();
    m_stream << (quint16) m_timeUnits.first;
    m_stream << (quint16) m_time1DType;

    m_stream << m_timeNames.second.toUtf8();
    m_stream << (quint16) m_timeUnits.second;
    m_stream << (quint16) m_time2DType;

    m_file->flush();
    return true;
}

bool TimeSyncFileWriter::open(const microseconds_t &tolerance, const QString &modName, const QUuid &collectionId, const QVariantHash &userData)
{
    QVariantHash udata = userData;
    udata["tolerance_us"] = QVariant::fromValue(tolerance.count());
    return open(modName, collectionId, udata);
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
    writeEntry(deviceTime.count(), masterTime.count());
}

void TimeSyncFileWriter::writeTimes(const long long &timeIndex, const microseconds_t &masterTime)
{
    writeEntry(timeIndex, masterTime.count());
}

template<class T>
void TimeSyncFileWriter::writeData(const T &data)
{
    auto d = (T) data;
    m_stream << d;
    m_blockCRC = crc32_z(m_blockCRC, (const Bytef*) &d, sizeof(d));
}

template<class T1, class T2>
void TimeSyncFileWriter::writeEntry(const T1 &time1, const T2 &time2)
{
    switch (m_time1DType) {
        case TimeSyncFileDataType::INT16:
            writeData<qint16>(time1); break;
        case TimeSyncFileDataType::INT32:
            writeData<qint32>(time1); break;
        case TimeSyncFileDataType::INT64:
            writeData<qint64>(time1); break;
        case TimeSyncFileDataType::UINT16:
            writeData<quint16>(time1); break;
        case TimeSyncFileDataType::UINT32:
            writeData<quint32>(time1); break;
        case TimeSyncFileDataType::UINT64:
            writeData<quint64>(time1); break;
        default:
            qFatal("Tried to write unknown datatype to timesync file for time1: %i", (int) m_time1DType);
            break;
    }

    switch (m_time2DType) {
        case TimeSyncFileDataType::INT16:
            writeData<qint16>(time2); break;
        case TimeSyncFileDataType::INT32:
            writeData<qint32>(time2); break;
        case TimeSyncFileDataType::INT64:
            writeData<qint64>(time2); break;
        case TimeSyncFileDataType::UINT16:
            writeData<quint16>(time2); break;
        case TimeSyncFileDataType::UINT32:
            writeData<quint32>(time2); break;
        case TimeSyncFileDataType::UINT64:
            writeData<quint64>(time2); break;
        default:
            qFatal("Tried to write unknown datatype to timesync file for time2: %i", (int) m_time1DType);
            break;
    }

    m_bIndex++;
    if (m_bIndex >= m_blockSize) {
        m_stream << (quint64) TSYNC_FILE_BLOCK_TERM;
        m_stream << (quint32) m_blockCRC;
        m_blockCRC = 0;
        m_bIndex = 0;
    }
}

TimeSyncFileReader::TimeSyncFileReader()
    : m_lastError(QString())
{}

template<class T>
static long long readTsyncValue(QDataStream &in, quint32 *crc)
{
    T value;
    in >> value;
    *crc = crc32_z(*crc, (const Bytef*) &value, sizeof(value));
    return value;
}

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
    quint64 magic;
    quint16 formatVMajor;
    quint16 formatVMinor;

    in >> magic >> formatVMajor >> formatVMinor;
    if ((magic != TSYNC_FILE_MAGIC) || (formatVMajor != TSYNC_FILE_VERSION_MAJOR)) {
        m_lastError = QStringLiteral("Unable to read data: This file is not a valid timesync metadata file.");
        return false;
    }

    in >> m_creationTime;

    QByteArray modNameUtf8;
    QByteArray collectionIdUtf8;
    QByteArray userJsonUtf8;

    in >> modNameUtf8 >> collectionIdUtf8 >> userJsonUtf8;
    m_moduleName = QString::fromUtf8(modNameUtf8);
    m_collectionId = QUuid(QString::fromUtf8(collectionIdUtf8));


    QJsonDocument jdoc = QJsonDocument::fromJson(userJsonUtf8);
    m_userData = QVariantHash();
    if (jdoc.isObject())
        m_userData = jdoc.object().toVariantHash();

    // block size
    qint32 bSize;
    in >> bSize;
    m_blockSize = bSize;

    // time info
    quint16 timeUnit1;
    quint16 timeUnit2;
    quint16 timeDType1_i;
    quint16 timeDType2_i;
    QByteArray timeName1Utf8;
    QByteArray timeName2Utf8;

    in >> timeName1Utf8
       >> timeUnit1
       >> timeDType1_i
       >> timeName2Utf8
       >> timeUnit2
       >> timeDType2_i;

    m_timeNames.first = QString::fromUtf8(timeName1Utf8);
    m_timeNames.second = QString::fromUtf8(timeName2Utf8);
    m_tolerance = microseconds_t(m_userData.value("tolerance_us").toLongLong());
    m_timeUnits.first = static_cast<TimeSyncFileTimeUnit>(timeUnit1);
    m_timeUnits.second = static_cast<TimeSyncFileTimeUnit>(timeUnit2);

    const auto timeDType1 = static_cast<TimeSyncFileDataType>(timeDType1_i);
    const auto timeDType2 = static_cast<TimeSyncFileDataType>(timeDType2_i);

    // read the time data
    m_times.clear();
    int bIndex = 0;
    quint32 crc = 0;
    while (!in.atEnd()) {
        long long timeVal1;
        long long timeVal2;

        switch (timeDType1) {
            case TimeSyncFileDataType::INT16:
                timeVal1 = readTsyncValue<qint16>(in, &crc); break;
            case TimeSyncFileDataType::INT32:
                timeVal1 = readTsyncValue<qint32>(in, &crc); break;
            case TimeSyncFileDataType::INT64:
                timeVal1 = readTsyncValue<qint64>(in, &crc); break;
            case TimeSyncFileDataType::UINT16:
                timeVal1 = readTsyncValue<quint16>(in, &crc); break;
            case TimeSyncFileDataType::UINT32:
                timeVal1 = readTsyncValue<quint32>(in, &crc); break;
            case TimeSyncFileDataType::UINT64:
                timeVal1 = readTsyncValue<quint64>(in, &crc); break;
            default:
                qFatal("Tried to read unknown datatype to timesync file for time1: %i", (int) timeDType1);
                break;
        }

        switch (timeDType2) {
            case TimeSyncFileDataType::INT16:
                timeVal2 = readTsyncValue<qint16>(in, &crc); break;
            case TimeSyncFileDataType::INT32:
                timeVal2 = readTsyncValue<qint32>(in, &crc); break;
            case TimeSyncFileDataType::INT64:
                timeVal2 = readTsyncValue<qint64>(in, &crc); break;
            case TimeSyncFileDataType::UINT16:
                timeVal2 = readTsyncValue<quint16>(in, &crc); break;
            case TimeSyncFileDataType::UINT32:
                timeVal2 = readTsyncValue<quint32>(in, &crc); break;
            case TimeSyncFileDataType::UINT64:
                timeVal2 = readTsyncValue<quint64>(in, &crc); break;
            default:
                qFatal("Tried to read unknown datatype to timesync file for time2: %i", (int) timeDType2);
                break;
        }

        m_times.append(qMakePair(timeVal1, timeVal2));

        bIndex++;
        if (bIndex == m_blockSize) {
            quint64 terminator;
            quint32 expectedCRC;
            in >> terminator >> expectedCRC;

            if (terminator != TSYNC_FILE_BLOCK_TERM) {
                qCritical().noquote() << "Unable to read all tsync data: Block separator was invalid.";
                return false;
            }
            if (expectedCRC != crc)
                qWarning().noquote() << "CRC check failed for tsync data block: Data is likely corrupted.";

            crc = 0;
            bIndex = 0;
        }
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
      m_calibrationMaxBlockN(500),
      m_calibrationIdx(0),
      m_haveExpectedOffset(false),
      m_freq(frequencyHz),
      m_tswriter(new TimeSyncFileWriter)
{
    if (m_id.isEmpty())
        m_id = createRandomString(4);

    // time one datapoint takes to acquire, if the frequency is accurate, in microseconds
    m_timePerPointUs = (1.0 / m_freq) * 1000.0 * 1000.0;

    // make our existence known to the system
    if (m_mod != nullptr)
        emit m_mod->synchronizerDetailsChanged(m_id, m_strategies, std::chrono::microseconds(m_toleranceUsec));
}

FreqCounterSynchronizer::~FreqCounterSynchronizer()
{
    stop();
}

int FreqCounterSynchronizer::indexOffset() const
{
    return m_indexOffset;
}

void FreqCounterSynchronizer::setCalibrationBlocksCount(int count)
{
    if (count <= 0)
        count = 3;
    m_calibrationMaxBlockN = count;
}

void FreqCounterSynchronizer::setTimeSyncBasename(const QString &fname)
{
    m_tswriter->setFileName(fname);
    m_strategies = m_strategies.setFlag(TimeSyncStrategy::WRITE_TSYNCFILE, !fname.isEmpty());
}

bool FreqCounterSynchronizer::isCalibrated() const
{
    return m_haveExpectedOffset;
}

void FreqCounterSynchronizer::setStrategies(const TimeSyncStrategies &strategies)
{
    if (m_haveExpectedOffset) {
        qWarning().noquote() << "Rejected strategy change on active FreqCounter Synchronizer for" << m_mod->name();
        return;
    }
    m_strategies = strategies;
    if (m_mod != nullptr)
        emit m_mod->synchronizerDetailsChanged(m_id, m_strategies, std::chrono::microseconds(m_toleranceUsec));
}

void FreqCounterSynchronizer::setTolerance(const std::chrono::microseconds &tolerance)
{
    if (m_haveExpectedOffset) {
        qWarning().noquote() << "Rejected tolerance change on active FreqCounter Synchronizer for" << m_mod->name();
        return;
    }
    m_toleranceUsec = tolerance.count();
    if (m_mod != nullptr)
        emit m_mod->synchronizerDetailsChanged(m_id, m_strategies, std::chrono::microseconds(m_toleranceUsec));
}

bool FreqCounterSynchronizer::start()
{
    if (m_haveExpectedOffset) {
        qWarning().noquote() << "Restarting a FreqCounter Synchronizer that has already been used is not permitted. This is an issue in " << m_mod->name();
        return false;
    }
    if (m_strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE)) {
        if (!m_tswriter->open(microseconds_t(m_toleranceUsec), m_mod->name())) {
            qCritical().noquote().nospace() << "Unable to open timesync file for " << m_mod->name() << "[" << m_id << "]: " << m_tswriter->lastError();
            return false;
        }
    }

    m_lastOffsetWithinTolerance = false;
    m_timeCorrectionOffset = microseconds_t(0);
    m_haveExpectedOffset = false;
    m_calibrationIdx = 0;
    m_expectedOffsetCalCount = 0;
    m_tsOffsetsUsec = VectorXl::Zero(m_calibrationMaxBlockN);
    m_lastTimeIndex = 0;
    m_indexOffset = 0;
    m_offsetChangeWaitBlocks = 0;
    m_applyIndexOffset = false;

    return true;
}

void FreqCounterSynchronizer::stop()
{
    m_tswriter->close();
}

void FreqCounterSynchronizer::processTimestamps(const microseconds_t &blocksRecvTimestamp, const microseconds_t &deviceLatency,
                                                int blockIndex, int blockCount, VectorXu &idxTimestamps)
{
    // basic input value sanity checks
    assert(blockCount >= 1);
    assert(blockIndex >= 0);
    assert(blockIndex < blockCount);

    // get last index value of vector before we made any adjustments to it
    const auto secondaryLastIdxUnadjusted = idxTimestamps[idxTimestamps.rows() - 1];

    // adjust timestamp based on our current offset
    if (m_applyIndexOffset && (m_indexOffset != 0))
        idxTimestamps -= VectorXu::Ones(idxTimestamps.rows()) * m_indexOffset;

    // timestamp when (as far and well as we can guess...) the current block was actually acquired, in microseconds
    // and based on the master clock timestamp generated upon data receival
    const microseconds_t masterAssumedAcqTS = blocksRecvTimestamp
                                                - microseconds_t(std::lround(m_timePerPointUs * ((blockCount - 1) * idxTimestamps.rows())))
                                                + microseconds_t(std::lround(m_timePerPointUs * (blockIndex * idxTimestamps.rows())))
                                                - deviceLatency;

    // value of the last entry of the current block
    const auto secondaryLastIdx = idxTimestamps[idxTimestamps.rows() - 1];

    // Timestamp, in microseconds, when according to the device frequency the last datapoint of this block was acquired
    // since we assume a zero-indexed time series, we need to add one to the secondary index
    // If the index offset has already been applied, take the value as-is, otherwise apply our current offset even if
    // modifications to the data are not permitted (we need the corrected last timestamp here, even if we don't apply
    // it to the output data and are just writing a tsync file)
    const auto secondaryLastTS = m_applyIndexOffset?
                                    microseconds_t(std::lround((secondaryLastIdx + 1) * m_timePerPointUs)) :
                                    microseconds_t(std::lround((secondaryLastIdxUnadjusted + 1 - m_indexOffset) * m_timePerPointUs));

    // calculate time offset
    const long long curOffsetUsec = (secondaryLastTS - masterAssumedAcqTS).count();

    // calculate offsets without the new datapoint included
    const auto avgOffsetUsec = m_tsOffsetsUsec.mean();
    const auto avgOffsetDeviationUsec = avgOffsetUsec - m_expectedOffset.count();

    // add new datapoint to our "memory" vector
    m_tsOffsetsUsec[m_calibrationIdx++] = curOffsetUsec;
    if (m_calibrationIdx >= m_calibrationMaxBlockN)
        m_calibrationIdx = 0;

    // we do nothing more until we have enought measurements to estimate the "natural" timer offset
    // of the secondary clock and master clock
    if (!m_haveExpectedOffset) {
        m_expectedOffsetCalCount++;

        // we want a bit more values than needed for perpetual calibration, because the first
        // few values in the vector stem from the initialization phase of Syntalos and may have
        // a higher variance than actually expected during normal operation (as in the startup
        // phase, the system load is high and lots of external devices are starting up)
        if (m_expectedOffsetCalCount < (m_calibrationMaxBlockN + (m_calibrationMaxBlockN / 2)))
            return;

        m_expectedSD = sqrt(vectorVariance(m_tsOffsetsUsec));
        m_expectedOffset = microseconds_t(std::lround(vectorMedianInplace(m_tsOffsetsUsec)));

        qCDebug(logTimeSync).noquote().nospace() << QTime::currentTime().toString() << "[" << m_id << "] "
                << "Determined expected time offset: " << m_expectedOffset.count() << "µs "
                << "SD: " << m_expectedSD;
        m_haveExpectedOffset = true;

        // if we are writing a timesync-file, write the initial two timestamps when we
        // calibrated the system to the file (as additional verification point)
        if (m_strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE))
            m_tswriter->writeTimes(secondaryLastTS, masterAssumedAcqTS);

        // send (possibly initial) offset info to the controller)
        if (m_mod != nullptr)
            emit m_mod->synchronizerOffsetChanged(m_id, microseconds_t(avgOffsetDeviationUsec));

        m_lastTimeIndex = secondaryLastIdx;
        return;
    }

    // do nothing if we have not enough average deviation from the norm
    if (abs(avgOffsetDeviationUsec) < m_toleranceUsec) {
        // we are within tolerance range!
        // share the good news with the controller! (immediately on change, or every 30sec otherwise)
        if ((blockIndex == 0) &&
                ((!m_lastOffsetWithinTolerance)
                 || (blocksRecvTimestamp.count() > (m_lastOffsetEmission.count() + (30 * 1000 * 1000))))) {
            if (m_mod != nullptr)
                emit m_mod->synchronizerOffsetChanged(m_id, microseconds_t(avgOffsetDeviationUsec));
            m_lastOffsetEmission = blocksRecvTimestamp;
        }

        // check if we would still be within half-tolerance if we did reset the index offset completely, and if that's the case
        // reset it as the external clock for some reason may be accurate again
        if ((m_indexOffset != 0) && (abs(avgOffsetDeviationUsec + m_timeCorrectionOffset.count()) < (m_toleranceUsec / 2))) {
            m_indexOffset = m_indexOffset / 2.0;
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

    const auto offsetsSD = sqrt(vectorVariance(m_tsOffsetsUsec, avgOffsetUsec));
    if (abs(avgOffsetUsec - curOffsetUsec) > offsetsSD) {
        // the current offset diff to the moving average offset is not within standard deviation range.
        // This means the data point we just added is likely a fluke, potentially due to a context switch
        // or system load spike. We just ignore those events completely and don't make time adjustments
        // to index offsets based on them.
        if (m_offsetChangeWaitBlocks > 0)
            m_offsetChangeWaitBlocks--;
        m_lastTimeIndex = secondaryLastIdx;
        return;
    }

    // don't do even more adjustments until we have lived with the current one for
    // half a calibration phase.
    // otherwise the system will rapidly shift the index around, usually never reaching
    // a stable equilibrium
    if (m_offsetChangeWaitBlocks > 0) {
        m_offsetChangeWaitBlocks--;
        m_lastTimeIndex = secondaryLastIdx;
        return;
    }

    // Emit offset information to the main controller about every 10sec or slower
    // in case we run at slower speeds
    if ((blockIndex == 0) && (masterAssumedAcqTS.count() > (m_lastOffsetEmission.count() + (10 * 1000 * 1000)))) {
        if (m_mod != nullptr)
            emit m_mod->synchronizerOffsetChanged(m_id, microseconds_t(avgOffsetDeviationUsec));
        m_lastOffsetEmission = blocksRecvTimestamp;
    }

    // calculate time-based correction offset
    m_timeCorrectionOffset = microseconds_t(std::lround((m_timeCorrectionOffset.count() + avgOffsetDeviationUsec) / 2));

    // translate the clock update offset to indices. We round up here as we are already below threshold,
    // and overshooting slightly appears to be the better solution than being too conservative
    const bool initialOffset = m_indexOffset == 0;
    m_indexOffset = static_cast<int>((m_timeCorrectionOffset.count() / 1000.0 / 1000.0) * m_freq);

    if (m_indexOffset != 0) {
        m_offsetChangeWaitBlocks = ceil(m_calibrationMaxBlockN / 16.0);

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
            idxTimestamps -= VectorXu::LinSpaced(idxTimestamps.rows(), 0, m_indexOffset);
    }

    // we're out of sync, record that fact to the tsync file if we are writing one
    // NOTE: we have to use the unadjusted time value for the device clock - sicne we didn't need that until now,
    // we calculate it here from the unadjusted last index value of the current block.
    if (m_strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE))
        m_tswriter->writeTimes(microseconds_t(std::lround((secondaryLastIdxUnadjusted + 1) * m_timePerPointUs)),
                               masterAssumedAcqTS);

    m_lastTimeIndex = secondaryLastIdx;
}

void FreqCounterSynchronizer::processTimestamps(const microseconds_t &recvTimestamp, const double &devLatencyMs, int blockIndex, int blockCount, VectorXu &idxTimestamps)
{
    // we want the device latency in microseconds
    auto deviceLatency = std::chrono::microseconds(static_cast<long>(devLatencyMs * 1000));
    processTimestamps(recvTimestamp, deviceLatency, blockIndex, blockCount, idxTimestamps);
}

// --------------------------
// SecondaryClockSynchronizer
// --------------------------

SecondaryClockSynchronizer::SecondaryClockSynchronizer(std::shared_ptr<SyncTimer> masterTimer, AbstractModule *mod, const QString &id)
    : m_mod(mod),
      m_id(id),
      m_strategies(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD | TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD),
      m_lastOffsetEmission(0),
      m_syTimer(masterTimer),
      m_toleranceUsec(SECONDARY_CLOCK_TOLERANCE.count()),
      m_calibrationMaxN(500),
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

microseconds_t SecondaryClockSynchronizer::clockCorrectionOffset() const
{
    return m_clockCorrectionOffset;
}

void SecondaryClockSynchronizer::setCalibrationPointsCount(int timepointCount)
{
    if (m_haveExpectedOffset) {
        qCWarning(logTimeSync).noquote() << "Rejected calibration point count change on active Clock Synchronizer for" << m_mod->name();
        return;
    }
    if (timepointCount > 10)
        m_calibrationMaxN = timepointCount;
}

void SecondaryClockSynchronizer::setExpectedClockFrequencyHz(double frequency)
{
    if (m_haveExpectedOffset) {
        qCWarning(logTimeSync).noquote() << "Rejected frequency change on active Clock Synchronizer for" << m_mod->name();
        return;
    }

    if (frequency == 0) {
        qCWarning(logTimeSync).noquote() << "Rejected bogus frequency change to 0 for" << m_mod->name();
        return;
    }

    // the amount of datapoints needed is on a curve, approaching 5 sec (or minmal required time)
    // if we get a lot of points in a short time, we don't need to wait that long to calculate the
    // average offset, but with a low frequency of new points we need a bit more data to calculate
    // the averages and their SD reliably
    m_calibrationMaxN = frequency * (5 + (30 / ((0.02 * frequency) + 1.4)));

    // set tolerance of half the time one sample takes to be acquired
    m_toleranceUsec = std::lround(((1000.0 / frequency) / 2) * 1000.0);
    emitSyncDetailsChanged();
}

void SecondaryClockSynchronizer::setTimeSyncBasename(const QString &fname)
{
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
        qCWarning(logTimeSync).noquote() << "Rejected strategy change on active Clock Synchronizer for" << m_mod->name();
        return;
    }
    m_strategies = strategies;
    emitSyncDetailsChanged();
}

void SecondaryClockSynchronizer::setTolerance(const microseconds_t &tolerance)
{
    if (m_haveExpectedOffset) {
        qCWarning(logTimeSync).noquote() << "Rejected tolerance change on active Clock Synchronizer for" << m_mod->name();
        return;
    }
    m_toleranceUsec = tolerance.count();
    emitSyncDetailsChanged();
}

bool SecondaryClockSynchronizer::start()
{
    if (m_haveExpectedOffset) {
        qCWarning(logTimeSync).noquote() << "Restarting a Clock Synchronizer that has already been used is not permitted. This is an issue in " << m_mod->name();
        return false;
    }
    if (m_strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE)) {
        if (!m_tswriter->open(microseconds_t(m_toleranceUsec), m_mod->name())) {
            qCCritical(logTimeSync).noquote().nospace() << "Unable to open timesync file for " << m_mod->name() << "[" << m_id << "]: " << m_tswriter->lastError();
            return false;
        }
    }

    if (m_calibrationMaxN <= 4)
        qCCritical(logTimeSync).noquote().nospace() << "Clock synchronizer for " << m_mod->name() << "[" << m_id << "] uses a tiny calibration array (length <= 4)";
    assert(m_calibrationMaxN > 0);

    m_lastOffsetWithinTolerance = false;
    m_clockCorrectionOffset = microseconds_t(0);
    m_haveExpectedOffset = false;
    m_calibrationIdx = 0;
    m_expectedOffsetCalCount = 0;
    m_clockOffsetsUsec = VectorXl::Zero(m_calibrationMaxN);
    m_lastMasterTS = m_syTimer->timeSinceStartMsec();

    return true;
}

void SecondaryClockSynchronizer::stop()
{
    m_tswriter->close();
}

void SecondaryClockSynchronizer::processTimestamp(microseconds_t &masterTimestamp, const microseconds_t &secondaryAcqTimestamp)
{
    const long long curOffsetUsec = (secondaryAcqTimestamp - masterTimestamp).count();

    // calculate offsets without the new datapoint included
    const auto avgOffsetUsec = m_clockOffsetsUsec.mean();
    const auto avgOffsetDeviationUsec = avgOffsetUsec - m_expectedOffset.count();
    const auto offsetsSD = sqrt(vectorVariance(m_clockOffsetsUsec, avgOffsetUsec));

    // add new datapoint to our "memory" vector
    m_clockOffsetsUsec[m_calibrationIdx++] = curOffsetUsec;
    if (m_calibrationIdx >= m_calibrationMaxN)
        m_calibrationIdx = 0;

    // we do nothing more until we have enought measurements to estimate the "natural" timer offset
    // of the secondary clock and master clock
    if (!m_haveExpectedOffset) {
        m_expectedOffsetCalCount++;

        // we want a bit more values than needed for perpetual calibration, because the first
        // few values in the vector stem from the initialization phase of Syntalos and may have
        // a higher variance than actually expected during normal operation (as in the startup
        // phase, the system load is high and lots of external devices are starting up)
        if (m_expectedOffsetCalCount < (m_calibrationMaxN + (m_calibrationMaxN / 2)))
            return;

        m_expectedSD = sqrt(vectorVariance(m_clockOffsetsUsec));
        m_expectedOffset = microseconds_t(std::lround(vectorMedianInplace(m_clockOffsetsUsec)));

        qCDebug(logTimeSync).noquote().nospace() << QTime::currentTime().toString() << "[" << m_id << "] "
                << "Determined expected time offset: " << m_expectedOffset.count() << "µs "
                << "SD: " << m_expectedSD;
        m_haveExpectedOffset = true;

        // if we are writing a timesync-file, write the initial two timestamps when we
        // calibrated the system to the file (as additional verification point)
        if (m_strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE))
            m_tswriter->writeTimes(secondaryAcqTimestamp, masterTimestamp);

        m_lastMasterTS = masterTimestamp;
        return;
    }

    if (abs(avgOffsetUsec - curOffsetUsec) > offsetsSD) {
        // the current offset diff to the moving average offset is not within standard deviation range.
        // This means the data point we just added is likely a fluke, potentially due to a context switch
        // or system load spike. We correct those unconditionally.
        masterTimestamp = microseconds_t(std::lround(((secondaryAcqTimestamp.count() - m_expectedOffset.count())
                                                 + (secondaryAcqTimestamp.count() - avgOffsetUsec)) / 2.0));

        /*
        qCDebug(logTimeSync).noquote().nospace() << QTime::currentTime().toString() << "[" << m_id << "] "
                << "Offset deviation diff not within SD. Adjusted for fluke offset by adding " << avgOffsetUsec*-1 << "µs "
                << "to secondary clock time " << secondaryAcqTimestamp.count() << "µs "
                << " SD: " << offsetsSD;
        */
    } else {
        // everything is normal, and we assume here that all values are within tolerance. Recalculate
        // the master timestamp based on that assumption as average between expected master timestamp
        // based on expected offset and the actual, measured master timestamp.
        masterTimestamp = microseconds_t(std::lround(((secondaryAcqTimestamp.count() - m_expectedOffset.count()) + masterTimestamp.count()) / 2.0));
    }

    // ensure time doesn't run backwards - at this point, this event may
    // only happen if the secondary clock  gives us the exact same
    // timestamp twice in a row.
    if (masterTimestamp < m_lastMasterTS)
        masterTimestamp = m_lastMasterTS;

    // do nothing if we have not enough average deviation from the norm
    if (abs(avgOffsetDeviationUsec) < m_toleranceUsec) {
        // we are within tolerance range!
        // share the good news with the controller! (immediately on change, or every 30sec otherwise)
        if ((!m_lastOffsetWithinTolerance) || (masterTimestamp.count() > (m_lastOffsetEmission.count() + (30 * 1000 * 1000)))) {
            if (m_mod != nullptr)
                emit m_mod->synchronizerOffsetChanged(m_id, microseconds_t(avgOffsetDeviationUsec));
            m_lastOffsetEmission = masterTimestamp;
        }
        m_lastOffsetWithinTolerance = true;
        m_clockCorrectionOffset = microseconds_t(0);
        m_lastMasterTS = masterTimestamp;
        return;
    }
    m_lastOffsetWithinTolerance = false;

    // Emit offset information to the main controller about every 10sec or slower
    // in case we run at slower speeds
    if (masterTimestamp.count() > (m_lastOffsetEmission.count() + (10 * 1000 * 1000))) {
        if (m_mod != nullptr)
            emit m_mod->synchronizerOffsetChanged(m_id, microseconds_t(avgOffsetDeviationUsec));
        m_lastOffsetEmission = masterTimestamp;
    }

    // try to adjust a potential external clock slowly (and also adjust our timestamps slowly)
    const auto newClockCorrectionOffset = microseconds_t(std::lround(((m_clockCorrectionOffset.count() * 15) + avgOffsetDeviationUsec) / (15 + 1.0)));

    // write offset info to tsync file before we make any adjustments to the master timestamp
    if (m_strategies.testFlag(TimeSyncStrategy::WRITE_TSYNCFILE)) {
        if (newClockCorrectionOffset != m_clockCorrectionOffset)
            m_tswriter->writeTimes(secondaryAcqTimestamp, masterTimestamp);
    }

    m_clockCorrectionOffset = newClockCorrectionOffset;

    // the clock is out of sync, let's make adjustments!

    if (m_strategies.testFlag(TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD)) {
        if (m_clockCorrectionOffset.count() > 0)
            masterTimestamp = secondaryAcqTimestamp - microseconds_t(avgOffsetUsec) - microseconds_t(m_clockCorrectionOffset);
    }
    if (m_strategies.testFlag(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD)) {
        if (m_clockCorrectionOffset.count() < 0)
            masterTimestamp = secondaryAcqTimestamp - microseconds_t(avgOffsetUsec) - microseconds_t(m_clockCorrectionOffset);
    }

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
        qCWarning(logTimeSync).noquote().nospace() << "[" << m_id << "] "
                << "Timestamp moved backwards when calculating adjusted new time: "
                << masterTimestamp.count() << " !< " << m_lastMasterTS.count() << " (mitigated by reusing previous time)";
        masterTimestamp = m_lastMasterTS;
    }
    m_lastMasterTS = masterTimestamp;
}

void SecondaryClockSynchronizer::emitSyncDetailsChanged()
{
    if (m_mod != nullptr)
        emit m_mod->synchronizerDetailsChanged(m_id, m_strategies, microseconds_t(m_toleranceUsec));
}

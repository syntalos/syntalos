/*
 * Copyright (C) 2019-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include "tsyncfile.h"

#include <QDebug>
#include <QDateTime>
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>

#include "utils/misc.h"

namespace Syntalos {
Q_LOGGING_CATEGORY(logTSyncFile, "tsyncfile")
}

using namespace Syntalos;

// TSYNC file magic number (saved as LE): 8A T S Y N C ⏲
#define TSYNC_FILE_MAGIC 0xF223434E5953548A

#define TSYNC_FILE_VERSION_MAJOR  1
#define TSYNC_FILE_VERSION_MINOR  2

#define TSYNC_FILE_BLOCK_TERM 0x1126000000000000

QString Syntalos::tsyncFileTimeUnitToString(const TSyncFileTimeUnit &tsftunit)
{
    switch (tsftunit) {
    case TSyncFileTimeUnit::INDEX:
        return QStringLiteral("index");
    case TSyncFileTimeUnit::MICROSECONDS:
        return QStringLiteral("µs");
    case TSyncFileTimeUnit::MILLISECONDS:
        return QStringLiteral("ms");
    case TSyncFileTimeUnit::SECONDS:
        return QStringLiteral("sec");
    default:
        return QStringLiteral("?");
    }
}

QString Syntalos::tsyncFileDataTypeToString(const TSyncFileDataType &dtype)
{
    switch (dtype) {
    case TSyncFileDataType::INT16:
        return QStringLiteral("int16");
    case TSyncFileDataType::INT32:
        return QStringLiteral("int32");
    case TSyncFileDataType::INT64:
        return QStringLiteral("int64");
    case TSyncFileDataType::UINT16:
        return QStringLiteral("uint16");
    case TSyncFileDataType::UINT32:
        return QStringLiteral("uint32");
    case TSyncFileDataType::UINT64:
        return QStringLiteral("uint64");
    default:
        return QStringLiteral("INVALID");
    }
}

QString Syntalos::tsyncFileModeToString(const TSyncFileMode &mode)
{
    switch (mode) {
    case TSyncFileMode::CONTINUOUS:
        return QStringLiteral("continuous");
    case TSyncFileMode::SYNCPOINTS:
        return QStringLiteral("syncpoints");
    default:
        return QStringLiteral("INVALID");
    }
}

// ------------------
// TimeSyncFileWriter
// ------------------

TimeSyncFileWriter::TimeSyncFileWriter()
    : m_file(new QFile()),
      m_bIndex(0)
{
    m_xxh3State = XXH3_createState();

    m_timeNames = qMakePair(QStringLiteral("device-time"),
                            QStringLiteral("master-time"));
    m_timeUnits = qMakePair(TSyncFileTimeUnit::MICROSECONDS,
                            TSyncFileTimeUnit::MICROSECONDS);
    m_time1DType = TSyncFileDataType::UINT32;
    m_time2DType = TSyncFileDataType::UINT32;
    m_tsMode = TSyncFileMode::CONTINUOUS;
    m_blockSize = 2800;

    m_stream.setVersion(QDataStream::Qt_5_12);
    m_stream.setByteOrder(QDataStream::LittleEndian);
}

TimeSyncFileWriter::~TimeSyncFileWriter()
{
    this->close();
    delete m_file;
    XXH3_freeState(m_xxh3State);
}

QString TimeSyncFileWriter::lastError() const
{
    return m_lastError;
}

void TimeSyncFileWriter::setTimeNames(const QString &time1Name, const QString &time2Name)
{
    m_timeNames = qMakePair(time1Name, time2Name);
}

void TimeSyncFileWriter::setTimeUnits(TSyncFileTimeUnit time1Unit, TSyncFileTimeUnit time2Unit)
{
    m_timeUnits = qMakePair(time1Unit, time2Unit);
}

void TimeSyncFileWriter::setTimeDataTypes(TSyncFileDataType time1DType, TSyncFileDataType time2DType)
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

QString TimeSyncFileWriter::fileName() const
{
    if (!m_file->isOpen())
        return QString();
    return m_file->fileName();
}

void TimeSyncFileWriter::setSyncMode(TSyncFileMode mode)
{
    m_tsMode = mode;
}

void TimeSyncFileWriter::setChunkSize(int size)
{
    m_blockSize = size;
}

void TimeSyncFileWriter::setCreationTimeOverride(const QDateTime &dt)
{
    m_creationTimeOverride = dt;
}

template<class T>
void TimeSyncFileWriter::csWriteValue(const T &data)
{
    static_assert(std::is_arithmetic<T>::value, "T must be an arithmetic type.");

    auto d = static_cast<T>(data);
    m_stream << d;
    XXH3_64bits_update(m_xxh3State, (const uint8_t*) &d, sizeof(d));
}

template <>
void TimeSyncFileWriter::csWriteValue<QByteArray>(const QByteArray &data)
{
    m_stream << data;
    XXH3_64bits_update(m_xxh3State, (const uint8_t*) data.constData(), sizeof(char) * data.size());
}

bool TimeSyncFileWriter::open(const QString &modName, const QUuid &collectionId, const QVariantHash &userData)
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
    XXH3_64bits_reset(m_xxh3State);
    m_stream.setDevice(m_file);

    // user-defined metadata
    QJsonDocument jdoc(QJsonObject::fromVariantHash(userData));
    const QString userDataJson(jdoc.toJson(QJsonDocument::Compact));

    // write file header
    QDateTime currentTime = m_creationTimeOverride;
    if (!currentTime.isValid())
        currentTime = QDateTime::currentDateTime();
    m_creationTimeOverride = QDateTime();

    m_stream << (quint64) TSYNC_FILE_MAGIC;

    csWriteValue<quint16>(TSYNC_FILE_VERSION_MAJOR);
    csWriteValue<quint16>(TSYNC_FILE_VERSION_MINOR);

    csWriteValue<qint64>(currentTime.toTime_t());

    csWriteValue(modName.toUtf8());
    csWriteValue(collectionId.toString(QUuid::WithoutBraces).toUtf8());
    csWriteValue(userDataJson.toUtf8()); // custom JSON values

    csWriteValue<quint16>((quint16) m_tsMode);
    csWriteValue<qint32>(m_blockSize);

    csWriteValue(m_timeNames.first.toUtf8());
    csWriteValue<quint16>((quint16) m_timeUnits.first);
    csWriteValue<quint16>((quint16) m_time1DType);

    csWriteValue(m_timeNames.second.toUtf8());
    csWriteValue<quint16>((quint16) m_timeUnits.second);
    csWriteValue<quint16>((quint16) m_time2DType);

    m_file->flush();
    const auto headerBytes = m_file->size();
    if (headerBytes <= 0)
        qFatal("Could not determine amount of bytes written for tsync file header.");
    const int padding = (headerBytes * -1) & (8 - 1); // 8-byte align header
    for (int i = 0; i < padding; i++)
        csWriteValue<quint8>(0);

    // write end of header and header CRC-32
    writeBlockTerminator(false);

    m_file->flush();
    return true;
}

bool TimeSyncFileWriter::open(const QString &modName, const QUuid &collectionId, const microseconds_t &tolerance, const QVariantHash &userData)
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
        // terminate the last open block, if we have one
        writeBlockTerminator();

        // finish writing file to disk
        m_file->flush();
        m_file->close();
    }
}

void TimeSyncFileWriter::writeTimes(const microseconds_t &deviceTime, const microseconds_t &masterTime)
{
    writeTimeEntry(deviceTime.count(), masterTime.count());
}

void TimeSyncFileWriter::writeTimes(const long long &timeIndex, const microseconds_t &masterTime)
{
    writeTimeEntry(timeIndex, masterTime.count());
}

void TimeSyncFileWriter::writeTimes(const long &time1, const long &time2)
{
    writeTimeEntry(time1, time2);
}

void TimeSyncFileWriter::writeTimes(const uint64_t &time1, const uint64_t &time2)
{
    writeTimeEntry(time1, time2);
}

void TimeSyncFileWriter::writeBlockTerminator(bool check)
{
    if (check && (m_bIndex == 0))
        return;
    m_stream << (quint64) TSYNC_FILE_BLOCK_TERM;
    m_stream << (quint64) XXH3_64bits_digest(m_xxh3State);
    XXH3_64bits_reset(m_xxh3State);
    m_bIndex = 0;
}

template<class T1, class T2>
void TimeSyncFileWriter::writeTimeEntry(const T1 &time1, const T2 &time2)
{
    static_assert(std::is_arithmetic<T1>::value, "T1 must be an arithmetic type.");
    static_assert(std::is_arithmetic<T2>::value, "T2 must be an arithmetic type.");

    switch (m_time1DType) {
        case TSyncFileDataType::INT16:
            csWriteValue<qint16>(time1); break;
        case TSyncFileDataType::INT32:
            csWriteValue<qint32>(time1); break;
        case TSyncFileDataType::INT64:
            csWriteValue<qint64>(time1); break;
        case TSyncFileDataType::UINT16:
            csWriteValue<quint16>(time1); break;
        case TSyncFileDataType::UINT32:
            csWriteValue<quint32>(time1); break;
        case TSyncFileDataType::UINT64:
            csWriteValue<quint64>(time1); break;
        default:
            qFatal("Tried to write unknown datatype to timesync file for time1: %i", (int) m_time1DType);
            break;
    }

    switch (m_time2DType) {
        case TSyncFileDataType::INT16:
            csWriteValue<qint16>(time2); break;
        case TSyncFileDataType::INT32:
            csWriteValue<qint32>(time2); break;
        case TSyncFileDataType::INT64:
            csWriteValue<qint64>(time2); break;
        case TSyncFileDataType::UINT16:
            csWriteValue<quint16>(time2); break;
        case TSyncFileDataType::UINT32:
            csWriteValue<quint32>(time2); break;
        case TSyncFileDataType::UINT64:
            csWriteValue<quint64>(time2); break;
        default:
            qFatal("Tried to write unknown datatype to timesync file for time2: %i", (int) m_time1DType);
            break;
    }

    m_bIndex++;
    if (m_bIndex >= m_blockSize)
        writeBlockTerminator();
}

TimeSyncFileReader::TimeSyncFileReader()
    : m_lastError(QString())
{
}

template<class T>
inline T csReadValue(QDataStream &in, XXH3_state_t *state)
{
    static_assert(std::is_arithmetic<T>::value, "T must be an arithmetic type.");

    T value;
    in >> value;
    XXH3_64bits_update(state, (const uint8_t*) &value, sizeof(value));
    return value;
}

template<>
inline QByteArray csReadValue(QDataStream &in, XXH3_state_t *state)
{
    QByteArray value;
    in >> value;
    XXH3_64bits_update(state, (const uint8_t*) value.constData(), sizeof(char) * value.size());
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
    in >> magic;
    if (magic != TSYNC_FILE_MAGIC) {
        m_lastError = QStringLiteral("Unable to read data: This file is not a valid timesync metadata file.");
        return false;
    }

    XXH3_state_t *csState = XXH3_createState();
    XXH3_64bits_reset(csState);

    const auto formatVMajor = csReadValue<quint16>(in, csState);
    const auto formatVMinor = csReadValue<quint16>(in, csState);
    if ((formatVMajor != TSYNC_FILE_VERSION_MAJOR) || (formatVMinor < TSYNC_FILE_VERSION_MINOR)) {
        m_lastError = QStringLiteral("Unable to read data: This file is using an incompatible (probably newer) version of the format which we can not read (%1.%2 vs %3.%4).")
                .arg(formatVMajor).arg(formatVMinor).arg(TSYNC_FILE_VERSION_MAJOR).arg(TSYNC_FILE_VERSION_MINOR);
        XXH3_freeState(csState);
        return false;
    }

    m_creationTime = csReadValue<qint64>(in, csState);

    const auto modNameUtf8 = csReadValue<QByteArray>(in, csState);
    const auto collectionIdUtf8 = csReadValue<QByteArray>(in, csState);
    const auto userJsonUtf8 = csReadValue<QByteArray>(in, csState);

    m_moduleName = QString::fromUtf8(modNameUtf8);
    m_collectionId = QUuid(QString::fromUtf8(collectionIdUtf8));

    QJsonDocument jdoc = QJsonDocument::fromJson(userJsonUtf8);
    m_userData = QVariantHash();
    if (jdoc.isObject())
        m_userData = jdoc.object().toVariantHash();

    // file storage mode
    m_tsMode = static_cast<TSyncFileMode>(csReadValue<quint16>(in, csState));

    // block size
    m_blockSize = csReadValue<qint32>(in, csState);

    // time info
    const auto timeName1Utf8 = csReadValue<QByteArray>(in, csState);
    const auto timeUnit1_i = csReadValue<quint16>(in, csState);
    const auto timeDType1_i = csReadValue<quint16>(in, csState);

    const auto timeName2Utf8 = csReadValue<QByteArray>(in, csState);
    const auto timeUnit2_i = csReadValue<quint16>(in, csState);
    const auto timeDType2_i = csReadValue<quint16>(in, csState);

    m_timeNames.first = QString::fromUtf8(timeName1Utf8);
    m_timeNames.second = QString::fromUtf8(timeName2Utf8);
    m_tolerance = microseconds_t(m_userData.value("tolerance_us").toLongLong());
    m_timeUnits.first = static_cast<TSyncFileTimeUnit>(timeUnit1_i);
    m_timeUnits.second = static_cast<TSyncFileTimeUnit>(timeUnit2_i);

    const auto timeDType1 = static_cast<TSyncFileDataType>(timeDType1_i);
    const auto timeDType2 = static_cast<TSyncFileDataType>(timeDType2_i);
    m_timeDTypes = qMakePair(timeDType1, timeDType2);

    // skip potential alignment bytes
    const int padding = (file.pos() * -1) & (8 - 1); // files use 8-byte alignment
    for (int i = 0; i < padding; i++)
        csReadValue<quint8>(in, csState);

    // check header CRC
    quint64 expectedHeaderCRC;
    quint64 blockTerm;
    in >> blockTerm >> expectedHeaderCRC;
    if (blockTerm != TSYNC_FILE_BLOCK_TERM) {
        m_lastError = QStringLiteral("Header block terminator not found: The file is either invalid or its header block was damaged.");
        XXH3_freeState(csState);
        return false;
    }
    if (expectedHeaderCRC != XXH3_64bits_digest(csState)) {
        m_lastError = QStringLiteral("Header checksum mismatch: The file is either invalid or its header block was damaged.");
        XXH3_freeState(csState);
        return false;
    }

    // read the time data
    m_times.clear();
    int bIndex = 0;
    XXH3_64bits_reset(csState);
    const auto data_sec_end = file.size() - 16;
    while (!in.atEnd()) {
        if (file.pos() == data_sec_end) {
            // read last 16 bytes, which *must* be the block terminator of the final block, otherwise
            // our file was truncated or corrupted.
            quint64 expectedCRC;
            in >> blockTerm >> expectedCRC;

            if (blockTerm != TSYNC_FILE_BLOCK_TERM) {
                m_lastError = QStringLiteral("Unable to read all tsync data: File was likely truncated (its last block is not complete).");
                XXH3_freeState(csState);
                return false;
            }
            if (expectedCRC != XXH3_64bits_digest(csState))
                qCWarning(logTSyncFile).noquote() << "CRC check failed for last tsync data block: Data is likely corrupted.";
            break;
        }

        long long timeVal1;
        long long timeVal2;

        switch (timeDType1) {
            case TSyncFileDataType::INT16:
                timeVal1 = csReadValue<qint16>(in, csState); break;
            case TSyncFileDataType::INT32:
                timeVal1 = csReadValue<qint32>(in, csState); break;
            case TSyncFileDataType::INT64:
                timeVal1 = csReadValue<qint64>(in, csState); break;
            case TSyncFileDataType::UINT16:
                timeVal1 = csReadValue<quint16>(in, csState); break;
            case TSyncFileDataType::UINT32:
                timeVal1 = csReadValue<quint32>(in, csState); break;
            case TSyncFileDataType::UINT64:
                timeVal1 = csReadValue<quint64>(in, csState); break;
            default:
                qFatal("Tried to read unknown datatype from timesync file for time1: %i", (int) timeDType1);
                break;
        }

        switch (timeDType2) {
            case TSyncFileDataType::INT16:
                timeVal2 = csReadValue<qint16>(in, csState); break;
            case TSyncFileDataType::INT32:
                timeVal2 = csReadValue<qint32>(in, csState); break;
            case TSyncFileDataType::INT64:
                timeVal2 = csReadValue<qint64>(in, csState); break;
            case TSyncFileDataType::UINT16:
                timeVal2 = csReadValue<quint16>(in, csState); break;
            case TSyncFileDataType::UINT32:
                timeVal2 = csReadValue<quint32>(in, csState); break;
            case TSyncFileDataType::UINT64:
                timeVal2 = csReadValue<quint64>(in, csState); break;
            default:
                qFatal("Tried to read unknown datatype from timesync file for time2: %i", (int) timeDType2);
                break;
        }

        m_times.push_back(std::make_pair(timeVal1, timeVal2));

        bIndex++;
        if (bIndex == m_blockSize) {
            quint64 expectedCRC;
            in >> blockTerm >> expectedCRC;

            if (blockTerm != TSYNC_FILE_BLOCK_TERM) {
                m_lastError = QStringLiteral("Unable to read all tsync data: Block separator was invalid.");
                XXH3_freeState(csState);
                return false;
            }
            if (expectedCRC != XXH3_64bits_digest(csState))
                qCWarning(logTSyncFile).noquote() << "CRC check failed for tsync data block: Data is likely corrupted.";

            XXH3_64bits_reset(csState);
            bIndex = 0;
        }
    }

    XXH3_freeState(csState);
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

QUuid TimeSyncFileReader::collectionId() const
{
    return m_collectionId;
}

time_t TimeSyncFileReader::creationTime() const
{
    return m_creationTime;
}

TSyncFileMode TimeSyncFileReader::syncMode() const
{
    return m_tsMode;
}

QVariantHash TimeSyncFileReader::userData() const
{
    return m_userData;
}

microseconds_t TimeSyncFileReader::tolerance() const
{
    return m_tolerance;
}

QPair<QString, QString> TimeSyncFileReader::timeNames() const
{
    return m_timeNames;
}

QPair<TSyncFileTimeUnit, TSyncFileTimeUnit> TimeSyncFileReader::timeUnits() const
{
    return m_timeUnits;
}

QPair<TSyncFileDataType, TSyncFileDataType> TimeSyncFileReader::timeDTypes() const
{
    return m_timeDTypes;
}

std::vector<std::pair<long long, long long> > TimeSyncFileReader::times() const
{
    return m_times;
}

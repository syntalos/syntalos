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

#include "tsyncfile.h"

#include <QDebug>
#include <QDateTime>
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>

#include "utils/misc.h"
#include "utils/crc32c.h"

namespace Syntalos {
Q_LOGGING_CATEGORY(logTSyncFile, "tsyncfile")
}

using namespace Syntalos;

// TSYNC file magic number (saved as LE): 8A T S Y N C ⏲
#define TSYNC_FILE_MAGIC 0xF223434E5953548A

#define TSYNC_FILE_VERSION_MAJOR  1
#define TSYNC_FILE_VERSION_MINOR  0

#define TSYNC_FILE_BLOCK_TERM 0x11260000

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
    crc32c_init();

    m_timeNames = qMakePair(QStringLiteral("device-time"),
                            QStringLiteral("master-time"));
    m_timeUnits = qMakePair(TSyncFileTimeUnit::MICROSECONDS,
                            TSyncFileTimeUnit::MICROSECONDS);
    m_time1DType = TSyncFileDataType::UINT32;
    m_time2DType = TSyncFileDataType::UINT32;
    m_tsMode = TSyncFileMode::CONTINUOUS;
    m_blockSize = 3600;

    m_stream.setVersion(QDataStream::Qt_5_12);
    m_stream.setByteOrder(QDataStream::LittleEndian);
}

TimeSyncFileWriter::~TimeSyncFileWriter()
{
    this->close();
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

template<class T>
void TimeSyncFileWriter::crcWriteValue(const T &data)
{
    static_assert(std::is_arithmetic<T>::value, "T must be an arithmetic type.");

    auto d = static_cast<T>(data);
    m_stream << d;
    m_blockCRC = crc32c(m_blockCRC, (const uint8_t*) &d, sizeof(d));
}

template <>
void TimeSyncFileWriter::crcWriteValue<QByteArray>(const QByteArray &data)
{
    m_stream << data;
    m_blockCRC = crc32c(m_blockCRC, (const uint8_t*) data.constData(), sizeof(char) * data.size());
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
    m_blockCRC = 0;
    m_stream.setDevice(m_file);

    // user-defined metadata
    QJsonDocument jdoc(QJsonObject::fromVariantHash(userData));
    const QString userDataJson(jdoc.toJson(QJsonDocument::Compact));

    // write file header
    QDateTime currentTime(QDateTime::currentDateTime());

    m_stream << (quint64) TSYNC_FILE_MAGIC;

    crcWriteValue<quint16>(TSYNC_FILE_VERSION_MAJOR);
    crcWriteValue<quint16>(TSYNC_FILE_VERSION_MINOR);

    crcWriteValue<qint64>(currentTime.toTime_t());

    crcWriteValue(modName.toUtf8());
    crcWriteValue(collectionId.toString(QUuid::WithoutBraces).toUtf8());
    crcWriteValue(userDataJson.toUtf8()); // custom JSON values

    crcWriteValue<quint16>((quint16) m_tsMode);
    crcWriteValue<qint32>(m_blockSize);

    crcWriteValue(m_timeNames.first.toUtf8());
    crcWriteValue<quint16>((quint16) m_timeUnits.first);
    crcWriteValue<quint16>((quint16) m_time1DType);

    crcWriteValue(m_timeNames.second.toUtf8());
    crcWriteValue<quint16>((quint16) m_timeUnits.second);
    crcWriteValue<quint16>((quint16) m_time2DType);

    m_file->flush();
    const auto headerBytes = m_file->size();
    if (headerBytes <= 0)
        qFatal("Could not determine amount of bytes written for tsync file header.");
    const int padding = (headerBytes * -1) & (8 - 1); // 8-byte align header
    for (int i = 0; i < padding; i++)
        crcWriteValue<quint8>(0);

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

void TimeSyncFileWriter::writeBlockTerminator(bool check)
{
    if (check && (m_bIndex == 0))
        return;
    m_stream << (quint32) TSYNC_FILE_BLOCK_TERM;
    m_stream << (quint32) m_blockCRC;
    m_blockCRC = 0;
    m_bIndex = 0;
}

template<class T1, class T2>
void TimeSyncFileWriter::writeTimeEntry(const T1 &time1, const T2 &time2)
{
    static_assert(std::is_arithmetic<T1>::value, "T1 must be an arithmetic type.");
    static_assert(std::is_arithmetic<T2>::value, "T2 must be an arithmetic type.");

    switch (m_time1DType) {
        case TSyncFileDataType::INT16:
            crcWriteValue<qint16>(time1); break;
        case TSyncFileDataType::INT32:
            crcWriteValue<qint32>(time1); break;
        case TSyncFileDataType::INT64:
            crcWriteValue<qint64>(time1); break;
        case TSyncFileDataType::UINT16:
            crcWriteValue<quint16>(time1); break;
        case TSyncFileDataType::UINT32:
            crcWriteValue<quint32>(time1); break;
        case TSyncFileDataType::UINT64:
            crcWriteValue<quint64>(time1); break;
        default:
            qFatal("Tried to write unknown datatype to timesync file for time1: %i", (int) m_time1DType);
            break;
    }

    switch (m_time2DType) {
        case TSyncFileDataType::INT16:
            crcWriteValue<qint16>(time2); break;
        case TSyncFileDataType::INT32:
            crcWriteValue<qint32>(time2); break;
        case TSyncFileDataType::INT64:
            crcWriteValue<qint64>(time2); break;
        case TSyncFileDataType::UINT16:
            crcWriteValue<quint16>(time2); break;
        case TSyncFileDataType::UINT32:
            crcWriteValue<quint32>(time2); break;
        case TSyncFileDataType::UINT64:
            crcWriteValue<quint64>(time2); break;
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
    crc32c_init();
}

template<class T>
inline T crcReadValue(QDataStream &in, quint32 *crc)
{
    static_assert(std::is_arithmetic<T>::value, "T must be an arithmetic type.");

    T value;
    in >> value;
    *crc = crc32c(*crc, (const uint8_t*) &value, sizeof(value));
    return value;
}

template<>
inline QByteArray crcReadValue(QDataStream &in, quint32 *crc)
{
    QByteArray value;
    in >> value;
    *crc = crc32c(*crc, (const uint8_t*) value.constData(), sizeof(char) * value.size());
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

    quint32 crc = 0;
    const auto formatVMajor = crcReadValue<quint16>(in, &crc);
    const auto formatVMinor = crcReadValue<quint16>(in, &crc);
    if ((formatVMajor != TSYNC_FILE_VERSION_MAJOR)) {
        m_lastError = QStringLiteral("Unable to read data: This file is using an incompatible (probably newer) version of the format which we can not read (%1.%2 vs %3.%4).")
                .arg(formatVMajor).arg(formatVMinor).arg(TSYNC_FILE_VERSION_MAJOR).arg(TSYNC_FILE_VERSION_MINOR);
        return false;
    }

    m_creationTime = crcReadValue<qint64>(in, &crc);

    const auto modNameUtf8 = crcReadValue<QByteArray>(in, &crc);
    const auto collectionIdUtf8 = crcReadValue<QByteArray>(in, &crc);
    const auto userJsonUtf8 = crcReadValue<QByteArray>(in, &crc);

    m_moduleName = QString::fromUtf8(modNameUtf8);
    m_collectionId = QUuid(QString::fromUtf8(collectionIdUtf8));

    QJsonDocument jdoc = QJsonDocument::fromJson(userJsonUtf8);
    m_userData = QVariantHash();
    if (jdoc.isObject())
        m_userData = jdoc.object().toVariantHash();

    // file storage mode
    m_tsMode = static_cast<TSyncFileMode>(crcReadValue<quint16>(in, &crc));

    // block size
    m_blockSize = crcReadValue<qint32>(in, &crc);

    // time info
    const auto timeName1Utf8 = crcReadValue<QByteArray>(in, &crc);
    const auto timeUnit1_i = crcReadValue<quint16>(in, &crc);
    const auto timeDType1_i = crcReadValue<quint16>(in, &crc);

    const auto timeName2Utf8 = crcReadValue<QByteArray>(in, &crc);
    const auto timeUnit2_i = crcReadValue<quint16>(in, &crc);
    const auto timeDType2_i = crcReadValue<quint16>(in, &crc);

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
        crcReadValue<quint8>(in, &crc);

    // check header CRC
    quint32 expectedHeaderCRC;
    quint32 blockTerm;
    in >> blockTerm >> expectedHeaderCRC;
    if (blockTerm != TSYNC_FILE_BLOCK_TERM) {
        m_lastError = QStringLiteral("Header block terminator not found: The file is either invalid or its header block was damaged.");
        return false;
    }
    if (expectedHeaderCRC != crc) {
        m_lastError = QStringLiteral("Header checksum mismatch: The file is either invalid or its header block was damaged.");
        return false;
    }

    // read the time data
    m_times.clear();
    int bIndex = 0;
    crc = 0;
    const auto data_sec_end = file.size() - 8;
    while (!in.atEnd()) {
        if (file.pos() == data_sec_end) {
            // read last 8 bytes, which *must* be the block terminator of the final block, otherwise
            // our file was truncated or corrupted.
            quint32 expectedCRC;
            in >> blockTerm >> expectedCRC;

            if (blockTerm != TSYNC_FILE_BLOCK_TERM) {
                m_lastError = QStringLiteral("Unable to read all tsync data: File was likely truncated (its last block is not complete).");
                return false;
            }
            if (expectedCRC != crc)
                qCWarning(logTSyncFile).noquote() << "CRC check failed for last tsync data block: Data is likely corrupted.";
            break;
        }

        long long timeVal1;
        long long timeVal2;

        switch (timeDType1) {
            case TSyncFileDataType::INT16:
                timeVal1 = crcReadValue<qint16>(in, &crc); break;
            case TSyncFileDataType::INT32:
                timeVal1 = crcReadValue<qint32>(in, &crc); break;
            case TSyncFileDataType::INT64:
                timeVal1 = crcReadValue<qint64>(in, &crc); break;
            case TSyncFileDataType::UINT16:
                timeVal1 = crcReadValue<quint16>(in, &crc); break;
            case TSyncFileDataType::UINT32:
                timeVal1 = crcReadValue<quint32>(in, &crc); break;
            case TSyncFileDataType::UINT64:
                timeVal1 = crcReadValue<quint64>(in, &crc); break;
            default:
                qFatal("Tried to read unknown datatype from timesync file for time1: %i", (int) timeDType1);
                break;
        }

        switch (timeDType2) {
            case TSyncFileDataType::INT16:
                timeVal2 = crcReadValue<qint16>(in, &crc); break;
            case TSyncFileDataType::INT32:
                timeVal2 = crcReadValue<qint32>(in, &crc); break;
            case TSyncFileDataType::INT64:
                timeVal2 = crcReadValue<qint64>(in, &crc); break;
            case TSyncFileDataType::UINT16:
                timeVal2 = crcReadValue<quint16>(in, &crc); break;
            case TSyncFileDataType::UINT32:
                timeVal2 = crcReadValue<quint32>(in, &crc); break;
            case TSyncFileDataType::UINT64:
                timeVal2 = crcReadValue<quint64>(in, &crc); break;
            default:
                qFatal("Tried to read unknown datatype from timesync file for time2: %i", (int) timeDType2);
                break;
        }

        m_times.push_back(std::make_pair(timeVal1, timeVal2));

        bIndex++;
        if (bIndex == m_blockSize) {
            quint32 expectedCRC;
            in >> blockTerm >> expectedCRC;

            if (blockTerm != TSYNC_FILE_BLOCK_TERM) {
                m_lastError = QStringLiteral("Unable to read all tsync data: Block separator was invalid.");
                return false;
            }
            if (expectedCRC != crc)
                qCWarning(logTSyncFile).noquote() << "CRC check failed for tsync data block: Data is likely corrupted.";

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

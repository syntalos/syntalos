/*
 * Copyright (C) 2019-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include <bit>
#include <cstring>
#include <format>
#include <fstream>
#include <sstream>

#include "loginternal.h"

using namespace Syntalos;
SY_DEFINE_LOG_CATEGORY(logTSyncFile, "tsyncfile");

// TSYNC file magic number (saved as LE): 8A T S Y N C ⏲
#define TSYNC_FILE_MAGIC         UINT64_C(0xF223434E5953548A)
#define TSYNC_FILE_VERSION_MAJOR 1
#define TSYNC_FILE_VERSION_MINOR 2
#define TSYNC_FILE_BLOCK_TERM    UINT64_C(0x1126000000000000)

std::string Syntalos::tsyncFileTimeUnitToString(const TSyncFileTimeUnit &tsftunit)
{
    switch (tsftunit) {
    case TSyncFileTimeUnit::INDEX:
        return "index";
    case TSyncFileTimeUnit::MICROSECONDS:
        return "µs";
    case TSyncFileTimeUnit::MILLISECONDS:
        return "ms";
    case TSyncFileTimeUnit::SECONDS:
        return "sec";
    default:
        return "?";
    }
}

std::string Syntalos::tsyncFileDataTypeToString(const TSyncFileDataType &dtype)
{
    switch (dtype) {
    case TSyncFileDataType::INT16:
        return "int16";
    case TSyncFileDataType::INT32:
        return "int32";
    case TSyncFileDataType::INT64:
        return "int64";
    case TSyncFileDataType::UINT16:
        return "uint16";
    case TSyncFileDataType::UINT32:
        return "uint32";
    case TSyncFileDataType::UINT64:
        return "uint64";
    default:
        return "INVALID";
    }
}

std::string Syntalos::tsyncFileModeToString(const TSyncFileMode &mode)
{
    switch (mode) {
    case TSyncFileMode::CONTINUOUS:
        return "continuous";
    case TSyncFileMode::SYNCPOINTS:
        return "syncpoints";
    default:
        return "INVALID";
    }
}

// ============================================================
// TimeSyncFileWriter
// ============================================================

TimeSyncFileWriter::TimeSyncFileWriter()
    : m_bIndex(0)
{
    m_xxh3State = XXH3_createState();

    m_timeNames = {"device-time", "master-time"};
    m_timeUnits = {TSyncFileTimeUnit::MICROSECONDS, TSyncFileTimeUnit::MICROSECONDS};
    m_time1DType = TSyncFileDataType::UINT32;
    m_time2DType = TSyncFileDataType::UINT32;
    m_tsMode = TSyncFileMode::CONTINUOUS;
    m_blockSize = 2800;
}

TimeSyncFileWriter::~TimeSyncFileWriter()
{
    close();
    XXH3_freeState(m_xxh3State);
}

std::string TimeSyncFileWriter::lastError() const
{
    return m_lastError;
}

void TimeSyncFileWriter::setTimeNames(const std::string &time1Name, const std::string &time2Name)
{
    m_timeNames = {time1Name, time2Name};
}

void TimeSyncFileWriter::setTimeUnits(TSyncFileTimeUnit time1Unit, TSyncFileTimeUnit time2Unit)
{
    m_timeUnits = {time1Unit, time2Unit};
}

void TimeSyncFileWriter::setTimeDataTypes(TSyncFileDataType time1DType, TSyncFileDataType time2DType)
{
    m_time1DType = time1DType;
    m_time2DType = time2DType;
}

void TimeSyncFileWriter::setFileName(const std::string &fname)
{
    if (m_file.is_open())
        m_file.close();

    m_fname = fname;
    if (!m_fname.ends_with(".tsync"))
        m_fname.append(".tsync");
}

std::string TimeSyncFileWriter::fileName() const
{
    return m_file.is_open() ? m_fname : std::string{};
}

void TimeSyncFileWriter::setSyncMode(TSyncFileMode mode)
{
    m_tsMode = mode;
}

void TimeSyncFileWriter::setChunkSize(int size)
{
    m_blockSize = size;
}

void TimeSyncFileWriter::setCreationTimeOverride(const std::chrono::system_clock::time_point &tp)
{
    m_creationTimeOverride = tp;
}

template<class T>
void TimeSyncFileWriter::csWriteValue(const T &data)
{
    static_assert(std::is_arithmetic_v<T>);

    T le = data;
    if constexpr (std::endian::native == std::endian::big)
        le = std::byteswap(data);
    m_file.write(reinterpret_cast<const char *>(&le), sizeof(le));
    XXH3_64bits_update(m_xxh3State, &le, sizeof(le));
}

template<>
void TimeSyncFileWriter::csWriteValue<std::string>(const std::string &str)
{
    const auto len = static_cast<uint32_t>(str.size());
    const auto data = str.data();

    uint32_t lenLE = len;
    if constexpr (std::endian::native == std::endian::big)
        lenLE = std::byteswap(len);
    m_file.write(reinterpret_cast<const char *>(&lenLE), 4);
    XXH3_64bits_update(m_xxh3State, &lenLE, 4);
    if (len > 0) {
        m_file.write(data, static_cast<std::streamsize>(len));
        XXH3_64bits_update(m_xxh3State, data, len);
    }
}

/**
 * Write without checksumming (for magic number etc.)
 */
template<class T>
void TimeSyncFileWriter::writeRawLE(T val)
{
    T le = val;
    if constexpr (std::endian::native == std::endian::big)
        le = std::byteswap(val);
    m_file.write(reinterpret_cast<const char *>(&le), sizeof(le));
}

bool TimeSyncFileWriter::open(const std::string &modName, const Uuid &collectionId, const MetaStringMap &userData)
{
    if (m_file.is_open())
        m_file.close();

    m_file.open(m_fname, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!m_file) {
        m_lastError = std::format("Unable to open file '{}' for writing.", m_fname);
        return false;
    }

    // ensure block size is not extremely small
    if (m_blockSize >= 0 && m_blockSize < 128)
        m_blockSize = 128;

    m_bIndex = 0;
    XXH3_64bits_reset(m_xxh3State);

    // serialize userData to JSON
    std::ostringstream jsonBuf;
    writeJsonObject(jsonBuf, userData);
    const auto userDataJson = jsonBuf.str();

    // compute creation time
    const auto creationTimeSecs =
        std::chrono::duration_cast<std::chrono::seconds>(
            (m_creationTimeOverride.value_or(std::chrono::system_clock::now())).time_since_epoch())
            .count();
    m_creationTimeOverride.reset();

    // write file header - magic number raw (not checksummed)
    writeRawLE<uint64_t>(TSYNC_FILE_MAGIC);

    csWriteValue<uint16_t>(TSYNC_FILE_VERSION_MAJOR);
    csWriteValue<uint16_t>(TSYNC_FILE_VERSION_MINOR);
    csWriteValue<int64_t>(static_cast<int64_t>(creationTimeSecs));

    csWriteValue(modName);
    csWriteValue(collectionId.toHex()); // UUID as hex string
    csWriteValue(userDataJson);

    csWriteValue<uint16_t>(static_cast<uint16_t>(m_tsMode));
    csWriteValue<int32_t>(m_blockSize);

    csWriteValue(m_timeNames.first);
    csWriteValue<uint16_t>(static_cast<uint16_t>(m_timeUnits.first));
    csWriteValue<uint16_t>(static_cast<uint16_t>(m_time1DType));

    csWriteValue(m_timeNames.second);
    csWriteValue<uint16_t>(static_cast<uint16_t>(m_timeUnits.second));
    csWriteValue<uint16_t>(static_cast<uint16_t>(m_time2DType));

    m_file.flush();
    const auto headerBytes = static_cast<int>(m_file.tellp());
    if (headerBytes <= 0) {
        m_lastError = "Could not determine amount of bytes written for tsync file header.";
        return false;
    }
    const int padding = (headerBytes * -1) & (8 - 1); // 8-byte align header
    for (int i = 0; i < padding; i++)
        csWriteValue<uint8_t>(0);

    // write end of header and header CRC-32
    writeBlockTerminator(false);
    m_file.flush();
    return true;
}

bool TimeSyncFileWriter::open(
    const std::string &modName,
    const Uuid &collectionId,
    const microseconds_t &tolerance,
    const MetaStringMap &userData)
{
    auto udata = userData;
    udata["tolerance_us"] = MetaValue{static_cast<int64_t>(tolerance.count())};
    return open(modName, collectionId, udata);
}

void TimeSyncFileWriter::flush()
{
    if (m_file.is_open())
        m_file.flush();
}

void TimeSyncFileWriter::close()
{
    if (m_file.is_open()) {
        // terminate the last open block, if we have one
        writeBlockTerminator();
        // finish writing file to disk
        m_file.flush();
        m_file.close();
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
    if (check && m_bIndex == 0)
        return;
    writeRawLE<uint64_t>(TSYNC_FILE_BLOCK_TERM);
    writeRawLE<uint64_t>(XXH3_64bits_digest(m_xxh3State));
    XXH3_64bits_reset(m_xxh3State);
    m_bIndex = 0;
}

template<class T1, class T2>
void TimeSyncFileWriter::writeTimeEntry(const T1 &time1, const T2 &time2)
{
    static_assert(std::is_arithmetic_v<T1>);
    static_assert(std::is_arithmetic_v<T2>);

    switch (m_time1DType) {
    case TSyncFileDataType::INT16:
        csWriteValue<int16_t>(time1);
        break;
    case TSyncFileDataType::INT32:
        csWriteValue<int32_t>(time1);
        break;
    case TSyncFileDataType::INT64:
        csWriteValue<int64_t>(time1);
        break;
    case TSyncFileDataType::UINT16:
        csWriteValue<uint16_t>(time1);
        break;
    case TSyncFileDataType::UINT32:
        csWriteValue<uint32_t>(time1);
        break;
    case TSyncFileDataType::UINT64:
        csWriteValue<uint64_t>(time1);
        break;
    default:
        SY_LOG_CRITICAL(
            logTSyncFile,
            "Tried to write unknown datatype to timesync file for time1: {}",
            static_cast<int>(m_time1DType));
        break;
    }

    switch (m_time2DType) {
    case TSyncFileDataType::INT16:
        csWriteValue<int16_t>(time2);
        break;
    case TSyncFileDataType::INT32:
        csWriteValue<int32_t>(time2);
        break;
    case TSyncFileDataType::INT64:
        csWriteValue<int64_t>(time2);
        break;
    case TSyncFileDataType::UINT16:
        csWriteValue<uint16_t>(time2);
        break;
    case TSyncFileDataType::UINT32:
        csWriteValue<uint32_t>(time2);
        break;
    case TSyncFileDataType::UINT64:
        csWriteValue<uint64_t>(time2);
        break;
    default:
        SY_LOG_CRITICAL(
            logTSyncFile,
            "Tried to write unknown datatype to timesync file for time2: {}",
            static_cast<int>(m_time2DType));
        break;
    }

    m_bIndex++;
    if (m_bIndex >= m_blockSize)
        writeBlockTerminator();
}

// ============================================================
// TimeSyncFileReader
// ============================================================

TimeSyncFileReader::TimeSyncFileReader()
    : m_lastError({}),
      m_creationTime(0),
      m_tsMode(TSyncFileMode::CONTINUOUS),
      m_blockSize(0)
{
}

/**
 * Read T as little-endian from is, feed into xxh3.
 */
template<class T>
static T csReadLE(std::istream &is, XXH3_state_t *state)
{
    static_assert(std::is_arithmetic_v<T>);
    T le{};
    is.read(reinterpret_cast<char *>(&le), sizeof(le));
    XXH3_64bits_update(state, &le, sizeof(le));
    if constexpr (std::endian::native == std::endian::big)
        le = std::byteswap(le);
    return le;
}

/**
 * Read checksummed byte array (quint32 length + raw)
 */
static std::string csReadBytes(std::istream &is, XXH3_state_t *state)
{
    uint32_t lenLE = 0;
    is.read(reinterpret_cast<char *>(&lenLE), 4);
    XXH3_64bits_update(state, &lenLE, 4);
    if constexpr (std::endian::native == std::endian::big)
        lenLE = std::byteswap(lenLE);
    if (lenLE == 0xFFFFFFFFu)
        return {}; // null QByteArray sentinel
    std::string result(lenLE, '\0');
    if (lenLE > 0) {
        is.read(result.data(), lenLE);
        XXH3_64bits_update(state, result.data(), lenLE);
    }
    return result;
}

template<class T>
static T readRawLE(std::istream &is)
{
    T le{};
    is.read(reinterpret_cast<char *>(&le), sizeof(le));
    if constexpr (std::endian::native == std::endian::big)
        le = std::byteswap(le);
    return le;
}

bool TimeSyncFileReader::open(const std::string &fname)
{
    std::ifstream file(fname, std::ios::binary);
    if (!file) {
        m_lastError = std::format("Unable to open file '{}' for reading.", fname);
        return false;
    }

    // read magic (not checksummed)
    const auto magic = readRawLE<uint64_t>(file);
    if (magic != TSYNC_FILE_MAGIC) {
        m_lastError = "Unable to read data: This file is not a valid timesync metadata file.";
        return false;
    }

    XXH3_state_t *csState = XXH3_createState();
    XXH3_64bits_reset(csState);

    const auto formatVMajor = csReadLE<uint16_t>(file, csState);
    const auto formatVMinor = csReadLE<uint16_t>(file, csState);
    if (formatVMajor != TSYNC_FILE_VERSION_MAJOR || formatVMinor < TSYNC_FILE_VERSION_MINOR) {
        m_lastError = std::format(
            "Unable to read data: This file is using an incompatible (probably newer) format version: file {}.{} vs "
            "supported {}.{}",
            formatVMajor,
            formatVMinor,
            TSYNC_FILE_VERSION_MAJOR,
            TSYNC_FILE_VERSION_MINOR);
        XXH3_freeState(csState);
        return false;
    }

    m_creationTime = static_cast<time_t>(csReadLE<int64_t>(file, csState));

    const auto modNameStr = csReadBytes(file, csState);
    const auto collIdStr = csReadBytes(file, csState);
    const auto userJsonStr = csReadBytes(file, csState);

    m_moduleName = modNameStr;
    const auto parsedUuid = Uuid::fromHex(collIdStr);
    m_collectionId = parsedUuid.value_or(Uuid());

    if (!parsedUuid.has_value())
        SY_LOG_WARNING(logTSyncFile, "Failed to parse collection UUID from timesync file: '{}'", collIdStr);

    // parse basic JSON user data
    m_userData.clear();
    const auto parsedUData = stringMapFromJson(userJsonStr);
    if (!parsedUData.has_value()) {
        SY_LOG_WARNING(
            logTSyncFile,
            "Failed to parse user metadata JSON from timesync file: {} (Data was: '{}')",
            parsedUData.error(),
            userJsonStr);
    } else {
        m_userData = std::move(parsedUData.value());
    }

    // file storage mode
    m_tsMode = static_cast<TSyncFileMode>(csReadLE<uint16_t>(file, csState));
    // block size
    m_blockSize = csReadLE<int32_t>(file, csState);

    const auto timeName1 = csReadBytes(file, csState);
    const auto timeUnit1 = csReadLE<uint16_t>(file, csState);
    const auto timeDType1_i = csReadLE<uint16_t>(file, csState);

    // time info
    const auto timeName2 = csReadBytes(file, csState);
    const auto timeUnit2 = csReadLE<uint16_t>(file, csState);
    const auto timeDType2_i = csReadLE<uint16_t>(file, csState);

    m_timeNames = {timeName1, timeName2};

    // tolerance from user data
    if (auto it = m_userData.find("tolerance_us"); it != m_userData.end()) {
        if (auto v = std::get_if<int64_t>(&static_cast<MetaValue::Base &>(it->second)))
            m_tolerance = microseconds_t(*v);
    }

    m_timeUnits = {static_cast<TSyncFileTimeUnit>(timeUnit1), static_cast<TSyncFileTimeUnit>(timeUnit2)};

    const auto timeDType1 = static_cast<TSyncFileDataType>(timeDType1_i);
    const auto timeDType2 = static_cast<TSyncFileDataType>(timeDType2_i);
    m_timeDTypes = {timeDType1, timeDType2};

    // skip alignment padding
    const auto headerPos = static_cast<int>(file.tellg());
    const int padding = (headerPos * -1) & (8 - 1);
    for (int i = 0; i < padding; i++)
        csReadLE<uint8_t>(file, csState);

    // check header CRC
    const auto blockTerm = readRawLE<uint64_t>(file);
    const auto expectedHeaderCRC = readRawLE<uint64_t>(file);
    if (blockTerm != TSYNC_FILE_BLOCK_TERM) {
        m_lastError = "Header block terminator not found: The file is either invalid or its header block was damaged.";
        XXH3_freeState(csState);
        return false;
    }
    if (expectedHeaderCRC != XXH3_64bits_digest(csState)) {
        m_lastError = "Header checksum mismatch: The file is either invalid or its header block was damaged.";
        XXH3_freeState(csState);
        return false;
    }

    // read time data
    m_times.clear();
    int bIndex = 0;
    XXH3_64bits_reset(csState);

    // determine file size
    file.seekg(0, std::ios::end);
    const auto fileSize = static_cast<int64_t>(file.tellg());
    const auto dataSectionEnd = fileSize - 16;           // last 16 bytes are the final block terminator
    file.seekg(headerPos + 16 + padding, std::ios::beg); // rewind to right after header terminator

    while (!file.eof() && file.good()) {
        const auto curPos = static_cast<int64_t>(file.tellg());
        if (curPos < 0 || curPos >= dataSectionEnd) {
            if (curPos == dataSectionEnd) {
                // read last 16 bytes, which *must* be the block terminator of the final block, otherwise
                // our file was truncated or corrupted.
                const auto finalTerm = readRawLE<uint64_t>(file);
                const auto finalCRC = readRawLE<uint64_t>(file);
                if (finalTerm != TSYNC_FILE_BLOCK_TERM) {
                    m_lastError =
                        "Unable to read all tsync data: File was likely truncated (its last block is not complete).";
                    XXH3_freeState(csState);
                    return false;
                }
                if (finalCRC != XXH3_64bits_digest(csState))
                    SY_LOG_WARNING(
                        logTSyncFile, "CRC check failed for last tsync data block: Data is likely corrupted.");
            }
            break;
        }

        long long timeVal1 = 0;
        long long timeVal2 = 0;

        switch (timeDType1) {
        case TSyncFileDataType::INT16:
            timeVal1 = csReadLE<int16_t>(file, csState);
            break;
        case TSyncFileDataType::INT32:
            timeVal1 = csReadLE<int32_t>(file, csState);
            break;
        case TSyncFileDataType::INT64:
            timeVal1 = csReadLE<int64_t>(file, csState);
            break;
        case TSyncFileDataType::UINT16:
            timeVal1 = csReadLE<uint16_t>(file, csState);
            break;
        case TSyncFileDataType::UINT32:
            timeVal1 = csReadLE<uint32_t>(file, csState);
            break;
        case TSyncFileDataType::UINT64:
            timeVal1 = static_cast<long long>(csReadLE<uint64_t>(file, csState));
            break;
        default:
            SY_LOG_CRITICAL(
                logTSyncFile,
                "Tried to read unknown datatype from timesync file for time1: {}",
                static_cast<int>(timeDType1));
            break;
        }

        switch (timeDType2) {
        case TSyncFileDataType::INT16:
            timeVal2 = csReadLE<int16_t>(file, csState);
            break;
        case TSyncFileDataType::INT32:
            timeVal2 = csReadLE<int32_t>(file, csState);
            break;
        case TSyncFileDataType::INT64:
            timeVal2 = csReadLE<int64_t>(file, csState);
            break;
        case TSyncFileDataType::UINT16:
            timeVal2 = csReadLE<uint16_t>(file, csState);
            break;
        case TSyncFileDataType::UINT32:
            timeVal2 = csReadLE<uint32_t>(file, csState);
            break;
        case TSyncFileDataType::UINT64:
            timeVal2 = static_cast<long long>(csReadLE<uint64_t>(file, csState));
            break;
        default:
            SY_LOG_CRITICAL(
                logTSyncFile,
                "Tried to read unknown datatype from timesync file for time2: {}",
                static_cast<int>(timeDType2));
            break;
        }

        m_times.emplace_back(timeVal1, timeVal2);

        bIndex++;
        if (bIndex == m_blockSize) {
            const auto term2 = readRawLE<uint64_t>(file);
            const auto crc2 = readRawLE<uint64_t>(file);
            if (term2 != TSYNC_FILE_BLOCK_TERM) {
                m_lastError = "Unable to read all tsync data: Block separator was invalid.";
                XXH3_freeState(csState);
                return false;
            }
            if (crc2 != XXH3_64bits_digest(csState))
                SY_LOG_WARNING(logTSyncFile, "CRC check failed for tsync data block: Data is likely corrupted.");
            XXH3_64bits_reset(csState);
            bIndex = 0;
        }
    }

    XXH3_freeState(csState);
    return true;
}

std::string TimeSyncFileReader::lastError() const
{
    return m_lastError;
}

std::string TimeSyncFileReader::moduleName() const
{
    return m_moduleName;
}

Uuid TimeSyncFileReader::collectionId() const
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

MetaStringMap TimeSyncFileReader::userData() const
{
    return m_userData;
}

microseconds_t TimeSyncFileReader::tolerance() const
{
    return m_tolerance;
}

std::pair<std::string, std::string> TimeSyncFileReader::timeNames() const
{
    return m_timeNames;
}

std::pair<TSyncFileTimeUnit, TSyncFileTimeUnit> TimeSyncFileReader::timeUnits() const
{
    return m_timeUnits;
}

std::pair<TSyncFileDataType, TSyncFileDataType> TimeSyncFileReader::timeDTypes() const
{
    return m_timeDTypes;
}

std::vector<std::pair<long long, long long>> TimeSyncFileReader::times() const
{
    return m_times;
}

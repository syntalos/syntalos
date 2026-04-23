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

#pragma once

#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <xxhash.h>

#include "datactl/syclock.h"
#include "datactl/streammeta.h"
#include "datactl/uuid.h"

namespace Syntalos
{

/**
 * @brief Timepoint storage of a TSync file
 */
enum class TSyncFileMode {
    CONTINUOUS = 0, /// Continuous time-point mapping with no gaps
    SYNCPOINTS = 1  /// Only synchronization points are saved
};

/**
 * @brief Unit types for time representation in a TSync file
 */
enum class TSyncFileTimeUnit {
    INDEX = 0, /// monotonically increasing counter without dimension
    NANOSECONDS = 1,
    MICROSECONDS = 2,
    MILLISECONDS = 3,
    SECONDS = 4
};

/**
 * @brief Data types use for storing time values in the data file.
 */
enum class TSyncFileDataType {
    INVALID = 0,
    INT16 = 2,
    INT32 = 3,
    INT64 = 4,

    UINT16 = 6,
    UINT32 = 7,
    UINT64 = 8
};

std::string tsyncFileTimeUnitToString(const TSyncFileTimeUnit &tsftunit);
std::string tsyncFileDataTypeToString(const TSyncFileDataType &dtype);
std::string tsyncFileModeToString(const TSyncFileMode &mode);

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

    TimeSyncFileWriter(const TimeSyncFileWriter &) = delete;
    TimeSyncFileWriter &operator=(const TimeSyncFileWriter &) = delete;

    [[nodiscard]] std::string lastError() const;

    void setTimeNames(const std::string &time1Name, const std::string &time2Name);
    void setTimeUnits(TSyncFileTimeUnit time1Unit, TSyncFileTimeUnit time2Unit);
    void setTimeDataTypes(TSyncFileDataType time1DType, TSyncFileDataType time2DType);

    std::string fileName() const;
    void setFileName(const std::string &fname);

    void setSyncMode(TSyncFileMode mode);
    void setChunkSize(int size);

    void setCreationTimeOverride(const std::chrono::system_clock::time_point &tp);

    bool open(const std::string &modName, const Uuid &collectionId, const MetaStringMap &userData = {});
    bool open(
        const std::string &modName,
        const Uuid &collectionId,
        const microseconds_t &tolerance,
        const MetaStringMap &userData = {});
    void flush();
    void close();

    void writeTimes(const microseconds_t &deviceTime, const microseconds_t &masterTime);
    void writeTimes(const long long &timeIndex, const microseconds_t &masterTime);
    void writeTimes(const long &time1, const long &time2);
    void writeTimes(const uint64_t &time1, const uint64_t &time2);

private:
    std::ofstream m_file;
    TSyncFileMode m_tsMode;
    int m_blockSize;
    int m_bIndex;
    XXH3_state_t *m_xxh3State;
    std::string m_lastError;
    std::string m_fname;
    std::optional<std::chrono::system_clock::time_point> m_creationTimeOverride;

    std::pair<std::string, std::string> m_timeNames;
    std::pair<TSyncFileTimeUnit, TSyncFileTimeUnit> m_timeUnits;
    TSyncFileDataType m_time1DType;
    TSyncFileDataType m_time2DType;

    void writeBlockTerminator(bool check = true);
    template<class T>
    void writeRawLE(T val);
    template<class T>
    void csWriteValue(const T &data);
    template<class T1, class T2>
    void writeTimeEntry(const T1 &time1, const T2 &time2);
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

    bool open(const std::string &fname);
    [[nodiscard]] std::string lastError() const;

    [[nodiscard]] std::string moduleName() const;
    Uuid collectionId() const;
    time_t creationTime() const;
    TSyncFileMode syncMode() const;

    MetaStringMap userData() const;
    microseconds_t tolerance() const;

    std::pair<std::string, std::string> timeNames() const;
    std::pair<TSyncFileTimeUnit, TSyncFileTimeUnit> timeUnits() const;
    std::pair<TSyncFileDataType, TSyncFileDataType> timeDTypes() const;

    std::vector<std::pair<long long, long long>> times() const;

private:
    std::string m_lastError;
    std::string m_moduleName;
    int64_t m_creationTime;
    Uuid m_collectionId;
    MetaStringMap m_userData;

    TSyncFileMode m_tsMode;
    int m_blockSize;

    microseconds_t m_tolerance;
    std::vector<std::pair<long long, long long>> m_times;
    std::pair<std::string, std::string> m_timeNames;
    std::pair<TSyncFileTimeUnit, TSyncFileTimeUnit> m_timeUnits;
    std::pair<TSyncFileDataType, TSyncFileDataType> m_timeDTypes;
};

} // namespace Syntalos

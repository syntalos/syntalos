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

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <charconv>
#include <cstdint>
#include <cassert>
#include <array>
#include <cmath>

#include "syclock.h"
#include "binarystream.h"
#include "eigenaux.h"

using namespace Syntalos;

/**
 * @brief The ModuleState enum
 *
 * Describes the state a module can be in. The state is usually displayed
 * to the user via a module indicator widget.
 */
enum class ModuleState : uint16_t {
    UNKNOWN,      /// Module is in an unknown state
    INITIALIZING, /// Module is initializing after being added
    IDLE,         /// Module is inactive and not started
    PREPARING,    /// Module is preparing a run
    DORMANT,      /// The module is inactive for this run, as it has no work to do
    READY,        /// Everything is prepared, we are ready to start
    RUNNING,      /// Module is running
    ERROR         /// Module failed to run / is in an error state
};

/**
 * @brief Base interface for all data types
 *
 * This structure defines basic interfaces that data types
 * need to possess.
 */
struct BaseDataType {
public:
    /**
     * @brief The TypeId enum
     *
     * Describes the type of data that is being transferred,
     * and provides the needed type reflection in a very efficient
     * way to allow for type checks in hot code paths.
     */
    enum TypeId {
        Unknown,
        ControlCommand,
        TableRow,
        Frame,
        FirmataControl,
        FirmataData,
        IntSignalBlock,
        FloatSignalBlock,
        Last
    };

    static std::string typeIdToString(int value)
    {
        if (value < 1 || value >= TypeId::Last)
            return "<<unknown>>";
        return typeIdToString(static_cast<TypeId>(value));
    }

    static std::string typeIdToString(TypeId value)
    {
        switch (value) {
        case Unknown:
            return "Unknown";
        case ControlCommand:
            return "ControlCommand";
        case TableRow:
            return "TableRow";
        case Frame:
            return "Frame";
        case FirmataControl:
            return "FirmataControl";
        case FirmataData:
            return "FirmataData";
        case IntSignalBlock:
            return "IntSignalBlock";
        case FloatSignalBlock:
            return "FloatSignalBlock";
        default:
            return "<<unknown>>";
        }
    }

    static TypeId typeIdFromString(const std::string &str)
    {
        if (str == "Unknown")
            return TypeId::Unknown;
        if (str == "ControlCommand")
            return TypeId::ControlCommand;
        if (str == "TableRow")
            return TypeId::TableRow;
        if (str == "Frame")
            return TypeId::Frame;
        if (str == "FirmataControl")
            return TypeId::FirmataControl;
        if (str == "FirmataData")
            return TypeId::FirmataData;
        if (str == "IntSignalBlock")
            return TypeId::IntSignalBlock;
        if (str == "FloatSignalBlock")
            return TypeId::FloatSignalBlock;
        return TypeId::Unknown;
    }

    /**
     * Unique ID for the respective data type.
     */
    [[nodiscard]] virtual TypeId typeId() const = 0;

    /**
     * @brief Calculate the size of the data in memory
     *
     * Quickly calculate the maximum size this data occupies
     * in memory. This will be used to allocate appropriate
     * shared memory blocks in advance.
     * Return -1 if the size is unknown.
     */
    [[nodiscard]] virtual ssize_t memorySize() const
    {
        // Size is not known in advance
        return -1;
    }

    /**
     * @brief Write the data to a memory block
     *
     * Write the data to a memory block. The required size of the
     * block is given by the memorySize() method, but a larger size
     * can be passed if needed.
     *
     * Return true if the data was written successfully.
     */
    virtual bool writeToMemory(void *memory, ssize_t size = -1) const
    {
        return false;
    };

    /**
     * @brief Serialize the data to a byte array
     *
     * Serialize the data to a byte array for local transmission.
     */
    [[nodiscard]] virtual std::vector<std::byte> toBytes() const = 0;
};

/**
 * @brief Helper macro to define a Syntalos stream data type.
 */
#define SY_DEFINE_DATA_TYPE(TypeName)                    \
    BaseDataType::TypeId typeId() const override         \
    {                                                    \
        return BaseDataType::TypeName;                   \
    }                                                    \
    static constexpr BaseDataType::TypeId staticTypeId() \
    {                                                    \
        return BaseDataType::TypeName;                   \
    }

/**
 * @brief Helper function to get the type ID of a data type
 */
template<typename T>
constexpr int syDataTypeId()
    requires std::is_base_of_v<BaseDataType, T>
{
    return T::staticTypeId();
}

/**
 * @brief Convenience function to deserialize a data type from memory
 */
template<typename T>
T deserializeFromMemory(const void *memory, size_t size)
    requires std::is_base_of_v<BaseDataType, T>
{
    return T::fromMemory(memory, size);
}

/**
 * @brief The ControlCommandKind enum
 *
 * Basic operations to control a module from another module.
 */
enum class ControlCommandKind {
    UNKNOWN,
    START, /// Start an operation
    PAUSE, /// Pause an operation, can be resumed with START
    STOP,  /// Stop an operation
    STEP,  /// Advance operation by one step
    CUSTOM
};

/**
 * @brief A control command to a module.
 *
 * Generic data type to stream commands to other modules.
 */
struct ControlCommand : BaseDataType {
    SY_DEFINE_DATA_TYPE(ControlCommand)

    ControlCommandKind kind{ControlCommandKind::UNKNOWN}; /// The command type
    milliseconds_t duration; /// Duration of the command before resetting to the previous state (zero for infinite)
    std::string command;     /// Custom command name, if in custom mode

    explicit ControlCommand()
        : duration(0)
    {
    }
    explicit ControlCommand(ControlCommandKind ckind)
        : kind(ckind),
          duration(0)
    {
    }

    void setDuration(ulong value)
    {
        duration = milliseconds_t(value);
    }

    [[nodiscard]] ulong getDurationAsInt() const
    {
        return duration.count();
    }

    [[nodiscard]] std::vector<std::byte> toBytes() const override
    {
        std::vector<std::byte> bytes;
        BinaryStreamWriter stream(bytes);

        stream.write(kind);
        stream.write(static_cast<uint64_t>(duration.count()));
        stream.write(command);

        return bytes;
    }

    static ControlCommand fromMemory(const void *memory, size_t size)
    {
        ControlCommand obj;
        BinaryStreamReader stream(memory, size);

        uint64_t durationValue;
        stream.read(obj.kind);
        stream.read(durationValue);
        stream.read(obj.command);
        obj.duration = milliseconds_t(durationValue);

        return obj;
    }
};

/**
 * @brief A new row  for a table
 *
 * Generic type emitted for adding a table row.
 */
struct TableRow : BaseDataType {
    SY_DEFINE_DATA_TYPE(TableRow)

    std::vector<std::string> data;

    explicit TableRow() = default;
    explicit TableRow(const std::vector<std::string> &row)
        : data(row)
    {
    }

    void reserve(int size)
    {
        data.reserve(size);
    }

    void append(const std::string &t)
    {
        data.push_back(t);
    }

    [[nodiscard]] int length() const
    {
        return data.size();
    }

    std::vector<std::byte> toBytes() const override
    {
        std::vector<std::byte> bytes;
        BinaryStreamWriter stream(bytes);

        stream.write(data);

        return bytes;
    }

    static TableRow fromMemory(const void *memory, size_t size)
    {
        TableRow obj;
        BinaryStreamReader stream(memory, size);

        stream.read(obj.data);

        return obj;
    }
};

/**
 * @brief The FirmataCommandKind enum
 *
 * Set which type of change should be made on a Firmata interface.
 */
enum class FirmataCommandKind {
    UNKNOWN,
    NEW_DIG_PIN,
    NEW_ANA_PIN,
    IO_MODE,
    WRITE_ANALOG,
    WRITE_DIGITAL,
    WRITE_DIGITAL_PULSE,
    SYSEX /// not implemented
};

/**
 * @brief Commands to control Firmata output.
 */
struct FirmataControl : BaseDataType {
    SY_DEFINE_DATA_TYPE(FirmataControl)

    FirmataCommandKind command;
    uint8_t pinId{0};
    std::string pinName;
    bool isOutput{false};
    bool isPullUp{false};
    uint16_t value;

    explicit FirmataControl()
        : command(FirmataCommandKind::UNKNOWN),
          value(0)
    {
    }

    explicit FirmataControl(FirmataCommandKind cmd)
        : command(cmd),
          isPullUp(false),
          value(0)
    {
    }

    FirmataControl(FirmataCommandKind kind, int pinId, std::string name = std::string())
        : command(kind),
          pinId(pinId),
          pinName(std::move(name)),
          isPullUp(false),
          value(0)
    {
    }

    FirmataControl(FirmataCommandKind kind, std::string name)
        : command(kind),
          pinName(std::move(name)),
          isPullUp(false),
          value(0)
    {
    }

    std::vector<std::byte> toBytes() const override
    {
        std::vector<std::byte> bytes;
        BinaryStreamWriter stream(bytes);

        stream.write(command);
        stream.write(pinId);
        stream.write(pinName);
        stream.write(isOutput);
        stream.write(isPullUp);
        stream.write(value);

        return bytes;
    }

    static FirmataControl fromMemory(const void *memory, size_t size)
    {
        FirmataControl obj;
        BinaryStreamReader stream(memory, size);

        stream.read(obj.command);
        stream.read(obj.pinId);
        stream.read(obj.pinName);
        stream.read(obj.isOutput);
        stream.read(obj.isPullUp);
        stream.read(obj.value);

        return obj;
    }
};

/**
 * @brief Output data returned from a Firmata device.
 */
struct FirmataData : BaseDataType {
    SY_DEFINE_DATA_TYPE(FirmataData)

    uint8_t pinId;
    std::string pinName;
    uint16_t value;
    bool isDigital;
    microseconds_t time;

    std::vector<std::byte> toBytes() const override
    {
        std::vector<std::byte> bytes;
        BinaryStreamWriter stream(bytes);

        stream.write(pinId);
        stream.write(pinName);
        stream.write(value);
        stream.write(isDigital);
        stream.write(static_cast<int64_t>(time.count()));

        return bytes;
    }

    static FirmataData fromMemory(const void *memory, size_t size)
    {
        FirmataData obj;
        BinaryStreamReader stream(memory, size);

        int64_t timeUs;
        stream.read(obj.pinId);
        stream.read(obj.pinName);
        stream.read(obj.value);
        stream.read(obj.isDigital);
        stream.read(timeUs);
        obj.time = microseconds_t(timeUs);

        return obj;
    }
};

/**
 * @brief Type of a signal from a signal source.
 *
 * This is usually set in the metadata of a data stream.
 */
enum class SignalDataType {
    Amplifier,
    AuxInput,
    SupplyVoltage,
    BoardAdc,
    BoardDigIn,
    BoardDigOut
};

/**
 * @brief A block of integer signal data from a data source
 *
 * This signal data block contains data for up to 16 channels. It contains
 * data as integers and is usually used for digital inputs.
 */
struct IntSignalBlock : BaseDataType {
    SY_DEFINE_DATA_TYPE(IntSignalBlock)

    explicit IntSignalBlock(uint sampleCount = 60, uint channelCount = 1)
    {
        assert(channelCount > 0);
        timestamps.resize(sampleCount);
        data.resize(sampleCount, channelCount);
    }

    size_t length() const
    {
        return timestamps.size();
    }

    size_t rows() const
    {
        return data.rows();
    }
    size_t cols() const
    {
        return data.cols();
    }

    VectorXul timestamps;
    MatrixXsi data;

    std::vector<std::byte> toBytes() const override
    {
        std::vector<std::byte> bytes;
        BinaryStreamWriter stream(bytes);

        serializeEigen(stream, timestamps);
        serializeEigen(stream, data);

        return bytes;
    }

    static IntSignalBlock fromMemory(const void *memory, size_t size)
    {
        IntSignalBlock obj;
        BinaryStreamReader stream(memory, size);

        obj.timestamps = deserializeEigen<VectorXul>(stream);
        obj.data = deserializeEigen<MatrixXsi>(stream);

        return obj;
    }
};

/**
 * @brief A block of floating-point signal data from an analog data source
 *
 * This signal data block contains data for up to 16 channels. It usually contains
 * possibly preprocessed / prefiltered analog data.
 */
struct FloatSignalBlock : BaseDataType {
    SY_DEFINE_DATA_TYPE(FloatSignalBlock)

    explicit FloatSignalBlock(uint sampleCount = 60, uint channelCount = 1)
    {
        assert(channelCount > 0);
        timestamps.resize(sampleCount);
        data.resize(sampleCount, channelCount);
    }

    explicit FloatSignalBlock(const std::vector<float> &floatVec, uint timestamp)
    {
        timestamps.array() += timestamp;
        data.resize(1, floatVec.size());
        for (size_t i = 0; i < floatVec.size(); ++i)
            data(0, i) = floatVec[i];
    }

    size_t length() const
    {
        return timestamps.size();
    }

    size_t rows() const
    {
        return data.rows();
    }
    size_t cols() const
    {
        return data.cols();
    }

    VectorXul timestamps;
    MatrixXd data;

    std::vector<std::byte> toBytes() const override
    {
        std::vector<std::byte> bytes;
        BinaryStreamWriter stream(bytes);

        serializeEigen(stream, timestamps);
        serializeEigen(stream, data);

        return bytes;
    }

    static FloatSignalBlock fromMemory(const void *memory, size_t size)
    {
        FloatSignalBlock obj;
        BinaryStreamReader stream(memory, size);

        obj.timestamps = deserializeEigen<VectorXul>(stream);
        obj.data = deserializeEigen<MatrixXd>(stream);

        return obj;
    }
};

/**
 * @brief Helper function to register all meta types for stream data
 *
 * This function registers all types with the meta object system and also
 * creates a global map of all available stream types.
 */
void registerStreamMetaTypes();

/**
 * @brief Get a mapping of type names to their IDs.
 */
std::vector<std::pair<std::string, int>> streamTypeIdIndex();

/**
 * @brief Convert a numeric value to a string using Syntalos' default notation.
 *
 * This function converts arithmetic types to strings in a locale-independent way.
 * For floating-point types, it uses the "general" format (shortest representation).
 * Special values (NaN, infinity) are handled consistently across all types.
 */
template<typename T>
inline std::string numToString(T x)
    requires std::is_arithmetic_v<T>
{
    if constexpr (std::is_same_v<T, bool>) {
        return x ? "true" : "false";
    } else {
        // Handle floating-point special cases
        if constexpr (std::is_floating_point_v<T>) {
            if (std::isnan(x))
                return "nan";
            if (std::isinf(x))
                return std::signbit(x) ? "-inf" : "inf";
            if (x == 0.0)
                x = 0.0; // canonicalize -0 to +0
        }

        std::array<char, 128> buf{};
        std::to_chars_result result{};

        if constexpr (std::is_floating_point_v<T>) {
            result = std::to_chars(buf.data(), buf.data() + buf.size(), x, std::chars_format::general);
        } else {
            result = std::to_chars(buf.data(), buf.data() + buf.size(), x);
        }

        if (result.ec != std::errc{}) {
            assert(false);
            return "<<conversion error>>";
        }

        return {buf.data(), result.ptr};
    }
}

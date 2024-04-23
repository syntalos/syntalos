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

#include <QDataStream>
#include <QMetaType>
#include <QMetaEnum>
#include <memory>

#include "syclock.h"
#include "eigenaux.h"

using namespace Syntalos;

Q_DECLARE_SMART_POINTER_METATYPE(std::shared_ptr)

/**
 * Helpers to (de)serialize enum classes into streams, in case
 * we are compiling with older versions of Qt.
 */
#if (QT_VERSION < QT_VERSION_CHECK(5, 14, 0))
template<typename T>
typename std::enable_if<std::is_enum<T>::value, QDataStream &>::type &operator<<(QDataStream &s, const T &t)
{
    return s << static_cast<typename std::underlying_type<T>::type>(t);
}

template<typename T>
typename std::enable_if<std::is_enum<T>::value, QDataStream &>::type &operator>>(QDataStream &s, T &t)
{
    return s >> reinterpret_cast<typename std::underlying_type<T>::type &>(t);
}
#endif

/**
 * @brief Connection heat level
 *
 * Warning level dependent on how full the buffer that is
 * repesented by a connection is.
 * A high heat means lots of pending stuff and potentially a
 * slow receiving module or not enough system resources.
 * This state is managed internally by Syntalos.
 */
enum class ConnectionHeatLevel {
    NONE,
    LOW,
    MEDIUM,
    HIGH
};
Q_DECLARE_METATYPE(ConnectionHeatLevel)

QString connectionHeatToHumanString(ConnectionHeatLevel heat);

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
    READY,        /// Everything is prepared, we are ready to start
    RUNNING,      /// Module is running
    ERROR         /// Module failed to run / is in an error state
};
Q_DECLARE_METATYPE(ModuleState)

/**
 * @brief Base interface for all data types
 *
 * This structure defines basic interfaces that data types
 * need to possess.
 */
struct BaseDataType {
    Q_GADGET
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
    Q_ENUM(TypeId)

    static QString typeIdToString(TypeId value)
    {
        const auto metaEnum = QMetaEnum::fromType<TypeId>();
        return QString(metaEnum.valueToKey(static_cast<int>(value)));
    }

    static QString typeIdToString(int value)
    {
        if (value < 1 || value >= TypeId::Last)
            return QStringLiteral("<<unknown>>");
        return typeIdToString(static_cast<TypeId>(value));
    }

    static TypeId typeIdFromString(const QString &str)
    {
        const auto metaEnum = QMetaEnum::fromType<TypeId>();
        bool ok;
        auto enumVal = static_cast<TypeId>(metaEnum.keyToValue(str.toLatin1(), &ok));
        if (ok)
            return enumVal;
        else
            return TypeId::Unknown;
    }

    /**
     * Unique ID for the respective data type.
     */
    virtual TypeId typeId() const = 0;

    /**
     * @brief Calculate the size of the data in memory
     *
     * Quickly calculate the maximum size this data occupies
     * in memory. This will be used to allocate appropriate
     * shared memory blocks in advance.
     * Return -1 if the size is unknown.
     */
    virtual ssize_t memorySize() const
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
    virtual QByteArray toBytes() const = 0;
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
    QString command;         /// Custom command name, if in custom mode

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

    ulong getDurationAsInt() const
    {
        return duration.count();
    }

    QByteArray toBytes() const override
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);

        stream << kind << (quint64)duration.count() << command;

        return bytes;
    }

    static ControlCommand fromMemory(const void *memory, size_t size)
    {
        ControlCommand obj;

        QByteArray block(reinterpret_cast<const char *>(memory), size);
        QDataStream stream(block);

        quint64 durationValue;
        stream >> obj.kind >> durationValue >> obj.command;
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

    QList<QString> data;

    explicit TableRow() {}
    explicit TableRow(const QList<QString> &row)
        : data(row)
    {
    }

    void reserve(int size)
    {
        data.reserve(size);
    }

    void append(const QString &t)
    {
        data.append(t);
    }

    int length() const
    {
        return data.length();
    }

    QByteArray toBytes() const override
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);

        stream << data;

        return bytes;
    }

    static TableRow fromMemory(const void *memory, size_t size)
    {
        TableRow obj;
        QByteArray block(reinterpret_cast<const char *>(memory), size);
        QDataStream stream(block);

        stream >> obj.data;

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
    uint8_t pinId;
    QString pinName;
    bool isOutput;
    bool isPullUp;
    uint16_t value;

    explicit FirmataControl()
        : isPullUp(false),
          value(0)
    {
    }

    explicit FirmataControl(FirmataCommandKind cmd)
        : command(cmd),
          isPullUp(false),
          value(0)
    {
    }

    QByteArray toBytes() const override
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);

        stream << command << pinId << pinName << isOutput << isPullUp << value;

        return bytes;
    }

    static FirmataControl fromMemory(const void *memory, size_t size)
    {
        FirmataControl obj;
        QByteArray block(reinterpret_cast<const char *>(memory), size);
        QDataStream stream(block);

        stream >> obj.command >> obj.pinId >> obj.pinName >> obj.isOutput >> obj.isPullUp >> obj.value;

        return obj;
    }
};

/**
 * @brief Output data returned from a Firmata device.
 */
struct FirmataData : BaseDataType {
    SY_DEFINE_DATA_TYPE(FirmataData)

    uint8_t pinId;
    QString pinName;
    uint16_t value;
    bool isDigital;
    milliseconds_t time;

    QByteArray toBytes() const override
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);

        stream << pinId << pinName << value << isDigital << static_cast<quint32>(time.count());

        return bytes;
    }

    static FirmataData fromMemory(const void *memory, size_t size)
    {
        FirmataData obj;
        QByteArray block(reinterpret_cast<const char *>(memory), size);
        QDataStream stream(block);

        quint32 timeMs;
        stream >> obj.pinId >> obj.pinName >> obj.value >> obj.isDigital >> timeMs;
        obj.time = milliseconds_t(timeMs);

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
        Q_ASSERT(channelCount > 0);
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

    VectorXu timestamps;
    MatrixXi data;

    QByteArray toBytes() const override
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);

        serializeEigen(stream, timestamps);
        serializeEigen(stream, data);

        return bytes;
    }

    static IntSignalBlock fromMemory(const void *memory, size_t size)
    {
        IntSignalBlock obj;
        QByteArray block(reinterpret_cast<const char *>(memory), size);
        QDataStream stream(block);

        obj.timestamps = deserializeEigen<VectorXu>(stream);
        obj.data = deserializeEigen<MatrixXi>(stream);

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
        Q_ASSERT(channelCount > 0);
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

    VectorXu timestamps;
    MatrixXd data;

    QByteArray toBytes() const override
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);

        serializeEigen(stream, timestamps);
        serializeEigen(stream, data);

        return bytes;
    }

    static FloatSignalBlock fromMemory(const void *memory, size_t size)
    {
        FloatSignalBlock obj;
        QByteArray block(reinterpret_cast<const char *>(memory), size);
        QDataStream stream(block);

        obj.timestamps = deserializeEigen<VectorXu>(stream);
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
QMap<QString, int> streamTypeIdMap();

class VariantDataStream;
namespace Syntalos
{
class VarStreamInputPort;
class AbstractModule;

} // namespace Syntalos

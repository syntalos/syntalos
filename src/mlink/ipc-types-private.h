/*
 * Copyright (C) 2020-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QObject>
#include <QIODevice>
#include <QVariantHash>
#include <QDataStream>

#include <iox2/bb/static_string.hpp>
#include <iox2/bb/static_vector.hpp>
#include <iox2/iceoryx2.hpp>

#include "syntalos-datactl"

namespace Syntalos
{

// number of elements to hold in the IPC queues
static constexpr uint64_t SY_IOX_QUEUE_CAPACITY = 12U;

// number of elements to hold in the publisher history
static constexpr uint64_t SY_IOX_HISTORY_SIZE = 2U;

// initial size of the shared memory block for IPC communication
static constexpr uint64_t SY_IOX_INITIAL_SLICE_LEN = 4096;

// maximum length for IPC service name components
static constexpr size_t SY_IOX_ID_MAX_LEN = IOX2_SERVICE_NAME_LENGTH;

/**
 * @brief IPC service topology limits
 */
struct IpcServiceTopology {
    // we set minimum safe values as defaults
    // (1 sender/receiver, 2 nodes, but doubled to prevent races and have two entities
    // exist in parallel briefly, as one replaces the other in any reset operations)
    uint maxSenders{1 * 2};
    uint maxReceivers{1 * 2};
    uint maxNodes{2 * 2};

    IpcServiceTopology() = default;
    IpcServiceTopology(uint sendN, uint recvN, uint nodes = 2)
        : maxSenders(sendN),
          maxReceivers(recvN),
          maxNodes(nodes)
    {
    }
};

/**
 * Helper to create a Syntalos IPC service topology, for setting limits on IOX connections.
 */
[[nodiscard]] inline constexpr IpcServiceTopology makeIpcServiceTopology(uint senderCount, uint receiverCount)
{
    const auto sendN = senderCount > 0 ? senderCount : 1U;
    // Keep one additional subscriber slot to tolerate reconnect races.
    const auto recvN = receiverCount + 1U;
    return IpcServiceTopology(sendN, recvN, sendN + receiverCount);
}

/**
 * @brief Action performed to modify a module port
 */
enum class PortAction : uint8_t {
    UNKNOWN, /// Undefined action
    ADD,     /// Add a new port
    REMOVE,  /// Remove an existing port
    CHANGE   /// Change an existing port
};

/**
 * @brief Information about an input port change
 */
struct InputPortChange {
    PortAction action{PortAction::UNKNOWN};

    QString id;
    QString title;
    int dataTypeId{-1};
    QVariantHash metadata;
    uint throttleItemsPerSec{0};

    InputPortChange() = default;
    explicit InputPortChange(PortAction pa)
        : action(pa),
          dataTypeId(-1),
          throttleItemsPerSec(0)
    {
    }

    [[nodiscard]] QByteArray toBytes() const
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);

        stream << action << id << title << dataTypeId << metadata << throttleItemsPerSec;

        return bytes;
    }

    static InputPortChange fromMemory(const void *memory, size_t size)
    {
        InputPortChange info;

        QByteArray block(reinterpret_cast<const char *>(memory), size);
        QDataStream stream(block);

        stream >> info.action >> info.id >> info.title >> info.dataTypeId >> info.metadata >> info.throttleItemsPerSec;

        return info;
    }

    friend QDataStream &operator<<(QDataStream &out, const InputPortChange &info)
    {
        out << info.action << info.id << info.title << info.dataTypeId << info.metadata << info.throttleItemsPerSec;
        return out;
    }

    friend QDataStream &operator>>(QDataStream &in, InputPortChange &info)
    {
        in >> info.action >> info.id >> info.title >> info.dataTypeId >> info.metadata >> info.throttleItemsPerSec;
        return in;
    }
};
static const std::string IN_PORT_CHANGE_CHANNEL_ID = "InPortChange";

/**
 * @brief Information about an output port change
 */
struct OutputPortChange {
    PortAction action{PortAction::UNKNOWN};

    QString id;
    QString title;
    int dataTypeId{-1};
    QVariantHash metadata;
    IpcServiceTopology topology;

    OutputPortChange() = default;
    explicit OutputPortChange(PortAction pa)
        : action(pa),
          dataTypeId(-1)
    {
    }

    [[nodiscard]] QByteArray toBytes() const
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);

        stream << action << id << title << dataTypeId << metadata << topology.maxSenders << topology.maxReceivers
               << topology.maxNodes;

        return bytes;
    }

    static OutputPortChange fromMemory(const void *memory, size_t size)
    {
        OutputPortChange info;

        QByteArray block(reinterpret_cast<const char *>(memory), size);
        QDataStream stream(block);

        stream >> info.action >> info.id >> info.title >> info.dataTypeId >> info.metadata >> info.topology.maxSenders
            >> info.topology.maxReceivers >> info.topology.maxNodes;

        return info;
    }

    friend QDataStream &operator<<(QDataStream &out, const OutputPortChange &info)
    {
        out << info.action << info.id << info.title << info.dataTypeId << info.metadata << info.topology.maxSenders
            << info.topology.maxReceivers << info.topology.maxNodes;
        return out;
    }

    friend QDataStream &operator>>(QDataStream &in, OutputPortChange &info)
    {
        in >> info.action >> info.id >> info.title >> info.dataTypeId >> info.metadata >> info.topology.maxSenders
            >> info.topology.maxReceivers >> info.topology.maxNodes;
        return in;
    }
};
static const std::string OUT_PORT_CHANGE_CHANNEL_ID = "OutPortChange";

/**
 * @brief request to update the metadata of an input port
 */
struct UpdateInputPortMetadataRequest {

    QString id;
    QVariantHash metadata;

    UpdateInputPortMetadataRequest() = default;

    [[nodiscard]] QByteArray toBytes() const
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);

        stream << id << metadata;

        return bytes;
    }

    static UpdateInputPortMetadataRequest fromMemory(const void *memory, size_t size)
    {
        UpdateInputPortMetadataRequest req;

        QByteArray block(reinterpret_cast<const char *>(memory), size);
        QDataStream stream(block);

        stream >> req.id >> req.metadata;

        return req;
    }
};
static const std::string IN_PORT_UPDATE_METADATA_ID = "UpdateInputPortMetadata";

/**
 * Generic response to a request.
 */
struct DoneResponse {
    bool success;
};

/**
 * Shared wakeup event service names for the control channel.
 *
 *  WORKER_CTL_EVENT_ID - the worker notifies the master whenever it publishes
 *                        on a control channel (error, state, port-changes …).
 *                        The master attaches one listener for this to its WaitSet.
 *
 *  MASTER_CTL_EVENT_ID - the master notifies the worker whenever it sends a
 *                        request (request-response commands).
 */
static const std::string WORKER_CTL_EVENT_ID = "worker-event";
static const std::string MASTER_CTL_EVENT_ID = "master-event";

/**
 * Event indicating an error
 */
struct ErrorEvent {
    iox2::bb::StaticString<128> title;
    iox2::bb::StaticString<2048> message;
};
static const std::string ERROR_CHANNEL_ID = "Error";

/**
 * Module state change event
 */
struct StateChangeEvent {
    ModuleState state;
};
static const std::string STATE_CHANNEL_ID = "State";

/**
 * Event sending a status message to master.
 */
struct StatusMessageEvent {
    iox2::bb::StaticString<512> text;
};
static const std::string STATUS_MESSAGE_CHANNEL_ID = "StatusMessage";

/**
 * Request the module API version supported by the worker.
 */
struct ApiVersionRequest {
};
static const std::string API_VERSION_CALL_ID = "ApiVersion";

/**
 * Response to an ApiVersionRequest
 */
struct ApiVersionResponse {
    iox2::bb::StaticString<64> apiVersion;
};

/**
 * Request to set the niceness of a worker
 */
struct SetNicenessRequest {
    int nice;
};
static const std::string SET_NICENESS_CALL_ID = "SetNiceness";

/**
 * Request to set the maximum realtime priority of a worker
 */
struct SetMaxRealtimePriority {
    int priority;
};
static const std::string SET_MAX_RT_PRIORITY_CALL_ID = "SetMaxRealtimePriority";

/**
 * Request to set the CPU affinity of a worker
 */
struct SetCPUAffinityRequest {
    iox2::bb::StaticVector<uint32_t, 256> cores; // array of CPU core indices to set affinity to
};
static const std::string SET_CPU_AFFINITY_CALL_ID = "SetCPUAffinity";

/**
 * Request to delete an input or output port
 */
struct DeletePortRequest {
    int portId;
};
static const std::string DELETE_PORT_CALL_ID = "DeletePort";

/**
 * Connect the input port of a linked module to an exported output.
 */
struct ConnectInputRequest {
    iox2::bb::StaticString<SY_IOX_ID_MAX_LEN> portId;
    iox2::bb::StaticString<SY_IOX_ID_MAX_LEN> instanceId;
    iox2::bb::StaticString<SY_IOX_ID_MAX_LEN> channelId;
    IpcServiceTopology topology;
};
static const std::string CONNECT_INPUT_CALL_ID = "ConnectInputPort";

/**
 * Instruct the module to load a script
 */
struct LoadScriptRequest {
    QString workingDir;
    QString venvDir;
    QString script;

    [[nodiscard]] QByteArray toBytes() const
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);

        stream << workingDir << script;

        return bytes;
    }

    static LoadScriptRequest fromMemory(const void *memory, size_t size)
    {
        LoadScriptRequest req;

        QByteArray block(reinterpret_cast<const char *>(memory), size);
        QDataStream stream(block);

        stream >> req.workingDir >> req.script;

        return req;
    }
};
static const std::string LOAD_SCRIPT_CALL_ID = "LoadScript";

struct SetPortsPresetRequest {
    QList<InputPortChange> inPorts;
    QList<OutputPortChange> outPorts;

    [[nodiscard]] QByteArray toBytes() const
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);

        stream << inPorts << outPorts;

        return bytes;
    }

    static SetPortsPresetRequest fromMemory(const void *memory, size_t size)
    {
        SetPortsPresetRequest req;

        QByteArray block(reinterpret_cast<const char *>(memory), size);
        QDataStream stream(block);

        stream >> req.inPorts >> req.outPorts;

        return req;
    }
};
static const std::string SET_PORTS_PRESET_CALL_ID = "SetPortsPresetRequest";

/**
 * Request to prepare the module for starting,
 * this enters the PREPARING stage
 */
struct PrepareStartRequest {
    QByteArray settings;

    [[nodiscard]] QByteArray toBytes() const
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);

        stream << settings;

        return bytes;
    }

    static PrepareStartRequest fromMemory(const void *memory, size_t size)
    {
        PrepareStartRequest req;

        QByteArray block(reinterpret_cast<const char *>(memory), size);
        QDataStream stream(block);

        stream >> req.settings;

        return req;
    }
};
static const std::string PREPARE_START_CALL_ID = "PrepareStart";

/**
 * Start module run, this enters the RUNNING stage
 */
struct StartRequest {
    int64_t startTimestampUsec;
};
static const std::string START_CALL_ID = "Start";

/**
 * Stop module run, this enters the IDLE stage
 */
struct StopRequest {
};
static const std::string STOP_CALL_ID = "Stop";

/**
 * Request to shutdown the module process cleanly
 */
struct ShutdownRequest {
};
static const std::string SHUTDOWN_CALL_ID = "Shutdown";

/**
 * Event from the module to indicate a settings change. Syntalos will store the new settings.
 */
struct SettingsChangeEvent {
    QByteArray settings;

    SettingsChangeEvent() = default;
    explicit SettingsChangeEvent(const ByteVector &bytes)
        : settings(reinterpret_cast<const char *>(bytes.data()), static_cast<qsizetype>(bytes.size()))
    {
    }

    explicit SettingsChangeEvent(QByteArray settings)
        : settings(std::move(settings))
    {
    }

    [[nodiscard]] QByteArray toBytes() const
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);

        stream << settings;

        return bytes;
    }

    static SettingsChangeEvent fromMemory(const void *memory, size_t size)
    {
        SettingsChangeEvent ev;

        QByteArray block(reinterpret_cast<const char *>(memory), size);
        QDataStream stream(block);

        stream >> ev.settings;

        return ev;
    }
};
static const std::string SETTINGS_CHANGE_CHANNEL_ID = "SettingsChange";

/**
 * Request to change show the GUI dialog to change settings.
 */
struct ShowSettingsRequest {
    QByteArray settings;

    [[nodiscard]] QByteArray toBytes() const
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);

        stream << settings;

        return bytes;
    }

    static ShowSettingsRequest fromMemory(const void *memory, size_t size)
    {
        ShowSettingsRequest req;

        QByteArray block(reinterpret_cast<const char *>(memory), size);
        QDataStream stream(block);

        stream >> req.settings;

        return req;
    }
};
static const std::string SHOW_SETTINGS_CALL_ID = "ShowSettings";

/**
 * Request to show the display window(s) of the module.
 */
struct ShowDisplayRequest {
};
static const std::string SHOW_DISPLAY_CALL_ID = "ShowDisplay";

} // namespace Syntalos

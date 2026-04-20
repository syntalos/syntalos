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

// max response buffer size
static constexpr uint64_t SY_IOX_MAX_RESPONSE_BUF_SIZE = 2U;

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
struct InputPortChangeRequest {
    PortAction action{PortAction::UNKNOWN};

    std::string id;
    std::string title;
    int dataTypeId{-1};
    MetaStringMap metadata;
    uint throttleItemsPerSec{0};

    InputPortChangeRequest() = default;
    explicit InputPortChangeRequest(PortAction pa)
        : action(pa),
          dataTypeId(-1),
          throttleItemsPerSec(0)
    {
    }

    void writeTo(BinaryStreamWriter &out) const
    {
        out.write(action);
        out.write(id);
        out.write(title);
        out.write(dataTypeId);
        out.write(metadata);
        out.write(throttleItemsPerSec);
    }

    static InputPortChangeRequest readFrom(BinaryStreamReader &in)
    {
        InputPortChangeRequest info;
        in.read(info.action);
        in.read(info.id);
        in.read(info.title);
        in.read(info.dataTypeId);
        in.read(info.metadata);
        in.read(info.throttleItemsPerSec);
        return info;
    }

    [[nodiscard]] ByteVector toBytes() const
    {
        ByteVector bytes;
        BinaryStreamWriter stream(bytes);
        writeTo(stream);
        return bytes;
    }

    static InputPortChangeRequest fromMemory(const void *memory, size_t size)
    {
        BinaryStreamReader stream(memory, size);
        return readFrom(stream);
    }
};
static const std::string IN_PORT_CHANGE_CHANNEL_ID = "InPortChange";

/**
 * @brief Information about an output port change
 */
struct OutputPortChangeRequest {
    PortAction action{PortAction::UNKNOWN};

    std::string id;
    std::string title;
    int dataTypeId{-1};
    MetaStringMap metadata;
    IpcServiceTopology topology;

    OutputPortChangeRequest() = default;
    explicit OutputPortChangeRequest(PortAction pa)
        : action(pa),
          dataTypeId(-1)
    {
    }

    void writeTo(BinaryStreamWriter &out) const
    {
        out.write(action);
        out.write(id);
        out.write(title);
        out.write(dataTypeId);
        out.write(metadata);
        out.write(topology.maxSenders);
        out.write(topology.maxReceivers);
        out.write(topology.maxNodes);
    }

    static OutputPortChangeRequest readFrom(BinaryStreamReader &in)
    {
        OutputPortChangeRequest info;
        in.read(info.action);
        in.read(info.id);
        in.read(info.title);
        in.read(info.dataTypeId);
        in.read(info.metadata);
        in.read(info.topology.maxSenders);
        in.read(info.topology.maxReceivers);
        in.read(info.topology.maxNodes);
        return info;
    }

    [[nodiscard]] ByteVector toBytes() const
    {
        ByteVector bytes;
        BinaryStreamWriter stream(bytes);
        writeTo(stream);
        return bytes;
    }

    static OutputPortChangeRequest fromMemory(const void *memory, size_t size)
    {
        BinaryStreamReader stream(memory, size);
        return readFrom(stream);
    }
};
static const std::string OUT_PORT_CHANGE_CHANNEL_ID = "OutPortChange";

/**
 * @brief request to update the metadata of an input port
 */
struct UpdateInputPortMetadataRequest {

    std::string id;
    MetaStringMap metadata;

    UpdateInputPortMetadataRequest() = default;

    [[nodiscard]] ByteVector toBytes() const
    {
        ByteVector bytes;
        BinaryStreamWriter stream(bytes);
        stream.write(id);
        stream.write(metadata);
        return bytes;
    }

    static UpdateInputPortMetadataRequest fromMemory(const void *memory, size_t size)
    {
        UpdateInputPortMetadataRequest req;
        BinaryStreamReader stream(memory, size);
        stream.read(req.id);
        stream.read(req.metadata);
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
    std::string workingDir;
    std::string venvDir;
    std::string script;
    bool resetPorts = false; /// If true, the worker should reset its port state before executing the script.

    [[nodiscard]] ByteVector toBytes() const
    {
        ByteVector bytes;
        BinaryStreamWriter stream(bytes);
        stream.write(workingDir);
        stream.write(venvDir);
        stream.write(script);
        stream.write(resetPorts);
        return bytes;
    }

    static LoadScriptRequest fromMemory(const void *memory, size_t size)
    {
        LoadScriptRequest req;
        BinaryStreamReader stream(memory, size);
        stream.read(req.workingDir);
        stream.read(req.venvDir);
        stream.read(req.script);
        stream.read(req.resetPorts);
        return req;
    }
};
static const std::string LOAD_SCRIPT_CALL_ID = "LoadScript";

struct SetPortsPresetRequest {
    std::vector<InputPortChangeRequest> inPorts;
    std::vector<OutputPortChangeRequest> outPorts;

    [[nodiscard]] ByteVector toBytes() const
    {
        ByteVector bytes;
        BinaryStreamWriter stream(bytes);
        stream.write(static_cast<uint64_t>(inPorts.size()));
        for (const auto &ip : inPorts)
            ip.writeTo(stream);
        stream.write(static_cast<uint64_t>(outPorts.size()));
        for (const auto &op : outPorts)
            op.writeTo(stream);
        return bytes;
    }

    static SetPortsPresetRequest fromMemory(const void *memory, size_t size)
    {
        SetPortsPresetRequest req;
        BinaryStreamReader stream(memory, size);
        uint64_t count;
        stream.read(count);
        req.inPorts.reserve(count);
        for (uint64_t i = 0; i < count; ++i)
            req.inPorts.push_back(InputPortChangeRequest::readFrom(stream));
        stream.read(count);
        req.outPorts.reserve(count);
        for (uint64_t i = 0; i < count; ++i)
            req.outPorts.push_back(OutputPortChangeRequest::readFrom(stream));
        return req;
    }
};
static const std::string SET_PORTS_PRESET_CALL_ID = "SetPortsPresetRequest";

/**
 * Request the module to save project settings.
 */
struct SaveSettingsRequest {
    std::string baseDir;

    [[nodiscard]] ByteVector toBytes() const
    {
        ByteVector bytes;
        BinaryStreamWriter stream(bytes);
        stream.write(baseDir);
        return bytes;
    }

    static SaveSettingsRequest fromMemory(const void *memory, size_t size)
    {
        SaveSettingsRequest req;
        BinaryStreamReader stream(memory, size);
        stream.read(req.baseDir);
        return req;
    }
};

/**
 * Generic response to a request.
 */
struct SaveSettingsResponse {
    bool success{false};
    ByteVector data{};

    [[nodiscard]] ByteVector toBytes() const
    {
        ByteVector bytes;
        BinaryStreamWriter stream(bytes);
        stream.write(success);
        stream.write(data);
        return bytes;
    }

    static SaveSettingsResponse fromMemory(const void *memory, size_t size)
    {
        SaveSettingsResponse res;
        BinaryStreamReader stream(memory, size);
        stream.read(res.success);
        stream.read(res.data);
        return res;
    }
};
static const std::string SAVE_SETTINGS_CALL_ID = "SaveSettings";

/**
 * Request the module to load project settings.
 */
struct LoadSettingsRequest {
    std::string baseDir;
    ByteVector data;

    [[nodiscard]] ByteVector toBytes() const
    {
        ByteVector bytes;
        BinaryStreamWriter stream(bytes);
        stream.write(baseDir);
        stream.write(data);
        return bytes;
    }

    static LoadSettingsRequest fromMemory(const void *memory, size_t size)
    {
        LoadSettingsRequest req;
        BinaryStreamReader stream(memory, size);
        stream.read(req.baseDir);
        stream.read(req.data);
        return req;
    }
};
static const std::string LOAD_SETTINGS_CALL_ID = "LoadSettings";

/**
 * Request to prepare the module for the upcoming run.
 * This makes the module enter the PREPARING stage.
 */
struct PrepareRunRequest {
    std::string subjectId;
    std::string subjectGroup;

    [[nodiscard]] ByteVector toBytes() const
    {
        ByteVector bytes;
        BinaryStreamWriter stream(bytes);
        stream.write(subjectId);
        stream.write(subjectGroup);
        return bytes;
    }

    static PrepareRunRequest fromMemory(const void *memory, size_t size)
    {
        PrepareRunRequest req;
        BinaryStreamReader stream(memory, size);
        stream.read(req.subjectId);
        stream.read(req.subjectGroup);
        return req;
    }
};
static const std::string PREPARE_RUN_CALL_ID = "PrepareRun";

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
 * Request to show the GUI dialog to change settings.
 */
struct ShowSettingsRequest {
};
static const std::string SHOW_SETTINGS_CALL_ID = "ShowSettings";

/**
 * Request to show the display window(s) of the module.
 */
struct ShowDisplayRequest {
};
static const std::string SHOW_DISPLAY_CALL_ID = "ShowDisplay";

} // namespace Syntalos

/*
 * Copyright (C) 2020-2024 Matthias Klumpp <matthias@tenstral.net>
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
#include <QVariantHash>
#include <QDataStream>
#include <iceoryx_hoofs/cxx/string.hpp>
#include <iceoryx_hoofs/cxx/vector.hpp>
#include <iceoryx_posh/popo/untyped_publisher.hpp>
#include <iceoryx_posh/popo/untyped_subscriber.hpp>

namespace Syntalos
{

// number of elements to hold in the IPC queue
static const uint64_t SY_IOX_QUEUE_CAPACITY = 1U;

// number of elements to keep for late connectors
static const uint64_t SY_IOX_HISTORY_SIZE = 0U;

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
    PortAction action;

    QString id;
    QString title;
    int dataTypeId;
    QVariantHash metadata;
    uint throttleItemsPerSec;

    InputPortChange() = default;
    explicit InputPortChange(PortAction pa)
        : action(pa),
          dataTypeId(-1),
          throttleItemsPerSec(0)
    {
    }

    QByteArray toBytes() const
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
static iox::capro::IdString_t IN_PORT_CHANGE_CHANNEL_ID = "InPortChange";

/**
 * @brief Information about an output port change
 */
struct OutputPortChange {
    PortAction action;

    QString id;
    QString title;
    int dataTypeId;
    QVariantHash metadata;

    OutputPortChange() = default;
    explicit OutputPortChange(PortAction pa)
        : action(pa),
          dataTypeId(-1)
    {
    }

    QByteArray toBytes()
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);

        stream << action << id << title << dataTypeId << metadata;

        return bytes;
    }

    static OutputPortChange fromMemory(const void *memory, size_t size)
    {
        OutputPortChange info;

        QByteArray block(reinterpret_cast<const char *>(memory), size);
        QDataStream stream(block);

        stream >> info.action >> info.id >> info.title >> info.dataTypeId >> info.metadata;

        return info;
    }

    friend QDataStream &operator<<(QDataStream &out, const OutputPortChange &info)
    {
        out << info.action << info.id << info.title << info.dataTypeId << info.metadata;
        return out;
    }

    friend QDataStream &operator>>(QDataStream &in, OutputPortChange &info)
    {
        in >> info.action >> info.id >> info.title >> info.dataTypeId >> info.metadata;
        return in;
    }
};
static iox::capro::IdString_t OUT_PORT_CHANGE_CHANNEL_ID = "OutPortChange";

/**
 * @brief request to update the metadata of an input port
 */
struct UpdateInputPortMetadataRequest {

    QString id;
    QVariantHash metadata;

    UpdateInputPortMetadataRequest() = default;

    QByteArray toBytes() const
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
static iox::capro::IdString_t IN_PORT_UPDATE_METADATA_ID = "UpdateInputPortMetadata";

/**
 * Generic response to a request.
 */
struct DoneResponse {
    bool success;
};

/**
 * Event indicating an error
 */
struct ErrorEvent {
    iox::cxx::string<128> title;
    iox::cxx::string<2048> message;
};
static iox::capro::IdString_t ERROR_CHANNEL_ID = "Error";

/**
 * Module state change event
 */
struct StateChangeEvent {
    ModuleState state;
};
static iox::capro::IdString_t STATE_CHANNEL_ID = "State";

/**
 * Event sending a status message to master.
 */
struct StatusMessageEvent {
    iox::cxx::string<512> text;
};
static iox::capro::IdString_t STATUS_MESSAGE_CHANNEL_ID = "StatusMessage";

/**
 * Request to set the niceness of a worker
 */
struct SetNicenessRequest {
    int nice;
};
static iox::capro::IdString_t SET_NICENESS_CALL_ID = "SetNiceness";

/**
 * Request to set the maximum realtime priority of a worker
 */
struct SetMaxRealtimePriority {
    int priority;
};
static iox::capro::IdString_t SET_MAX_RT_PRIORITY_CALL_ID = "SetMaxRealtimePriority";

/**
 * Request to set the CPU affinity of a worker
 */
struct SetCPUAffinityRequest {
    iox::cxx::vector<uint, 256> cores;
};
static iox::capro::IdString_t SET_CPU_AFFINITY_CALL_ID = "SetCPUAffinity";

/**
 * Request to delete an input or output port
 */
struct DeletePortRequest {
    int portId;
};
static iox::capro::IdString_t DELETE_PORT_CALL_ID = "DeletePort";

/**
 * Connect the input port of a linked module to an exported output
 */
struct ConnectInputRequest {
    iox::capro::IdString_t portId;
    iox::capro::IdString_t instanceId;
    iox::capro::IdString_t channelId;
};
static iox::capro::IdString_t CONNECT_INPUT_CALL_ID = "ConnectInputPort";

/**
 * Instruct the module to load a script
 */
struct LoadScriptRequest {
    QString workingDir;
    QString venvDir;
    QString script;

    QByteArray toBytes() const
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
static iox::capro::IdString_t LOAD_SCRIPT_CALL_ID = "LoadScript";

struct SetPortsPresetRequest {
    QList<InputPortChange> inPorts;
    QList<OutputPortChange> outPorts;

    QByteArray toBytes() const
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
static iox::capro::IdString_t SET_PORTS_PRESET_CALL_ID = "SetPortsPresetRequest";

/**
 * Request to prepare the module for starting,
 * this enters the PREPARING stage
 */
struct PrepareStartRequest {
    QByteArray settings;

    QByteArray toBytes() const
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
static iox::capro::IdString_t PREPARE_START_CALL_ID = "PrepareStart";

/**
 * Start module run, this enters the RUNNING stage
 */
struct StartRequest {
    int64_t startTimestampUsec;
};
static iox::capro::IdString_t START_CALL_ID = "Start";

/**
 * Stop module run, this enters the IDLE stage
 */
struct StopRequest {
};
static iox::capro::IdString_t STOP_CALL_ID = "Stop";

/**
 * Request to shutdown the module process cleanly
 */
struct ShutdownRequest {
};
static iox::capro::IdString_t SHUTDOWN_CALL_ID = "Shutdown";

/**
 * Event from the module to indicate a settings change. Syntalos will store the new settings.
 */
struct SettingsChangeEvent {
    QByteArray settings;

    SettingsChangeEvent() = default;
    explicit SettingsChangeEvent(QByteArray settings)
        : settings(std::move(settings))
    {
    }

    QByteArray toBytes() const
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
static iox::capro::IdString_t SETTINGS_CHANGE_CHANNEL_ID = "SettingsChange";

/**
 * Request to change show the GUI dialog to change settings.
 */
struct ShowSettingsRequest {
    QByteArray settings;

    QByteArray toBytes() const
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
static iox::capro::IdString_t SHOW_SETTINGS_CALL_ID = "ShowSettings";

/**
 * Request to show the display window(s) of the module.
 */
struct ShowDisplayRequest {
};
static iox::capro::IdString_t SHOW_DISPLAY_CALL_ID = "ShowDisplay";

} // namespace Syntalos

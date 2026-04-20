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
#include <datactl/syclock.h>
#include <datactl/datatypes.h>
#include <datactl/streammeta.h>

namespace Syntalos
{

class SyntalosLink;
struct OutputPortChangeRequest;
struct InputPortChangeRequest;

using LoadScriptFn = std::function<void(const std::string &script, const std::string &wdir)>;
using SaveSettingsFn = std::function<bool(const std::string &baseDir, ByteVector &settings)>;
using LoadSettingsFn = std::function<bool(const std::string &baseDir, const ByteVector &settings)>;
using PrepareRunFn = std::function<void()>;
using StartFn = std::function<void()>;
using StopFn = std::function<void()>;
using ShutdownFn = std::function<void()>;
using NewDataRawFn = std::function<void(const void *data, size_t size)>;

using ShowSettingsFn = std::function<void(void)>;
using ShowDisplayFn = std::function<void(void)>;

/**
 * @brief Reference for an input port
 */
class InputPortInfo
{
public:
    std::string id() const;
    int dataTypeId() const;
    std::string title() const;
    MetaStringMap metadata() const;

    /**
     * @brief Sets a function to be called when new data arrives
     *
     * The data memory block passed to this function is only valid during the call.
     */
    void setNewDataRawCallback(NewDataRawFn callback);
    void setThrottleItemsPerSec(uint itemsPerSec);

    /**
     * @brief Retrieves the metadata value associated with a given key.
     *
     * @param key The key to look up in the metadata.
     * @return An optional MetaValue containing the value associated with the key,
     *         or std::nullopt if the key does not exist.
     */
    [[nodiscard]] std::optional<MetaValue> metadataValue(const std::string &key) const;
    [[nodiscard]] MetaValue metadataValueOr(const std::string &key, const MetaValue &defaultVal) const;

    template<typename T>
    [[nodiscard]] T metadataValueOr(const std::string &key, T fallback) const
    {
        return metadata().valueOr<T>(key, std::move(fallback));
    }

private:
    friend SyntalosLink;
    explicit InputPortInfo(const InputPortChangeRequest &pc);

private:
    class Private;
    Q_DISABLE_COPY(InputPortInfo)
    std::unique_ptr<Private> d;
};

/**
 * @brief Reference for an output port
 */
class OutputPortInfo
{
public:
    std::string id() const;
    int dataTypeId() const;
    void setMetadataVar(const std::string &key, const MetaValue &value);

private:
    friend SyntalosLink;
    explicit OutputPortInfo(const OutputPortChangeRequest &pc);

private:
    class Private;
    Q_DISABLE_COPY(OutputPortInfo)
    std::unique_ptr<Private> d;
};

/**
 * @brief Convert raw received bytes into their data type
 */
template<typename T>
    requires std::derived_from<T, BaseDataType>
T streamDataFromRawMemory(const void *data, size_t size)
{
    return T::fromMemory(data, size);
}

/**
 * @brief Information about the current test subject.
 */
struct TestSubjectInfo {
    std::string id;
    std::string group;
};

/**
 * @brief Connection to a Syntalos instance
 */
class SyntalosLink
{

public:
    ~SyntalosLink();
    SyntalosLink(const SyntalosLink &) = delete;
    SyntalosLink &operator=(const SyntalosLink &) = delete;

    [[nodiscard]] std::string instanceId() const;

    void raiseError(const std::string &message);
    void raiseError(const std::string &title, const std::string &message);

    void setLoadScriptCallback(LoadScriptFn callback);
    void setSaveSettingsCallback(SaveSettingsFn callback);
    void setLoadSettingsCallback(LoadSettingsFn callback);
    void setPrepareRunCallback(PrepareRunFn callback);
    void setStartCallback(StartFn callback);
    void setStopCallback(StopFn callback);
    void setShutdownCallback(ShutdownFn callback);

    void setShowSettingsCallback(ShowSettingsFn callback);
    void setShowDisplayCallback(ShowDisplayFn callback);

    [[nodiscard]] SyncTimer *timer() const;

    const TestSubjectInfo &testSubject() const;

    /**
     * Wait for incoming messages for the given amount of microseconds, then return.
     *
     * When messages arrive, the respective callbacks are called.
     *
     * @param timeoutUsec Timeout after which we return, or -1 to loop forever.
     * @param eventFn Callback to an external even processing function to be called occasionally.
     */
    void awaitData(int timeoutUsec = -1, const std::function<void()> &eventFn = nullptr);

    /**
     * Wait for incoming messages forever, calling callbacks in the current thread when they arrive.
     *
     * If an event function is provided, it is called at the respective interval while we loop.
     *
     * @param eventFn The event processing function to call.
     * @param intervalUsec The interval at which the function should be called.
     */
    void awaitDataForever(const std::function<void()> &eventFn = nullptr, int intervalUsec = 125 * 1000);

    [[nodiscard]] ModuleState state() const;
    void setState(ModuleState state);

    /**
     * @brief Returns true if a shutdown (via IPC or SIGTERM/SIGINT) has been requested.
     *
     * Callers with their own spin loops should check this flag and exit promptly.
     */
    [[nodiscard]] bool isShutdownPending() const;

    void setStatusMessage(const std::string &message);

    int maxRealtimePriority() const;

    [[nodiscard]] std::vector<std::shared_ptr<InputPortInfo>> inputPorts() const;
    [[nodiscard]] std::vector<std::shared_ptr<OutputPortInfo>> outputPorts() const;

    std::shared_ptr<InputPortInfo> registerInputPort(
        const std::string &id,
        const std::string &title,
        const std::string &dataTypeName);
    std::shared_ptr<OutputPortInfo> registerOutputPort(
        const std::string &id,
        const std::string &title,
        const std::string &dataTypeName,
        const MetaStringMap &metadata = {});

    void updateInputPort(const std::shared_ptr<InputPortInfo> &iport);
    void updateOutputPort(const std::shared_ptr<OutputPortInfo> &oport);

    void removeInputPort(const std::shared_ptr<InputPortInfo> &iport);
    void removeOutputPort(const std::shared_ptr<OutputPortInfo> &oport);

    /**
     * @brief Drop all registered ports.
     *
     * This tears down every IPC publisher/subscriber and clears the port lists,
     * so the next round of registerInputPort() / registerOutputPort() calls starts
     * from a clean slate.
     */
    void resetPorts();

    bool submitOutput(const std::shared_ptr<OutputPortInfo> &oport, const BaseDataType &data);

private:
    explicit SyntalosLink(const std::string &instanceId);
    friend std::unique_ptr<SyntalosLink> initSyntalosModuleLink();

private:
    class Private;
    std::unique_ptr<Private> d;

    void processPendingControl();
};

std::unique_ptr<SyntalosLink> initSyntalosModuleLink();

} // namespace Syntalos

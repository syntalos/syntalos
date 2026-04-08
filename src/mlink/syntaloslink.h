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
#include <QVariantHash>
#include <datactl/syclock.h>
#include <datactl/datatypes.h>

namespace Syntalos
{

class SyntalosLink;
struct OutputPortChange;
struct InputPortChange;

using LoadScriptFn = std::function<void(const QString &script, const QString &wdir)>;
using PrepareStartFn = std::function<void(const ByteVector &settings)>;
using StartFn = std::function<void()>;
using StopFn = std::function<void()>;
using ShutdownFn = std::function<void()>;
using NewDataRawFn = std::function<void(const void *data, size_t size)>;

using ShowSettingsFn = std::function<void(const ByteVector &settings)>;
using ShowDisplayFn = std::function<void(void)>;

/**
 * @brief Reference for an input port
 */
class InputPortInfo
{
public:
    QString id() const;
    int dataTypeId() const;
    QString title() const;
    QVariantHash metadata() const;

    /**
     * @brief Sets a function to be called when new data arrives
     *
     * The data memory block passed to this function is only valid during the call.
     */
    void setNewDataRawCallback(NewDataRawFn callback);
    void setThrottleItemsPerSec(uint itemsPerSec);

private:
    friend SyntalosLink;
    explicit InputPortInfo(const InputPortChange &pc);

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
    QString id() const;
    int dataTypeId() const;
    void setMetadataVar(const QString &key, const QVariant &value);

private:
    friend SyntalosLink;
    explicit OutputPortInfo(const OutputPortChange &pc);

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
 * @brief Connection to a Syntalos instance
 */
class SyntalosLink : public QObject
{
    Q_GADGET
public:
    ~SyntalosLink() override;

    [[nodiscard]] QString instanceId() const;

    void raiseError(const QString &message);
    void raiseError(const QString &title, const QString &message);

    void setLoadScriptCallback(LoadScriptFn callback);
    void setPrepareStartCallback(PrepareStartFn callback);
    void setStartCallback(StartFn callback);
    void setStopCallback(StopFn callback);
    void setShutdownCallback(ShutdownFn callback);

    [[nodiscard]] SyncTimer *timer() const;

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

    void setStatusMessage(const QString &message);

    int maxRealtimePriority() const;

    void setSettingsData(const ByteVector &data);
    void setShowSettingsCallback(ShowSettingsFn callback);
    void setShowDisplayCallback(ShowDisplayFn callback);

    [[nodiscard]] std::vector<std::shared_ptr<InputPortInfo>> inputPorts() const;
    [[nodiscard]] std::vector<std::shared_ptr<OutputPortInfo>> outputPorts() const;

    std::shared_ptr<InputPortInfo> registerInputPort(
        const QString &id,
        const QString &title,
        const QString &dataTypeName);
    std::shared_ptr<OutputPortInfo> registerOutputPort(
        const QString &id,
        const QString &title,
        const QString &dataTypeName,
        const QVariantHash &metadata = QVariantHash());

    void updateOutputPort(const std::shared_ptr<OutputPortInfo> &oport);
    void updateInputPort(const std::shared_ptr<InputPortInfo> &iport);

    bool submitOutput(const std::shared_ptr<OutputPortInfo> &oport, const BaseDataType &data);

private:
    explicit SyntalosLink(const QString &instanceId, QObject *parent = nullptr);
    friend std::unique_ptr<SyntalosLink> initSyntalosModuleLink();

private:
    class Private;
    Q_DISABLE_COPY(SyntalosLink)
    std::unique_ptr<Private> d;

    void processPendingControl();
};

std::unique_ptr<SyntalosLink> initSyntalosModuleLink();

} // namespace Syntalos

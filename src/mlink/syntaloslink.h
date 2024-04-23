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
#include <datactl/syclock.h>
#include <datactl/datatypes.h>

namespace iox::popo
{
class NotificationInfo;
}

namespace Syntalos
{

class SyntalosLink;
struct OutputPortChange;
struct InputPortChange;

using LoadScriptFn = std::function<void(const QString &script, const QString &wdir)>;
using PrepareStartFn = std::function<void(const QByteArray &settings)>;
using StartFn = std::function<void()>;
using StopFn = std::function<void()>;
using ShutdownFn = std::function<void()>;
using NewDataRawFn = std::function<void(const void *data, size_t size)>;

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
    QScopedPointer<Private> d;
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
    QScopedPointer<Private> d;
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
    ~SyntalosLink();

    void raiseError(const QString &message);
    void raiseError(const QString &title, const QString &message);

    void setLoadScriptCallback(LoadScriptFn callback);
    void setPrepareStartCallback(PrepareStartFn callback);
    void setStartCallback(StartFn callback);
    void setStopCallback(StopFn callback);
    void setShutdownCallback(ShutdownFn callback);

    SyncTimer *timer() const;

    void awaitData(int timeoutUsec = -1);
    void awaitDataForever();

    ModuleState state() const;
    void setState(ModuleState state);

    void setStatusMessage(const QString &message);

    int maxRealtimePriority() const;

    std::vector<std::shared_ptr<InputPortInfo>> inputPorts() const;
    std::vector<std::shared_ptr<OutputPortInfo>> outputPorts() const;

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
    QScopedPointer<Private> d;

    void processNotification(const iox::popo::NotificationInfo *notification);
};

std::unique_ptr<SyntalosLink> initSyntalosModuleLink();

} // namespace Syntalos

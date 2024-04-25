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
#include <syntalos-datactl>

#include "syntaloslink.h"

namespace Syntalos
{

template<typename T>
    requires std::is_base_of_v<BaseDataType, T>
class OutputPortLink;

/**
 * @brief Convenience interface to write an OOP Syntalos module
 */
class SyntalosLinkModule : public QObject
{
    Q_GADGET
public:
    explicit SyntalosLinkModule(SyntalosLink *slink);
    ~SyntalosLinkModule();

    void raiseError(const QString &message);
    void raiseError(const QString &title, const QString &message);

    void awaitData(int timeoutUsec = -1);

    ModuleState state() const;
    void setState(ModuleState state);

    void setStatusMessage(const QString &message);

    virtual bool prepare(const QByteArray &settings);
    virtual void start();
    virtual void stop();

protected:
    SyncTimer *timer() const;

    /**
     * @brief Register an output port for this module
     *
     * This function should be called in the module's constructor to publish the intent
     * to produce an output stream of type T. Other modules may subscribe to this stream.
     *
     * @returns A reference to the output port, which can be used to submit new data
     */
    template<typename T>
        requires std::is_base_of_v<BaseDataType, T>
    std::shared_ptr<OutputPortLink<T>> registerOutputPort(
        const QString &id,
        const QString &title = QString(),
        const QVariantHash &metadata = QVariantHash())
    {
        // fetch existing output port first, if any exists
        for (auto &eop : m_slink->outputPorts()) {
            if (eop->id() == id) {
                return std::shared_ptr<OutputPortLink<T>>(new OutputPortLink<T>(this, eop));
            }
        }

        // register a new port if we found none
        auto opInfo = m_slink->registerOutputPort(id, title, BaseDataType::typeIdToString(syDataTypeId<T>()), metadata);
        if (!opInfo) {
            qWarning().noquote() << "Failed to register output port with ID:" << id;
            return nullptr;
        }

        auto oport = std::shared_ptr<OutputPortLink<T>>(new OutputPortLink<T>(this, opInfo));
        return oport;
    }

    /**
     * @brief Register an input port for this module
     *
     * This function should be called in the module's constructor to publish the intent
     * to accept input stream subscriptions of type T. The user may subscribe this module
     * to other modules which produce the data it accepts.
     *
     * In order to receive data, a callback function to be called when new data is available
     * must also be provided.
     */
    template<typename T, typename U>
        requires std::is_base_of_v<BaseDataType, T>
    std::shared_ptr<InputPortInfo> registerInputPort(
        const QString &id,
        const QString &title,
        U *instance,
        void (U::*fn)(const T &data))
    {
        // fetch existing output port first, if any exists
        for (auto &eip : m_slink->inputPorts()) {
            if (eip->id() == id)
                return eip;
        }

        auto iport = m_slink->registerInputPort(id, title, BaseDataType::typeIdToString(syDataTypeId<T>()));
        if (!iport) {
            qWarning().noquote() << "Failed to register input port" << id;
            return nullptr;
        }

        iport->setNewDataRawCallback([instance, fn](const void *data, size_t size) {
            std::invoke(fn, instance, T::fromMemory(data, size));
        });

        return iport;
    }

    bool m_running;

private:
    template<typename T>
        requires std::is_base_of_v<BaseDataType, T>
    friend class OutputPortLink;

    class Private;
    Q_DISABLE_COPY(SyntalosLinkModule)
    QScopedPointer<Private> d;
    SyntalosLink *m_slink;
};

/**
 * @brief Reference for an output port to emit new data
 */
template<typename T>
    requires std::is_base_of_v<BaseDataType, T>
class OutputPortLink
{
public:
    QString id() const
    {
        return m_info->id();
    }

    int dataTypeId() const
    {
        return m_info->dataTypeId();
    }

    void setMetadataVar(const QString &key, const QVariant &value)
    {
        m_info->setMetadataVar(key, value);
        m_mod->m_slink->updateOutputPort(m_info);
    }

    bool submit(const T &data)
    {
        return m_mod->m_slink->submitOutput(m_info, data);
    }

private:
    friend SyntalosLinkModule;
    explicit OutputPortLink(SyntalosLinkModule *mod, std::shared_ptr<OutputPortInfo> pinfo)
    {
        m_mod = mod;
        m_info = std::move(pinfo);
    }

private:
    Q_DISABLE_COPY(OutputPortLink)
    std::shared_ptr<OutputPortInfo> m_info;
    SyntalosLinkModule *m_mod;
};

} // namespace Syntalos

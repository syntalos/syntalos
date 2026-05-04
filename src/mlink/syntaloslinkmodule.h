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

#include <iostream>
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
class SyntalosLinkModule
{
public:
    explicit SyntalosLinkModule(SyntalosLink *slink);
    virtual ~SyntalosLinkModule();

    SyntalosLinkModule(const SyntalosLinkModule &) = delete;
    SyntalosLinkModule &operator=(const SyntalosLinkModule &) = delete;

    void raiseError(const std::string &message);
    void raiseError(const std::string &title, const std::string &message);

    void awaitData(int timeoutUsec = -1);

    ModuleState state() const;
    void setState(ModuleState state);

    void setStatusMessage(const std::string &message);

    virtual bool prepare();
    virtual void start();
    virtual void stop();

    /**
     * Called when Syntalos wants this module to quit.
     */
    virtual void shutdown();

protected:
    /**
     * Convenience helper for std::expected
     */
    template<typename T>
    T unwrapOrAbort(std::expected<T, std::string> result, std::string_view context = {})
    {
        if (!result) {
            if (context.empty())
                std::cerr << "Fatal: " << context << ": " << result.error() << std::endl;
            else
                std::cerr << "Fatal: " << result.error() << std::endl;
            std::abort();
        }
        return std::move(*result);
    }

    /**
     * The global experiment timer.
     */
    [[nodiscard]] SyncTimer *timer() const;

    /**
     * Information about the current test subject. Refreshed
     * before prepare() is called.
     */
    const TestSubjectInfo &testSubject() const;

    /**
     * The run info (EDL root group, UUID, etc.). Available after prepare() is called.
     */
    const RunInfo &runInfo() const;

    /**
     * @brief Create (or return cached) the default dataset for this module.
     *
     * Uses MUST_CREATE semantics; fails if the name is already taken in the assigned root group.
     * The result is cached: repeated calls with the same name return the same dataset.
     *
     * @param preferredName Dataset name; defaults to the module name if empty.
     */
    auto createDefaultDataset(const std::string &preferredName = {})
        -> std::expected<std::shared_ptr<EDLDataset>, std::string>;

    /**
     * @brief Create a new dataset in the given group.
     *
     * Uses MUST_CREATE semantics: fails if a dataset with that name already exists
     * in the group (both locally and on the master side).
     *
     * @param parent Group to create the dataset in.
     * @param name   Dataset name.
     */
    auto createDatasetInGroup(const std::shared_ptr<EDLGroup> &parent, const std::string &name)
        -> std::expected<std::shared_ptr<EDLDataset>, std::string>;

    /**
     * @brief Create (or open) a sub-group under the module's assigned root.
     *
     * Uses CREATE_OR_OPEN semantics: safe to call multiple times with the same name.
     *
     * @param name Group name.
     */
    auto createStorageGroup(const std::string &name) -> std::expected<std::shared_ptr<EDLGroup>, std::string>;

    /**
     * @brief Create (or open) a sub-group under an existing group.
     */
    auto createStorageGroup(const std::shared_ptr<EDLGroup> &parent, const std::string &name)
        -> std::expected<std::shared_ptr<EDLGroup>, std::string>;

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
    auto registerOutputPort(const std::string &id, const std::string &title = {}, const MetaStringMap &metadata = {})
        -> std::expected<std::shared_ptr<OutputPortLink<T>>, std::string>
    {
        auto opInfo = m_slink->registerOutputPort(
            id, title, static_cast<BaseDataType::TypeId>(syDataTypeId<T>()), metadata);
        if (!opInfo.has_value())
            return std::unexpected(opInfo.error());

        auto oport = std::shared_ptr<OutputPortLink<T>>(new OutputPortLink<T>(this, *opInfo));
        return oport;
    }
    template<typename T>
        requires std::is_base_of_v<BaseDataType, T>
    auto registerOutputPortOrAbort(
        const std::string &id,
        const std::string &title = {},
        const MetaStringMap &metadata = {}) -> std::shared_ptr<OutputPortLink<T>>
    {
        return unwrapOrAbort(registerOutputPort<T>(id, title, metadata));
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
    auto registerInputPort(const std::string &id, const std::string &title, U *instance, void (U::*fn)(const T &data))
        -> std::expected<std::shared_ptr<InputPortInfo>, std::string>
    {
        auto res = m_slink->registerInputPort(id, title, static_cast<BaseDataType::TypeId>(syDataTypeId<T>()));
        if (!res.has_value())
            return res;

        auto iport = *res;
        iport->setNewDataRawCallback([instance, fn](const void *data, size_t size) {
            std::invoke(fn, instance, T::fromMemory(data, size));
        });

        return iport;
    }
    template<typename T, typename U>
        requires std::is_base_of_v<BaseDataType, T>
    auto registerInputPortOrAbort(
        const std::string &id,
        const std::string &title,
        U *instance,
        void (U::*fn)(const T &data)) -> std::shared_ptr<InputPortInfo>
    {
        return unwrapOrAbort(registerInputPort<T, U>(id, title, instance, fn));
    }

    /**
     * Called when settings should be saved.
     *
     * @param settings The settings data.
     * @param baseDir Base directory where the settings file will be stored.
     */
    virtual void saveSettings(ByteVector &settings, const fs::path &baseDir);

    /**
     * Called when settings should be loaded.
     *
     * @param settings The settings data.
     * @param baseDir Base directory from where the settings were loaded.
     * @return Return true if loading was successful, false on error.
     */
    virtual bool loadSettings(const ByteVector &settings, const fs::path &baseDir);

protected:
    bool m_running;

private:
    template<typename T>
        requires std::is_base_of_v<BaseDataType, T>
    friend class OutputPortLink;

    class Private;
    std::unique_ptr<Private> d;
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
    OutputPortLink(const OutputPortLink &) = delete;
    OutputPortLink &operator=(const OutputPortLink &) = delete;

    std::string id() const
    {
        return m_info->id();
    }

    int dataTypeId() const
    {
        return m_info->dataTypeId();
    }

    void setMetadataVar(const std::string &key, const MetaValue &value)
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
    std::shared_ptr<OutputPortInfo> m_info;
    SyntalosLinkModule *m_mod;
};

} // namespace Syntalos

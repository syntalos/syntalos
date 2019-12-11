/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <thread>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <optional>
#include <QVariant>

#include "readerwriterqueue.h"
#include "datatypes.h"

using namespace moodycamel;

class VariantStreamSubscription
{
public:
    virtual ~VariantStreamSubscription();
    virtual QVariant nextVar() = 0;
    virtual QString dataTypeName() const = 0;
};

class VariantDataStream
{
public:
    virtual ~VariantDataStream();
    virtual std::shared_ptr<VariantStreamSubscription> subscribeVar() = 0;
    virtual void terminate() = 0;
    virtual QString dataTypeName() const = 0;
};

template<typename T>
class DataStream;

template<typename T>
class StreamSubscription : public VariantStreamSubscription
{
    friend DataStream<T>;
public:
    StreamSubscription(const std::string& name)
        : m_queue(BlockingReaderWriterQueue<std::optional<T>>(256))
    {
        m_name = name;
        m_terminated = false;
    }

    std::optional<T> next()
    {
        if (m_terminated && m_queue.peek() == nullptr)
            return std::nullopt;
        std::optional<T> data;
        m_queue.wait_dequeue(data);
        return data;
    }

    QVariant nextVar() override
    {
        if (m_terminated && m_queue.peek() == nullptr)
            return QVariant();
        std::optional<T> data;
        m_queue.wait_dequeue(data);
        if (!data.has_value())
            return QVariant();
        return QVariant::fromValue(data.value());
    }

    QString dataTypeName() const override
    {
        return QMetaType::typeName(qMetaTypeId<T>());
    }

    auto name() const
    {
        return m_name;
    }

private:
    BlockingReaderWriterQueue<std::optional<T>> m_queue;
    std::atomic_bool m_terminated;

    std::string m_name;

    void push(const T &data)
    {
        m_queue.enqueue(std::optional<T>(data));
    }

    void terminate()
    {
        m_terminated = true;
        m_queue.enqueue(std::nullopt);
    }
};


template<typename T>
class DataStream : public VariantDataStream
{
public:
    DataStream()
        : m_allowSubscribe(true)
    {
        m_ownerId = std::this_thread::get_id();
    }

    ~DataStream() override
    {
        terminate();
    }

    QString dataTypeName() const override
    {
        return QMetaType::typeName(qMetaTypeId<T>());
    }

    std::shared_ptr<StreamSubscription<T>> subscribe(const std::string& name)
    {
        //if (!m_allowSubscribe)
        //    return nullptr;
        std::lock_guard<std::mutex> lock(m_mutex);
        std::shared_ptr<StreamSubscription<T>> sub(new StreamSubscription<T> (name));
        m_subs.push_back(sub);
        return sub;
    }

    std::shared_ptr<VariantStreamSubscription> subscribeVar() override
    {
        return subscribe(nullptr);
    }

    void push(const T &data)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_allowSubscribe = false;
        for(auto& sub: m_subs)
            sub->push(data);
    }

    void terminate() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::for_each(m_subs.begin(), m_subs.end(), [](std::shared_ptr<StreamSubscription<T>> sub){ sub->terminate(); });
        m_subs.clear();
        m_allowSubscribe = true;
    }

private:
    std::thread::id m_ownerId;
    std::atomic_bool m_allowSubscribe;
    std::mutex m_mutex;
    std::vector<std::shared_ptr<StreamSubscription<T>>> m_subs;
};

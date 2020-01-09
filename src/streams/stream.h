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
#include <QDebug>

#include "readerwriterqueue.h"
#include "datatypes.h"

using namespace moodycamel;

class VariantStreamSubscription
{
public:
    virtual ~VariantStreamSubscription();
    virtual QString dataTypeName() const = 0;
    virtual QVariant nextVar() = 0;
    virtual QHash<QString, QVariant> metadata() const = 0;
    virtual bool active() const = 0;
};

class VariantDataStream
{
public:
    virtual ~VariantDataStream();
    virtual QString dataTypeName() const = 0;
    virtual std::shared_ptr<VariantStreamSubscription> subscribeVar() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool active() const = 0;
    virtual QHash<QString, QVariant> metadata() = 0;
};

template<typename T>
class DataStream;

template<typename T>
class StreamSubscription : public VariantStreamSubscription
{
    friend DataStream<T>;
public:
    StreamSubscription()
        : m_queue(BlockingReaderWriterQueue<std::optional<T>>(256)),
          m_active(true)
    {
    }

    std::optional<T> next()
    {
        if (!m_active && m_queue.peek() == nullptr)
            return std::nullopt;
        std::optional<T> data;
        m_queue.wait_dequeue(data);
        return data;
    }

    QVariant nextVar() override
    {
        if (!m_active && m_queue.peek() == nullptr)
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

    QHash<QString, QVariant> metadata() const override
    {
        return m_metadata;
    }

    bool active() const override
    {
        return m_active;
    }

private:
    BlockingReaderWriterQueue<std::optional<T>> m_queue;
    std::atomic_bool m_active;
    QHash<QString, QVariant> m_metadata;

    void setMetadata(const QHash<QString, QVariant> &metadata)
    {
        m_metadata = metadata;
    }

    void push(const T &data)
    {
        m_queue.enqueue(std::optional<T>(data));
    }

    void terminate()
    {
        m_active = false;
        m_queue.enqueue(std::nullopt);
    }
};


template<typename T>
class DataStream : public VariantDataStream
{
public:
    DataStream()
        : m_active(false)
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

    QHash<QString, QVariant> metadata() override
    {
        return m_metadata;
    }

    void setMetadataVal(const QString &key, const QVariant &value)
    {
        m_metadata[key] = value;
    }

    std::shared_ptr<StreamSubscription<T>> subscribe()
    {
        // we don't permit subscriptions to an active stream
        assert(!m_active);
        if (m_active)
            return nullptr;
        std::lock_guard<std::mutex> lock(m_mutex);
        std::shared_ptr<StreamSubscription<T>> sub(new StreamSubscription<T>());
        m_subs.push_back(sub);
        return sub;
    }

    std::shared_ptr<VariantStreamSubscription> subscribeVar() override
    {
        return subscribe();
    }

    void start() override
    {
        m_ownerId = std::this_thread::get_id();
        for(auto& sub: m_subs)
            sub->setMetadata(m_metadata);
        m_active = true;
    }

    void stop() override
    {
        std::for_each(m_subs.begin(), m_subs.end(), [](std::shared_ptr<StreamSubscription<T>> sub){ sub->terminate(); });
        m_active = false;
    }

    void push(const T &data)
    {
        if (!m_active)
            return;
        for(auto& sub: m_subs)
            sub->push(data);
    }

    void terminate()
    {
        stop();
        m_subs.clear();
    }

    bool active() const override
    {
        return m_active;
    }

private:
    std::thread::id m_ownerId;
    std::atomic_bool m_active;
    std::mutex m_mutex;
    std::vector<std::shared_ptr<StreamSubscription<T>>> m_subs;
    QHash<QString, QVariant> m_metadata;
};


#pragma once

#include <boost/circular_buffer.hpp>
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>
#include <queue>
#include <condition_variable>

#include "readerwriterqueue.h"

using namespace moodycamel;

template<typename T>
class MKLFQStream;

template<typename T>
class MKLFQStreamSubscription
{
    friend MKLFQStream<T>;
public:
    MKLFQStreamSubscription(const std::string& name)
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
class MKLFQStream
{
public:
    MKLFQStream()
        : m_allowSubscribe(true)
    {
        m_ownerId = std::this_thread::get_id();
    }

    ~MKLFQStream()
    {
        terminate();
    }

    std::shared_ptr<MKLFQStreamSubscription<T>> subscribe(const std::string& name)
    {
        //if (!m_allowSubscribe)
        //    return nullptr;
        std::lock_guard<std::mutex> lock(m_mutex);
        std::shared_ptr<MKLFQStreamSubscription<T>> sub(new MKLFQStreamSubscription<T> (name));
        m_subs.push_back(sub);
        return sub;
    }

    void push(const T &data)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_allowSubscribe = false;
        for(auto& sub: m_subs)
            sub->push(data);
    }

    void terminate()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::for_each(m_subs.begin(), m_subs.end(), [](std::shared_ptr<MKLFQStreamSubscription<T>> sub){ sub->terminate(); });
        m_subs.clear();
        m_allowSubscribe = true;
    }

private:
    std::thread::id m_ownerId;
    std::atomic_bool m_allowSubscribe;
    std::mutex m_mutex;
    std::vector<std::shared_ptr<MKLFQStreamSubscription<T>>> m_subs;
};

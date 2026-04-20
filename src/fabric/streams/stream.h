/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QDebug>
#include <QVariant>
#include <algorithm>
#include <functional>
#include <atomic>
#include <cmath>
#include <mutex>
#include <optional>
#include <sys/eventfd.h>
#include <thread>
#include <unistd.h>

#include "datactl/datatypes.h"
#include "datactl/streammeta.h"
#include "readerwriterqueue.h"
#include "datactl/syclock.h"

using namespace moodycamel;
using namespace Syntalos;

enum class CommonMetadataKey {
    SrcModType,
    SrcModName,
    SrcModPortTitle,
    DataNameProposal
};

inline uint qHash(CommonMetadataKey key, uint seed)
{
    return ::qHash(static_cast<uint>(key), seed);
}

// clang-format off
typedef QHash<CommonMetadataKey, std::string> CommonMetadataKeyMap;
Q_GLOBAL_STATIC_WITH_ARGS(CommonMetadataKeyMap,
      _commonMetadataKeyMap,
      ({
          {CommonMetadataKey::SrcModType, "src_mod_type"},
          {CommonMetadataKey::SrcModName, "src_mod_name"},
          {CommonMetadataKey::SrcModPortTitle, "src_mod_port_title"},
          {CommonMetadataKey::DataNameProposal, "data_name_proposal"},
      })
);
// clang-format on

/**
 * @brief A function that can be used to process a variant value
 */
using ProcessVarFn = std::function<void(BaseDataType &)>;

class VariantStreamSubscription
{
public:
    virtual ~VariantStreamSubscription();
    virtual int dataTypeId() const = 0;
    virtual QString dataTypeName() const = 0;
    virtual bool callIfNextVar(const ProcessVarFn &fn) = 0;
    virtual bool unsubscribe() = 0;
    virtual bool isActive() const = 0;
    virtual bool hasPending() const = 0;
    virtual size_t approxPendingCount() const = 0;
    virtual int enableNotify() = 0;
    virtual void disableNotify() = 0;
    virtual void setThrottleItemsPerSec(uint itemsPerSec, bool allowMore = true) = 0;

    virtual void suspend() = 0;
    virtual void resume() = 0;
    virtual void clearPending() = 0;

    virtual MetaStringMap metadata() const = 0;
    virtual MetaValue metadataValue(const QString &key, const MetaValue &defaultValue = nullptr) const = 0;
    virtual MetaValue metadataValue(CommonMetadataKey key, const MetaValue &defaultValue = nullptr) const = 0;

    // used internally by Syntalos
    virtual void forcePushNullopt() = 0;
};

class VariantDataStream
{
public:
    virtual ~VariantDataStream();
    [[nodiscard]] virtual QString dataTypeName() const = 0;
    [[nodiscard]] virtual int dataTypeId() const = 0;
    virtual std::shared_ptr<VariantStreamSubscription> subscribeVar() = 0;
    virtual bool commitMetadata() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual bool isActive() const = 0;
    [[nodiscard]] virtual bool hasSubscribers() const = 0;
    [[nodiscard]] virtual size_t subscriberCount() const = 0;
    virtual void pushRawData(int typeId, const void *data, size_t size) = 0;
    virtual MetaStringMap metadata() = 0;
    virtual void setMetadata(const MetaStringMap &metadata) = 0;
    virtual void setMetadataValue(const std::string &key, const MetaValue &value) = 0;
    virtual void setCommonMetadata(const QString &srcModType, const QString &srcModName, const QString &portTitle) = 0;

    /**
     * Set if this stream will not be used. A dormant stream can not receive any data.
     * @param dormant True if dormant.
     */
    virtual void setDormant(bool dormant) = 0;

    /**
     * Check if this stream is dormant (either by having no subscribers, or by
     * being set to dormant explicitly).
     * @return True if stream is dormant.
     */
    virtual bool isDormant() const = 0;
};

template<typename T>
class DataStream;

template<typename T>
class StreamSubscription : public VariantStreamSubscription
{
    friend DataStream<T>;

public:
    explicit StreamSubscription(DataStream<T> *stream)
        : m_stream(stream),
          m_queue(BlockingReaderWriterQueue<std::optional<T>>(256)),
          m_eventfd(-1),
          m_notify(false),
          m_active(true),
          m_suspended(false),
          m_throttle(0),
          m_skippedElements(0)
    {
        m_lastItemTime = currentTimePoint();
        m_eventfd = eventfd(0, EFD_NONBLOCK);
        if (m_eventfd < 0) {
            qFatal("Unable to obtain eventfd for new stream subscription: %s", std::strerror(errno));
            assert(0);
        }
    }

    ~StreamSubscription() override
    {
        m_active = false;
        unsubscribe();
        m_notify = false;
        close(m_eventfd);
    }

    /**
     * @brief Obtain next element from stream, block in case there is no new element
     * @return The obtained value, or std::nullopt in case the stream ended.
     */
    std::optional<T> next()
    {
        if (!m_active && m_queue.peek() == nullptr)
            return std::nullopt;
        std::optional<T> data;
        m_queue.wait_dequeue(data);
        return data;
    }

    /**
     * @brief Obtain the next stream element if there is any, otherwise return std::nullopt
     * This function behaves the same as next(), but does return immediately without blocking.
     * To see if the stream as ended, check the active() property on this subscription.
     */
    std::optional<T> peekNext()
    {
        if (!m_active && m_queue.peek() == nullptr)
            return std::nullopt;
        std::optional<T> data;

        if (!m_queue.try_dequeue(data))
            return std::nullopt;

        return data;
    }

    /**
     * @brief Call function on the next element, if there is any.
     * @param fn The function to call with the next element.
     * @return True if an element was processed, false if there was no element.
     */
    bool callIfNextVar(const ProcessVarFn &fn) override
    {
        auto res = peekNext();
        if (res.has_value()) {
            fn(res.value());
            return true;
        }
        return false;
    }

    int dataTypeId() const override
    {
        return syDataTypeId<T>();
    }

    QString dataTypeName() const override
    {
        return QString::fromStdString(BaseDataType::typeIdToString(syDataTypeId<T>()));
    }

    MetaStringMap metadata() const override
    {
        return m_metadata;
    }

    MetaValue metadataValue(const QString &key, const MetaValue &defaultValue = nullptr) const override
    {
        return m_metadata.valueOr(key.toStdString(), defaultValue);
    }

    MetaValue metadataValue(CommonMetadataKey key, const MetaValue &defaultValue = nullptr) const override
    {
        return m_metadata.valueOr(_commonMetadataKeyMap->value(key), defaultValue);
    }

    template<typename MT>
    [[nodiscard]] MT metadataValue(const QString &key, MT fallback) const
    {
        const auto v = m_metadata.value(key.toStdString());
        return v.has_value() ? v->template getOr<MT>(std::move(fallback)) : std::move(fallback);
    }

    template<typename MT>
    [[nodiscard]] MT metadataValue(CommonMetadataKey key, MT fallback) const
    {
        const auto v = m_metadata.value(_commonMetadataKeyMap->value(key));
        return v.has_value() ? v->template getOr<MT>(std::move(fallback)) : std::move(fallback);
    }

    bool unsubscribe() override
    {
        // check if we are already unsubscribed
        if (m_stream == nullptr)
            return true;

        if (m_stream->unsubscribe(this)) {
            m_stream = nullptr;
            return true;
        }
        return false;
    }

    bool isActive() const override
    {
        return m_active;
    }

    bool isSuspended() const
    {
        return m_suspended;
    }

    /**
     * @brief Enable notifiucations on this stream subscription
     * @return An eventfd file descriptor that is written to when new data is received.
     */
    int enableNotify() override
    {
        m_notify = true;
        return m_eventfd;
    }

    /**
     * @brief Disable notifications via eventFD
     *
     * Do not disable notifications unless you know for sure that nothing is
     * listening on this subscription anymore.
     */
    void disableNotify() override
    {
        m_notify = false;
    }

    /**
     * @brief Stop receiving data, but do not unsubscribe from the stream
     */
    void suspend() override
    {
        // suspend receiving new data
        m_suspended = true;

        // drop currently pending data
        while (m_queue.pop()) {
        }
    }

    /**
     * @brief Resume data transmission, reverses suspend()
     */
    void resume() override
    {
        m_suspended = false;
    }

    /**
     * @brief Clear all pending data from the subscription
     */
    void clearPending() override
    {
        m_suspended = true;
        while (m_queue.pop()) {
        }
        m_suspended = false;
    }

    size_t approxPendingCount() const override
    {
        return m_queue.size_approx();
    }

    bool hasPending() const override
    {
        return m_queue.size_approx() > 0;
    }

    uint throttleValue() const
    {
        return m_throttle;
    }

    uint retrieveApproxSkippedElements()
    {
        const uint si = m_skippedElements;
        m_skippedElements = 0;
        return si;
    }

    /**
     * @brief Set a throttle on the output frequency of this subscription
     * By setting a positive integer value, the output of this
     * subscription is effectively limited to the given integer value per second.
     * This will result in some values being thrown away.
     * By setting a throttle value of 0, all output is passed through and no limits apply.
     * Internally, the throttle value is represented as the minimum needed time in microseconds
     * between elements. This effectively means you can not throttle a connection over 1000000 items/sec.
     */
    void setThrottleItemsPerSec(uint itemsPerSec, bool allowMore = true) override
    {
        uint newThrottle;
        if (itemsPerSec == 0)
            newThrottle = 0;
        else
            newThrottle = allowMore ? std::floor((1000.0 / itemsPerSec) * 1000)
                                    : std::ceil((1000.0 / itemsPerSec) * 1000);

        // clear current queue contents quickly in case we throttle down the subscription
        // (this prevents clients from skipping elements too much if they are overeager
        // when adjusting the throttle value)
        if (newThrottle > m_throttle) {
            // suspending and immediately resuming efficiently clears the current buffer
            suspend();
            resume();
        }

        // apply
        m_throttle = newThrottle;
        m_skippedElements = 0;
    }

    void forcePushNullopt() override
    {
        m_queue.emplace(std::nullopt);
    }

private:
    DataStream<T> *m_stream;
    BlockingReaderWriterQueue<std::optional<T>> m_queue;
    int m_eventfd;
    std::atomic_bool m_notify;
    std::atomic_bool m_active;
    std::atomic_bool m_suspended;
    std::atomic_uint m_throttle;
    std::atomic_uint m_skippedElements;

    // NOTE: These two variables are intentionally *not* threadsafe and are
    // only ever manipulated by the stream (in case of the time) or only
    // touched once when a stream is started (in case of the metadata).
    MetaStringMap m_metadata;
    symaster_timepoint m_lastItemTime;

    void setMetadata(const MetaStringMap &metadata)
    {
        m_metadata = metadata;
    }

    // Common implementation for both lvalue and rvalue push paths.
    // U is deduced as either `const T &` (copy) or `T` (move) via std::forward.
    template<typename U>
    void pushImpl(U &&data)
    {
        // don't accept any new data if we are suspended
        if (m_suspended)
            return;

        // check if we can throttle the enqueueing speed of data
        if (m_throttle != 0) {
            const auto timeNow = currentTimePoint();
            const auto durUsec = timeDiffUsec(timeNow, m_lastItemTime);
            if (durUsec.count() < m_throttle) {
                m_skippedElements++;
                return;
            }
            m_lastItemTime = timeNow;
        }

        // Actually send the data to the subscribers
        // Construct std::optional<T> directly in the ring-buffer slot.
        m_queue.emplace(std::in_place, std::forward<U>(data));

        // ping the eventfd, in case anyone is listening for messages
        if (m_notify) {
            const uint64_t buffer = 1;
            if (write(m_eventfd, &buffer, sizeof(buffer)) == -1)
                qWarning().noquote() << "Unable to write to eventfd in" << dataTypeName()
                                     << "data subscription. FD:" << m_eventfd << "Error:" << std::strerror(errno);
        }
    }

    /**
     * Push an element into the subscription queue.
     */
    void push(const T &data)
    {
        pushImpl(data.clone());
    }

    /**
     * Move an element into the subscription queue.
     *
     * This moves the element if possible, zero-copy when
     * the caller can donate ownership.
     */
    void push(T &&data)
    {
        pushImpl(std::move(data));
    }

    void stop()
    {
        m_active = false;
        m_queue.emplace(std::nullopt);
    }

    void reset()
    {
        m_suspended = false;
        m_active = true;
        m_throttle = 0;
        m_lastItemTime = currentTimePoint();
        while (m_queue.pop()) {
        } // ensure the queue is empty
    }
};

template<typename T>
class DataStream : public VariantDataStream
{
public:
    static_assert(std::derived_from<T, BaseDataType>, "DataStream<T> requires T to derive from BaseDataType.");
    static_assert(supports_explicit_clone<T>, "DataStream<T> requires T to implement T clone() const.");

    DataStream()
        : m_active(false),
          m_explicitDormant(false)
    {
        m_ownerId = std::this_thread::get_id();
    }

    ~DataStream() override
    {
        terminate();
    }

    int dataTypeId() const override
    {
        return syDataTypeId<T>();
    }

    QString dataTypeName() const override
    {
        return QString::fromStdString(BaseDataType::typeIdToString(syDataTypeId<T>()));
    }

    MetaStringMap metadata() override
    {
        return m_metadata;
    }

    void setMetadata(const MetaStringMap &metadata) override
    {
        m_metadata = metadata;
    }

    void setMetadataValue(const std::string &key, const MetaValue &value) override
    {
        m_metadata[key] = value;
    }

    void setSuggestedDataName(const QString &value)
    {
        m_metadata[_commonMetadataKeyMap->value(CommonMetadataKey::DataNameProposal)] = value.toStdString();
    }

    void removeMetadata(const QString &key)
    {
        m_metadata.erase(key.toStdString());
    }

    void setCommonMetadata(const QString &srcModType, const QString &srcModName, const QString &portTitle) override
    {
        setMetadataValue(_commonMetadataKeyMap->value(CommonMetadataKey::SrcModType), srcModType.toStdString());
        setMetadataValue(_commonMetadataKeyMap->value(CommonMetadataKey::SrcModName), srcModName.toStdString());
        if (!portTitle.isEmpty())
            setMetadataValue(_commonMetadataKeyMap->value(CommonMetadataKey::SrcModPortTitle), portTitle.toStdString());
    }

    std::shared_ptr<StreamSubscription<T>> subscribe()
    {
        // we don't permit subscriptions to an active stream
        assert(!m_active);
        if (m_active)
            return nullptr;
        std::lock_guard<std::mutex> lock(m_mutex);
        std::shared_ptr<StreamSubscription<T>> sub(new StreamSubscription<T>(this));
        sub->setMetadata(m_metadata);
        m_subs.push_back(sub);

        // suspend if we are dormant
        if (m_explicitDormant)
            sub->suspend();
        else
            sub->resume();

        return sub;
    }

    std::shared_ptr<VariantStreamSubscription> subscribeVar() override
    {
        return subscribe();
    }

    bool unsubscribe(StreamSubscription<T> *sub)
    {
        // we don't permit unsubscribing from an active stream
        assert(!m_active);
        if (m_active)
            return false;
        std::lock_guard<std::mutex> lock(m_mutex);
        for (uint i = 0; i < m_subs.size(); i++) {
            if (m_subs.at(i).get() == sub) {
                m_subs.erase(m_subs.begin() + i);
                return true;
            }
        }

        return false;
    }

    bool commitMetadata() override
    {
        if (m_active) {
            qWarning().noquote() << "Attempted to commit metadata to an active stream. This is not permitted and may "
                                    "lead to undefined behavior.";
            return false;
        }
        for (auto const &sub : m_subs) {
            sub->reset();
            sub->setMetadata(m_metadata);
        }

        return true;
    }

    void start() override
    {
        m_ownerId = std::this_thread::get_id();
        commitMetadata();

        // Trying to start a dormant stream is illegal, so we
        // do not allow this action - this is a bug in the caller,
        // and needs to be fixed.
        if (m_explicitDormant) {
            qWarning().noquote() << "Attempted to start a stream that is set to dormant. This is not permitted. "
                                    "Offending stream is set to inactive:"
                                 << streamDebugId();
            m_active = false;

            // if this stream is dormant, then we suspend all subscribers
            for (auto &sub : m_subs)
                sub->suspend();
            return;
        }

        m_active = true;
    }

    void stop() override
    {
        for (auto const &sub : m_subs)
            sub->stop();
        m_active = false;
    }

    /**
     * Push data to subscribers, copying it.
     */
    void push(const T &data)
    {
        if (!m_active)
            return;
        for (auto &sub : m_subs)
            sub->push(data);
    }

    /**
     * Push data to subscribers: copies to all but the last subscriber, moves into the last.
     *
     * In the common single-subscriber case this is a zero-copy push.
     * Callers that already own a temporary (or use std::move) should prefer this overload.
     */
    void push(T &&data)
    {
        if (!m_active)
            return;
        if (m_subs.empty())
            return;
        // copy to every subscriber except the last, then donate ownership to the last one
        const auto lastIdx = m_subs.size() - 1;
        for (size_t i = 0; i < lastIdx; ++i)
            m_subs[i]->push(static_cast<const T &>(data));
        m_subs[lastIdx]->push(std::move(data));
    }

    void pushRawData(int typeId, const void *data, size_t size) override
    {
        if (!m_active)
            return;
        if (typeId != syDataTypeId<T>()) {
            qCritical().noquote() << "Invalid data type ID" << typeId << "for stream " << streamDebugId();
            return;
        }

        if constexpr (supports_buffer_reuse<T>::value) {
            // Deserialize into the per-stream scratch object, reusing its
            // internal heap buffers whenever the scratch object is the sole
            // owner of that memory.
            // Each sub->push() enqueues a shallow copy (ref-counted), so the
            // scratch object's refcount rises until subscribers consume their
            // items; fromMemoryInto() allocates a fresh buffer automatically
            // when refcount > 1, keeping all queued copies valid.
            T::fromMemoryInto(data, size, m_scratchObj);
            push(m_scratchObj);
        } else {
            push(T::fromMemory(data, size));
        }
    }

    void terminate()
    {
        stop();

        // "unsubscribe" use forcefully from any active subscription,
        // as this stream is terminated.
        for (auto const &sub : m_subs)
            sub->m_stream = nullptr;
        m_subs.clear();
    }

    bool isActive() const override
    {
        return m_active;
    }

    bool isDormant() const override
    {
        if (m_explicitDormant)
            return true;
        return m_subs.empty();
    }

    void setDormant(bool dormant) override
    {
        m_explicitDormant = dormant;

        // if this stream is dormant, then we suspend all subscribers
        for (auto &sub : m_subs) {
            if (dormant)
                sub->suspend();
            else
                sub->resume();
        }
    }

    [[nodiscard]] bool hasSubscribers() const override
    {
        return !m_subs.empty();
    }

    [[nodiscard]] size_t subscriberCount() const override
    {
        return m_subs.size();
    }

private:
    // Empty tag type used as the scratch member when buffer reuse is disabled.
    struct NoScratch {
    };

    std::thread::id m_ownerId{};
    std::atomic_bool m_active;
    std::atomic_bool m_explicitDormant;
    std::mutex m_mutex;
    std::vector<std::shared_ptr<StreamSubscription<T>>> m_subs;
    MetaStringMap m_metadata;

    // Per-stream scratch object for buffer-reuse types (e.g. Frame).
    // For all other types this collapses to a zero-size NoScratch member.
    [[no_unique_address]] std::conditional_t<supports_buffer_reuse<T>::value, T, NoScratch> m_scratchObj;

    /**
     * Create a simple ID for this stream, for log messages only.
     */
    QString streamDebugId()
    {
        const auto modName = m_metadata.valueOr<std::string>(
            _commonMetadataKeyMap->value(CommonMetadataKey::SrcModName), "unknown-module");
        const auto portName = m_metadata.valueOr<std::string>(
            _commonMetadataKeyMap->value(CommonMetadataKey::SrcModPortTitle), "unknown-port");
        const auto dataTypeStr = dataTypeName().toStdString();
        return QString::fromStdString(modName + "▶ " + portName + "[" + dataTypeStr + "]");
    }
};

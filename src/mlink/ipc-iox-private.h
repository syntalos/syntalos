/*
 * Copyright (C) 2019-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include <string>
#include <expected>
#include <iox2/iceoryx2.hpp>

#include "mlink/ipc-types-private.h"

namespace Syntalos::ipc
{

template<typename T>
using IoxPublisher = iox2::Publisher<iox2::ServiceType::Ipc, T, void>;

template<typename T>
using IoxSubscriber = iox2::Subscriber<iox2::ServiceType::Ipc, T, void>;

using IoxSlicePublisher = iox2::Publisher<iox2::ServiceType::Ipc, iox2::bb::Slice<std::byte>, void>;
using IoxSliceSubscriber = iox2::Subscriber<iox2::ServiceType::Ipc, iox2::bb::Slice<std::byte>, void>;
using IoxListener = iox2::Listener<iox2::ServiceType::Ipc>;
using IoxNotifier = iox2::Notifier<iox2::ServiceType::Ipc>;

template<typename Req, typename Res>
using IoxServer = iox2::Server<iox2::ServiceType::Ipc, Req, void, Res, void>;
using IoxUntypedServer = iox2::Server<iox2::ServiceType::Ipc, iox2::bb::Slice<std::byte>, void, DoneResponse, void>;

template<typename Req, typename Res>
using IoxClient = iox2::Client<iox2::ServiceType::Ipc, Req, void, Res, void>;
using IoxUntypedClient = iox2::Client<iox2::ServiceType::Ipc, iox2::bb::Slice<std::byte>, void, DoneResponse, void>;

using IoxByteSlice = iox2::bb::ImmutableSlice<std::byte>;
using IoxServiceNameString = iox2::bb::StaticString<IOX2_SERVICE_NAME_LENGTH>;

using IoxWaitSet = iox2::WaitSet<iox2::ServiceType::Ipc>;
using IoxWaitSetGuard = iox2::WaitSetGuard<iox2::ServiceType::Ipc>;

/**
 * @brief Construct the service name for a given module instance and channel.
 */
std::string makeModuleServiceName(const std::string &instanceId, const std::string &channelName);

/**
 * @brief Find and cleanup dead nodes explicitly.
 */
void findAndCleanupDeadNodes();

/**
 * The iox2 configuration for Syntalos IPC services.
 */
const iox2::Config &ioxDefaultConfig();

/**
 * Create a node with Syntalos' default configuration.
 */
inline auto makeIoxNode(const std::string &nodeName)
{
    return iox2::NodeBuilder()
        .config(ioxDefaultConfig())
        .name(iox2::NodeName::create(nodeName.c_str()).value())
        .signal_handling_mode(iox2::SignalHandlingMode::HandleTerminationRequests)
        .create<iox2::ServiceType::Ipc>()
        .value();
}

/**
 * @brief Event identifiers for the per-channel event service.
 *
 * Both publisher and subscriber share the same event service (same
 * ServiceName as the pub/sub service). They use these IDs so that each side
 * knows exactly what happened on a WaitSet wakeup.
 */
enum class SyPubSubEvent : size_t {
    Unknown = 0,
    PublisherConnected = 1,     /// Publisher joined the channel.
    PublisherDisconnected = 2,  /// Publisher left the channel.
    SubscriberConnected = 3,    /// Subscriber joined the channel.
    SubscriberDisconnected = 4, /// Subscriber left the channel.
    Sample = 5                  /// Publisher sent a new sample; subscribers should drain.
};

/**
 * @brief Publisher side of a Syntalos data channel.
 *
 * Combines a byte-slice iox2 publisher with an event notifier and listener
 * on the same service name. On creation, it fires PublisherConnected; on
 * destruction it fires PublisherDisconnected. Every send() fires SentSample
 * so attached subscribers wake immediately.
 *
 * The object is FileDescriptorBased (via its listener) so it can be attached
 * to a WaitSet to receive SubscriberConnected / SubscriberDisconnected events
 * from the other side.
 *
 * No history is used — Syntalos guarantees workers are ready before the
 * master starts emitting data.
 */
class SyPublisher : public iox2::FileDescriptorBased
{
public:
    SyPublisher(const SyPublisher &) = delete;
    SyPublisher(SyPublisher &&other) noexcept
        : m_publisher{std::move(other.m_publisher)},
          m_notifier{std::move(other.m_notifier)},
          m_listener{std::move(other.m_listener)},
          m_serviceName{std::move(other.m_serviceName)},
          m_valid{other.m_valid}
    {
        other.m_valid = false;
    }
    SyPublisher &operator=(const SyPublisher &) = delete;
    SyPublisher &operator=(SyPublisher &&other) noexcept
    {
        if (this != &other) {
            m_publisher = std::move(other.m_publisher);
            m_notifier = std::move(other.m_notifier);
            m_listener = std::move(other.m_listener);
            m_serviceName = std::move(other.m_serviceName);
            m_valid = other.m_valid;
            other.m_valid = false;
        }
        return *this;
    }

    ~SyPublisher() override
    {
        if (!m_valid)
            return;
        auto r = m_notifier.notify_with_custom_event_id(
            iox2::EventId(static_cast<size_t>(SyPubSubEvent::PublisherDisconnected)));

        // don't fail silently, but there isn't much we can do about it at this point
        if (!r.has_value())
            std::cerr << "ipc: Failed to emit PublisherDisconnected in SyPublisher destructor: "
                      << iox2::bb::into<const char *>(r.error()) << std::endl;
    }

    /**
     * Create a SyPublisher for the given instance/channel.
     * Either side may arrive first.
     */
    static SyPublisher create(
        iox2::Node<iox2::ServiceType::Ipc> &node,
        const std::string &instanceId,
        const std::string &channelName,
        const IpcServiceTopology &topology = IpcServiceTopology())
    {
        // Main service name to emit samples & sample notifications
        const auto svcNameStr = makeModuleServiceName(instanceId, channelName);
        auto svcName = iox2::ServiceName::create(svcNameStr.c_str()).value();

        // We need to create a separate service name to receive events from the clients,
        // otherwise we will end up talking to ourselves on the same service when emitting
        // samples, which is extremely inefficient.
        const auto svcNameCtlEvStr = makeModuleServiceName("Ctl/" + instanceId, channelName);
        auto svcNameCtlEv = iox2::ServiceName::create(svcNameCtlEvStr.c_str()).value();

        auto maybePubSvc = node.service_builder(svcName)
                               .publish_subscribe<iox2::bb::Slice<std::byte>>()
                               .max_publishers(topology.maxSenders)
                               .max_subscribers(topology.maxReceivers)
                               .max_nodes(topology.maxNodes)
                               .history_size(SY_IOX_HISTORY_SIZE)
                               .subscriber_max_buffer_size(SY_IOX_QUEUE_CAPACITY)
                               .subscriber_max_borrowed_samples(1)
                               .open_or_create();
        if (!maybePubSvc.has_value())
            throw std::runtime_error(
                "Publisher: Failed to open/create pub-sub service for '" + svcNameStr
                + "': " + iox2::bb::into<const char *>(maybePubSvc.error()));

        auto maybePub = std::move(maybePubSvc)
                            .value()
                            .publisher_builder()
                            .unable_to_deliver_strategy(iox2::UnableToDeliverStrategy::Block)
                            .initial_max_slice_len(SY_IOX_INITIAL_SLICE_LEN)
                            .allocation_strategy(iox2::AllocationStrategy::PowerOfTwo)
                            .create();
        if (!maybePub.has_value())
            throw std::runtime_error(
                "Publisher: Failed to create publisher for '" + svcNameStr
                + "': " + iox2::bb::into<const char *>(maybePub.error()));

        // Shared event service to notify clients
        auto maybeEvSvc = node.service_builder(svcName)
                              .event()
                              .max_notifiers(topology.maxSenders)
                              .max_listeners(topology.maxReceivers)
                              .max_nodes(topology.maxNodes)
                              .open_or_create();
        if (!maybeEvSvc.has_value())
            throw std::runtime_error(
                "Publisher: Failed to open/create event service for '" + svcNameStr
                + "': " + iox2::bb::into<const char *>(maybeEvSvc.error()));

        auto maybeNotifier = maybeEvSvc.value().notifier_builder().create();
        if (!maybeNotifier.has_value())
            throw std::runtime_error(
                "Publisher: Failed to create notifier for '" + svcNameStr
                + "': " + iox2::bb::into<const char *>(maybeNotifier.error()));

        // Event service for the clients to notify the publisher (us)
        auto maybeCtlEvSv = node.service_builder(svcNameCtlEv)
                                .event()
                                .max_notifiers(topology.maxReceivers)
                                .max_listeners(topology.maxSenders)
                                .max_nodes(topology.maxNodes)
                                .open_or_create();
        if (!maybeCtlEvSv.has_value())
            throw std::runtime_error(
                "Publisher: Failed to open/create control event service for '" + svcNameCtlEvStr
                + "': " + iox2::bb::into<const char *>(maybeCtlEvSv.error()));

        auto maybeListener = std::move(maybeCtlEvSv).value().listener_builder().create();
        if (!maybeListener.has_value())
            throw std::runtime_error(
                "Publisher: Failed to create control listener for '" + svcNameCtlEvStr
                + "': " + iox2::bb::into<const char *>(maybeListener.error()));

        SyPublisher pub{
            std::move(svcName),
            std::move(maybePub).value(),
            std::move(maybeNotifier).value(),
            std::move(maybeListener).value()};

        // Announce presence to any existing subscribers
        pub.m_notifier
            .notify_with_custom_event_id(iox2::EventId(static_cast<size_t>(SyPubSubEvent::PublisherConnected)))
            .value();
        pub.m_publisher.update_connections().value();

        return pub;
    }

    [[nodiscard]] auto file_descriptor() const -> iox2::FileDescriptorView override
    {
        return m_listener.file_descriptor();
    }

    [[nodiscard]] auto serviceName() const -> iox2::ServiceName
    {
        return m_serviceName;
    }

    void handleEvents()
    {
        for (;;) {
            auto event = m_listener.try_wait_one();
            if (!event.has_value() || !event->has_value())
                break;
            const auto eventId = static_cast<SyPubSubEvent>(event->value().as_value());

            switch (eventId) {
            case SyPubSubEvent::SubscriberConnected: {
                m_publisher.update_connections().value();
                break;
            }
            case SyPubSubEvent::SubscriberDisconnected: {
                // No action needed (but useful for debugging)
                break;
            }
            default: {
                qWarning().noquote().nospace()
                    << "ipc: Received unexpected event ID on " << m_serviceName.to_string().unchecked_access().c_str()
                    << ": " << static_cast<size_t>(eventId);
                break;
            }
            }
        }
    }

    /// The uninitialised loan type returned by loanSlice().
    using SliceLoan = iox2::SampleMutUninit<iox2::ServiceType::Ipc, iox2::bb::Slice<std::byte>, void>;

    /**
     * Loan a shared-memory slice of @p size bytes from the publisher.
     *
     * The caller writes into the slice via loan.payload_mut(), then passes
     * the loan to sendSlice() to publish it and notify subscribers.
     */
    [[nodiscard]] auto loanSlice(size_t size)
    {
        auto maybeSlice = m_publisher.loan_slice_uninit(static_cast<uint64_t>(size));
        if (!maybeSlice.has_value())
            throw std::runtime_error(
                std::string("Publisher::loanSlice: failed to loan slice: ")
                + iox2::bb::into<const char *>(maybeSlice.error()));
        return std::move(maybeSlice).value();
    }

    /**
     * Mark a loaned slice as initialized, publish it, and fire a
     * notification so subscribers wake up.
     */
    void sendSlice(SliceLoan &&loan)
    {
        iox2::send(iox2::assume_init(std::move(loan))).value();
        m_notifier.notify_with_custom_event_id(iox2::EventId(static_cast<size_t>(SyPubSubEvent::Sample))).value();
    }

    /**
     * Convenience overload: copy @p size bytes from @p data into a freshly
     * loaned slice and publish it.  Use loanSlice()/commitAndSend() directly
     * when the data type can write into shared memory itself (zero-copy).
     */
    void sendBytes(const std::byte *data, size_t size)
    {
        auto loan = loanSlice(size);
        std::memcpy(loan.payload_mut().data(), data, size);
        sendSlice(std::move(loan));
    }

private:
    SyPublisher(iox2::ServiceName &&svcName, IoxSlicePublisher &&pub, IoxNotifier &&notifier, IoxListener &&listener)
        : m_publisher{std::move(pub)},
          m_notifier{std::move(notifier)},
          m_listener{std::move(listener)},
          m_serviceName{std::move(svcName)},
          m_valid{true}
    {
    }

    IoxSlicePublisher m_publisher;
    IoxNotifier m_notifier;
    IoxListener m_listener;
    iox2::ServiceName m_serviceName;
    bool m_valid = false;
};

/**
 * @brief Subscriber side of a Syntalos data channel.
 *
 * Combines a byte-slice iox2 subscriber with an event notifier and listener
 * on the same service name. On creation, it fires SubscriberConnected. On
 * destruction, it fires SubscriberDisconnected.
 *
 * The object is FileDescriptorBased (via its listener) so it can be attached
 * to a WaitSet and wake when the publisher fires SentSample. The caller
 * should then call handleEvents() to dispatch events and read data.
 */
class SySubscriber : public iox2::FileDescriptorBased
{
public:
    SySubscriber(const SySubscriber &) = delete;
    SySubscriber(SySubscriber &&other) noexcept
        : m_subscriber{std::move(other.m_subscriber)},
          m_notifier{std::move(other.m_notifier)},
          m_listener{std::move(other.m_listener)},
          m_serviceName{std::move(other.m_serviceName)},
          m_valid{other.m_valid}
    {
        other.m_valid = false;
    }
    SySubscriber &operator=(const SySubscriber &) = delete;
    SySubscriber &operator=(SySubscriber &&other) noexcept
    {
        if (this != &other) {
            m_subscriber = std::move(other.m_subscriber);
            m_notifier = std::move(other.m_notifier);
            m_listener = std::move(other.m_listener);
            m_serviceName = std::move(other.m_serviceName);
            m_valid = other.m_valid;
            other.m_valid = false;
        }
        return *this;
    }

    ~SySubscriber() override
    {
        if (!m_valid)
            return;
        auto r = m_notifier.notify_with_custom_event_id(
            iox2::EventId(static_cast<size_t>(SyPubSubEvent::SubscriberDisconnected)));

        // don't fail silently, but there isn't much we can do about it at this point
        if (!r.has_value())
            std::cerr << "ipc: Failed to emit SubscriberDisconnected in SySubscriber destructor: "
                      << iox2::bb::into<const char *>(r.error()) << std::endl;
    }

    /**
     * Create a SySubscriber for the given instance/channel.
     * Uses open_or_create so either side may arrive first.
     */
    static SySubscriber create(
        iox2::Node<iox2::ServiceType::Ipc> &node,
        const std::string &instanceId,
        const std::string &channelName,
        const IpcServiceTopology &topology = IpcServiceTopology())
    {
        // Main service name to receive samples & notifications
        const auto svcNameStr = makeModuleServiceName(instanceId, channelName);
        auto svcName = iox2::ServiceName::create(svcNameStr.c_str()).value();

        // Separate service name to send control events bck to the publisher
        const auto svcNameCtlEvStr = makeModuleServiceName("Ctl/" + instanceId, channelName);
        auto svcNameCtlEv = iox2::ServiceName::create(svcNameCtlEvStr.c_str()).value();

        auto maybeSubSvc = node.service_builder(svcName)
                               .publish_subscribe<iox2::bb::Slice<std::byte>>()
                               .max_publishers(topology.maxSenders)
                               .max_subscribers(topology.maxReceivers)
                               .max_nodes(topology.maxNodes)
                               .history_size(SY_IOX_HISTORY_SIZE)
                               .subscriber_max_buffer_size(SY_IOX_QUEUE_CAPACITY)
                               .subscriber_max_borrowed_samples(1)
                               .open_or_create();
        if (!maybeSubSvc.has_value())
            throw std::runtime_error(
                "Subscriber: failed to open pub-sub service for '" + svcNameStr
                + "': " + iox2::bb::into<const char *>(maybeSubSvc.error()));

        auto maybeSub = std::move(maybeSubSvc).value().subscriber_builder().buffer_size(SY_IOX_QUEUE_CAPACITY).create();
        if (!maybeSub.has_value())
            throw std::runtime_error(
                "Subscriber: failed to create subscriber for '" + svcNameStr
                + "': " + iox2::bb::into<const char *>(maybeSub.error()));

        // Event service to get notified about new samples
        auto maybeEvSvc = node.service_builder(svcName)
                              .event()
                              .max_notifiers(topology.maxSenders)
                              .max_listeners(topology.maxReceivers)
                              .max_nodes(topology.maxNodes)
                              .open_or_create();
        if (!maybeEvSvc.has_value())
            throw std::runtime_error(
                "Subscriber: failed to open/create event service for '" + svcNameStr
                + "': " + iox2::bb::into<const char *>(maybeEvSvc.error()));

        auto maybeListener = std::move(maybeEvSvc).value().listener_builder().create();
        if (!maybeListener.has_value())
            throw std::runtime_error(
                "Subscriber: failed to create listener for '" + svcNameStr
                + "': " + iox2::bb::into<const char *>(maybeListener.error()));

        // Event service to notify the publisher that we exist
        auto maybeCtlEvSvc = node.service_builder(svcNameCtlEv)
                                 .event()
                                 .max_notifiers(topology.maxReceivers)
                                 .max_listeners(topology.maxSenders)
                                 .max_nodes(topology.maxNodes)
                                 .open_or_create();
        if (!maybeCtlEvSvc.has_value())
            throw std::runtime_error(
                "Subscriber: failed to open/create control event service for '" + svcNameCtlEvStr
                + "': " + iox2::bb::into<const char *>(maybeCtlEvSvc.error()));

        auto maybeNotifier = maybeCtlEvSvc.value().notifier_builder().create();
        if (!maybeNotifier.has_value())
            throw std::runtime_error(
                "Subscriber: failed to create notifier for '" + svcNameCtlEvStr
                + "': " + iox2::bb::into<const char *>(maybeNotifier.error()));

        SySubscriber sub{
            std::move(svcName),
            std::move(maybeSub).value(),
            std::move(maybeNotifier).value(),
            std::move(maybeListener).value()};

        // Announce presence to any existing publisher
        sub.m_notifier
            .notify_with_custom_event_id(iox2::EventId(static_cast<size_t>(SyPubSubEvent::SubscriberConnected)))
            .value();

        return sub;
    }

    // FileDescriptorBased — allows WaitSet attachment (wakes on SentSample etc.)
    auto file_descriptor() const -> iox2::FileDescriptorView override
    {
        return m_listener.file_descriptor();
    }

    [[nodiscard]] auto serviceName() const -> iox2::ServiceName
    {
        return m_serviceName;
    }

    /**
     * Drain the event listener completely, calling @p callback(payload) for every sample.
     *
     * Any event that isn't on a received sample is handled internally.
     *
     * After draining all listener events we also pull any samples that are already
     * in the subscriber's internal buffer but whose Sample notification FD-event has
     * not yet been delivered to the listener. This closes the race between
     * sendSlice() (which enqueues the sample) and notify() (which fires the FD-event):
     * without this extra pass a sample could sit in the buffer indefinitely if the
     * caller stops polling the WaitSet before the notification arrives.
     */
    template<typename Fn>
    void handleEvents(Fn &&callback)
    {
        // Per the iceoryx2 FAQ: We MUST drain all pending events before returning,
        // otherwise the WaitSet fires again immediately (100% CPU / notification flood).
        // try_wait_all() only returns a "reasonable batch", so we loop with
        // try_wait_one() until the listener is truly empty.
        for (auto event = m_listener.try_wait_one(); event.has_value() && event->has_value();
             event = m_listener.try_wait_one()) {
            const auto eventId = static_cast<SyPubSubEvent>(event->value().as_value());
            if (eventId == SyPubSubEvent::Sample) {
                // We received a sample. We should receive exactly the same amount of events as samples are
                // in the pipeline, however, if we ever miss an event, we would miss a sample.
                // This is handled by the extra pass on the listener after draining all events.
                const auto &maybeReceived = m_subscriber.receive();
                if (!maybeReceived.has_value()) [[unlikely]] {
                    std::cerr << "ipc: Failed to receive sample on "
                              << m_serviceName.to_string().unchecked_access().c_str() << ": "
                              << iox2::bb::into<const char *>(maybeReceived.error()) << std::endl;
                    continue;
                }
                const auto &sample = maybeReceived.value();
                if (sample.has_value())
                    callback(sample->payload());
                continue;
            }
        }

        // Flush any samples that arrived in the subscriber buffer before their
        // Sample notification FD-event reached the listener, or for which we may
        // have missed a sample notification.
        for (;;) {
            const auto &maybeReceived = m_subscriber.receive();
            if (!maybeReceived.has_value()) [[unlikely]] {
                std::cerr << "ipc: Failed to receive sample on " << m_serviceName.to_string().unchecked_access().c_str()
                          << ": " << iox2::bb::into<const char *>(maybeReceived.error()) << std::endl;
                break;
            }
            const auto &sample = maybeReceived.value();
            if (!sample.has_value())
                break;
            callback(sample->payload());
        }
    }

    /**
     * Discard any pending data.
     */
    void drain()
    {
        for (;;) {
            auto ev = m_listener.try_wait_one();
            if (!ev.has_value() || !ev->has_value())
                break;
        }

        for (;;) {
            auto mSample = m_subscriber.receive();
            if (!mSample.has_value() || !mSample.value().has_value())
                break;
        }
    }

private:
    SySubscriber(
        iox2::ServiceName serviceName,
        IoxSliceSubscriber &&sub,
        IoxNotifier &&notifier,
        IoxListener &&listener)
        : m_subscriber{std::move(sub)},
          m_notifier{std::move(notifier)},
          m_listener{std::move(listener)},
          m_serviceName{std::move(serviceName)},
          m_valid{true}
    {
    }

    IoxSliceSubscriber m_subscriber;
    IoxNotifier m_notifier;
    IoxListener m_listener;
    iox2::ServiceName m_serviceName;
    bool m_valid = false;
};

/**
 * Create a typed fixed-size publisher on an open-or-created pub-sub service.
 * Uses blocking delivery.
 */
template<typename T>
IoxPublisher<T> makeTypedPublisher(
    iox2::Node<iox2::ServiceType::Ipc> &node,
    const std::string &svcNameStr,
    const IpcServiceTopology &topology = IpcServiceTopology())
{
    auto maybeSvc = node.service_builder(iox2::ServiceName::create(svcNameStr.c_str()).value())
                        .publish_subscribe<T>()
                        .max_publishers(topology.maxSenders)
                        .max_subscribers(topology.maxReceivers)
                        .max_nodes(topology.maxNodes)
                        .subscriber_max_buffer_size(SY_IOX_QUEUE_CAPACITY)
                        .subscriber_max_borrowed_samples(1)
                        .open_or_create();
    if (!maybeSvc.has_value())
        throw std::runtime_error(
            "Failed to open/create pub-sub service for '" + svcNameStr
            + "': " + iox2::bb::into<const char *>(maybeSvc.error()));

    auto maybePub = std::move(maybeSvc)
                        .value()
                        .publisher_builder()
                        .unable_to_deliver_strategy(iox2::UnableToDeliverStrategy::Block)
                        .create();
    if (!maybePub.has_value())
        throw std::runtime_error(
            "Failed to create publisher for '" + svcNameStr + "': " + iox2::bb::into<const char *>(maybePub.error()));
    return std::move(maybePub).value();
}

/**
 * Create a typed fixed-size subscriber.
 */
template<typename T>
iox2::Subscriber<iox2::ServiceType::Ipc, T, void> makeTypedSubscriber(
    iox2::Node<iox2::ServiceType::Ipc> &node,
    const std::string &svcNameStr,
    const IpcServiceTopology &topology = IpcServiceTopology())
{
    auto maybeSvc = node.service_builder(iox2::ServiceName::create(svcNameStr.c_str()).value())
                        .publish_subscribe<T>()
                        .max_publishers(topology.maxSenders)
                        .max_subscribers(topology.maxReceivers)
                        .max_nodes(topology.maxNodes)
                        .subscriber_max_buffer_size(SY_IOX_QUEUE_CAPACITY)
                        .subscriber_max_borrowed_samples(1)
                        .open_or_create();
    if (!maybeSvc.has_value())
        throw std::runtime_error(
            "Failed to open/create pub-sub service for '" + svcNameStr
            + "': " + iox2::bb::into<const char *>(maybeSvc.error()));
    auto maybeSub = std::move(maybeSvc).value().subscriber_builder().buffer_size(SY_IOX_QUEUE_CAPACITY).create();
    if (!maybeSub.has_value())
        throw std::runtime_error(
            "Failed to create subscriber for '" + svcNameStr + "': " + iox2::bb::into<const char *>(maybeSub.error()));
    return std::move(maybeSub).value();
}

/**
 * Create a dynamic-size (byte-slice) publisher on an open-or-created pub-sub service.
 * Used for variable-size control-plane channels (port changes, settings, ...).
 * For data-plane output ports, use SyPublisher instead.
 */
inline IoxSlicePublisher makeSlicePublisher(
    iox2::Node<iox2::ServiceType::Ipc> &node,
    const std::string &svcNameStr,
    const IpcServiceTopology &topology = IpcServiceTopology())
{
    auto maybeSvc = node.service_builder(iox2::ServiceName::create(svcNameStr.c_str()).value())
                        .publish_subscribe<iox2::bb::Slice<std::byte>>()
                        .max_publishers(topology.maxSenders)
                        .max_subscribers(topology.maxReceivers)
                        .max_nodes(topology.maxNodes)
                        .subscriber_max_buffer_size(SY_IOX_QUEUE_CAPACITY)
                        .subscriber_max_borrowed_samples(1)
                        .open_or_create();
    if (!maybeSvc.has_value())
        throw std::runtime_error(
            "Failed to open/create slice pub-sub service for '" + svcNameStr
            + "': " + iox2::bb::into<const char *>(maybeSvc.error()));

    auto maybePub = std::move(maybeSvc)
                        .value()
                        .publisher_builder()
                        .unable_to_deliver_strategy(iox2::UnableToDeliverStrategy::Block)
                        .initial_max_slice_len(SY_IOX_INITIAL_SLICE_LEN)
                        .allocation_strategy(iox2::AllocationStrategy::PowerOfTwo)
                        .create();
    if (!maybePub.has_value())
        throw std::runtime_error(
            "Failed to create slice publisher for '" + svcNameStr
            + "': " + iox2::bb::into<const char *>(maybePub.error()));
    return std::move(maybePub).value();
}

/**
 * Create a dynamic-size (byte-slice) subscriber.
 * Used for variable-size control-plane channels (port changes, settings, ...).
 * For data-plane output ports, use SySubscriber instead.
 */
inline IoxSliceSubscriber makeSliceSubscriber(
    iox2::Node<iox2::ServiceType::Ipc> &node,
    const std::string &svcNameStr,
    const IpcServiceTopology &topology = IpcServiceTopology())
{
    auto maybeSvc = node.service_builder(iox2::ServiceName::create(svcNameStr.c_str()).value())
                        .publish_subscribe<iox2::bb::Slice<std::byte>>()
                        .max_publishers(topology.maxSenders)
                        .max_subscribers(topology.maxReceivers)
                        .max_nodes(topology.maxNodes)
                        .subscriber_max_buffer_size(SY_IOX_QUEUE_CAPACITY)
                        .subscriber_max_borrowed_samples(1)
                        .open_or_create();
    if (!maybeSvc.has_value())
        throw std::runtime_error(
            "Failed to open/create slice pub-sub service for '" + svcNameStr
            + "': " + iox2::bb::into<const char *>(maybeSvc.error()));
    auto maybeSub = std::move(maybeSvc).value().subscriber_builder().buffer_size(SY_IOX_QUEUE_CAPACITY).create();
    if (!maybeSub.has_value())
        throw std::runtime_error(
            "Failed to create slice subscriber for '" + svcNameStr
            + "': " + iox2::bb::into<const char *>(maybeSub.error()));
    return std::move(maybeSub).value();
}

/**
 * Create/open an event-service listener.
 */
inline IoxListener makeEventListener(
    iox2::Node<iox2::ServiceType::Ipc> &node,
    const std::string &svcNameStr,
    const IpcServiceTopology &topology = IpcServiceTopology())
{
    auto maybeSvc = node.service_builder(iox2::ServiceName::create(svcNameStr.c_str()).value())
                        .event()
                        .max_notifiers(topology.maxSenders)
                        .max_listeners(topology.maxReceivers)
                        .max_nodes(topology.maxNodes)
                        .open_or_create();
    if (!maybeSvc.has_value())
        throw std::runtime_error(
            "Failed to open/create event service for '" + svcNameStr
            + "': " + iox2::bb::into<const char *>(maybeSvc.error()));
    auto maybeListener = std::move(maybeSvc).value().listener_builder().create();
    if (!maybeListener.has_value())
        throw std::runtime_error(
            "Failed to create listener for '" + svcNameStr
            + "': " + iox2::bb::into<const char *>(maybeListener.error()));
    return std::move(maybeListener).value();
}

/**
 * Create/open an event-service notifier.
 */
inline IoxNotifier makeEventNotifier(
    iox2::Node<iox2::ServiceType::Ipc> &node,
    const std::string &svcNameStr,
    const IpcServiceTopology &topology = IpcServiceTopology())
{
    auto maybeSvc = node.service_builder(iox2::ServiceName::create(svcNameStr.c_str()).value())
                        .event()
                        .max_notifiers(topology.maxSenders)
                        .max_listeners(topology.maxReceivers)
                        .max_nodes(topology.maxNodes)
                        .open_or_create();
    if (!maybeSvc.has_value())
        throw std::runtime_error(
            "Failed to open/create event service for '" + svcNameStr
            + "': " + iox2::bb::into<const char *>(maybeSvc.error()));
    auto maybeNotifier = std::move(maybeSvc).value().notifier_builder().create();
    if (!maybeNotifier.has_value())
        throw std::runtime_error(
            "Failed to create notifier for '" + svcNameStr
            + "': " + iox2::bb::into<const char *>(maybeNotifier.error()));
    return std::move(maybeNotifier).value();
}

/**
 * Create/open a typed request-response server.
 */
template<typename Req, typename Res>
IoxServer<Req, Res> makeTypedServer(
    iox2::Node<iox2::ServiceType::Ipc> &node,
    const std::string &svcNameStr,
    const IpcServiceTopology &topology = IpcServiceTopology())
{
    auto maybeSvc = node.service_builder(iox2::ServiceName::create(svcNameStr.c_str()).value())
                        .request_response<Req, Res>()
                        .max_servers(topology.maxSenders)
                        .max_clients(topology.maxReceivers)
                        .max_nodes(topology.maxNodes)
                        .open_or_create();
    if (!maybeSvc.has_value())
        throw std::runtime_error(
            "Failed to open/create request-response service for '" + svcNameStr
            + "': " + iox2::bb::into<const char *>(maybeSvc.error()));
    auto maybeSrv = std::move(maybeSvc)
                        .value()
                        .server_builder()
                        .unable_to_deliver_strategy(iox2::UnableToDeliverStrategy::Block)
                        .create();
    if (!maybeSrv.has_value())
        throw std::runtime_error(
            "Failed to create server for '" + svcNameStr + "': " + iox2::bb::into<const char *>(maybeSrv.error()));
    return std::move(maybeSrv).value();
}

/**
 * Create/open a typed request-response client.
 */
template<typename Req, typename Res>
IoxClient<Req, Res> makeTypedClient(
    iox2::Node<iox2::ServiceType::Ipc> &node,
    const std::string &svcNameStr,
    const IpcServiceTopology &topology = IpcServiceTopology())
{
    auto maybeSvc = node.service_builder(iox2::ServiceName::create(svcNameStr.c_str()).value())
                        .request_response<Req, Res>()
                        .max_servers(topology.maxSenders)
                        .max_clients(topology.maxReceivers)
                        .max_nodes(topology.maxNodes)
                        .open_or_create();
    if (!maybeSvc.has_value())
        throw std::runtime_error(
            "Failed to open/create request-response service for '" + svcNameStr
            + "': " + iox2::bb::into<const char *>(maybeSvc.error()));
    auto maybeClient = std::move(maybeSvc).value().client_builder().create();
    if (!maybeClient.has_value())
        throw std::runtime_error(
            "Failed to create client for '" + svcNameStr + "': " + iox2::bb::into<const char *>(maybeClient.error()));
    return std::move(maybeClient).value();
}

/**
 * Create/open a sliced request-response server.
 */
inline IoxUntypedServer makeSliceServer(
    iox2::Node<iox2::ServiceType::Ipc> &node,
    const std::string &svcNameStr,
    const IpcServiceTopology &topology = IpcServiceTopology())
{
    auto maybeSvc = node.service_builder(iox2::ServiceName::create(svcNameStr.c_str()).value())
                        .request_response<iox2::bb::Slice<std::byte>, DoneResponse>()
                        .max_servers(topology.maxSenders)
                        .max_clients(topology.maxReceivers)
                        .max_nodes(topology.maxNodes)
                        .open_or_create();
    if (!maybeSvc.has_value())
        throw std::runtime_error(
            "Failed to open/create untyped request-response service for '" + svcNameStr
            + "': " + iox2::bb::into<const char *>(maybeSvc.error()));
    auto maybeSrv = std::move(maybeSvc)
                        .value()
                        .server_builder()
                        .unable_to_deliver_strategy(iox2::UnableToDeliverStrategy::Block)
                        .create();
    if (!maybeSrv.has_value())
        throw std::runtime_error(
            "Failed to create untyped server for '" + svcNameStr
            + "': " + iox2::bb::into<const char *>(maybeSrv.error()));
    return std::move(maybeSrv).value();
}

/**
 * Create/open a sliced request-response client.
 */
inline IoxUntypedClient makeSliceClient(
    iox2::Node<iox2::ServiceType::Ipc> &node,
    const std::string &svcNameStr,
    const IpcServiceTopology &topology = IpcServiceTopology())
{
    auto maybeSvc = node.service_builder(iox2::ServiceName::create(svcNameStr.c_str()).value())
                        .request_response<iox2::bb::Slice<std::byte>, DoneResponse>()
                        .max_servers(topology.maxSenders)
                        .max_clients(topology.maxReceivers)
                        .max_nodes(topology.maxNodes)
                        .open_or_create();
    if (!maybeSvc.has_value())
        throw std::runtime_error(
            "Failed to open/create untyped request-response service for '" + svcNameStr
            + "': " + iox2::bb::into<const char *>(maybeSvc.error()));
    auto maybeClient = std::move(maybeSvc)
                           .value()
                           .client_builder()
                           .initial_max_slice_len(SY_IOX_INITIAL_SLICE_LEN)
                           .allocation_strategy(iox2::AllocationStrategy::PowerOfTwo)
                           .create();
    if (!maybeClient.has_value())
        throw std::runtime_error(
            "Failed to create untyped client for '" + svcNameStr
            + "': " + iox2::bb::into<const char *>(maybeClient.error()));
    return std::move(maybeClient).value();
}

/**
 * Drain all pending events from a listener.
 */
inline void drainListenerEvents(IoxListener &listener)
{
    for (;;) {
        auto ev = listener.try_wait_one();
        if (!ev.has_value() || !ev->has_value())
            break;
    }
}

} // namespace Syntalos::ipc

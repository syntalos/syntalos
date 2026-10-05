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

#include <QObject>
#include <QString>
#include <atomic>
#include <concepts>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "datactl/datatypes.h"
#include "datactl/syclock.h"
#include "logging.h"
#include "streams/stream.h"

namespace Syntalos
{

class AbstractModule;
class MLinkModule;

/**
 * @brief A value that the main thread can change while a worker is running.
 *
 * This is a handle: copies share the same value. The module keeps one copy
 * and hands another one to its worker thread. Any thread may call set() and get(),
 * while takeIfChanged() is meant for a single consumer, which picks up changes at
 * a point where it is convenient for it.
 */
template<typename T>
class LiveValue
{
public:
    LiveValue()
        : d(std::make_shared<Data>())
    {
    }

    LiveValue(T initialValue)
        : d(std::make_shared<Data>())
    {
        d->value = std::move(initialValue);
    }

    /**
     * @brief Set a new value and flag it as changed.
     */
    void set(T value)
    {
        const std::lock_guard<std::mutex> lock(d->mutex);
        d->value = std::move(value);
        d->changed.store(true, std::memory_order_release);
    }

    /**
     * @brief Get a copy of the current value.
     */
    [[nodiscard]] T get() const
    {
        const std::lock_guard<std::mutex> lock(d->mutex);
        return d->value;
    }

    /**
     * @brief Get the value if it was set since the last call to this function.
     *
     * This is cheap if nothing has changed (a single atomic load), so it can be
     * called in a tight loop.
     */
    [[nodiscard]] std::optional<T> takeIfChanged()
    {
        if (!d->changed.load(std::memory_order_acquire))
            return std::nullopt;

        const std::lock_guard<std::mutex> lock(d->mutex);
        d->changed.store(false, std::memory_order_release);
        return d->value;
    }

private:
    struct Data {
        mutable std::mutex mutex;
        T value{};
        std::atomic_bool changed{false};
    };
    std::shared_ptr<Data> d;
};

/**
 * @brief A mutex-protected value shared between the main thread and a worker.
 *
 * This is a handle: copies share the same value. The value can only be reached
 * while holding its mutex, either via the accessor returned by lock() or
 * via the get() / set() / take() convenience functions.
 */
template<typename T>
class Guarded
{
public:
    /**
     * @brief Exclusive access to the guarded value, for as long as this object lives.
     */
    class Access
    {
    public:
        T *operator->()
        {
            return m_value;
        }

        T &operator*()
        {
            return *m_value;
        }

    private:
        friend class Guarded<T>;
        Access(std::mutex &mutex, T *value)
            : m_lock(mutex),
              m_value(value)
        {
        }

        std::unique_lock<std::mutex> m_lock;
        T *m_value;
    };

    Guarded()
        : d(std::make_shared<Data>())
    {
    }

    Guarded(T initialValue)
        : d(std::make_shared<Data>())
    {
        d->value = std::move(initialValue);
    }

    /**
     * @brief Lock the value and access it for as long as the returned object lives.
     */
    [[nodiscard]] Access lock()
    {
        return Access(d->mutex, &d->value);
    }

    /**
     * @brief Get a copy of the current value.
     */
    [[nodiscard]] T get() const
        requires std::copy_constructible<T>
    {
        const std::lock_guard<std::mutex> lock(d->mutex);
        return d->value;
    }

    void set(T value)
    {
        const std::lock_guard<std::mutex> lock(d->mutex);
        d->value = std::move(value);
    }

    /**
     * @brief Move the value out (leaves a default-constructed one behind)
     */
    [[nodiscard]] T take()
    {
        const std::lock_guard<std::mutex> lock(d->mutex);
        return std::exchange(d->value, T{});
    }

private:
    struct Data {
        mutable std::mutex mutex;
        T value{};
    };
    std::shared_ptr<Data> d;
};

/**
 * @brief A callback that a worker can invoke from its thread, to run code on the main thread.
 *
 * Created by the module via AbstractModule::mainCallback() and handed to the worker.
 * Calling it copies the arguments and queues the wrapped function into the event loop
 * of the module's thread, so the function may safely touch the module and its GUI.
 * The call returns immediately; nothing is executed if the module no longer exists by
 * the time the event loop gets to it.
 */
template<typename... Args>
class MainCallback
{
public:
    MainCallback() = default;

    MainCallback(QObject *context, std::function<void(const Args &...)> fn)
        : d(std::make_shared<Data>(context, std::move(fn)))
    {
    }

    void operator()(Args... args) const
    {
        if (!d)
            return;
        QMetaObject::invokeMethod(
            d->context,
            [d = this->d, argTuple = std::make_tuple(std::move(args)...)]() {
                std::apply(d->fn, argTuple);
            },
            Qt::QueuedConnection);
    }

    explicit operator bool() const
    {
        return d != nullptr;
    }

private:
    struct Data {
        Data(QObject *ctx, std::function<void(const Args &...)> f)
            : context(ctx),
              fn(std::move(f))
        {
        }

        QObject *context;
        std::function<void(const Args &...)> fn;
    };
    std::shared_ptr<const Data> d;
};

/**
 * @brief Thread-safe link from a worker to the module it belongs to.
 *
 * Every worker struct has one as member named "mod" of type WorkerContext. It is bound to the
 * module when the worker is passed to AbstractModule::setWorker(). Everything in here
 * can be used by any thread.
 */
class Q_DECL_EXPORT WorkerContext
{
public:
    WorkerContext() = default;

    /**
     * @brief The logger of the module.
     */
    QuillLogger *log = nullptr;

    /**
     * @brief The master timer of the current run.
     */
    std::shared_ptr<SyncTimer> timer;

    /**
     * @brief True for as long as the current run is supposed to continue.
     */
    [[nodiscard]] bool running() const
    {
        return m_running != nullptr && m_running->load();
    }

    /**
     * @brief Block until the run is started.
     *
     * This flags the module as READY and parks the thread until all modules are
     * started together. It must be called by run() once its setup is complete.
     */
    void waitForStart() const;

    void raiseError(const QString &message) const;
    void raiseError(const std::string &message) const;
    void raiseError(const char *message) const;

    void setStatusMessage(const QString &message) const;
    void setStatusMessage(const std::string &message) const;
    void setStatusMessage(const char *message) const;

    /**
     * @brief Mark the module as having nothing to do in this run.
     */
    void setStateDormant() const;

    /**
     * @brief Mark the module as ready to start, without waiting for the start.
     *
     * Calling this explicitly is usually not necessary, waitForStart() does it.
     */
    void setStateReady() const;

    /**
     * @brief The name of the module, as it was when the worker was created.
     */
    [[nodiscard]] QString moduleName() const
    {
        return m_modName;
    }

private:
    friend class AbstractModule;
    friend class MLinkModule;

    AbstractModule *m_mod = nullptr;
    const std::atomic_bool *m_running = nullptr;
    QString m_modName;
};

namespace detail
{

/**
 * @brief Connection between a WorkerTimer handle and the event loop that runs the timer.
 */
struct WorkerTimerState {
    std::mutex mutex;

    /// Set by the event loop while it is running: arms the timer to fire once after the
    /// given amount of microseconds, or disarms it if the value is negative.
    std::function<void(std::int64_t)> arm;
};

} // namespace detail

/**
 * @brief A one-shot timer of an event-driven worker.
 *
 * Created with WorkerEvents::timer() in the setup() function of a worker. The timer does
 * nothing until it is started, and then calls its function once in the worker's event loop.
 * It can be started again at any time, also from within its own function.
 * A default-constructed timer does nothing.
 */
class Q_DECL_EXPORT WorkerTimer
{
public:
    WorkerTimer() = default;

    /**
     * @brief Have the timer fire once after the given delay.
     *
     * If the timer is already waiting, it is set to the new delay. This has no effect
     * while the event loop of the worker is not running.
     */
    void start(const milliseconds_t &delay) const;
    void start(const microseconds_t &delay) const;

    /**
     * @brief Cancel the timer, if it is waiting.
     */
    void stop() const;

private:
    friend class WorkerEvents;
    std::shared_ptr<detail::WorkerTimerState> d;
};

/// Callable type for timed worker events, the argument is the interval in milliseconds
using WorkerTimedEventFn = std::function<void(int &)>;

/// Callable type for worker events that fire when a subscription has new data
using WorkerDataEventFn = std::function<void()>;

/**
 * @brief Event registrations of an event-driven worker.
 *
 * Passed to the setup() function of a worker, which uses it to request callbacks
 * in the event loop the module is assigned to. All callbacks of a worker
 * are run by the same thread and never concurrently.
 */
class WorkerEvents
{
public:
    struct DataEvent {
        std::shared_ptr<VariantStreamSubscription> subscription;
        WorkerDataEventFn fn;
    };

    struct TimedEvent {
        int intervalMsec;
        WorkerTimedEventFn fn;
    };

    struct OneShotEvent {
        std::shared_ptr<detail::WorkerTimerState> timer;
        WorkerDataEventFn fn;
    };

    /**
     * @brief Call a function when the given subscription has received new data.
     */
    template<typename Fn>
        requires std::invocable<Fn &>
    void onData(std::shared_ptr<VariantStreamSubscription> subscription, Fn &&fn)
    {
        m_dataEvents.push_back(DataEvent{std::move(subscription), WorkerDataEventFn(std::forward<Fn>(fn))});
    }

    /**
     * @brief Call a function at an interval.
     *
     * If the interval is 0, the function is called as soon as possible. The function
     * receives a reference to its interval in milliseconds, which it may adjust to be run
     * less or more frequently, or set to a negative value to not be called again.
     * Since the function shares an event loop with other events, it will not be called
     * in exactly the requested interval, and lost time is not caught up.
     */
    template<typename Fn>
        requires std::invocable<Fn &, int &>
    void every(const milliseconds_t &interval, Fn &&fn)
    {
        m_timedEvents.push_back(
            TimedEvent{static_cast<int>(interval.count()), WorkerTimedEventFn(std::forward<Fn>(fn))});
    }

    /**
     * @brief Create a timer that calls a function once, some time after it was started.
     *
     * Unlike every(), which runs from the start of the run, the returned timer is started by
     * the worker when it needs it, e.g. to do something a certain time after data has arrived.
     */
    template<typename Fn>
        requires std::invocable<Fn &>
    [[nodiscard]] WorkerTimer timer(Fn &&fn)
    {
        WorkerTimer timer;
        timer.d = std::make_shared<detail::WorkerTimerState>();
        m_oneShotEvents.push_back(OneShotEvent{timer.d, WorkerDataEventFn(std::forward<Fn>(fn))});
        return timer;
    }

    [[nodiscard]] const std::vector<DataEvent> &dataEvents() const
    {
        return m_dataEvents;
    }

    [[nodiscard]] const std::vector<TimedEvent> &timedEvents() const
    {
        return m_timedEvents;
    }

    [[nodiscard]] const std::vector<OneShotEvent> &oneShotEvents() const
    {
        return m_oneShotEvents;
    }

private:
    std::vector<DataEvent> m_dataEvents;
    std::vector<TimedEvent> m_timedEvents;
    std::vector<OneShotEvent> m_oneShotEvents;
};

/**
 * @brief A worker with a run() function, to be executed in a dedicated thread.
 */
template<typename W>
concept ThreadWorker = requires(W &w) { w.run(); };

/**
 * @brief A worker with a setup() function, driven by events.
 */
template<typename W>
concept EventWorker = requires(W &w, WorkerEvents &ev) { w.setup(ev); };

/**
 * @brief A struct that has a link to its module, as a `WorkerContext mod{};` member.
 */
template<typename W>
concept WorkerDeclared = requires(W &w) {
    { w.mod } -> std::same_as<WorkerContext &>;
};

namespace detail
{

inline QString workerErrorToString(const QString &error)
{
    return error;
}

inline QString workerErrorToString(const std::string &error)
{
    return QString::fromStdString(error);
}

inline QString workerErrorToString(const char *error)
{
    return QString::fromUtf8(error);
}

/**
 * @brief Type-erased owner of a module worker.
 */
class Q_DECL_EXPORT WorkerHolderBase
{
public:
    WorkerHolderBase() = default;
    virtual ~WorkerHolderBase();

    /**
     * @brief True if the worker wants a dedicated thread, false if it is driven by events.
     */
    [[nodiscard]] virtual bool isThreadWorker() const = 0;

    /**
     * @brief Run the worker in the current thread.
     * @return The error the worker has failed with, if any.
     */
    virtual std::expected<void, QString> run() = 0;

    [[nodiscard]] const WorkerEvents &events() const
    {
        return m_events;
    }

protected:
    WorkerEvents m_events;

private:
    Q_DISABLE_COPY(WorkerHolderBase)
};

template<typename W>
class WorkerHolder final : public WorkerHolderBase
{
public:
    explicit WorkerHolder(W &&w)
        : worker(std::move(w))
    {
    }

    [[nodiscard]] bool isThreadWorker() const override
    {
        return ThreadWorker<W>;
    }

    std::expected<void, QString> run() override
    {
        if constexpr (ThreadWorker<W>) {
            if constexpr (std::is_void_v<decltype(worker.run())>) {
                worker.run();
            } else {
                auto res = worker.run();
                if (!res.has_value())
                    return std::unexpected(workerErrorToString(res.error()));
            }
        }
        return {};
    }

    /**
     * @brief Let an event-driven worker register its events.
     *
     * Must only be called once the worker is at its final address, as the
     * callbacks usually refer to it.
     */
    void setupEvents()
    {
        if constexpr (EventWorker<W>)
            worker.setup(m_events);
    }

    W worker;
};

} // namespace detail

} // namespace Syntalos

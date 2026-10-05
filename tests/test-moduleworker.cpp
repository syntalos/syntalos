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

#include <QtTest>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>

#include "moduleapi.h"
#include "moduleeventthread.h"

using namespace Syntalos;
namespace Syntalos
{

/**
 * Worker that counts up for as long as the run lasts.
 */
struct CountingWorker {
    WorkerContext mod{};
    int increment;
    LiveValue<int> liveIncrement;
    MainCallback<int> onStarted;
    std::shared_ptr<std::atomic_int> iterations;
    int total = 0;

    void run()
    {
        mod.waitForStart();
        onStarted(increment);
        while (mod.running()) {
            if (auto newIncrement = liveIncrement.takeIfChanged())
                increment = *newIncrement;
            total += increment;
            iterations->fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

/**
 * Worker that fails right after the start.
 */
struct FailingWorker {
    WorkerContext mod{};
    QString message;

    std::expected<void, QString> run()
    {
        mod.waitForStart();
        return std::unexpected(message);
    }
};

/**
 * Worker that is driven by events instead of having its own thread.
 */
struct EventCountingWorker {
    WorkerContext mod{};
    std::shared_ptr<StreamSubscription<TableRow>> sub;
    int received = 0;
    int ticks = 0;

    void setup(WorkerEvents &ev)
    {
        ev.onData(sub, [this] {
            while (sub->peekNext().has_value())
                received++;
        });
        ev.every(milliseconds_t(10), [this](int &intervalMsec) {
            ticks++;
            intervalMsec = -1;
        });
    }
};

/**
 * Worker to be run by a real event loop: It starts a one-shot timer
 * when data arrives, and the timer starts itself again once.
 */
struct EventLoopWorker {
    WorkerContext mod{};
    std::shared_ptr<StreamSubscription<TableRow>> sub;
    milliseconds_t timerDelay;
    std::shared_ptr<std::atomic_int> received;
    std::shared_ptr<std::atomic_int> timerFired;
    std::shared_ptr<std::atomic_int> ticks;
    std::shared_ptr<std::atomic<long long>> shortestDelayMsec;

    WorkerTimer timer{};
    std::chrono::steady_clock::time_point timerStarted{};
    int restartsLeft = 1;

    void setup(WorkerEvents &ev)
    {
        ev.onData(sub, [this] {
            while (sub->peekNext().has_value())
                received->fetch_add(1);
            startTimer();
        });
        timer = ev.timer([this] {
            const auto delay = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - timerStarted)
                                   .count();
            if (delay < shortestDelayMsec->load())
                shortestDelayMsec->store(delay);
            timerFired->fetch_add(1);
            if (restartsLeft-- > 0)
                startTimer();
        });
        ev.every(milliseconds_t(2), [this](int &) {
            ticks->fetch_add(1);
        });
    }

    void startTimer()
    {
        timerStarted = std::chrono::steady_clock::now();
        timer.start(timerDelay);
    }
};

class WorkerTestModule : public AbstractModule
{
    Q_OBJECT
public:
    explicit WorkerTestModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
    }

    bool prepare(const RunInfo &) override
    {
        return true;
    }

    using AbstractModule::hasWorker;
    using AbstractModule::mainCallback;
    using AbstractModule::modifyWorker;
    using AbstractModule::setWorker;
    using AbstractModule::takeWorker;

    void setRunning(bool running)
    {
        m_running = running;
    }
};

class TestModuleWorker : public QObject
{
    Q_OBJECT
private slots:
    void liveValueIsShared()
    {
        LiveValue<int> a(5);
        auto b = a;
        QCOMPARE(b.get(), 5);
        QVERIFY(!b.takeIfChanged().has_value());

        a.set(7);
        QCOMPARE(b.get(), 7);
        const auto changed = b.takeIfChanged();
        QVERIFY(changed.has_value());
        QCOMPARE(*changed, 7);
        // a change is only reported once
        QVERIFY(!b.takeIfChanged().has_value());
        QVERIFY(!a.takeIfChanged().has_value());
    }

    void liveValueAcrossThreads()
    {
        LiveValue<QString> value;
        auto reader = value;

        auto f = std::async(std::launch::async, [reader]() mutable {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (std::chrono::steady_clock::now() < deadline) {
                if (auto v = reader.takeIfChanged())
                    return *v;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return QString();
        });

        value.set(QStringLiteral("changed"));
        QCOMPARE(f.get(), QStringLiteral("changed"));
    }

    void guardedValue()
    {
        Guarded<std::vector<int>> queue;
        auto other = queue;

        queue.set({1, 2});
        const auto taken = other.take();
        QCOMPARE(taken.size(), size_t(2));
        QVERIFY(queue.take().empty());

        // move-only values can be handed over as well
        Guarded<std::unique_ptr<int>> slot;
        slot.set(std::make_unique<int>(42));
        auto value = slot.take();
        QVERIFY(value != nullptr);
        QCOMPARE(*value, 42);
        QVERIFY(slot.take() == nullptr);
    }

    void threadWorkerRuns()
    {
        WorkerTestModule mod;
        OptionalWaitCondition startCond;

        int startedWith = -1;
        LiveValue<int> liveIncrement;
        auto iterations = std::make_shared<std::atomic_int>(0);

        mod.setState(ModuleState::PREPARING);
        mod.setWorker(
            CountingWorker{
                .increment = 1,
                .liveIncrement = liveIncrement,
                .onStarted = mod.mainCallback([&startedWith](int increment) {
                    startedWith = increment;
                }),
                .iterations = iterations,
            });
        QVERIFY(mod.workerHolder() != nullptr);
        QVERIFY(mod.workerHolder()->isThreadWorker());
        QVERIFY(mod.hasWorker());

        mod.setWorkerActive(true);
        std::thread thread([&] {
            mod.runWorker(&startCond);
        });

        // the worker parks on the start barrier and flags the module as ready
        QTRY_VERIFY(mod.state() == ModuleState::READY);
        QCOMPARE(iterations->load(), 0);

        // a running worker can not be taken away
        QVERIFY(!mod.takeWorker<CountingWorker>().has_value());

        // while the worker waits for the start it can be modified, but only when the module is started
        QVERIFY(!mod.modifyWorker<CountingWorker>([](CountingWorker &w) {
            w.total = -1;
        }));
        mod.setWorkerStartPhase(true);
        QVERIFY(mod.modifyWorker<CountingWorker>([](CountingWorker &w) {
            w.total = 5000;
        }));
        QVERIFY(!mod.modifyWorker<FailingWorker>([](FailingWorker &) {}));
        mod.setWorkerStartPhase(false);

        mod.setRunning(true);
        startCond.wakeAll();
        QTRY_VERIFY(iterations->load() > 0);

        // the callback was invoked by the worker thread, but must run in this thread
        QTRY_COMPARE(startedWith, 1);

        // change a value while the worker is running
        liveIncrement.set(1000);
        const auto iterationsAtChange = iterations->load();
        QTRY_VERIFY(iterations->load() > iterationsAtChange + 2);

        mod.setRunning(false);
        thread.join();
        mod.setWorkerActive(false);

        // the worker has finished, but it belongs to the run until the module takes it
        QVERIFY(mod.hasWorker());

        auto worker = mod.takeWorker<CountingWorker>();
        QVERIFY(worker.has_value());
        QCOMPARE(worker->increment, 1000);
        QVERIFY(worker->total >= 6000);
        QCOMPARE(worker->mod.moduleName(), mod.name());

        // the worker is gone once it has been taken
        QVERIFY(!mod.hasWorker());
        QVERIFY(mod.workerHolder() == nullptr);
        QVERIFY(!mod.takeWorker<CountingWorker>().has_value());
    }

    void workerErrorIsRaised()
    {
        WorkerTestModule mod;
        OptionalWaitCondition startCond;
        QStringList errors;
        connect(&mod, &AbstractModule::error, this, [&errors](const QString &message) {
            errors.append(message);
        });

        mod.setState(ModuleState::PREPARING);
        mod.setWorker(FailingWorker{.message = QStringLiteral("Something broke")});

        mod.setRunning(true);
        startCond.wakeAll();
        std::thread thread([&] {
            mod.runWorker(&startCond);
        });
        thread.join();

        QVERIFY(mod.state() == ModuleState::ERROR);
        QTRY_COMPARE(errors.size(), 1);
        QCOMPARE(errors.first(), QStringLiteral("Something broke"));

        // a wrong type yields nothing and leaves the worker alone
        QVERIFY(!mod.takeWorker<CountingWorker>().has_value());
        QVERIFY(mod.takeWorker<FailingWorker>().has_value());
    }

    void moduleWithoutWorkerPassesBarrier()
    {
        WorkerTestModule mod;
        OptionalWaitCondition startCond;
        mod.setState(ModuleState::PREPARING);

        std::thread thread([&] {
            mod.runWorker(&startCond);
        });
        QTRY_VERIFY(mod.state() == ModuleState::READY);
        startCond.wakeAll();
        thread.join();
    }

    void eventWorkerRegistersEvents()
    {
        WorkerTestModule mod;
        DataStream<TableRow> stream;
        auto sub = stream.subscribe();
        stream.start();

        mod.setWorker(EventCountingWorker{.sub = sub});
        auto holder = mod.workerHolder();
        QVERIFY(holder != nullptr);
        QVERIFY(!holder->isThreadWorker());

        const auto &events = holder->events();
        QCOMPARE(events.dataEvents().size(), size_t(1));
        QCOMPARE(events.timedEvents().size(), size_t(1));
        QCOMPARE(events.timedEvents().front().intervalMsec, 10);
        QVERIFY(events.dataEvents().front().subscription == sub);

        // run the registered events like an event loop would
        stream.push(TableRow());
        stream.push(TableRow());
        events.dataEvents().front().fn();
        int interval = events.timedEvents().front().intervalMsec;
        events.timedEvents().front().fn(interval);
        QCOMPARE(interval, -1);
        stream.stop();

        const auto worker = mod.takeWorker<EventCountingWorker>();
        QVERIFY(worker.has_value());
        QCOMPARE(worker->received, 2);
        QCOMPARE(worker->ticks, 1);
    }

    void eventLoopRunsWorkerTimers()
    {
        WorkerTestModule mod;
        OptionalWaitCondition startCond;
        DataStream<TableRow> stream;
        auto sub = stream.subscribe();
        stream.start();

        auto received = std::make_shared<std::atomic_int>(0);
        auto timerFired = std::make_shared<std::atomic_int>(0);
        auto ticks = std::make_shared<std::atomic_int>(0);
        auto shortestDelayMsec = std::make_shared<std::atomic<long long>>(100000);

        // a timer handle that was never created by a worker does nothing
        WorkerTimer unusedTimer;
        unusedTimer.start(milliseconds_t(1));
        unusedTimer.stop();

        mod.setWorker(
            EventLoopWorker{
                .sub = sub,
                .timerDelay = milliseconds_t(40),
                .received = received,
                .timerFired = timerFired,
                .ticks = ticks,
                .shortestDelayMsec = shortestDelayMsec,
            });

        ModuleEventThread evThread(QStringLiteral("test"));
        mod.setWorkerActive(true);
        evThread.run({&mod}, &startCond);

        // nothing happens before the run is started
        QTest::qWait(50);
        QCOMPARE(ticks->load(), 0);

        mod.setState(ModuleState::RUNNING);
        startCond.wakeAll();
        QTRY_VERIFY(ticks->load() > 3);

        // the one-shot timer does nothing until the worker starts it
        QCOMPARE(timerFired->load(), 0);

        // data starts the timer, which fires once and then starts itself one more time
        stream.push(TableRow());
        QTRY_COMPARE(received->load(), 1);
        QTRY_COMPARE(timerFired->load(), 2);
        QVERIFY2(
            shortestDelayMsec->load() >= 40,
            qPrintable(QStringLiteral("Timer fired after %1 msec").arg(shortestDelayMsec->load())));

        // ... and never again
        QTest::qWait(150);
        QCOMPARE(timerFired->load(), 2);

        evThread.stop();
        mod.setWorkerActive(false);
        stream.stop();

        // once the event loop is gone, the timer of the worker can not be started anymore
        auto worker = mod.takeWorker<EventLoopWorker>();
        QVERIFY(worker.has_value());
        worker->timer.start(milliseconds_t(1));
        worker->timer.stop();
    }

    void raiseErrorOnlyEscalatesOnce()
    {
        WorkerTestModule mod;
        std::atomic_int errorCount{0};
        connect(
            &mod,
            &AbstractModule::error,
            this,
            [&errorCount](const QString &) {
                errorCount++;
            },
            Qt::DirectConnection);

        mod.setState(ModuleState::RUNNING);
        mod.setWorker(FailingWorker{.message = QStringLiteral("unused")});
        const auto ctx = mod.takeWorker<FailingWorker>()->mod;

        std::vector<std::thread> threads;
        for (int i = 0; i < 8; i++) {
            threads.emplace_back([ctx, i] {
                ctx.raiseError(QStringLiteral("Error %1").arg(i));
            });
        }
        for (auto &t : threads)
            t.join();

        QVERIFY(mod.state() == ModuleState::ERROR);
        QCOMPARE(errorCount.load(), 1);
    }

    void interruptWakesBlockedReader()
    {
        DataStream<TableRow> stream;
        auto sub = stream.subscribe();
        stream.start();
        stream.push(TableRow());

        std::atomic_int received{0};
        auto f = std::async(std::launch::async, [&] {
            while (sub->next().has_value())
                received++;
        });

        // the reader consumes what is there and then blocks, as the stream is still active
        QTRY_COMPARE(received.load(), 1);
        QVERIFY(f.wait_for(std::chrono::milliseconds(250)) == std::future_status::timeout);

        // pending data is still delivered after an interruption, then the reader is released
        stream.push(TableRow());
        sub->interrupt();
        QVERIFY2(
            f.wait_for(std::chrono::seconds(5)) == std::future_status::ready,
            "Reader was not released by interrupt()");
        QCOMPARE(received.load(), 2);

        // once the interruption is cleared, the subscription delivers data as before
        sub->clearInterrupt();
        stream.push(TableRow());
        QVERIFY(sub->next().has_value());
        stream.stop();
        QVERIFY(!sub->next().has_value());
    }
};

} // namespace Syntalos

QTEST_MAIN(TestModuleWorker)
#include "test-moduleworker.moc"

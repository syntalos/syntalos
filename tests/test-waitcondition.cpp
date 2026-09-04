/*
 * Copyright (C) 2024-2026 Matthias Klumpp <matthias@tenstral.net>
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
#include <vector>

#include "optionalwaitcondition.h"

using namespace Syntalos;
namespace Syntalos
{

/**
 * @brief Wait for all futures to complete within a bounded time.
 * @return true if all threads returned in time.
 */
static bool waitForAll(std::vector<std::future<void>> &futures, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    for (auto &f : futures) {
        if (f.wait_until(deadline) != std::future_status::ready)
            return false;
    }
    return true;
}

class TestWaitCondition : public QObject
{
    Q_OBJECT
private slots:
    void barrierReleasesAllWaiters()
    {
        OptionalWaitCondition cond;
        std::atomic_uint passed{0};

        const uint threadCount = 8;
        std::vector<std::future<void>> futures;
        for (uint i = 0; i < threadCount; ++i) {
            futures.push_back(std::async(std::launch::async, [&]() {
                cond.wait();
                passed++;
            }));
        }

        // wait until every thread is actually parked
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (cond.waitingCount() < threadCount) {
            QVERIFY2(std::chrono::steady_clock::now() < deadline, "Threads did not park on the barrier in time");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        QCOMPARE(passed.load(), 0u);
        QVERIFY(!cond.isReleased());

        cond.wakeAll();
        QVERIFY(cond.isReleased());
        QVERIFY2(waitForAll(futures, std::chrono::seconds(5)), "Not all threads were woken by wakeAll()");
        QCOMPARE(passed.load(), threadCount);
        QCOMPARE(cond.waitingCount(), threadCount);
    }

    void lateArrivalPassesThrough()
    {
        OptionalWaitCondition cond;
        cond.wakeAll();

        auto f = std::async(std::launch::async, [&]() {
            cond.wait();
        });
        QVERIFY2(
            f.wait_for(std::chrono::seconds(5)) == std::future_status::ready,
            "Late waiter blocked on an already released barrier");
        // a thread that never parked must not be counted
        QCOMPARE(cond.waitingCount(), 0u);
    }

    void resetRearmsBarrier()
    {
        OptionalWaitCondition cond;
        cond.wakeAll();
        cond.wait(); // passes through
        cond.reset();
        QVERIFY(!cond.isReleased());
        QCOMPARE(cond.waitingCount(), 0u);

        auto f = std::async(std::launch::async, [&]() {
            cond.wait();
        });
        // the thread must block now, since the barrier was re-armed
        QVERIFY(f.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout);
        cond.wakeAll();
        QVERIFY(f.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
        QCOMPARE(cond.waitingCount(), 1u);
    }

    /**
     * Hammer the race window between a waiter checking the barrier state and parking
     * itself, while the releasing thread fires wakeAll() at an arbitrary moment.
     * With a lost wakeup, one of the iterations would block forever.
     */
    void noLostWakeupUnderRace()
    {
        const int iterations = 4000;
        for (int i = 0; i < iterations; ++i) {
            OptionalWaitCondition cond;
            std::atomic_bool go{false};

            auto waiter = std::async(std::launch::async, [&]() {
                while (!go.load(std::memory_order_acquire)) {
                }
                cond.wait();
            });
            auto releaser = std::async(std::launch::async, [&]() {
                while (!go.load(std::memory_order_acquire)) {
                }
                // vary the phase a bit between iterations
                for (int spin = 0; spin < (i % 7) * 10; ++spin)
                    std::atomic_signal_fence(std::memory_order_seq_cst);
                cond.wakeAll();
            });

            go.store(true, std::memory_order_release);
            releaser.wait();
            if (waiter.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
                QFAIL(qPrintable(QStringLiteral("Lost wakeup detected in iteration %1").arg(i)));
                // there is no way to recover a thread stuck forever, so give up
                std::terminate();
            }
        }
    }
};

} // namespace Syntalos

QTEST_MAIN(TestWaitCondition)

#include "test-waitcondition.moc"

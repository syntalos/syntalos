#include <QtTest>
#include <atomic>
#include <chrono>
#include <pthread.h>
#include <sched.h>
#include <thread>
#include <time.h>

#include "datactl/priv/rtkit.h"

class TestRtKit : public QObject
{
    Q_OBJECT

private slots:

    /**
     * Realtime threads that compute for longer than the RLIMIT_RTTIME budget must be
     * demoted by our SIGXCPU handler instead of getting the whole process SIGKILLed.
     */
    void overuseDemotesThreads()
    {
        RtKit rtkit;
        const auto maxRTTimeUsec = rtkit.queryRTTimeUSecMax();

        // burn CPU without ever sleeping, well past the hard limit
        const auto overrun = [&] {
            timespec start{};
            clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
            while (true) {
                timespec now{};
                clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
                const auto usec = (now.tv_sec - start.tv_sec) * 1000000LL + (now.tv_nsec - start.tv_nsec) / 1000;
                if (usec > maxRTTimeUsec + maxRTTimeUsec / 2)
                    break;
            }
            return sched_getscheduler(0) & ~SCHED_RESET_ON_FORK;
        };

        // both threads are realtime before the first one overruns, like in a real run
        std::atomic_bool firstDone = false;
        bool elevated2 = false;
        int policyAfter2 = -1;
        std::thread second([&] {
            pthread_setname_np(pthread_self(), "rt-spinner2");
            elevated2 = setCurrentThreadRealtime(1);
            while (!firstDone)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            if (elevated2)
                policyAfter2 = overrun();
        });

        bool elevated1 = false;
        int policyAfter1 = -1;
        std::thread([&] {
            pthread_setname_np(pthread_self(), "rt-spinner1");
            elevated1 = setCurrentThreadRealtime(1);
            if (elevated1)
                policyAfter1 = overrun();
        }).join();
        firstDone = true;
        second.join();

        if (!elevated1 || !elevated2)
            QSKIP("Could not obtain realtime scheduling (no rtkit?)");
        QCOMPARE(policyAfter1, SCHED_OTHER);
        QCOMPARE(policyAfter2, SCHED_OTHER);
    }
};

QTEST_MAIN(TestRtKit)
#include "test-rtkit.moc"

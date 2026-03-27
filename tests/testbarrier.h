#ifndef BARRIER_H
#define BARRIER_H

#include <QMutex>
#include <QSharedPointer>
#include <QWaitCondition>
#include <chrono>

class BarrierData
{
public:
    BarrierData(size_t count)
        : count(count),
          goal(count)
    {
        m_startTime = std::chrono::high_resolution_clock::now();
    }

    void wait()
    {
        mutex.lock();
        --count;
        if (count > 0)
            condition.wait(&mutex);
        else
            condition.wakeAll();

        auto finish = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = finish - m_startTime;
        m_timeElapsed = elapsed.count() * 1000;
        mutex.unlock();
    }

    void reset()
    {
        count = goal;
    }

    [[nodiscard]] double timeElapsed() const
    {
        return m_timeElapsed;
    }

private:
    Q_DISABLE_COPY(BarrierData)
    size_t count;
    size_t goal;
    QMutex mutex;
    QWaitCondition condition;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_startTime;
    double m_timeElapsed;
};

class Barrier
{
public:
    // Create a barrier that will wait for count threads
    Barrier(size_t count)
        : d(new BarrierData(count))
    {
    }
    void wait()
    {
        d->wait();
    }

    [[nodiscard]] double timeElapsed() const
    {
        return d->timeElapsed();
    }

private:
    std::unique_ptr<BarrierData> d;
};

#endif // BARRIER_H

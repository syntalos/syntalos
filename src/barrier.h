#ifndef BARRIER_H
#define BARRIER_H

#include <QMutex>
#include <QWaitCondition>
#include <QSharedPointer>

class BarrierData
{
public:
    BarrierData(int count) : count(count), goal(count) {}

    void wait()
    {
        mutex.lock();
        --count;
        if (count > 0)
            condition.wait(&mutex);
        else
            condition.wakeAll();
        mutex.unlock();
    }

    void reset()
    {
        count = goal;
    }

private:
    Q_DISABLE_COPY(BarrierData)
    int count;
    int goal;
    QMutex mutex;
    QWaitCondition condition;
};

class Barrier {
public:
    // Create a barrier that will wait for count threads
    Barrier(int count) : d(new BarrierData(count)) {}
    void wait()
    {
        d->wait();
    }

private:
    QSharedPointer<BarrierData> d;
};

#endif // BARRIER_H

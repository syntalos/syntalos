#include <QtTest>

#include "datactl/frametype.h"
#include "logging.h"
#include "streams/stream.h"

using namespace Syntalos;

class TestStreamLifecycle : public QObject
{
    Q_OBJECT

public:
    TestStreamLifecycle()
    {
        initializeSyLogSystem(quill::LogLevel::Debug);
    }

private slots:
    void suspensionSurvivesStart()
    {
        DataStream<Frame> stream;
        auto subscription = stream.subscribe();

        subscription->suspend();
        stream.start();

        for (uint64_t i = 0; i < 4; ++i)
            stream.push(Frame(i));
        QCOMPARE(subscription->approxPendingCount(), size_t(0));

        subscription->resume();
        stream.push(Frame(4));
        QCOMPARE(subscription->approxPendingCount(), size_t(1));
    }

    void suspensionSurvivesClearPending()
    {
        DataStream<Frame> stream;
        auto subscription = stream.subscribe();
        stream.start();

        subscription->suspend();
        subscription->clearPending();
        stream.push(Frame(0));
        QCOMPARE(subscription->approxPendingCount(), size_t(0));

        subscription->resume();
        stream.push(Frame(1));
        QCOMPARE(subscription->approxPendingCount(), size_t(1));
    }

    void suspensionSurvivesThrottleChange()
    {
        DataStream<Frame> stream;
        auto subscription = stream.subscribe();
        stream.start();

        subscription->suspend();
        subscription->setThrottleItemsPerSec(1);
        stream.push(Frame(0));
        QCOMPARE(subscription->approxPendingCount(), size_t(0));

        subscription->resume();
        subscription->setThrottleItemsPerSec(0);
        stream.push(Frame(1));
        QCOMPARE(subscription->approxPendingCount(), size_t(1));
    }
};

QTEST_MAIN(TestStreamLifecycle)
#include "test-stream-lifecycle.moc"

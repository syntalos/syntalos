
#include <iostream>
#include <QtTest>
#include <QDebug>

#include "hrclock.h"

int slow_work_with_result(int para)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return 40 + para;
}

class TestTimer : public QObject
{
    Q_OBJECT
private slots:
    void runFuncTimer()
    {
        std::unique_ptr<HRTimer> timer(new HRTimer());

        timer->start();

        auto res = TIMER_FUNC_TIMESTAMP(timer, slow_work_with_result(2));
        std::this_thread::sleep_for(std::chrono::milliseconds(12));
        QVERIFY((res.count() < 251) && (res.count() > 249));

        QVERIFY(timer->timeSinceStartMsec().count() >= 512);
    }
};

QTEST_MAIN(TestTimer)
#include "test-timer.moc"

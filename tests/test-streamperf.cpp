
#include <QtTest>
#include <atomic>
#include <iostream>
#include <opencv2/imgproc.hpp>
#include <pthread.h>
#include <thread>

#include "symemopt.h"
#include "datactl/frametype.h"
#include "streams/stream.h"
#include "testbarrier.h"

static const int N_OF_DATAFRAMES = 2000;

using namespace Syntalos;

static cv::Mat process_data_instant(const Frame &data)
{
    return data.mat;
}

static cv::Mat process_data_fast(const Frame &data)
{
    auto result = data.mat.clone();
    cv::blur(result, result, cv::Size(5, 5));

    return result;
}

static cv::Mat process_data_slow(const Frame &data)
{
    for (uint i = 0; i < 4; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return process_data_fast(data);
}

static Frame transform_data_fast(const Frame &data, size_t id)
{
    Frame newData;

    newData.mat = process_data_fast(data);
    cv::putText(
        newData.mat,
        std::string("E ") + std::to_string(id),
        cv::Point(24, 320),
        cv::FONT_HERSHEY_COMPLEX,
        1.5,
        cv::Scalar(140, 140, 255));
    newData.index = id;

    return newData;
}

static Frame create_data_200Hz(size_t index)
{
    Frame data;
    data.index = index;

    cv::Mat frame(cv::Size(800, 600), CV_8UC3);
    frame.setTo(cv::Scalar(67, 42, 30));
    cv::putText(
        frame,
        std::string("Frame ") + std::to_string(index),
        cv::Point(24, 240),
        cv::FONT_HERSHEY_COMPLEX,
        1.5,
        cv::Scalar(255, 255, 255));
    data.mat = frame;

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    return data;
}

static void producer_fast(const std::string &threadName, Barrier *barrier, DataStream<Frame> *stream)
{
    pthread_setname_np(pthread_self(), threadName.c_str());

    barrier->wait();
    for (size_t i = 1; i <= N_OF_DATAFRAMES; ++i) {
        auto data = create_data_200Hz(i);
        stream->push(data);
    }
    stream->terminate();
}

static void consumer_fast(const std::string &threadName, Barrier *barrier, DataStream<Frame> *stream)
{
    pthread_setname_np(pthread_self(), threadName.c_str());
    size_t lastId = 0;

    auto sub = stream->subscribe();
    barrier->wait();
    while (true) {
        auto data = sub->next();
        if (!data.has_value())
            break; // subscription has been terminated
        auto result = process_data_fast(data.value());

        // display_frame(result, sub->name());

        if (data->index != (lastId + 1))
            std::cout << "Value dropped (fast consumer) [" << data->index << "]" << std::endl;
        lastId = data->index;
    }
    if (lastId != N_OF_DATAFRAMES)
        std::cout << "Fast consumer received only " << lastId << " data elements out of " << N_OF_DATAFRAMES
                  << std::endl;
}

static void consumer_slow(const std::string &threadName, Barrier *barrier, DataStream<Frame> *stream)
{
    pthread_setname_np(pthread_self(), threadName.c_str());
    size_t lastId = 0;

    auto sub = stream->subscribe();
    barrier->wait();
    while (true) {
        auto data = sub->next();
        if (!data.has_value())
            break; // subscription has been terminated
        process_data_slow(data.value());

        if (data->index != (lastId + 1))
            std::cout << "Value dropped (slow consumer) [" << data->index << "]" << std::endl;
        lastId = data->index;
    }

    if (lastId != N_OF_DATAFRAMES)
        std::cout << "Slow consumer received only " << lastId << " data elements out of " << N_OF_DATAFRAMES
                  << std::endl;
}

static void consumer_instant(const std::string &threadName, Barrier *barrier, DataStream<Frame> *stream)
{
    pthread_setname_np(pthread_self(), threadName.c_str());

    auto sub = stream->subscribe();
    barrier->wait();
    while (true) {
        auto data = sub->next();
        if (!data.has_value())
            break; // subscription has been terminated
        process_data_instant(data.value());
    }
}

static void transformer_fast(
    const std::string &threadName,
    Barrier *barrier,
    DataStream<Frame> *recvStream,
    DataStream<Frame> *prodStream)
{
    pthread_setname_np(pthread_self(), threadName.c_str());
    size_t count = 1;

    auto sub = recvStream->subscribe();
    barrier->wait();
    while (true) {
        auto data = sub->next();
        if (!data.has_value())
            break; // subscription has been terminated
        auto newData = transform_data_fast(data.value(), count);
        // display_frame(newData.frame, sub->name());

        prodStream->push(newData);
        count++;
    }
    prodStream->terminate();
}

class TestStreamPerf : public QObject
{
    Q_OBJECT

public:
    TestStreamPerf()
    {
        // we want to use mimalloc for this (threaded) test
        configureMimallocDefaultAllocator();

        // we also need the logging system initialized
        initializeSyLogSystem(quill::LogLevel::Debug);
    }

private slots:
    void run6threads()
    {
        Barrier barrier(6);

        std::vector<std::thread> threads;
        std::shared_ptr<DataStream<Frame>> prodStream(new DataStream<Frame>());
        std::shared_ptr<DataStream<Frame>> transStream(new DataStream<Frame>());

        QBENCHMARK {
            threads.push_back(std::thread(producer_fast, "producer", &barrier, prodStream.get()));
            threads.push_back(std::thread(consumer_fast, "consumer_fast", &barrier, prodStream.get()));
            threads.push_back(std::thread(consumer_slow, "consumer_slow", &barrier, prodStream.get()));
            threads.push_back(std::thread(consumer_instant, "consumer_instant", &barrier, prodStream.get()));

            threads.push_back(
                std::thread(transformer_fast, "transformer", &barrier, prodStream.get(), transStream.get()));
            threads.push_back(std::thread(consumer_fast, "consumer_tfo", &barrier, transStream.get()));

            for (auto &t : threads)
                t.join();
        }
    }

    void runOvercapacity()
    {
        std::vector<std::thread> threads;
        const size_t threadCount = std::thread::hardware_concurrency() * 2 + 2;
        Barrier barrier(threadCount);

        auto prodStream = std::make_shared<DataStream<Frame>>();
        auto transStream = std::make_shared<DataStream<Frame>>();

        QBENCHMARK {
            threads.emplace_back(producer_fast, "producer", &barrier, prodStream.get());
            threads.emplace_back(consumer_fast, "consumer_fast", &barrier, prodStream.get());
            threads.emplace_back(consumer_instant, "consumer_instant", &barrier, prodStream.get());

            threads.emplace_back(transformer_fast, "transformer", &barrier, prodStream.get(), transStream.get());

            for (uint i = 0; i < threadCount - 4; ++i) {
                // we connect half of the regular consumers to the producer, the rest goes to the transformer
                if ((i % 2) == 0)
                    threads.emplace_back(
                        consumer_fast, std::string("consumer_raw_") + std::to_string(i), &barrier, prodStream.get());
                else
                    threads.emplace_back(
                        consumer_fast, std::string("consumer_tf_") + std::to_string(i), &barrier, transStream.get());
            }

            std::cout << "Running " << threads.size() << " threads." << std::endl;

            for (auto &t : threads)
                t.join();
        }
    }
};

QTEST_MAIN(TestStreamPerf)
#include "test-streamperf.moc"

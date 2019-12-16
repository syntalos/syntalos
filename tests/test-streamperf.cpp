
#include <iostream>
#include <thread>
#include <atomic>
#include <pthread.h>
#include <opencv2/imgproc.hpp>
#include <QtTest>

#include "barrier.h"
#include "streams/stream.h"

static const int N_OF_DATAFRAMES = 2000;

typedef struct
{
    size_t id;
    time_t timestamp;
    cv::Mat frame;
} MyDataFrame;
Q_DECLARE_METATYPE(MyDataFrame)

cv::Mat process_data_instant(const MyDataFrame &data)
{
    return data.frame;
}

cv::Mat process_data_fast(const MyDataFrame &data)
{
    auto result = data.frame.clone();
    cv::blur(result, result, cv::Size(5, 5));

    return result;
}

cv::Mat process_data_slow(const MyDataFrame &data)
{
    for (uint i = 0; i < 4; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return process_data_fast(data);
}

MyDataFrame transform_data_fast(const MyDataFrame &data, size_t id)
{
    MyDataFrame newData;

    newData.frame = process_data_fast(data);
    cv::putText(newData.frame,
                std::string("E ") + std::to_string(id),
                cv::Point(24, 320),
                cv::FONT_HERSHEY_COMPLEX,
                1.5,
                cv::Scalar(140, 140, 255));
    newData.id = id;

    return newData;
}

MyDataFrame create_data_200Hz(size_t index)
{
    MyDataFrame data;
    data.id = index;

    cv::Mat frame(cv::Size(800, 600), CV_8UC3);
    frame.setTo(cv::Scalar(67, 42, 30));
    cv::putText(frame,
                std::string("Frame ") + std::to_string(index),
                cv::Point(24, 240),
                cv::FONT_HERSHEY_COMPLEX,
                1.5,
                cv::Scalar(255,255,255));
    data.frame = frame;

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    return data;
}

void producer_fast(const std::string& threadName, Barrier *barrier, DataStream<MyDataFrame> *stream)
{
    pthread_setname_np(pthread_self(), threadName.c_str());

    barrier->wait();
    for (size_t i = 1; i <= N_OF_DATAFRAMES; ++i) {
        auto data = create_data_200Hz(i);
        stream->push(data);
    }
    stream->terminate();
}

void consumer_fast(const std::string& threadName, Barrier *barrier, DataStream<MyDataFrame> *stream)
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

        //display_frame(result, sub->name());

        if (data->id != (lastId + 1))
            std::cout << "Value dropped (fast consumer) [" << data->id << "]" << std::endl;
        lastId = data->id;
    }
    if (lastId != N_OF_DATAFRAMES)
        std::cout << "Fast consumer received only " << lastId << " data elements out of " << N_OF_DATAFRAMES << std::endl;
}

void consumer_slow(const std::string& threadName, Barrier *barrier, DataStream<MyDataFrame> *stream)
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

        if (data->id != (lastId + 1))
            std::cout << "Value dropped (slow consumer) [" << data->id << "]" << std::endl;
        lastId = data->id;
    }

    if (lastId != N_OF_DATAFRAMES)
        std::cout << "Slow consumer received only " << lastId << " data elements out of " << N_OF_DATAFRAMES << std::endl;
}

void consumer_instant(const std::string& threadName, Barrier *barrier, DataStream<MyDataFrame> *stream)
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

void transformer_fast(const std::string& threadName, Barrier *barrier, DataStream<MyDataFrame> *recvStream, DataStream<MyDataFrame> *prodStream)
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
        //display_frame(newData.frame, sub->name());

        prodStream->push(newData);
        count++;
    }
    prodStream->terminate();
}

class TestStreamPerf : public QObject
{
    Q_OBJECT
private slots:
    void run6threads()
    {
        Barrier barrier(6);

        std::vector<std::thread> threads;
        std::shared_ptr<DataStream<MyDataFrame>> prodStream(new DataStream<MyDataFrame>());
        std::shared_ptr<DataStream<MyDataFrame>> transStream(new DataStream<MyDataFrame>());

        QBENCHMARK {
            threads.push_back(std::thread(producer_fast, "producer", &barrier, prodStream.get()));
            threads.push_back(std::thread(consumer_fast, "consumer_fast", &barrier, prodStream.get()));
            threads.push_back(std::thread(consumer_slow, "consumer_slow", &barrier, prodStream.get()));
            threads.push_back(std::thread(consumer_instant, "consumer_instant", &barrier, prodStream.get()));

            threads.push_back(std::thread(transformer_fast, "transformer", &barrier, prodStream.get(), transStream.get()));
            threads.push_back(std::thread(consumer_fast, "consumer_tfo", &barrier, transStream.get()));

            for(auto& t: threads)
                t.join();
        }
    }

    void runOvercapacity()
    {
        std::vector<std::thread> threads;
        const auto threadCount = std::thread::hardware_concurrency() * 2 + 2;
        Barrier barrier(threadCount);

        std::shared_ptr<DataStream<MyDataFrame>> prodStream(new DataStream<MyDataFrame>());
        std::shared_ptr<DataStream<MyDataFrame>> transStream(new DataStream<MyDataFrame>());

        QBENCHMARK {
            threads.push_back(std::thread(producer_fast, "producer", &barrier, prodStream.get()));
            threads.push_back(std::thread(consumer_fast, "consumer_fast", &barrier, prodStream.get()));
            threads.push_back(std::thread(consumer_instant, "consumer_instant", &barrier, prodStream.get()));

            threads.push_back(std::thread(transformer_fast, "transformer", &barrier, prodStream.get(), transStream.get()));

            for (uint i = 0; i < threadCount - 4; ++i) {
                // we connect half of the regular consumers to the producer, the rest goes to the transformer
                if ((i % 2) == 0)
                    threads.push_back(std::thread(consumer_fast, std::string("consumer_raw_") + std::to_string(i), &barrier, prodStream.get()));
                else
                    threads.push_back(std::thread(consumer_fast, std::string("consumer_tf_") + std::to_string(i), &barrier, transStream.get()));
            }

            std::cout << "Running " << threads.size() << " threads." << std::endl;

            for(auto& t: threads)
                t.join();
        }
    }
};

QTEST_MAIN(TestStreamPerf)
#include "test-streamperf.moc"

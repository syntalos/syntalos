/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <barrier>
#include <cstdlib>
#include <thread>

#include "datactl/frametype.h"

using namespace Syntalos;

namespace
{

/**
 * Perform the refcount part of cv::Mat::release() in this translation unit.
 *
 * Distribution OpenCV libraries are not normally instrumented by ThreadSanitizer.
 * Keeping OpenCV's exact CV_XADD operation visible here lets the sanitizer observe
 * a subscriber releasing its shallow copy concurrently with Syntalos deciding whether
 * to recycle the producer's buffer.
 */
void releaseSubscriber(cv::Mat &subscriber)
{
    if (subscriber.u && CV_XADD(&subscriber.u->refcount, -1) == 1)
        subscriber.deallocate();

    subscriber.u = nullptr;
    subscriber.datastart = subscriber.dataend = subscriber.datalimit = subscriber.data = nullptr;
    for (int i = 0; i < subscriber.dims; ++i)
        subscriber.size.p[i] = 0;
}

} // namespace

int main()
{
    cv::Mat producer(8, 8, CV_8UC1);
    cv::Mat subscriber = producer;
    std::barrier startLine(2);

    std::thread subscriberThread([&subscriber, &startLine] {
        startLine.arrive_and_wait();
        releaseSubscriber(subscriber);
    });

    startLine.arrive_and_wait();
    matEnsureExclusive(producer, producer.rows, producer.cols, producer.type());
    subscriberThread.join();

    return producer.u != nullptr && producer.u->refcount == 1 ? EXIT_SUCCESS : EXIT_FAILURE;
}

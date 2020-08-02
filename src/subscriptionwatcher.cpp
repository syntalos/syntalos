/*
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include "subscriptionwatcher.h"

#include <sys/eventfd.h>
#include <sys/epoll.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
class SubscriptionWatcher::Private
{
public:
    Private() { }
    ~Private() { }

    bool valid;
    int epollFD;
    std::vector<std::shared_ptr<VariantStreamSubscription>> subs;
};
#pragma GCC diagnostic pop

std::optional<std::unique_ptr<SubscriptionWatcher>> SubscriptionWatcher::construct(std::initializer_list<std::shared_ptr<VariantStreamSubscription>> subscriptions)
{
    std::unique_ptr<SubscriptionWatcher> watcher(new SubscriptionWatcher(subscriptions));
    if (watcher->isValid())
        return watcher;
    return std::nullopt;
}

bool SubscriptionWatcher::isValid() const
{
    return d->valid;
}

SubscriptionWatcher::WaitResult SubscriptionWatcher::wait()
{
    // skip all the epoll waiting in case we already have new data
    for (const auto sub : d->subs) {
        if (sub->hasPending())
            return NEWDATA;
    }

    // watch for new data
    const int timeout = 40000; // 40msec
    struct epoll_event events[10];
    int ret;
    uint64_t count = 0;

    while (true) {
        ret = epoll_wait(d->epollFD, &events[0], 10, timeout);
        if (ret > 0) {
            int i = 0;
            bool newData = false;
            for (; i < ret; i++) {
                if (events[i].events & EPOLLHUP) {
                    return DONE;
                } else if (events[i].events & EPOLLERR) {
                    qWarning("Eventfd has epoll error");
                  //  return ERROR;
                } else if (events[i].events & EPOLLIN) {
                    auto efd = events[i].data.fd;
                    ret = read(efd, &count, sizeof(count));
                    if (ret < 0)
                        qDebug("Eventfd read failed: %s", std::strerror(errno));

                    newData = true;
                }
            }
            if (newData)
                return NEWDATA;

        } else if (ret == 0) {
            // we hit a timeout, check if we got any data in subscriptions, just in case
            for (const auto sub : d->subs) {
                if (sub->hasPending())
                    return NEWDATA;
            }

            // continue blocking indefinitely
            continue;
        } else {
            qCritical("Error during epoll wait: %s", std::strerror(errno));
            return ERROR;
        }
    }
}

SubscriptionWatcher::SubscriptionWatcher(std::initializer_list<std::shared_ptr<VariantStreamSubscription> > subscriptions)
    : d(new SubscriptionWatcher::Private)
{
    d->valid = false;
    d->epollFD = -1;

    d->epollFD = epoll_create1(EPOLL_CLOEXEC);
    if (d->epollFD < 0) {
        qCritical("Unable to create epoll: %s", std::strerror(errno));
        return;
    }

    // add eventfds to the list of watched file descriptors
    for (const auto sub : subscriptions) {
        const auto efd = sub->enableNotify();
        qDebug() << "Enabled notify for" << sub->dataTypeName() << "subscription";

        struct epoll_event revent;
        revent.events = EPOLLHUP | EPOLLERR | EPOLLIN;
        revent.data.fd = efd;

        if (epoll_ctl(d->epollFD, EPOLL_CTL_ADD, efd, &revent) < 0) {
            qCritical("Unable to add eventfd epoll watch: %s", std::strerror(errno));
            close(d->epollFD);
            d->epollFD = -1;
            return;
        }

        d->subs.push_back(sub);
    }

    d->valid = true;
}

SubscriptionWatcher::~SubscriptionWatcher()
{
    d->subs.clear();
    if (d->epollFD >= 0)
        close(d->epollFD);
}

/*
 * Copyright (C) 2020-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "subscriptionnotifier.h"

#include <glib.h>
#include <QSocketNotifier>

#include "streams/stream.h"

using namespace Syntalos;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
class SubscriptionNotifier::Private
{
public:
    Private() {}
    ~Private() {}

    std::unique_ptr<QSocketNotifier> notifier;
    std::shared_ptr<VariantStreamSubscription> sub;
};
#pragma GCC diagnostic pop

SubscriptionNotifier::SubscriptionNotifier(
    const std::shared_ptr<VariantStreamSubscription> &subscription,
    QObject *parent)
    : QObject(parent),
      d(new SubscriptionNotifier::Private)
{
    d->sub = subscription;

    auto efd = d->sub->enableNotify();
    d->notifier = std::make_unique<QSocketNotifier>(efd, QSocketNotifier::Read);
    connect(
        d->notifier.get(),
        &QSocketNotifier::activated,
        [this, efd](QSocketDescriptor socket, QSocketNotifier::Type type) {
            uint64_t buffer;
            if (read(efd, &buffer, sizeof(buffer)) == -1 && errno != EAGAIN)
                qWarning().noquote() << "subscription-notifier: Failed to read from eventfd:" << std::strerror(errno);

            Q_EMIT this->dataReceived();
        });
}

SubscriptionNotifier::~SubscriptionNotifier()
{
    if (d->sub)
        d->sub->disableNotify();
}

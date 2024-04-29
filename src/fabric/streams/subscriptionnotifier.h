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

#pragma once

#include "moduleapi.h"
#include "optionalwaitcondition.h"
#include <QObject>

namespace Syntalos
{

/**
 * @brief Notifies about new data on a stream subscription in a Qt event loop.
 *
 * Using this class, an event loop can listen for incoming data on a Syntalos
 * stream subscription and trigger a function when new data is received.
 */
class SubscriptionNotifier : public QObject
{
    Q_OBJECT
public:
    explicit SubscriptionNotifier(
        const std::shared_ptr<VariantStreamSubscription> &subscription,
        QObject *parent = nullptr);
    ~SubscriptionNotifier();

signals:
    void dataReceived();

private:
    class Private;
    Q_DISABLE_COPY(SubscriptionNotifier)
    QScopedPointer<Private> d;
};

} // namespace Syntalos

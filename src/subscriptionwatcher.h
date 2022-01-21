/*
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#include "moduleapi.h"

/**
 * @brief Helper to efficiently watch for new data in multiple subscriptions
 */
class SubscriptionWatcher
{
public:
    enum WaitResult {
        NEWDATA,
        DONE,
        ERROR,
    };

    static std::optional<std::unique_ptr<SubscriptionWatcher>>
        construct(std::initializer_list<std::shared_ptr<VariantStreamSubscription>> subscriptions);
    ~SubscriptionWatcher();

    bool isValid() const;

    WaitResult wait();

private:
    class Private;
    Q_DISABLE_COPY(SubscriptionWatcher)
    std::unique_ptr<Private> d;

    explicit SubscriptionWatcher(std::initializer_list<std::shared_ptr<VariantStreamSubscription>> subscriptions);
};

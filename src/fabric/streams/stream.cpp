/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "stream.h"

#include "datactl/datatypes.h"
#include "datactl/frametype.h"

VariantStreamSubscription::~VariantStreamSubscription() = default;

VariantDataStream::~VariantDataStream() = default;

bool checkStreamTypesCompatible(int fromTypeId, int toTypeId)
{
    if (fromTypeId == toTypeId)
        return true;

    return forEachStreamType([&](auto fromTag) {
        using From = typename decltype(fromTag)::type;
        if (From::staticTypeId() != fromTypeId)
            return false;
        return forEachStreamType([&](auto toTag) {
            using To = typename decltype(toTag)::type;
            // Accepts either a `To(const From &)` or a `To(From &&)` converting constructor
            if constexpr (!std::same_as<From, To> && std::constructible_from<To, From>)
                return To::staticTypeId() == toTypeId;
            else
                return false;
        });
    });
}

std::shared_ptr<VariantStreamSubscription> wrapSubscriptionForType(
    std::shared_ptr<VariantStreamSubscription> sub,
    int targetTypeId)
{
    if (!sub || sub->dataTypeId() == targetTypeId)
        return sub;

    std::shared_ptr<VariantStreamSubscription> wrapped;
    forEachStreamType([&](auto fromTag) {
        using From = typename decltype(fromTag)::type;
        if (From::staticTypeId() != sub->dataTypeId())
            return false;
        return forEachStreamType([&](auto toTag) {
            using To = typename decltype(toTag)::type;
            // Accepts either a `To(const From &)` or a `To(From &&)` converting constructor
            if constexpr (!std::same_as<From, To> && std::constructible_from<To, From>) {
                if (To::staticTypeId() != targetTypeId)
                    return false;
                auto inner = std::dynamic_pointer_cast<StreamSubscription<From>>(sub);
                if (!inner)
                    return false;
                wrapped = std::make_shared<StreamSubscriptionAdapter<From, To>>(std::move(inner));
                return true;
            } else {
                return false;
            }
        });
    });
    return wrapped ? wrapped : sub;
}

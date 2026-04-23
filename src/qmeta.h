/*
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include <QMetaType>
#include <chrono>
#include <syntalos-datactl>

Q_DECLARE_METATYPE(std::chrono::milliseconds);
Q_DECLARE_METATYPE(std::chrono::microseconds);
Q_DECLARE_METATYPE(std::chrono::nanoseconds);

Q_DECLARE_METATYPE(Syntalos::Uuid);
Q_DECLARE_METATYPE(Syntalos::TimeSyncStrategies);

namespace Syntalos
{

/**
 * @brief Registers custom meta types with the Qt meta-object system.
 *
 * This method is used to register Syntalos types with the Qt type system
 * to enable features such as QVariant type conversions, signal-slot
 * communication, and property management for these custom types.
 */
void registerSyMetaTypes();

} // namespace Syntalos

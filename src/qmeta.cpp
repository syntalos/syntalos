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

#include "qmeta.h"

namespace Syntalos
{

void registerSyMetaTypes()
{
    // Synchronizers and other timer-dependent classes may need to send these types
    // in queued connections, so we ensure metatypes are registered for them
    qRegisterMetaType<std::chrono::milliseconds>();
    qRegisterMetaType<std::chrono::microseconds>();
    qRegisterMetaType<std::chrono::nanoseconds>();

    qRegisterMetaType<Syntalos::Uuid>();
    qRegisterMetaType<Syntalos::TimeSyncStrategies>();
}

} // namespace Syntalos

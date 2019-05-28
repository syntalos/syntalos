/*
 * Copyright (C) 2019 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef RPCSHAREDINFO_H
#define RPCSHAREDINFO_H

#include <QObject>

enum class MaPyFunction : uint
{
    X_UNKNOWN,

    G_getPythonScript,
    G_canStart,
    G_timeSinceStartMsec,

    T_newEventTable,
    T_setHeader,
    T_addEvent,

    F_getFirmataModuleId,
    F_newDigitalPin,
    F_fetchDigitalInput,
    F_pinSetValue,
    F_pinSignalPulse
};

#endif // RPCSHAREDINFO_H

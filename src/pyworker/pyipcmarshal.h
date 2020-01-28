/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#include <memory>
#include <QVariant>
#include <boost/python.hpp>

class SharedMemory;
using namespace boost;

struct FrameData
{
    time_t time_msec;
    python::object mat;
};

enum CtlCommandKind {
    CTL_UNKNOWN  = 0,
    CTL_START = 1,
    CTL_STOP = 2,
    CTL_STEP = 3,
    CTL_CUSTOM = 4
};

struct ControlCommandPy
{
    CtlCommandKind kind;
    std::string command;
};

python::object unmarshalDataToPyObject(int typeId, const QVariantList &params, std::unique_ptr<SharedMemory> &shm);

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

#include "streams/datatypes.h"

class SharedMemory;
using namespace boost;

struct PyFrame
{
    time_t time_msec;
    python::object mat;
};

python::object unmarshalDataToPyObject(int typeId, const QVariantList &params, std::unique_ptr<SharedMemory> &shm);
bool marshalPyDataElement(int typeId, const boost::python::api::object &pyObj, QVariantList &params, std::unique_ptr<SharedMemory> &shm);

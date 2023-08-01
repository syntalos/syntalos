/*
 * Copyright (C) 2019-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include <QVariant>
#include <memory>
#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>

#include "streams/datatypes.h"

namespace py = pybind11;

class SharedMemory;

py::object unmarshalDataToPyObject(int typeId, const QVariant &argData, std::unique_ptr<SharedMemory> &shm);
bool marshalPyDataElement(int typeId, const py::object &pyObj, QVariant &argData, std::unique_ptr<SharedMemory> &shm);

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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>
#include <pybind11/eigen.h>

#include "cvmatndsliceconvert.h"
#include "ipcmarshal.h"
#include "pyipcmarshal.h"

/**
 * @brief Create a Python object from received data.
 */
py::object unmarshalDataToPyObject(int typeId, const QVariant &argData, std::unique_ptr<SharedMemory> &shm)
{
    /**
     ** Frame
     **/

    if (typeId == qMetaTypeId<Frame>()) {
        auto floatingMat = cvMatFromShm(shm, false);

        Frame frame;
        frame.mat = floatingMat;

        const auto plist = argData.toList();
        if (plist.length() == 2) {
            frame.index = plist[0].toUInt();
            frame.time = milliseconds_t(plist[1].toLongLong());
        }

        // floating mat gets copied here, for use in Python
        return py::cast(frame);
    }

    if (!argData.isValid())
        return py::none();

    /**
     ** Control Command
     **/

    if (typeId == qMetaTypeId<ControlCommand>())
        return py::cast(qvariant_cast<ControlCommand>(argData));

    /**
     ** Firmata
     **/

    if (typeId == qMetaTypeId<FirmataControl>())
        return py::cast(qvariant_cast<FirmataControl>(argData));

    if (typeId == qMetaTypeId<FirmataData>())
        return py::cast(qvariant_cast<FirmataData>(argData));

    /**
     ** Table Rows
     **/

    if (typeId == qMetaTypeId<TableRow>()) {
        auto rows = argData.toList();
        py::list pyRow;
        for (const QVariant &colVar : rows) {
            const auto col = colVar.toString();
            pyRow.append(col.toStdString());
        }
        return pyRow;
    }

    /**
     ** Signal Blocks
     **/

    if (typeId == qMetaTypeId<IntSignalBlock>())
        return py::cast(qvariant_cast<IntSignalBlock>(argData));
    if (typeId == qMetaTypeId<FloatSignalBlock>())
        return py::cast(qvariant_cast<FloatSignalBlock>(argData));

    return py::none();
}

template<typename T>
static bool marshalAndAddSimple(const int &typeId, const py::object &pyObj, QVariant &argData)
{
    if (typeId == qMetaTypeId<T>()) {
        const T etype = pyObj.cast<T>();
        argData = QVariant::fromValue(etype);
        return true;
    }
    return false;
}

/**
 * @brief Prepare data from a Python object for transmission.
 */
bool marshalPyDataElement(int typeId, const py::object &pyObj, QVariant &argData, std::unique_ptr<SharedMemory> &shm)
{
    /**
     ** Frame
     **/

    if (typeId == qMetaTypeId<Frame>()) {
        const auto frame = pyObj.cast<Frame>();

        if (!cvMatToShm(shm, frame.mat))
            return false;

        QVariantList plist;
        plist.reserve(2);
        plist.append((uint)frame.index);
        plist.append(QVariant::fromValue(frame.time.count()));
        argData = QVariant::fromValue(plist);
        return true;
    }

    /**
     ** Control Command
     **/

    if (marshalAndAddSimple<ControlCommand>(typeId, pyObj, argData))
        return true;

    /**
     ** Firmata
     **/

    if (marshalAndAddSimple<FirmataControl>(typeId, pyObj, argData))
        return true;
    if (marshalAndAddSimple<FirmataData>(typeId, pyObj, argData))
        return true;

    /**
     ** Table Rows
     **/

    if (typeId == qMetaTypeId<TableRow>()) {
        const py::list pyList = pyObj.cast<py::list>();
        const auto pyListLen = py::len(pyList);

        TableRow row;
        row.reserve(pyListLen);
        for (size_t i = 0; i < pyListLen; i++) {
            const auto lo = pyList[i];
            if (PyLong_CheckExact(lo.ptr())) {
                const auto value = lo.cast<long>();
                row.append(QString::number(value));
            } else if (PyFloat_CheckExact(lo.ptr())) {
                const auto value = lo.cast<double>();
                row.append(QString::number(value));
            } else if (PyUnicode_CheckExact(lo.ptr())) {
                row.append(QString::fromStdString(lo.cast<std::string>()));
            } else {
                try {
                    row.append(QString::number(lo.cast<milliseconds_t>().count()));
                } catch (const py::cast_error &) {
                    // try string again, as last resort
                    row.append(QString::fromStdString(lo.cast<std::string>()));
                }
            }
        }
        argData = QVariant::fromValue(row);
        return true;
    }

    /**
     ** Signal Blocks
     **/

    if (marshalAndAddSimple<IntSignalBlock>(typeId, pyObj, argData))
        return true;
    if (marshalAndAddSimple<FloatSignalBlock>(typeId, pyObj, argData))
        return true;

    return false;
}

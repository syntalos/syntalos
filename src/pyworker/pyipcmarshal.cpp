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

#include <boost/python.hpp>

#include "pyipcmarshal.h"
#include "ipcmarshal.h"
#include "cvmatndsliceconvert.h"

using namespace boost;

/**
 * @brief Create a Python object from received data.
 */
python::object unmarshalDataToPyObject(int typeId, const QVariant &argData, std::unique_ptr<SharedMemory> &shm)
{
    /**
     ** Frame
     **/

    if (typeId == qMetaTypeId<Frame>()) {
        auto floatingMat = cvMatFromShm(shm, false);
        auto matPyO = cvMatToNDArray(floatingMat);

        PyFrame pyFrame;
        pyFrame.mat = boost::python::object(boost::python::handle<>(matPyO));

        const auto plist = argData.toList();
        if (plist.length() == 2) {
            pyFrame.index = plist[0].toUInt();
            pyFrame.time_msec = plist[1].toLongLong();
        }

        return python::object(pyFrame);
    }

    if (!argData.isValid())
        return python::object();

    /**
     ** Control Command
     **/

    if (typeId == qMetaTypeId<ControlCommand>())
        return python::object(qvariant_cast<ControlCommand>(argData));

    /**
     ** Firmata
     **/

    if (typeId == qMetaTypeId<FirmataControl>())
        return python::object(qvariant_cast<FirmataControl>(argData));

    if (typeId == qMetaTypeId<FirmataData>())
        return python::object(qvariant_cast<FirmataData>(argData));

    /**
     ** Table Rows
     **/

    if (typeId == qMetaTypeId<TableRow>()) {
        auto rows = argData.toList();
        python::list pyRow;
        for (const QVariant &colVar : rows) {
            const auto col = colVar.toString();
            pyRow.append(col.toStdString());
        }
        return std::move(pyRow);
    }

    return python::object();
}

template<typename T>
static bool marshalAndAddSimple(const int &typeId, const boost::python::object &pyObj, QVariant &argData)
{
    if (typeId == qMetaTypeId<T>()) {
        const T etype = python::extract<T>(pyObj);
        argData = QVariant::fromValue(etype);
        return true;
    }
    return false;
}

/**
 * @brief Prepare data from a Python object for transmission.
 */
bool marshalPyDataElement(int typeId, const boost::python::object &pyObj,
                          QVariant &argData, std::unique_ptr<SharedMemory> &shm)
{
    /**
     ** Frame
     **/

    if (typeId == qMetaTypeId<Frame>()) {
        python::object pyMat = python::extract<python::object>(pyObj.attr("mat"));
        auto mat = cvMatFromNdArray(pyMat.ptr());
        if (!cvMatToShm(shm, mat))
            return false;

        const uint index = python::extract<uint>(pyObj.attr("index"));
        const long time_msec = python::extract<long>(pyObj.attr("time_msec"));

        QVariantList plist;
        plist.reserve(2);
        plist.append(index);
        plist.append(QVariant::fromValue(time_msec));
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
        const python::list pyList = python::extract<python::list>(pyObj);
        const auto pyListLen = python::len(pyList);
        if (pyListLen < 0)
            return true;
        TableRow row;
        row.reserve(pyListLen);
        for (ssize_t i = 0; i < pyListLen; i++) {
            const auto loP = pyList[i];
            const python::object lo = python::extract<python::object>(loP);
            if (PyLong_CheckExact(lo.ptr())) {
                const long value = python::extract<long>(lo.ptr());
                row.append(QString::number(value));
            } else {
                const auto value = python::extract<std::string>(lo);
                row.append(QString::fromStdString(value));
            }
        }
        argData = QVariant::fromValue(row);
        return true;
    }

    return false;
}

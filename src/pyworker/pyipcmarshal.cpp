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
python::object unmarshalDataToPyObject(int typeId, const QVariantList &params, std::unique_ptr<SharedMemory> &shm)
{
    /**
     ** Frame
     **/

    if (typeId == qMetaTypeId<Frame>()) {
        auto floatingMat = cvMatFromShm(shm, false);
        auto matPyO = cvMatToNDArray(floatingMat);

        PyFrame pyFrame;
        pyFrame.index = params[0].toUInt();
        pyFrame.mat = boost::python::object(boost::python::handle<>(matPyO));
        pyFrame.time_msec = params[1].toLongLong();

        return python::object(pyFrame);
    }

    if (params.length() == 0)
        return python::object();

    /**
     ** Control Command
     **/

    if (typeId == qMetaTypeId<ControlCommand>())
        return python::object(qvariant_cast<ControlCommand>(params[0]));

    /**
     ** Firmata
     **/

    if (typeId == qMetaTypeId<FirmataControl>())
        return python::object(qvariant_cast<FirmataControl>(params[0]));

    if (typeId == qMetaTypeId<FirmataData>())
        return python::object(qvariant_cast<FirmataData>(params[0]));

    /**
     ** Table Rows
     **/

    if (typeId == qMetaTypeId<TableRow>()) {
        auto rows = params[0].toList();
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
static bool marshalAndAddSimple(const int &typeId, const boost::python::object &pyObj, QVariantList &params)
{
    if (typeId == qMetaTypeId<T>()) {
        const T etype = python::extract<T>(pyObj);
        params.append(QVariant::fromValue(etype));
        return true;
    }
    return false;
}

/**
 * @brief Prepare data from a Python object for transmission.
 */
bool marshalPyDataElement(int typeId, const boost::python::object &pyObj,
                          QVariantList &params, std::unique_ptr<SharedMemory> &shm)
{
    /**
     ** Frame
     **/

    if (typeId == qMetaTypeId<Frame>()) {
        python::object pyMat = python::extract<python::object>(pyObj.attr("mat"));
        auto mat = cvMatFromNdArray(pyMat.ptr());
        if (!cvMatToShm(shm, mat))
            return false;

        const long time_msec = python::extract<long>(pyObj.attr("time_msec"));
        params.append(QVariant::fromValue(time_msec));
        return true;
    }

    /**
     ** Control Command
     **/

    if (marshalAndAddSimple<ControlCommand>(typeId, pyObj, params))
        return true;

    /**
     ** Firmata
     **/

    if (marshalAndAddSimple<FirmataControl>(typeId, pyObj, params))
        return true;
    if (marshalAndAddSimple<FirmataData>(typeId, pyObj, params))
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
            row.append(QString::fromStdString(boost::python::extract<std::string>(pyList[i])));
        }
        params.append(QVariant::fromValue(row));
        return true;
    }

    return false;
}

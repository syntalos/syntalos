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
    if (typeId == qMetaTypeId<Frame>()) {
        auto floatingMat = cvMatFromShm(shm, false);
        auto matPyO = cvMatToNDArray(floatingMat);

        FrameData pyFrame;
        pyFrame.mat = boost::python::object(boost::python::handle<>(matPyO));
        pyFrame.time_msec = params[0].toLongLong();

        return python::object(pyFrame);
    }

    if (typeId == qMetaTypeId<ControlCommand>()) {
        ControlCommandPy ctl;
        ctl.kind = static_cast<CtlCommandKind>(params[0].toInt());
        ctl.command = params[0].toString().toStdString();
        return python::object(ctl);
    }

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

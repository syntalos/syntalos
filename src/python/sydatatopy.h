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
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <QSize>
#include "datactl/datatypes.h"

namespace pybind11
{

namespace detail
{

/**
 * TableRow conversion
 */
template<>
struct type_caster<TableRow> {
public:
    PYBIND11_TYPE_CASTER(TableRow, const_name("TableRow"));

    bool load(handle src, bool)
    {
        if (!isinstance<sequence>(src))
            return false;

        auto seq = reinterpret_borrow<sequence>(src);
        for (size_t i = 0; i < seq.size(); i++)
            value.append(seq[i].cast<std::string>());
        return true;
    }

    static handle cast(const TableRow &row, return_value_policy /* policy */, handle /* parent */)
    {
        list lst;
        for (const std::string &s : row.data)
            lst.append(pybind11::cast(s));
        return lst.release();
    }
};

/**
 * QSize conversion
 */
template<>
struct type_caster<QSize> {
public:
    PYBIND11_TYPE_CASTER(QSize, const_name("QSize"));

    bool load(handle src, bool)
    {
        /* Extract PyObject from handle */
        if (!py::isinstance<py::tuple>(src)) {
            return false;
        }
        auto t = reinterpret_borrow<py::tuple>(src);
        if (t.size() != 2) {
            return false;
        }

        value.setWidth(t[0].cast<int>());
        value.setHeight(t[1].cast<int>());

        /* Indicate success */
        return true;
    }

    static handle cast(const QSize &src, return_value_policy /* policy */, handle /* parent */)
    {
        return py::make_tuple(src.width(), src.height()).release();
    }
};

/**
 * QVariantHash conversion
 */
template<>
class type_caster<QVariantHash>
{
public:
    PYBIND11_TYPE_CASTER(QVariantHash, const_name("QVariantHash"));

    bool load(const handle &src, bool)
    {
        if (!py::isinstance<py::dict>(src)) {
            return false;
        }

        auto dict = py::cast<py::dict>(src);
        for (auto item : dict) {
            auto key = py::cast<QString>(item.first);
            auto &val = item.second;

            if (py::isinstance<py::bool_>(val)) {
                value.insert(key, py::cast<bool>(val));
            } else if (py::isinstance<py::int_>(val)) {
                value.insert(key, py::cast<int>(val));
            } else if (py::isinstance<py::float_>(val)) {
                value.insert(key, py::cast<double>(val));
            } else if (py::isinstance<QSize>(val)) {
                value.insert(key, py::cast<QSize>(val));
            } else {
                value.insert(key, py::cast<QString>(val));
            }
        }
        return true;
    }

    static handle cast(const QVariantHash &src, return_value_policy /* policy */, handle /* parent */)
    {
        py::dict dict;
        for (auto it = src.constBegin(); it != src.constEnd(); ++it) {
            const auto &val = it.value();
            switch (val.userType()) {
            case QMetaType::Bool:
                dict[py::cast(it.key().toStdString())] = py::cast(val.toBool());
                break;
            case QMetaType::Int:
                dict[py::cast(it.key().toStdString())] = py::cast(val.toInt());
                break;
            case QMetaType::Double:
                dict[py::cast(it.key().toStdString())] = py::cast(val.toDouble());
                break;
            case QMetaType::QString:
                dict[py::cast(it.key().toStdString())] = py::cast(val.toString());
                break;
            case QMetaType::QSize:
                dict[py::cast(it.key().toStdString())] = py::cast(val.toSize());
                break;
            case QMetaType::QStringList:
                dict[py::cast(it.key().toStdString())] = py::cast(val.toStringList());
                break;
            default:
                dict[py::cast(it.key().toStdString())] = py::cast(val.toString());
            }
        }
        return dict.release();
    }
};

/**
 * QByteArray conversion
 */
template<>
struct type_caster<QByteArray> {
public:
    // Declare the type name
    PYBIND11_TYPE_CASTER(QByteArray, _("QByteArray"));

    bool load(handle src, bool)
    {
        if (!PyBytes_Check(src.ptr()))
            return false;

        Py_ssize_t length = PyBytes_Size(src.ptr());
        const char *data = PyBytes_AsString(src.ptr());
        value = QByteArray(data, length);

        return true;
    }

    static handle cast(const QByteArray &src, return_value_policy /* policy */, handle /* parent */)
    {
        return pybind11::bytes(src.constData(), src.size()).release();
    }
};

} // namespace detail
} // namespace pybind11

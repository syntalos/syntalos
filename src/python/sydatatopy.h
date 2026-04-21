/*
 * Copyright (C) 2019-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "datactl/datatypes.h"

namespace pybind11
{

namespace detail
{

namespace py = pybind11;
using namespace Syntalos;

/**
 * TableRow conversion
 */
template<>
struct type_caster<TableRow> {
public:
    // Accept any sequence and stringify each item when loading a TableRow.
    PYBIND11_TYPE_CASTER(TableRow, const_name("typing.Sequence[typing.Any]"));

    bool load(handle src, bool)
    {
        if (!isinstance<sequence>(src))
            return false;

        auto seq = reinterpret_borrow<sequence>(src);
        for (auto &&i : seq) {
            auto item = i;
            if (py::isinstance<py::str>(item))
                value.append(item.cast<std::string>());
            else
                value.append(py::str(item).cast<std::string>());
        }
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

template<>
struct type_caster<Syntalos::MetaValue> {
public:
    PYBIND11_TYPE_CASTER(Syntalos::MetaValue, const_name("object"));
    bool load(handle src, bool convert);
    static handle cast(const Syntalos::MetaValue &src, return_value_policy policy, handle parent);
};

template<>
struct type_caster<Syntalos::MetaArray> {
public:
    PYBIND11_TYPE_CASTER(Syntalos::MetaArray, const_name("list[typing.Any]"));
    bool load(handle src, bool convert);
    static handle cast(const Syntalos::MetaArray &src, return_value_policy policy, handle parent);
};

template<>
struct type_caster<Syntalos::MetaStringMap> {
public:
    PYBIND11_TYPE_CASTER(Syntalos::MetaStringMap, const_name("dict[str, object]"));
    bool load(handle src, bool convert);
    static handle cast(const Syntalos::MetaStringMap &src, return_value_policy policy, handle parent);
};

inline bool type_caster<Syntalos::MetaValue>::load(handle src, bool convert)
{
    if (src.is_none()) {
        value = nullptr;
        return true;
    }
    // bool must be checked before int (Python bool is a subclass of int)
    if (py::isinstance<py::bool_>(src)) {
        value = src.cast<bool>();
        return true;
    }
    if (PyLong_CheckExact(src.ptr())) {
        value = src.cast<int64_t>();
        return true;
    }
    if (py::isinstance<py::float_>(src)) {
        value = src.cast<double>();
        return true;
    }
    if (PyUnicode_CheckExact(src.ptr())) {
        value = src.cast<std::string>();
        return true;
    }
    if (py::isinstance<py::list>(src)) {
        type_caster<Syntalos::MetaArray> acaster;
        if (!acaster.load(src, convert))
            return false;
        value = static_cast<Syntalos::MetaArray &>(acaster);
        return true;
    }
    if (py::isinstance<py::dict>(src)) {
        type_caster<Syntalos::MetaStringMap> mcaster;
        if (!mcaster.load(src, convert))
            return false;
        value = static_cast<Syntalos::MetaStringMap &>(mcaster);
        return true;
    }
    // MetaSize is a registered pybind11 class - check it unambiguously by type.
    if (py::isinstance<Syntalos::MetaSize>(src)) {
        value = src.cast<Syntalos::MetaSize>();
        return true;
    }
    return false;
}

inline handle type_caster<Syntalos::MetaValue>::cast(
    const Syntalos::MetaValue &src,
    return_value_policy policy,
    handle parent)
{
    return std::visit(
        [&](const auto &v) -> handle {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>)
                return py::none().release();
            else if constexpr (std::is_same_v<T, bool>)
                return py::bool_(v).release();
            else if constexpr (std::is_same_v<T, int64_t>)
                return py::int_(v).release();
            else if constexpr (std::is_same_v<T, double>)
                return py::float_(v).release();
            else if constexpr (std::is_same_v<T, std::string>)
                return py::str(v).release();
            else if constexpr (std::is_same_v<T, Syntalos::MetaSize>)
                // Always copy MetaSize into Python: this converter is often used on
                // temporary metadata maps, so returning references can dangle.
                return py::cast(v, py::return_value_policy::copy, parent).release();
            else if constexpr (std::is_same_v<T, Syntalos::MetaArray>)
                return type_caster<Syntalos::MetaArray>::cast(v, policy, parent);
            else if constexpr (std::is_same_v<T, Syntalos::MetaStringMap>)
                return type_caster<Syntalos::MetaStringMap>::cast(v, policy, parent);
            else
                return py::none().release();
        },
        static_cast<const Syntalos::MetaValue::Base &>(src));
}

inline bool type_caster<Syntalos::MetaArray>::load(handle src, bool convert)
{
    if (!py::isinstance<py::list>(src))
        return false;
    auto lst = reinterpret_borrow<py::list>(src);
    value.clear();
    for (auto item : lst) {
        type_caster<Syntalos::MetaValue> vcaster;
        if (!vcaster.load(item, convert))
            return false;
        value.push_back(static_cast<Syntalos::MetaValue &>(vcaster));
    }
    return true;
}

inline handle type_caster<Syntalos::MetaArray>::cast(
    const Syntalos::MetaArray &src,
    return_value_policy policy,
    handle parent)
{
    py::list lst;
    for (const auto &elem : src)
        lst.append(type_caster<Syntalos::MetaValue>::cast(elem, policy, parent));
    return lst.release();
}

inline bool type_caster<Syntalos::MetaStringMap>::load(handle src, bool convert)
{
    if (!py::isinstance<py::dict>(src))
        return false;
    auto dict = reinterpret_borrow<py::dict>(src);
    value.clear();
    for (auto item : dict) {
        type_caster<Syntalos::MetaValue> vcaster;
        if (!vcaster.load(item.second, convert))
            return false;
        value[item.first.cast<std::string>()] = static_cast<Syntalos::MetaValue &>(vcaster);
    }
    return true;
}

inline handle type_caster<Syntalos::MetaStringMap>::cast(
    const Syntalos::MetaStringMap &src,
    return_value_policy policy,
    handle parent)
{
    py::dict dict;
    for (const auto &[key, val] : src)
        dict[py::str(key)] = type_caster<Syntalos::MetaValue>::cast(val, policy, parent);
    return dict.release();
}

/**
 * ByteVector conversion
 */
template<>
struct type_caster<Syntalos::ByteVector> {
public:
    PYBIND11_TYPE_CASTER(Syntalos::ByteVector, const_name("bytes | bytearray"));

    bool load(handle src, bool)
    {
        Py_ssize_t length = 0;
        const char *data = nullptr;

        if (PyBytes_Check(src.ptr())) {
            char *bytesData = nullptr;
            if (PyBytes_AsStringAndSize(src.ptr(), &bytesData, &length) != 0)
                return false;
            data = bytesData;
        } else if (PyByteArray_Check(src.ptr())) {
            length = PyByteArray_Size(src.ptr());
            data = PyByteArray_AsString(src.ptr());
            if (data == nullptr)
                return false;
        } else {
            return false;
        }

        value = ByteVector(
            reinterpret_cast<const std::byte *>(data), reinterpret_cast<const std::byte *>(data) + length);
        return true;
    }

    static handle cast(const Syntalos::ByteVector &src, return_value_policy /* policy */, handle /* parent */)
    {
        return pybind11::bytearray(reinterpret_cast<const char *>(src.data()), src.size()).release();
    }
};

} // namespace detail
} // namespace pybind11

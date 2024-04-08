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
#include "streams/datatypes.h"

namespace pybind11
{

namespace detail
{

/**
 * QStringList conversion
 */
template<>
struct type_caster<TableRow> {
public:
    PYBIND11_TYPE_CASTER(TableRow, _("TableRow"));

    bool load(handle src, bool)
    {
        if (!isinstance<sequence>(src))
            return false;
        sequence seq = reinterpret_borrow<sequence>(src);
        for (size_t i = 0; i < seq.size(); i++)
            value.append(seq[i].cast<QString>());
        return true;
    }

    static handle cast(const TableRow &row, return_value_policy /* policy */, handle /* parent */)
    {
        list lst;
        for (const QString &s : row.data)
            lst.append(pybind11::cast(s));
        return lst.release();
    }
};

} // namespace detail
} // namespace pybind11

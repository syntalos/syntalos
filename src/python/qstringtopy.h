/*
 * Copyright (C) 2020-2024 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (c) 2009-2019 BlueQuartz Software, LLC
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * Neither the name of BlueQuartz Software, the US Air Force, nor the names of its
 * contributors may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

// clang-format off
#include <pybind11/pybind11.h>
#include <QString>
#include <QStringList>
// clang-format on

namespace pybind11
{

namespace detail
{

/**
 * QString conversion
 */
template<>
struct type_caster<QString> {
public:
    PYBIND11_TYPE_CASTER(QString, _("QString"));

    /**
     * @brief Conversion part 1 (Python->C++): convert a PyObject into a QString
     * instance or return false upon failure. The second argument
     * indicates whether implicit conversions should be applied.
     * @param src
     * @return boolean
     */
    bool load(handle src, bool)
    {
        if (!src)
            return false;

        PyObject *str_obj;
        if (PyUnicode_Check(src.ptr())) {
            // we already have a string, use it verbatim
            str_obj = src.ptr();

            Py_ssize_t size;
            const char *buffer = PyUnicode_AsUTF8AndSize(str_obj, &size);
            if (!buffer)
                return false;
            value = QString::fromUtf8(buffer, static_cast<int>(size));
        } else {
            // convert any Python object to string via its string representation
            str_obj = PyObject_Str(src.ptr());
            if (!str_obj) {
                PyErr_Clear();
                return false;
            }

            Py_ssize_t size;
            const char *buffer = PyUnicode_AsUTF8AndSize(str_obj, &size);
            if (!buffer)
                return false;
            value = QString::fromUtf8(buffer, static_cast<int>(size));

            Py_DECREF(str_obj);
        }

        return (!PyErr_Occurred());
    }

    /**
     * @brief Conversion part 2 (C++ -> Python): convert an QString instance into
     * a Python object. The second and third arguments are used to
     * indicate the return value policy and parent object (for
     * ``return_value_policy::reference_internal``) and are generally
     * ignored by implicit casters.
     *
     * @param src
     * @return
     */
    static handle cast(const QString &src, return_value_policy /* policy */, handle /* parent */)
    {
        assert(sizeof(QChar) == 2);
        return PyUnicode_FromKindAndData(PyUnicode_2BYTE_KIND, src.constData(), src.length());
    }
};

/**
 * QStringList conversion
 */
template<>
struct type_caster<QStringList> {
public:
    PYBIND11_TYPE_CASTER(QStringList, _("QStringList"));

    bool load(handle src, bool)
    {
        if (!isinstance<sequence>(src))
            return false;
        sequence seq = reinterpret_borrow<sequence>(src);
        for (size_t i = 0; i < seq.size(); i++)
            value.push_back(seq[i].cast<QString>());
        return true;
    }

    static handle cast(const QStringList &src, return_value_policy /* policy */, handle /* parent */)
    {
        list lst;
        for (const QString &s : src)
            lst.append(pybind11::cast(s));
        return lst.release();
    }
};

} // namespace detail
} // namespace pybind11

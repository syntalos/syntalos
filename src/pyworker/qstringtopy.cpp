/*
 * Copyright (C) 2020 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: FSFAP
 *
 * Adapted from
 *    https://www.boost.org/doc/libs/1_72_0/libs/python/doc/html/faq/how_can_i_automatically_convert_.html
 */

#include "qstringtopy.h"
#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/python/to_python_converter.hpp>
#include <QString>

namespace bpy = boost::python;

struct qstring_to_python_str
{
    static PyObject* convert(QString const& s)
    {
        return bpy::incref(bpy::object(s.toStdString()).ptr());
    }
};

struct qstring_from_python_str
{
    qstring_from_python_str()
    {
        boost::python::converter::registry::push_back(
                    &convertible,
                    &construct,
                    boost::python::type_id<QString>());
    }

    static void* convertible(PyObject* obj_ptr)
    {
        if (!PyUnicode_Check(obj_ptr)) return 0;
        return obj_ptr;
    }

    static void construct(
            PyObject* obj_ptr,
            boost::python::converter::rvalue_from_python_stage1_data* data)
    {
        const char* value = PyUnicode_AsUTF8(obj_ptr);
        if (value == 0) boost::python::throw_error_already_set();
        void* storage = (
                    (boost::python::converter::rvalue_from_python_storage<QString>*)
                    data)->storage.bytes;
        new (storage) QString(value);
        data->convertible = storage;
    }
};

void initQStringPyConvert()
{
    // If there is a QString member variable, conversion is not enough.
    bpy::to_python_converter<
            QString,
            qstring_to_python_str>();

    qstring_from_python_str();
}

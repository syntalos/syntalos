/*
 * Copyright (C) 2020 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: FSFAP
 */

#include "timetopy.h"
#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/python/to_python_converter.hpp>
#include "syclock.h"

using namespace Syntalos;
namespace bpy = boost::python;

struct milliseconds_to_python_num
{
    static PyObject* convert(const milliseconds_t &msec)
    {
        return bpy::incref(bpy::object(msec.count()).ptr());
    }
};

struct milliseconds_from_python_num
{
    milliseconds_from_python_num()
    {
        boost::python::converter::registry::push_back(
                    &convertible,
                    &construct,
                    boost::python::type_id<milliseconds_t>());
    }

    static void* convertible(PyObject* obj_ptr)
    {
        if (!PyLong_Check(obj_ptr)) return 0;
        return obj_ptr;
    }

    static void construct(
            PyObject* obj_ptr,
            boost::python::converter::rvalue_from_python_stage1_data* data)
    {
        const auto value = PyLong_AsLong(obj_ptr);
        if (value < 0) boost::python::throw_error_already_set();
        void* storage = (
                    (boost::python::converter::rvalue_from_python_storage<milliseconds_t>*)
                    data)->storage.bytes;
        new (storage) milliseconds_t(value);
        data->convertible = storage;
    }
};

void initChronoTimePyConvert()
{
    bpy::to_python_converter<
            milliseconds_t,
            milliseconds_to_python_num>();

    milliseconds_from_python_num();
}

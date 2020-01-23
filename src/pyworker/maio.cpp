/*
 * Copyright (C) 2016-2018 Matthias Klumpp <matthias@tenstral.net>
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

#include <Python.h>
#include "maio.h"

#include <QDebug>
#include <QJsonArray>
#include <QStringList>
#include <boost/python.hpp>
#include <boost/python/list.hpp>
#include <boost/python/extract.hpp>
#include <stdexcept>
#include <iostream>

PyBridge::PyBridge(QObject *parent)
    : QObject(parent),
      m_timer(new HRTimer)
{

}

PyBridge::~PyBridge()
{
    delete m_timer;
}

HRTimer *PyBridge::timer() const
{
    return m_timer;
}

using namespace boost::python;

struct MazeAmazePyError : std::runtime_error {
    explicit MazeAmazePyError(const char* what_arg);
    explicit MazeAmazePyError(const std::string& what_arg);
};
MazeAmazePyError::MazeAmazePyError(const char* what_arg)
    : std::runtime_error(what_arg) {};
MazeAmazePyError::MazeAmazePyError(const std::string& what_arg)
    : std::runtime_error(what_arg) {};

void translateException(const MazeAmazePyError& e) {
    PyErr_SetString(PyExc_RuntimeError, e.what());
};

static long long time_since_start_msec()
{
    auto pb = PyBridge::instance();
    return pb->timer()->timeSinceStartMsec().count();
}

static void println(const std::string& text)
{
    std::cout << text << std::endl;
}


BOOST_PYTHON_MODULE(maio)
{
    register_exception_translator<MazeAmazePyError>(&translateException);

    def("println", println, "Print text to stdout.");
    def("time_since_start_msec", time_since_start_msec, "Get time since experiment started in milliseconds.");
};

void pythonRegisterMaioModule()
{
    PyImport_AppendInittab("maio", &PyInit_maio);
}

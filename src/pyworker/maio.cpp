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

#include "worker.h"

PyBridge::PyBridge(OOPWorker *worker)
    : QObject(worker),
      m_timer(new HRTimer),
      m_worker(worker)
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

OOPWorker *PyBridge::worker()
{
    return m_worker;
}

using namespace boost;

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

enum InputWaitResult {
    IWR_NONE  = 0,
    IWR_NEWDATA = 1,
    IWR_CANCELLED = 2
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

static InputWaitResult await_new_input()
{
    auto pb = PyBridge::instance();
    auto res = pb->worker()->waitForInput();
    if (res.has_value())
        return res.value()? IWR_NEWDATA : IWR_NONE;
    else
        return IWR_CANCELLED;
}

struct FrameData
{
    time_t time_msec;
    PyObject *mat;
};

struct InputPort
{
    InputPort(std::string name, int id)
        : _name(name),
          _inst_id(id)
    {
    }

    python::object next()
    {
        FrameData data;
        data.time_msec = 5;

        return python::object(data);
    }

    std::string _name;
    int _inst_id;
};

static python::object get_input_port(const std::string& id)
{
    auto pb = PyBridge::instance();
    auto res = pb->worker()->inputPortInfoByIdString(QString::fromStdString(id));
    if (!res.has_value())
        return python::object();

    InputPort pyPort(res->idstr().toStdString(), res->id());
    return python::object(pyPort);
}


using namespace boost::python;
BOOST_PYTHON_MODULE(maio)
{
    python::register_exception_translator<MazeAmazePyError>(&translateException);

    class_<FrameData>("FrameData", init<>())
                .def_readonly("time_msec", &FrameData::time_msec)
                .def_readonly("mat", &FrameData::mat)
            ;

    class_<InputPort>("InputPort", init<std::string, int>())
                .def("next", &InputPort::next)
                .def_readonly("name", &InputPort::_name)
            ;

    enum_<InputWaitResult>("InputWaitResult")
                .value("NONE", IWR_NONE)
                .value("NEWDATA", IWR_NEWDATA)
                .value("CANCELLED", IWR_CANCELLED);

    def("println", println, "Print text to stdout.");
    def("time_since_start_msec", time_since_start_msec, "Get time since experiment started in milliseconds.");
    def("await_new_input", await_new_input, "Wait for any new input to arrive via our input ports.");

    def("get_input_port", get_input_port, "Get reference to input port with the give ID.");
};

void pythonRegisterMaioModule()
{
    PyImport_AppendInittab("maio", &PyInit_maio);
}

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
#include "pyipcmarshal.h"
#include "cvmatndsliceconvert.h"

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

static long time_since_start_msec()
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

struct InputPort
{
    InputPort(std::string name, int id)
        : _name(name),
          _inst_id(id)
    {
    }

    python::object next()
    {
        auto pb = PyBridge::instance();
        if (pb->incomingData[_inst_id].isEmpty())
            return python::object();

        return pb->incomingData[_inst_id].dequeue();
    }

    std::string _name;
    int _inst_id;
};

struct OutputPort
{
    OutputPort(std::string name, int id)
        : _name(name),
          _inst_id(id)
    {
    }

    void submit(python::object pyObj)
    {
        auto pb = PyBridge::instance();
        if (!pb->worker()->submitOutput(_inst_id, pyObj))
            throw MazeAmazePyError("Could not submit data on output port.");
    }

    void set_metadata_value_str(const std::string &key, const std::string &value)
    {
        auto pb = PyBridge::instance();
        pb->worker()->setOutPortMetadataValue(_inst_id,
                                              QString::fromStdString(key),
                                              QVariant::fromValue(QString::fromStdString(value)));
    }

    void set_metadata_value_int(const std::string &key, int value)
    {
        auto pb = PyBridge::instance();
        pb->worker()->setOutPortMetadataValue(_inst_id,
                                              QString::fromStdString(key),
                                              QVariant::fromValue(value));
    }

    void set_metadata_value_dim(const std::string &key, const python::list &value)
    {
        auto pb = PyBridge::instance();
        if (python::len(value) < 2)
            throw MazeAmazePyError("Dimension list needs at least two entries");
        const int width = python::extract<int>(value[0]);
        const int height = python::extract<int>(value[1]);
        QSize size(width, height);
        pb->worker()->setOutPortMetadataValue(_inst_id,
                                              QString::fromStdString(key),
                                              QVariant::fromValue(size));
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

static python::object get_output_port(const std::string& id)
{
    auto pb = PyBridge::instance();
    auto res = pb->worker()->outputPortInfoByIdString(QString::fromStdString(id));
    if (!res.has_value())
        return python::object();

    OutputPort pyPort(res->idstr().toStdString(), res->id());
    return python::object(pyPort);
}


using namespace boost::python;
BOOST_PYTHON_MODULE(maio)
{
    initNDArray();
    python::register_exception_translator<MazeAmazePyError>(&translateException);

    class_<InputPort>("InputPort", init<std::string, int>())
                .def("next", &InputPort::next)
                .def_readonly("name", &InputPort::_name)
            ;

    class_<OutputPort>("OutputPort", init<std::string, int>())
                .def("submit", &OutputPort::submit)
                .def_readonly("name", &OutputPort::_name)
                .def("set_metadata_value_str", &OutputPort::set_metadata_value_str)
                .def("set_metadata_value_int", &OutputPort::set_metadata_value_int)
                .def("set_metadata_value_dim", &OutputPort::set_metadata_value_dim)
            ;

    enum_<InputWaitResult>("InputWaitResult")
                .value("NONE", IWR_NONE)
                .value("NEWDATA", IWR_NEWDATA)
                .value("CANCELLED", IWR_CANCELLED);

    class_<FrameData>("FrameData", init<>())
                .def_readwrite("time_msec", &FrameData::time_msec)
                .def_readwrite("mat", &FrameData::mat)
            ;

    enum_<CtlCommandKind>("ControlCommandKind")
                .value("UNKNOWN", CTL_UNKNOWN)
                .value("START", CTL_START)
                .value("STOP", CTL_STOP)
                .value("STEP", CTL_STEP)
                .value("CUSTOM", CTL_CUSTOM);

    class_<ControlCommandPy>("ControlCommand", init<>())
                .def_readwrite("kind", &ControlCommandPy::kind)
                .def_readwrite("command", &ControlCommandPy::command)
            ;

    def("println", println, "Print text to stdout.");
    def("time_since_start_msec", time_since_start_msec, "Get time since experiment started in milliseconds.");
    def("await_new_input", await_new_input, "Wait for any new input to arrive via our input ports.");

    def("get_input_port", get_input_port, "Get reference to input port with the give ID.");
    def("get_output_port", get_output_port, "Get reference to output port with the give ID.");
};

void pythonRegisterMaioModule()
{
    PyImport_AppendInittab("maio", &PyInit_maio);
}

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
#include "syio.h"

#include <QDebug>
#include <QStringList>
#include <boost/python.hpp>
#include <boost/python/list.hpp>
#include <boost/python/extract.hpp>
#include <stdexcept>
#include <iostream>

#include "worker.h"
#include "qstringtopy.h"
#include "cvmatndsliceconvert.h"
#include "pyipcmarshal.h"

PyBridge::PyBridge(OOPWorker *worker)
    : QObject(worker),
      m_syTimer(new SyncTimer),
      m_worker(worker)
{

}

PyBridge::~PyBridge()
{
    delete m_syTimer;
}

SyncTimer *PyBridge::timer() const
{
    return m_syTimer;
}

OOPWorker *PyBridge::worker()
{
    return m_worker;
}

namespace bpy = boost::python;

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

    void set_metadata_value(const std::string &key, const python::object &obj)
    {
        auto pb = PyBridge::instance();
        if (PyLong_CheckExact(obj.ptr())) {
            // we have an integer type
            const long value = python::extract<long>(obj);
            pb->worker()->setOutPortMetadataValue(_inst_id,
                                                  QString::fromStdString(key),
                                                  QVariant::fromValue(value));
        } else if (PyUnicode_CheckExact(obj.ptr())) {
            // we have a (unicode) string type
            const auto value = python::extract<std::string>(obj);
            pb->worker()->setOutPortMetadataValue(_inst_id,
                                                  QString::fromStdString(key),
                                                  QVariant::fromValue(QString::fromStdString(value)));
        } else if (PyList_Check(obj.ptr())) {
            const python::list pyList = python::extract<python::list>(obj);
            const auto pyListLen = python::len(pyList);
            if (pyListLen <= 0)
                return;
            QVariantList varList;
            varList.reserve(pyListLen);
            for (ssize_t i = 0; i < pyListLen; i++) {
                const auto loP = pyList[i];
                const python::object lo = python::extract<python::object>(loP);
                if (PyLong_CheckExact(lo.ptr())) {
                    const long value = python::extract<long>(lo.ptr());
                    varList.append(QVariant::fromValue(value));
                } else if (PyUnicode_CheckExact(lo.ptr())) {
                    const auto value = python::extract<std::string>(lo);
                    varList.append(QString::fromStdString(value));
                } else {
                    throw MazeAmazePyError(std::string("Invalid type found in list metadata entry: ") +
                                           std::string(boost::python::extract<std::string>(lo.attr("__class__").attr("__name__"))));
                }
            }
            pb->worker()->setOutPortMetadataValue(_inst_id,
                                                  QString::fromStdString(key),
                                                  varList);
        } else {
            throw MazeAmazePyError(std::string("Can not set a metadata value for this type: ") +
                                   std::string(boost::python::extract<std::string>(obj.attr("__class__").attr("__name__"))));
        }
    }

    void set_metadata_value_size(const std::string &key, const python::list &value)
    {
        auto pb = PyBridge::instance();
        if (python::len(value) != 2)
            throw MazeAmazePyError("Dimension list needs exactly two entries");
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

static FirmataControl new_firmata_control_with_id(FirmataCommandKind kind, int pinId)
{
    FirmataControl ctl;
    ctl.command = kind;
    ctl.pinId = pinId;
    return ctl;
}

static FirmataControl new_firmata_control_with_name(FirmataCommandKind kind, const std::string &name)
{
    FirmataControl ctl;
    ctl.command = kind;
    ctl.pinName = QString::fromStdString(name);
    return ctl;
}

using namespace bpy;
BOOST_PYTHON_MODULE(syio)
{
    initNDArray();
    initQStringPyConvert();

    bpy::register_exception_translator<MazeAmazePyError>(&translateException);

    class_<InputPort>("InputPort", init<std::string, int>())
                .def("next", &InputPort::next)
                .def_readonly("name", &InputPort::_name)
            ;

    class_<OutputPort>("OutputPort", init<std::string, int>())
                .def("submit", &OutputPort::submit)
                .def_readonly("name", &OutputPort::_name)
                .def("set_metadata_value", &OutputPort::set_metadata_value, "Set (immutable) metadata value for this port.")
                .def("set_metadata_value_size", &OutputPort::set_metadata_value_size, "Set (immutable) metadata value for a 2D size type for this port.")
            ;

    enum_<InputWaitResult>("InputWaitResult")
                .value("NONE", IWR_NONE)
                .value("NEWDATA", IWR_NEWDATA)
                .value("CANCELLED", IWR_CANCELLED)
            ;

    /**
     ** Frames
     **/

    class_<PyFrame>("Frame", init<>())
                .def_readwrite("time_msec", &PyFrame::time_msec)
                .def_readwrite("mat", &PyFrame::mat)
            ;

    /**
     ** Control Command
     **/

    enum_<ControlCommandKind>("ControlCommandKind")
            .value("UNKNOWN", ControlCommandKind::UNKNOWN)
            .value("START", ControlCommandKind::START)
            .value("PAUSE", ControlCommandKind::PAUSE)
            .value("STOP", ControlCommandKind::STOP)
            .value("STEP", ControlCommandKind::STEP)
            .value("CUSTOM", ControlCommandKind::CUSTOM)
            ;

    class_<ControlCommand>("ControlCommand", init<>())
                .def_readwrite("kind", &ControlCommand::kind)
                .def_readwrite("command", &ControlCommand::command)
            ;

    /**
     ** Firmata
     **/

    enum_<FirmataCommandKind>("FirmataCommandKind")
            .value("UNKNOWN", FirmataCommandKind::UNKNOWN)
            .value("NEW_DIG_PIN", FirmataCommandKind::NEW_DIG_PIN)
            .value("NEW_ANA_PIN", FirmataCommandKind::NEW_ANA_PIN)
            .value("IO_MODE", FirmataCommandKind::IO_MODE)
            .value("WRITE_ANALOG", FirmataCommandKind::WRITE_ANALOG)
            .value("WRITE_DIGITAL", FirmataCommandKind::WRITE_DIGITAL)
            .value("WRITE_DIGITAL_PULSE", FirmataCommandKind::WRITE_DIGITAL_PULSE)
            .value("SYSEX", FirmataCommandKind::SYSEX)
            ;

    class_<FirmataControl>("FirmataControl", init<>())
                .def_readwrite("command", &FirmataControl::command)
                .def_readwrite("pin_id", &FirmataControl::pinId)
                .add_property("pin_name", make_getter(&FirmataControl::pinName, return_value_policy<return_by_value>()),
                                          make_setter(&FirmataControl::pinName, return_value_policy<return_by_value>()))
                .def_readwrite("output", &FirmataControl::output)
                .def_readwrite("pull_up", &FirmataControl::pullUp)
                .def_readwrite("value", &FirmataControl::value)
            ;

    class_<FirmataData>("FirmataData", init<>())
                .def_readwrite("pin_id", &FirmataData::pinId)
                .add_property("pin_name", make_getter(&FirmataData::pinName, return_value_policy<return_by_value>()),
                                          make_setter(&FirmataData::pinName, return_value_policy<return_by_value>()))
                .def_readwrite("value", &FirmataData::value)
                .def_readwrite("analog", &FirmataData::analog)
                .def_readwrite("timestamp", &FirmataData::timestamp)
            ;


    /**
     ** Additional Functions
     **/

    def("println", println, "Print text to stdout.");
    def("time_since_start_msec", time_since_start_msec, "Get time since experiment started in milliseconds.");
    def("await_new_input", await_new_input, "Wait for any new input to arrive via our input ports.");

    def("get_input_port", get_input_port, "Get reference to input port with the give ID.");
    def("get_output_port", get_output_port, "Get reference to output port with the give ID.");

    // Firmata helpers
    def("new_firmata_control_with_id", new_firmata_control_with_id, "Create new Firmata control command with a given pin ID.");
    def("new_firmata_control_with_name", new_firmata_control_with_name, "Create new Firmata control command with a given pin name (the name needs to be registered previously).");
};

void pythonRegisterSyioModule()
{
    PyImport_AppendInittab("syio", &PyInit_syio);
}

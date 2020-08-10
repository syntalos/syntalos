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

#include <stdexcept>
#include <iostream>
#include <pybind11/pybind11.h>
#include <pybind11/stl_bind.h>
#include <pybind11/chrono.h>

#include "qstringtopy.h"
#include "cvmatndsliceconvert.h"
#include "worker.h"
#include "qstringtopy.h"
#include "cvmatndsliceconvert.h"
#include "pyipcmarshal.h"


namespace py = pybind11;


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

struct SyntalosPyError : std::runtime_error {
    explicit SyntalosPyError(const char* what_arg);
    explicit SyntalosPyError(const std::string& what_arg);
};
SyntalosPyError::SyntalosPyError(const char* what_arg)
    : std::runtime_error(what_arg) {};
SyntalosPyError::SyntalosPyError(const std::string& what_arg)
    : std::runtime_error(what_arg) {};

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

static long time_since_start_usec()
{
    auto pb = PyBridge::instance();
    return pb->timer()->timeSinceStartUsec().count();
}

static void println(const std::string& text)
{
    std::cout << text << std::endl;
}

static void raise_error(const std::string& message)
{
    auto pb = PyBridge::instance();
    pb->worker()->raiseError(QString::fromStdString(message));
}

static void wait(unsigned int msec)
{
    auto timer = QTime::currentTime().addMSecs(msec);
    while (QTime::currentTime() < timer)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

static void wait_sec(unsigned int sec)
{
    auto timer = QTime::currentTime().addMSecs(sec * 1000);
    while (QTime::currentTime() < timer)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 500);
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

    py::object next()
    {
        auto pb = PyBridge::instance();
        if (pb->incomingData[_inst_id].isEmpty())
            return py::none();

        return pb->incomingData[_inst_id].dequeue();
    }

    void setThrottleItemsPerSec(uint itemsPerSec, bool allowMore = true)
    {
        auto pb = PyBridge::instance();
        pb->worker()->setInputThrottleItemsPerSec(_inst_id, itemsPerSec, allowMore);
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

    void submit(py::object pyObj)
    {
        auto pb = PyBridge::instance();
        if (!pb->worker()->submitOutput(_inst_id, pyObj))
            throw SyntalosPyError("Could not submit data on output port.");
    }

    void set_metadata_value(const std::string &key, const py::object &obj)
    {
        auto pb = PyBridge::instance();
        if (PyLong_CheckExact(obj.ptr())) {
            // we have an integer type
            const long value = obj.cast<long>();
            pb->worker()->setOutPortMetadataValue(_inst_id,
                                                  QString::fromStdString(key),
                                                  QVariant::fromValue(value));
        } else if (PyUnicode_CheckExact(obj.ptr())) {
            // we have a (unicode) string type
            const auto value = obj.cast<std::string>();
            pb->worker()->setOutPortMetadataValue(_inst_id,
                                                  QString::fromStdString(key),
                                                  QVariant::fromValue(QString::fromStdString(value)));
        } else if (PyList_Check(obj.ptr())) {
            const auto pyList = obj.cast<py::list>();
            const auto pyListLen = py::len(pyList);
            if (pyListLen == 0)
                return;
            QVariantList varList;
            varList.reserve(pyListLen);
            for (size_t i = 0; i < pyListLen; i++) {
                const auto lo = pyList[i];
                if (PyLong_CheckExact(lo.ptr())) {
                    const long value = lo.cast<long>();
                    varList.append(QVariant::fromValue(value));
                } else if (PyUnicode_CheckExact(lo.ptr())) {
                    const auto value = lo.cast<std::string>();
                    varList.append(QString::fromStdString(value));
                } else {
                    throw SyntalosPyError(std::string("Invalid type found in list metadata entry: ") +
                                           std::string(lo.attr("__py::class__").attr("__name__").cast<std::string>()));
                }
            }
            pb->worker()->setOutPortMetadataValue(_inst_id,
                                                  QString::fromStdString(key),
                                                  varList);
        } else {
            throw SyntalosPyError(std::string("Can not set a metadata value for this type: ") +
                                  std::string(obj.attr("__py::class__").attr("__name__").cast<std::string>()));
        }
    }

    void set_metadata_value_size(const std::string &key, const py::list &value)
    {
        auto pb = PyBridge::instance();
        if (py::len(value) != 2)
            throw SyntalosPyError("2D Dimension list needs exactly two entries");
        const int width = value[0].cast<int>();
        const int height = value[1].cast<int>();
        QSize size(width, height);
        pb->worker()->setOutPortMetadataValue(_inst_id,
                                              QString::fromStdString(key),
                                              QVariant::fromValue(size));
    }

    std::string _name;
    int _inst_id;
};

static py::object get_input_port(const std::string& id)
{
    auto pb = PyBridge::instance();
    auto res = pb->worker()->inputPortInfoByIdString(QString::fromStdString(id));
    if (!res.has_value())
        return py::none();

    InputPort pyPort(res->idstr().toStdString(), res->id());
    return py::cast(pyPort);
}

static py::object get_output_port(const std::string& id)
{
    auto pb = PyBridge::instance();
    auto res = pb->worker()->outputPortInfoByIdString(QString::fromStdString(id));
    if (!res.has_value())
        return py::none();

    OutputPort pyPort(res->idstr().toStdString(), res->id());
    return py::cast(pyPort);
}

static FirmataControl new_firmatactl_with_id_name(FirmataCommandKind kind, int pinId, const std::string &name)
{
    FirmataControl ctl;
    ctl.command = kind;
    ctl.pinId = pinId;
    ctl.pinName = QString::fromStdString(name);
    return ctl;
}

static FirmataControl new_firmatactl_with_id(FirmataCommandKind kind, int pinId)
{
    FirmataControl ctl;
    ctl.command = kind;
    ctl.pinId = pinId;
    return ctl;
}

static FirmataControl new_firmatactl_with_name(FirmataCommandKind kind, const std::string &name)
{
    FirmataControl ctl;
    ctl.command = kind;
    ctl.pinName = QString::fromStdString(name);
    return ctl;
}

PYBIND11_MODULE(syio, m)
{
    m.doc() = "Syntalos Interface";

    NDArrayConverter::initNDArray();
    py::bind_vector<std::vector<double>>(m, "VectorDouble");

    py::register_exception<SyntalosPyError>(m, "SyntalosPyError");

    py::class_<InputPort>(m, "InputPort")
            .def(py::init<std::string, int>())
            .def("next", &InputPort::next, "Retrieve the next element, return None if no element is available.")
            .def("set_throttle_items_per_sec", &InputPort::setThrottleItemsPerSec, "Limit the amount of input received to a set amount of elements per second.",
                 py::arg("items_per_sec"), py::arg("allow_more") = true)
            .def_readonly("name", &InputPort::_name)
    ;

    py::class_<OutputPort>(m, "OutputPort")
            .def(py::init<std::string, int>())
            .def("submit", &OutputPort::submit)
            .def_readonly("name", &OutputPort::_name)
            .def("set_metadata_value", &OutputPort::set_metadata_value, "Set (immutable) metadata value for this port.")
            .def("set_metadata_value_size", &OutputPort::set_metadata_value_size, "Set (immutable) metadata value for a 2D size type for this port.")
    ;

    py::enum_<InputWaitResult>(m, "InputWaitResult")
            .value("NONE", IWR_NONE)
            .value("NEWDATA", IWR_NEWDATA)
            .value("CANCELLED", IWR_CANCELLED)
            .export_values()
    ;

    /**
     ** Frames
     **/

    py::class_<Frame>(m, "Frame")
            .def(py::init<>())
            .def_readwrite("index", &Frame::index)
            .def_readwrite("time_msec", &Frame::time)
            .def_readwrite("mat", &Frame::mat)
    ;

    /**
     ** Control Command
     **/

    py::enum_<ControlCommandKind>(m, "ControlCommandKind")
            .value("UNKNOWN", ControlCommandKind::UNKNOWN)
            .value("START", ControlCommandKind::START)
            .value("PAUSE", ControlCommandKind::PAUSE)
            .value("STOP", ControlCommandKind::STOP)
            .value("STEP", ControlCommandKind::STEP)
            .value("CUSTOM", ControlCommandKind::CUSTOM)
            .export_values()
    ;

    py::class_<ControlCommand>(m, "ControlCommand")
            .def(py::init<>())
            .def_readwrite("kind", &ControlCommand::kind)
            .def_readwrite("command", &ControlCommand::command)
    ;

    /**
     ** Firmata
     **/

    py::enum_<FirmataCommandKind>(m, "FirmataCommandKind")
            .value("UNKNOWN", FirmataCommandKind::UNKNOWN)
            .value("NEW_DIG_PIN", FirmataCommandKind::NEW_DIG_PIN)
            .value("NEW_ANA_PIN", FirmataCommandKind::NEW_ANA_PIN)
            .value("IO_MODE", FirmataCommandKind::IO_MODE)
            .value("WRITE_ANALOG", FirmataCommandKind::WRITE_ANALOG)
            .value("WRITE_DIGITAL", FirmataCommandKind::WRITE_DIGITAL)
            .value("WRITE_DIGITAL_PULSE", FirmataCommandKind::WRITE_DIGITAL_PULSE)
            .value("SYSEX", FirmataCommandKind::SYSEX)
            .export_values()
    ;

    py::class_<FirmataControl>(m, "FirmataControl")
            .def(py::init<>())
            .def_readwrite("command", &FirmataControl::command)
            .def_readwrite("pin_id", &FirmataControl::pinId)
            .def_readwrite("pin_name", &FirmataControl::pinName)
            .def_readwrite("is_output", &FirmataControl::isOutput)
            .def_readwrite("is_pullup", &FirmataControl::isPullUp)
            .def_readwrite("value", &FirmataControl::value)
    ;

    py::class_<FirmataData>(m, "FirmataData")
            .def(py::init<>())
            .def_readwrite("pin_id", &FirmataData::pinId)
            .def_readwrite("pin_name", &FirmataData::pinName)
            .def_readwrite("value", &FirmataData::value)
            .def_readwrite("is_digital", &FirmataData::isDigital)
            .def_readwrite("time", &FirmataData::time)
    ;

    /**
     ** Additional Functions
     **/

    m.def("println", println, "Print text to stdout.");
    m.def("raise_error", raise_error, "Emit an error message string, immediately terminating the current action and (if applicable) the experiment.");
    m.def("time_since_start_msec", time_since_start_msec, "Get time since experiment started in milliseconds.");
    m.def("time_since_start_usec", time_since_start_usec, "Get time since experiment started in microseconds.");
    m.def("wait", wait, "Wait (roughly) for the given amount of milliseconds without blocking communication with the master process.");
    m.def("wait_sec", wait_sec, "Wait (roughly) for the given amount of seconds without blocking communication with the master process.");
    m.def("await_new_input", await_new_input, "Wait for any new input to arrive via our input ports.");

    m.def("get_input_port", get_input_port, "Get reference to input port with the give ID.");
    m.def("get_output_port", get_output_port, "Get reference to output port with the give ID.");

    // Firmata helpers
    m.def("new_firmatactl_with_id_name", new_firmatactl_with_id_name, "Create new Firmata control command with a given pin ID and registered name.");
    m.def("new_firmatactl_with_id", new_firmatactl_with_id, "Create new Firmata control command with a given pin ID.");
    m.def("new_firmatactl_with_name", new_firmatactl_with_name, "Create new Firmata control command with a given pin name (the name needs to be registered previously).");
};

void pythonRegisterSyioModule()
{
    PyImport_AppendInittab("syio", &PyInit_syio);
}

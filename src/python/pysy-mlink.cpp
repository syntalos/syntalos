/*
 * Copyright (C) 2016-2018 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "pysy-mlink.h"

#include <QDebug>
#include <QStringList>
#include <QTime>
#include <QCoreApplication>

#include <iostream>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/eigen.h>
#include <stdexcept>

#include <syntaloslink.h>
#include "cvnp/cvnp.h"
#include "datactl/datatypes.h"
#include "datactl/frametype.h"
#include "qstringtopy.h" // needed for QString registration
#include "sydatatopy.h"  // needed for stream data type conversion
#include "pyworker.h"

namespace py = pybind11;

PyBridge::PyBridge(SyntalosLink *mlink)
    : QObject(mlink),
      m_mlink(mlink)
{
}

PyBridge::~PyBridge() {}

SyntalosLink *PyBridge::link()
{
    return m_mlink;
}

using PyNewDataFn = std::function<void(const py::object &obj)>;

SyntalosPyError::SyntalosPyError(const char *what_arg)
    : std::runtime_error(what_arg) {};
SyntalosPyError::SyntalosPyError(const std::string &what_arg)
    : std::runtime_error(what_arg) {};

static uint64_t time_since_start_msec()
{
    auto pb = PyBridge::instance();
    return pb->link()->timer()->timeSinceStartMsec().count();
}

static uint64_t time_since_start_usec()
{
    auto pb = PyBridge::instance();
    return pb->link()->timer()->timeSinceStartUsec().count();
}

static void println(const std::string &text)
{
    std::cout << text << std::endl;
}

static void raise_error(const std::string &message)
{
    auto pb = PyBridge::instance();
    pb->link()->raiseError(QString::fromStdString(message));
}

static void wait(uint msec)
{
    auto pb = PyBridge::instance();
    auto timer = QTime::currentTime().addMSecs(msec);
    while (QTime::currentTime() < timer)
        pb->link()->awaitData(10 * 1000);
}

static void wait_sec(uint sec)
{
    auto pb = PyBridge::instance();
    auto timer = QTime::currentTime().addMSecs(sec * 1000);
    while (QTime::currentTime() < timer)
        pb->link()->awaitData(100 * 1000);
}

static bool is_running()
{
    auto pb = PyBridge::instance();
    return pb->link()->state() == ModuleState::RUNNING;
}

static void await_data(int timeout_usec)
{
    auto pb = PyBridge::instance();
    pb->link()->awaitData(timeout_usec);
}

static void schedule_delayed_call(int delay_msec, const std::function<void()> &fn)
{
    if (delay_msec < 0)
        throw SyntalosPyError("Delay must be positive or zero.");
    QTimer::singleShot(delay_msec, [fn]() {
        try {
            fn();
        } catch (py::error_already_set &e) {
            auto pb = PyBridge::instance();
            pb->link()->raiseError(e.what());
        }
    });
}

static void call_on_show_settings(ShowSettingsFn fn)
{
    auto pb = PyBridge::instance();
    pb->link()->setShowSettingsCallback([fn](const QByteArray &settings) {
        QTimer::singleShot(0, [fn, settings]() {
            try {
                fn(settings);
            } catch (py::error_already_set &e) {
                auto pb = PyBridge::instance();
                pb->link()->raiseError(e.what());
            }
        });
    });
}

static void call_on_show_display(const ShowDisplayFn &fn)
{
    auto pb = PyBridge::instance();
    pb->link()->setShowDisplayCallback([fn]() {
        QTimer::singleShot(0, [fn]() {
            try {
                fn();
            } catch (py::error_already_set &e) {
                auto pb = PyBridge::instance();
                pb->link()->raiseError(e.what());
            }
        });
    });
}

static void save_settings(const QByteArray &settings_data)
{
    auto pb = PyBridge::instance();
    pb->link()->setSettingsData(settings_data);
}

struct InputPort {
    InputPort(const std::shared_ptr<InputPortInfo> &iport)
        : _iport(iport)
    {
        _id = _iport->id().toStdString();
        _dataTypeId = _iport->dataTypeId();
    }

    void set_on_data(const PyNewDataFn &fn)
    {
        _on_data_cb = fn;
        if (!_on_data_cb) {
            _iport->setNewDataRawCallback(nullptr);
            return;
        }

        _iport->setNewDataRawCallback([this](const void *data, size_t size) {
            try {
                switch (_dataTypeId) {
                case syDataTypeId<ControlCommand>():
                    _on_data_cb(py::cast(ControlCommand::fromMemory(data, size)));
                    break;
                case syDataTypeId<TableRow>():
                    _on_data_cb(py::cast(TableRow::fromMemory(data, size)));
                    break;
                case syDataTypeId<Frame>():
                    _on_data_cb(py::cast(Frame::fromMemory(data, size)));
                    break;
                case syDataTypeId<FirmataControl>():
                    _on_data_cb(py::cast(FirmataControl::fromMemory(data, size)));
                    break;
                case syDataTypeId<FirmataData>():
                    _on_data_cb(py::cast(FirmataData::fromMemory(data, size)));
                    break;
                case syDataTypeId<IntSignalBlock>():
                    _on_data_cb(py::cast(IntSignalBlock::fromMemory(data, size)));
                    break;
                case syDataTypeId<FloatSignalBlock>():
                    _on_data_cb(py::cast(FloatSignalBlock::fromMemory(data, size)));
                    break;
                }
            } catch (py::error_already_set &e) {
                auto pb = PyBridge::instance();
                pb->link()->raiseError(e.what());
            }
        });
    }

    PyNewDataFn get_on_data() const
    {
        return _on_data_cb;
    }

    QVariantHash metadata() const
    {
        return _iport->metadata();
    }

    void set_throttle_items_per_sec(uint itemsPerSec)
    {
        auto pb = PyBridge::instance();
        _iport->setThrottleItemsPerSec(itemsPerSec);
        pb->link()->updateInputPort(_iport);
    }

    std::string _id;
    int _dataTypeId;
    const std::shared_ptr<InputPortInfo> _iport;
    PyNewDataFn _on_data_cb;
};

struct OutputPort {
    OutputPort(const std::shared_ptr<OutputPortInfo> &oport)
        : _oport(oport)
    {
        _id = _oport->id().toStdString();
        _dataTypeId = _oport->dataTypeId();
    }

    bool _submit_output_private(const py::object &pyObj)
    {
        auto slink = PyBridge::instance()->link();
        switch (_oport->dataTypeId()) {
        case syDataTypeId<ControlCommand>():
            return slink->submitOutput(_oport, py::cast<ControlCommand>(pyObj));
        case syDataTypeId<TableRow>():
            return slink->submitOutput(_oport, py::cast<TableRow>(pyObj));
        case syDataTypeId<Frame>():
            return slink->submitOutput(_oport, py::cast<Frame>(pyObj));
        case syDataTypeId<FirmataControl>():
            return slink->submitOutput(_oport, py::cast<FirmataControl>(pyObj));
        case syDataTypeId<FirmataData>():
            return slink->submitOutput(_oport, py::cast<FirmataData>(pyObj));
        case syDataTypeId<IntSignalBlock>():
            return slink->submitOutput(_oport, py::cast<IntSignalBlock>(pyObj));
        case syDataTypeId<FloatSignalBlock>():
            return slink->submitOutput(_oport, py::cast<FloatSignalBlock>(pyObj));
        default:
            return false;
        }
    }

    void submit(const py::object &pyObj)
    {
        if (!_submit_output_private(pyObj))
            throw SyntalosPyError(
                "Data submission failed: "
                "Tried to send data via output port that can't carry it (sent data and port type are mismatched, or "
                "data can't be serialized).");
    }

    void _set_metadata_value_private(const QString &key, const QVariant &value)
    {
        auto slink = PyBridge::instance()->link();
        _oport->setMetadataVar(key, value);
        slink->updateOutputPort(_oport);
    }

    void set_metadata_value(const std::string &key, const py::object &obj)
    {
        if (PyLong_CheckExact(obj.ptr())) {
            // we have an integer type
            const long value = obj.cast<long>();
            _set_metadata_value_private(QString::fromStdString(key), QVariant::fromValue(value));
        } else if (py::isinstance<py::float_>(obj)) {
            _set_metadata_value_private(QString::fromStdString(key), QVariant::fromValue(obj.cast<double>()));
        } else if (PyUnicode_CheckExact(obj.ptr())) {
            // we have a (unicode) string type
            const auto value = obj.cast<std::string>();
            _set_metadata_value_private(
                QString::fromStdString(key), QVariant::fromValue(QString::fromStdString(value)));
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
                    throw SyntalosPyError(
                        std::string("Invalid type found in list metadata entry: ")
                        + std::string(lo.attr("__py::class__").attr("__name__").cast<std::string>()));
                }
            }
            _set_metadata_value_private(QString::fromStdString(key), varList);
        } else {
            throw SyntalosPyError(
                std::string("Can not set a metadata value for this type: ")
                + std::string(obj.attr("__py::class__").attr("__name__").cast<std::string>()));
        }
    }

    void set_metadata_value_size(const std::string &key, const py::object &value)
    {
        if (py::len(value) != 2)
            throw SyntalosPyError("2D Dimension list needs exactly two entries");

        if (!py::isinstance<py::sequence>(value)) {
            throw SyntalosPyError("Expected a sequence as size parameter.");
        }

        const auto seq = value.cast<py::sequence>();
        const auto width = seq[0].cast<int>();
        const auto height = seq[1].cast<int>();

        QSize size(width, height);
        _set_metadata_value_private(QString::fromStdString(key), QVariant::fromValue(size));
    }

    FirmataControl firmata_register_digital_pin(
        int pinId,
        const std::string &name,
        bool isOutput,
        bool isPullUp = false)
    {
        FirmataControl ctl;
        ctl.command = FirmataCommandKind::NEW_DIG_PIN;
        ctl.pinName = QString::fromStdString(name);
        ctl.pinId = pinId;
        ctl.isOutput = isOutput;
        ctl.isPullUp = isPullUp;

        submit(py::cast(ctl));
        return ctl;
    }

    FirmataControl firmata_submit_digital_value(const std::string &name, bool value)
    {
        FirmataControl ctl;
        ctl.command = FirmataCommandKind::WRITE_DIGITAL;
        ctl.pinName = QString::fromStdString(name);
        ctl.value = value;

        submit(py::cast(ctl));
        return ctl;
    }

    FirmataControl firmata_submit_digital_pulse(const std::string &name, int duration_msec = 50)
    {
        FirmataControl ctl;
        ctl.command = FirmataCommandKind::WRITE_DIGITAL_PULSE;
        ctl.pinName = QString::fromStdString(name);
        ctl.value = duration_msec;

        submit(py::cast(ctl));
        return ctl;
    }

    std::string _id;
    int _dataTypeId;
    const std::shared_ptr<OutputPortInfo> _oport;
};

static py::object get_input_port(const std::string &id)
{
    auto pb = PyBridge::instance();

    std::shared_ptr<InputPortInfo> res = nullptr;
    const auto idstr = QString::fromStdString(id);
    for (auto &iport : pb->link()->inputPorts()) {
        if (iport->id() == idstr) {
            res = iport;
            break;
        }
    }
    if (!res)
        return py::none();

    InputPort pyPort(res);
    return py::cast(pyPort);
}

static py::object get_output_port(const std::string &id)
{
    auto pb = PyBridge::instance();

    const auto idstr = QString::fromStdString(id);
    std::shared_ptr<OutputPortInfo> res = nullptr;
    for (auto &oport : pb->link()->outputPorts()) {
        if (oport->id() == idstr) {
            res = oport;
            break;
        }
    }
    if (!res)
        return py::none();

    OutputPort pyPort(res);
    return py::cast(pyPort);
}

static FirmataControl new_firmatactl_with_id_name(FirmataCommandKind kind, int pinId, const std::string &name)
{
    return {kind, pinId, QString::fromStdString(name)};
}

static FirmataControl new_firmatactl_with_id(FirmataCommandKind kind, int pinId)
{
    return {kind, pinId};
}

static FirmataControl new_firmatactl_with_name(FirmataCommandKind kind, const std::string &name)
{
    return {kind, QString::fromStdString(name)};
}

static SyntalosLink *init_link(SyntalosLink *slink = nullptr)
{
    SyntalosLink *finalLink = slink;
    if (finalLink == nullptr)
        finalLink = initSyntalosModuleLink().get();
    PyBridge::instance(finalLink);

    return finalLink;
}

#pragma GCC visibility push(default)
PYBIND11_MODULE(syntalos_mlink, m)
{
    m.doc() = "Syntalos Python Module Interface";

    py::bind_vector<std::vector<double>>(m, "VectorDouble");
    py::register_exception<SyntalosPyError>(m, "SyntalosPyError");
    pydef_cvnp(m);
    py::class_<SyntalosLink>(m, "SyntalosLink");

    m.def(
        "init_link",
        init_link,
        py::arg("slink") = nullptr,
        "Initialize the connection with a running Syntalos instance.");

    py::class_<InputPort>(m, "InputPort", "Representation of a module input port.")
        .def_property(
            "on_data",
            &InputPort::get_on_data,
            &InputPort::set_on_data,
            "Set function to be called when new data arrives.")
        .def_property_readonly("metadata", &InputPort::metadata, "Obtain the metadata associated with this input port.")
        .def(
            "set_throttle_items_per_sec",
            &InputPort::set_throttle_items_per_sec,
            "Limit the amount of input received to a set amount of elements per second.",
            py::arg("items_per_sec"))
        .def_readonly("name", &InputPort::_id);

    py::class_<OutputPort>(m, "OutputPort", "Representation of a module output port.")
        .def(
            "submit",
            &OutputPort::submit,
            "Submit the given entity to the output port for transfer to its destination(s).")
        .def_readonly("name", &OutputPort::_id)
        .def("set_metadata_value", &OutputPort::set_metadata_value, "Set (immutable) metadata value for this port.")
        .def(
            "set_metadata_value_size",
            &OutputPort::set_metadata_value_size,
            "Set (immutable) metadata value for a 2D size type for this port.")

        // convenience functions
        .def(
            "firmata_register_digital_pin",
            &OutputPort::firmata_register_digital_pin,
            py::arg("pin_id"),
            py::arg("name"),
            py::arg("is_output"),
            py::arg("is_pullup") = false,
            "Convenience function to create a command to register a named digital pin and immediately submit it on "
            "this port. "
            "The registered pin can later be referred to by its name.")
        .def(
            "firmata_submit_digital_value",
            &OutputPort::firmata_submit_digital_value,
            py::arg("name"),
            py::arg("value"),
            "Convenience function to write a digital value to a named pin.")
        .def(
            "firmata_submit_digital_pulse",
            &OutputPort::firmata_submit_digital_pulse,
            py::arg("name"),
            py::arg("duration_msec") = 50,
            "Convenience function to emit a digital pulse on a named pin.");

    /**
     ** Frames
     **/

    py::class_<Frame>(m, "Frame", "A video frame.")
        .def(py::init<>())
        .def_readwrite("index", &Frame::index, "Number of the frame.")
        .def_readwrite("time_usec", &Frame::time, "Time when the frame was recorded.")
        .def_readwrite("mat", &Frame::mat, "Frame image data.");

    /**
     ** Control Command
     **/

    py::enum_<ControlCommandKind>(m, "ControlCommandKind")
        .value("UNKNOWN", ControlCommandKind::UNKNOWN)
        .value("START", ControlCommandKind::START)
        .value("PAUSE", ControlCommandKind::PAUSE)
        .value("STOP", ControlCommandKind::STOP)
        .value("STEP", ControlCommandKind::STEP)
        .value("CUSTOM", ControlCommandKind::CUSTOM);

    py::class_<ControlCommand>(m, "ControlCommand")
        .def(py::init<>())
        .def(py::init<ControlCommandKind>())
        .def_readwrite("kind", &ControlCommand::kind)
        .def_property("duration", &ControlCommand::getDurationAsInt, &ControlCommand::setDuration)
        .def_readwrite("command", &ControlCommand::command);

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
        .value("SYSEX", FirmataCommandKind::SYSEX);

    py::class_<FirmataControl>(m, "FirmataControl")
        .def(py::init<>())
        .def(py::init<FirmataCommandKind>())
        .def_readwrite("command", &FirmataControl::command)
        .def_readwrite("pin_id", &FirmataControl::pinId)
        .def_readwrite("pin_name", &FirmataControl::pinName)
        .def_readwrite("is_output", &FirmataControl::isOutput)
        .def_readwrite("is_pullup", &FirmataControl::isPullUp)
        .def_readwrite("value", &FirmataControl::value);

    py::class_<FirmataData>(m, "FirmataData")
        .def(py::init<>())
        .def_readwrite("pin_id", &FirmataData::pinId)
        .def_readwrite("pin_name", &FirmataData::pinName)
        .def_readwrite("value", &FirmataData::value)
        .def_readwrite("is_digital", &FirmataData::isDigital)
        .def_readwrite("time", &FirmataData::time, "Time when the data was acquired.");

    /**
     ** Signal Blocks
     **/

    py::class_<IntSignalBlock>(m, "IntSignalBlock", "A block of timestamped integer signal data.")
        .def(py::init<>())
        .def_readwrite("timestamps", &IntSignalBlock::timestamps, "Timestamps of the data blocks.")
        .def_readwrite("data", &IntSignalBlock::data, "The data matrix.")
        .def_property_readonly("length", &IntSignalBlock::length)
        .def_property_readonly("rows", &IntSignalBlock::rows)
        .def_property_readonly("cols", &IntSignalBlock::cols);
    py::class_<FloatSignalBlock>(m, "FloatSignalBlock", "A block of timestamped float signal data.")
        .def(py::init<>())
        .def_readwrite("timestamps", &FloatSignalBlock::timestamps, "Timestamps of the data blocks.")
        .def_readwrite("data", &FloatSignalBlock::data, "The data matrix.")
        .def_property_readonly("length", &FloatSignalBlock::length)
        .def_property_readonly("rows", &FloatSignalBlock::rows)
        .def_property_readonly("cols", &FloatSignalBlock::cols);

    /**
     ** Additional Functions
     **/

    m.def("println", println, py::arg("text"), "Print text to stdout.");
    m.def(
        "raise_error",
        raise_error,
        py::arg("message"),
        "Emit an error message string, immediately terminating the current action and (if applicable) the experiment.");
    m.def("time_since_start_msec", time_since_start_msec, "Get time since experiment started in milliseconds.");
    m.def("time_since_start_usec", time_since_start_usec, "Get time since experiment started in microseconds.");
    m.def(
        "wait",
        wait,
        py::arg("msec"),
        "Wait (roughly) for the given amount of milliseconds without blocking communication with the master process.");
    m.def(
        "wait_sec",
        wait_sec,
        py::arg("sec"),
        "Wait (roughly) for the given amount of seconds without blocking communication with the master process.");
    m.def(
        "await_data",
        await_data,
        py::arg("timeout_usec") = -1,
        "Wait for new data to arrive and call selected callbacks. Also keep communication with the Syntalos master "
        "process.");
    m.def(
        "is_running",
        is_running,
        "Return True if the experiment is still running, False if we are supposed to shut down.");
    m.def(
        "schedule_delayed_call",
        &schedule_delayed_call,
        py::arg("delay_msec"),
        py::arg("callable_fn"),
        "Schedule call to a callable to be processed after a set amount of milliseconds.");

    m.def("get_input_port", get_input_port, py::arg("id"), "Get reference to input port with the give ID.");
    m.def("get_output_port", get_output_port, py::arg("id"), "Get reference to output port with the give ID.");

    m.def(
        "call_on_show_settings",
        &call_on_show_settings,
        py::arg("callable_fn"),
        "Call the given function when the module's settings dialog should be shown.");
    m.def(
        "call_on_show_display",
        &call_on_show_display,
        py::arg("callable_fn"),
        "Call the given function when the module's display window should be shown.");
    m.def(
        "save_settings",
        &save_settings,
        py::arg("settings_data"),
        "Send module settings data to Syntalos for safekeeping.");

    // Firmata helpers
    m.def(
        "new_firmatactl_with_id_name",
        new_firmatactl_with_id_name,
        py::arg("kind"),
        py::arg("pin_id"),
        py::arg("name"),
        "Create new Firmata control command with a given pin ID and registered name.");
    m.def(
        "new_firmatactl_with_id",
        new_firmatactl_with_id,
        py::arg("kind"),
        py::arg("pin_id"),
        "Create new Firmata control command with a given pin ID.");
    m.def(
        "new_firmatactl_with_name",
        new_firmatactl_with_name,
        py::arg("kind"),
        py::arg("name"),
        "Create new Firmata control command with a given pin name (the name needs to be registered previously).");
};
#pragma GCC visibility pop

void pythonRegisterSyioModule()
{
    PyImport_AppendInittab("syntalos_mlink", &PyInit_syntalos_mlink);
}

/*
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
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
#include <QTimer>
#include <QCoreApplication>

#include <iostream>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/eigen.h>
#include <stdexcept>

#include <syntalos-mlink>
#include "cvnp/cvnp.h"
#include "datactl/datatypes.h"
#include "datactl/frametype.h"
#include "qstringtopy.h" // needed for QString registration
#include "sydatatopy.h"  // needed for stream data type conversion

namespace py = pybind11;

PyBridge::PyBridge(SyntalosLink *mlink)
    : m_mlink(mlink)
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
    pb->link()->raiseError(message);
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
    pb->link()->setShowSettingsCallback([fn](const ByteVector &settings) {
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

static void save_settings(const ByteVector &settings_data)
{
    auto pb = PyBridge::instance();
    pb->link()->setSettingsData(settings_data);
}

struct InputPort {
    InputPort(const std::shared_ptr<InputPortInfo> &iport)
        : _iport(iport)
    {
        _id = _iport->id();
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

    MetaStringMap metadata() const
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
        _id = _oport->id();
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

    void _set_metadata_value_private(const std::string &key, const MetaValue &value)
    {
        auto slink = PyBridge::instance()->link();
        _oport->setMetadataVar(key, value);
        slink->updateOutputPort(_oport);
    }

    void set_metadata_value(const std::string &key, const py::object &obj)
    {
        if (PyLong_CheckExact(obj.ptr())) {
            // we have an integer type
            _set_metadata_value_private(key, obj.cast<int64_t>());
        } else if (py::isinstance<py::float_>(obj)) {
            _set_metadata_value_private(key, obj.cast<double>());
        } else if (PyUnicode_CheckExact(obj.ptr())) {
            // we have a (unicode) string type
            _set_metadata_value_private(key, obj.cast<std::string>());
        } else if (py::isinstance<MetaSize>(obj)) {
            _set_metadata_value_private(key, obj.cast<MetaSize>());
        } else if (PyList_Check(obj.ptr())) {
            const auto pyList = obj.cast<py::list>();
            const auto pyListLen = py::len(pyList);
            if (pyListLen == 0)
                return;
            MetaArray varList;
            varList.reserve(pyListLen);
            for (size_t i = 0; i < pyListLen; i++) {
                const auto lo = pyList[i];
                if (PyLong_CheckExact(lo.ptr())) {
                    varList.push_back(lo.cast<int64_t>());
                } else if (PyUnicode_CheckExact(lo.ptr())) {
                    varList.push_back(lo.cast<std::string>());
                } else {
                    throw SyntalosPyError(
                        std::string("Invalid type found in list metadata entry: ")
                        + lo.attr("__class__").attr("__name__").cast<std::string>());
                }
            }
            _set_metadata_value_private(key, varList);
        } else {
            throw SyntalosPyError(
                std::string("Cannot set metadata value of type: ")
                + obj.attr("__class__").attr("__name__").cast<std::string>());
        }
    }

    void set_metadata_value_size(const std::string &key, const py::object &value)
    {
        // Accept a MetaSize object directly
        if (py::isinstance<MetaSize>(value)) {
            _set_metadata_value_private(key, value.cast<MetaSize>());
            return;
        }

        if (!py::isinstance<py::sequence>(value))
            throw SyntalosPyError("Expected a MetaSize or a [width, height] sequence.");
        if (py::len(value) != 2)
            throw SyntalosPyError("Size sequence must have exactly two entries: [width, height].");

        const auto seq = value.cast<py::sequence>();
        _set_metadata_value_private(key, MetaSize(seq[0].cast<int32_t>(), seq[1].cast<int32_t>()));
    }

    FirmataControl firmata_register_digital_pin(
        int pinId,
        const std::string &name,
        bool isOutput,
        bool isPullUp = false)
    {
        FirmataControl ctl;
        ctl.command = FirmataCommandKind::NEW_DIG_PIN;
        ctl.pinName = name;
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
        ctl.pinName = name;
        ctl.value = value;

        submit(py::cast(ctl));
        return ctl;
    }

    FirmataControl firmata_submit_digital_pulse(const std::string &name, int duration_msec = 50)
    {
        FirmataControl ctl;
        ctl.command = FirmataCommandKind::WRITE_DIGITAL_PULSE;
        ctl.pinName = name;
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

/**
 * Declare a new input port for this module.
 *
 * Must be called at module level (top-level script code) so ports are registered
 * before Syntalos tries to restore project connections.
 *
 * @param id          Unique port identifier string.
 * @param title       Human-readable port title shown in the flow graph.
 * @param data_type   Data type name (e.g. "Frame", "TableRow", "IntSignalBlock").
 * @returns InputPort handle, or None if registration failed (e.g. duplicate ID).
 */
static py::object register_input_port(const std::string &id, const std::string &title, const std::string &data_type)
{
    auto pb = PyBridge::instance();
    auto res = pb->link()->registerInputPort(id, title, data_type);
    if (!res)
        return py::none();

    InputPort pyPort(res);
    return py::cast(pyPort);
}

/**
 * Declare a new output port for this module.
 *
 * Must be called at module level (top-level script code) so ports are registered
 * before Syntalos tries to restore project connections.
 *
 * @param id          Unique port identifier string.
 * @param title       Human-readable port title shown in the flow graph.
 * @param data_type   Data type name (e.g. "Frame", "TableRow", "IntSignalBlock").
 * @returns OutputPort handle, or None if registration failed (e.g. duplicate ID).
 */
static py::object register_output_port(const std::string &id, const std::string &title, const std::string &data_type)
{
    auto pb = PyBridge::instance();
    auto res = pb->link()->registerOutputPort(id, title, data_type);
    if (!res)
        return py::none();

    OutputPort pyPort(res);
    return py::cast(pyPort);
}

static FirmataControl new_firmatactl_with_id_name(FirmataCommandKind kind, int pinId, const std::string &name)
{
    return {kind, pinId, name};
}

static FirmataControl new_firmatactl_with_id(FirmataCommandKind kind, int pinId)
{
    return {kind, pinId};
}

static FirmataControl new_firmatactl_with_name(FirmataCommandKind kind, const std::string &name)
{
    return {kind, name};
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

    pydef_cvnp(m);
    py::register_exception<SyntalosPyError>(m, "SyntalosPyError");
    py::class_<SyntalosLink>(m, "SyntalosLink");

    m.def(
        "init_link",
        init_link,
        py::arg("slink") = nullptr,
        "Initialize the connection with a running Syntalos instance.\n"
        "\n"
        ":return: The active :class:`SyntalosLink` instance.\n"
        ":rtype: SyntalosLink");

    /**
     ** MetaSize
     **/

    py::class_<MetaSize>(m, "MetaSize", "Two-dimensional size value used in stream metadata.")
        .def(py::init<>(), "Construct a zero-size MetaSize.")
        .def(py::init<int32_t, int32_t>(), py::arg("width"), py::arg("height"), "Construct with explicit dimensions.")
        .def_readwrite("width", &MetaSize::width, "Width in pixels (or other integer units).")
        .def_readwrite("height", &MetaSize::height, "Height in pixels (or other integer units).")
        .def(
            "__repr__",
            [](const MetaSize &s) {
                return "MetaSize(" + std::to_string(s.width) + ", " + std::to_string(s.height) + ")";
            })
        .def("__eq__", &MetaSize::operator==)
        .def("__iter__", [](const MetaSize &s) {
            // Allow unpacking: width, height = meta_size
            return py::iter(py::make_tuple(s.width, s.height));
        });

    /**
     ** Frames
     **/

    py::class_<Frame>(m, "Frame", "A video frame.")
        .def(py::init<>())
        .def_readwrite("index", &Frame::index, "Number of the frame.")
        .def_readwrite("time", &Frame::time, "Time when the frame was recorded, as a duration.")
        .def_readwrite("mat", &Frame::mat, "Frame image data as a NumPy array (OpenCV ``Mat``).")
        // Convenience helpers
        .def_property(
            "time_usec",
            [](const Frame &f) {
                return f.time.count();
            },
            [](Frame &f, uint64_t v) {
                f.time = microseconds_t(v);
            },
            "Time when the frame was recorded, as an integer in µs.");

    /**
     ** Control Command
     **/

    py::enum_<ControlCommandKind>(
        m, "ControlCommandKind", "The type of a control command sent to controllable modules.")
        .value("UNKNOWN", ControlCommandKind::UNKNOWN)
        .value("START", ControlCommandKind::START, "Start an operation.")
        .value("PAUSE", ControlCommandKind::PAUSE, "Pause an operation; can be resumed with ``START``.")
        .value("STOP", ControlCommandKind::STOP, "Stop an operation.")
        .value("STEP", ControlCommandKind::STEP, "Advance operation by one step.")
        .value("CUSTOM", ControlCommandKind::CUSTOM, "Custom command.");

    py::class_<ControlCommand>(m, "ControlCommand", "A control command for a module.")
        .def(py::init<>())
        .def(py::init<ControlCommandKind>())
        .def_readwrite("kind", &ControlCommand::kind, "The :class:`ControlCommandKind` of this command.")
        .def_property(
            "duration",
            &ControlCommand::getDurationAsInt,
            &ControlCommand::setDuration,
            "Optional duration associated with this command, in milliseconds.")
        .def_readwrite(
            "command", &ControlCommand::command, "Custom command string (used when ``kind`` is ``CUSTOM``).");

    /**
     ** Firmata
     **/

    py::enum_<FirmataCommandKind>(m, "FirmataCommandKind", "Type of change to be made on a Firmata interface.")
        .value("UNKNOWN", FirmataCommandKind::UNKNOWN)
        .value("NEW_DIG_PIN", FirmataCommandKind::NEW_DIG_PIN, "Register a new digital pin.")
        .value("NEW_ANA_PIN", FirmataCommandKind::NEW_ANA_PIN, "Register a new analog pin.")
        .value("IO_MODE", FirmataCommandKind::IO_MODE, "Change a pin's I/O mode.")
        .value("WRITE_ANALOG", FirmataCommandKind::WRITE_ANALOG, "Write an analog value to a pin.")
        .value("WRITE_DIGITAL", FirmataCommandKind::WRITE_DIGITAL, "Write a digital value to a pin.")
        .value("WRITE_DIGITAL_PULSE", FirmataCommandKind::WRITE_DIGITAL_PULSE, "Emit a digital pulse on a pin.")
        .value("SYSEX", FirmataCommandKind::SYSEX, "Send a raw SysEx message.");

    py::class_<FirmataControl>(m, "FirmataControl", "Control command for a Firmata device.")
        .def(py::init<>())
        .def(py::init<FirmataCommandKind>())
        .def_readwrite("command", &FirmataControl::command, "The :class:`FirmataCommandKind` to execute.")
        .def_readwrite("pin_id", &FirmataControl::pinId, "Numeric pin identifier.")
        .def_readwrite("pin_name", &FirmataControl::pinName, "Registered name of the pin.")
        .def_readwrite("is_output", &FirmataControl::isOutput, "``True`` if the pin is configured as output.")
        .def_readwrite("is_pullup", &FirmataControl::isPullUp, "``True`` if the internal pull-up resistor is enabled.")
        .def_readwrite("value", &FirmataControl::value, "Value to write, or pulse duration in ms.");

    py::class_<FirmataData>(m, "FirmataData", "Data received from a Firmata device.")
        .def(py::init<>())
        .def_readwrite("pin_id", &FirmataData::pinId, "Numeric pin identifier.")
        .def_readwrite("pin_name", &FirmataData::pinName, "Registered name of the pin.")
        .def_readwrite("value", &FirmataData::value, "Received pin value.")
        .def_readwrite("is_digital", &FirmataData::isDigital, "``True`` if the value is digital, ``False`` if analog.")
        .def_readwrite("time", &FirmataData::time, "Time when the data was acquired, as a duration.")
        // Convenience helpers
        .def_property(
            "time_usec",
            [](const FirmataData &fm) {
                return fm.time.count();
            },
            [](FirmataData &fm, uint64_t v) {
                fm.time = microseconds_t(v);
            },
            "Time when the data was acquired, as an integer in µs.");

    /**
     ** Signal Blocks
     **/

    py::class_<IntSignalBlock>(m, "IntSignalBlock", "A block of timestamped integer signal data.")
        .def(py::init<>())
        .def_readwrite("timestamps", &IntSignalBlock::timestamps, "1-D array of sample timestamps in µs.")
        .def_readwrite("data", &IntSignalBlock::data, "2-D data matrix: rows = samples, columns = channels.")
        .def_property_readonly("length", &IntSignalBlock::length, "Number of samples (rows) in this block.")
        .def_property_readonly("rows", &IntSignalBlock::rows, "Number of rows (samples).")
        .def_property_readonly("cols", &IntSignalBlock::cols, "Number of columns (channels).");

    py::class_<FloatSignalBlock>(m, "FloatSignalBlock", "A block of timestamped float signal data.")
        .def(py::init<>())
        .def_readwrite("timestamps", &FloatSignalBlock::timestamps, "1-D array of sample timestamps in µs.")
        .def_readwrite("data", &FloatSignalBlock::data, "2-D data matrix: rows = samples, columns = channels.")
        .def_property_readonly("length", &FloatSignalBlock::length, "Number of samples (rows) in this block.")
        .def_property_readonly("rows", &FloatSignalBlock::rows, "Number of rows (samples).")
        .def_property_readonly("cols", &FloatSignalBlock::cols, "Number of columns (channels).");

    /**
     * Ports
     */

    py::class_<InputPort>(m, "InputPort", "A module input port.\n\nObtain an instance via :func:`get_input_port`.")
        .def_property(
            "on_data",
            &InputPort::get_on_data,
            &InputPort::set_on_data,
            "Callback invoked with each incoming data item.\n"
            "\n"
            "Assign a callable that accepts a single argument of the port's data type\n"
            "(e.g. :class:`Frame`, :class:`TableRow`). Set to ``None`` to remove the callback.")
        .def_property_readonly(
            "metadata",
            &InputPort::metadata,
            "Read-only ``dict[str, object]`` of metadata provided by the upstream module for this port.\n"
            "\n"
            "Values are native Python types: ``int``, ``float``, ``str``, ``bool``, ``None``,\n"
            ":class:`MetaSize`, or ``list`` / ``dict`` for nested structures.")
        .def(
            "set_throttle_items_per_sec",
            &InputPort::set_throttle_items_per_sec,
            "Limit the number of items delivered to ``on_data`` per second.\n"
            "\n"
            ":param items_per_sec: Maximum items per second; ``0`` disables throttling.\n"
            ":type items_per_sec: int",
            py::arg("items_per_sec"))
        .def_readonly("name", &InputPort::_id, "The unique port ID string.");

    py::class_<OutputPort>(m, "OutputPort", "A module output port.\n\nObtain an instance via :func:`get_output_port`.")
        .def(
            "submit",
            &OutputPort::submit,
            "Send a data item to all modules connected to this port.\n"
            "\n"
            ":param data: Data item matching this port's type (e.g. :class:`Frame`, :class:`TableRow`).\n"
            ":raises SyntalosPyError: If the item type does not match the port's declared data type.")
        .def_readonly("name", &OutputPort::_id, "The unique port ID string.")
        .def(
            "set_metadata_value",
            &OutputPort::set_metadata_value,
            "Set a metadata entry for this port.\n"
            "\n"
            "Metadata must be set before the run starts (i.e. in ``prepare()``); it is immutable once\n"
            "acquisition begins.\n"
            "\n"
            ":param key: Metadata key string.\n"
            ":param value: Metadata value. Accepted types: ``int``, ``float``, ``str``, :class:`MetaSize`,\n"
            "    or ``list`` of ``int``/``str`` values.\n"
            ":raises SyntalosPyError: If the value type is not supported.")
        .def(
            "set_metadata_value_size",
            &OutputPort::set_metadata_value_size,
            "Set a 2-D size metadata entry for this port (e.g. ``'size'`` for video streams).\n"
            "\n"
            ":param key: Metadata key string.\n"
            ":param value: Either a :class:`MetaSize` object or a sequence of exactly two integers ``[width, "
            "height]``.\n"
            ":raises SyntalosPyError: If ``value`` does not have exactly two elements.")

        // convenience functions
        .def(
            "firmata_register_digital_pin",
            &OutputPort::firmata_register_digital_pin,
            py::arg("pin_id"),
            py::arg("name"),
            py::arg("is_output"),
            py::arg("is_pullup") = false,
            "Register a named digital pin on the connected Firmata device and submit the command immediately.\n"
            "\n"
            "The pin can subsequently be referenced by ``name`` in :meth:`firmata_submit_digital_value`\n"
            "and :meth:`firmata_submit_digital_pulse`.\n"
            "\n"
            ":param pin_id: Numeric pin identifier on the device.\n"
            ":param name: Human-readable name to register for this pin.\n"
            ":param is_output: ``True`` to configure the pin as output, ``False`` for input.\n"
            ":param is_pullup: ``True`` to enable the internal pull-up resistor (default: ``False``).\n"
            ":return: The :class:`FirmataControl` command that was submitted.\n"
            ":rtype: FirmataControl")
        .def(
            "firmata_submit_digital_value",
            &OutputPort::firmata_submit_digital_value,
            py::arg("name"),
            py::arg("value"),
            "Write a digital value to a previously registered pin.\n"
            "\n"
            ":param name: Registered pin name.\n"
            ":param value: ``True`` / ``1`` for HIGH, ``False`` / ``0`` for LOW.\n"
            ":return: The :class:`FirmataControl` command that was submitted.\n"
            ":rtype: FirmataControl")
        .def(
            "firmata_submit_digital_pulse",
            &OutputPort::firmata_submit_digital_pulse,
            py::arg("name"),
            py::arg("duration_msec") = 50,
            "Emit a digital pulse on a previously registered pin.\n"
            "\n"
            ":param name: Registered pin name.\n"
            ":param duration_msec: Pulse duration in milliseconds (default: 50).\n"
            ":return: The :class:`FirmataControl` command that was submitted.\n"
            ":rtype: FirmataControl");

    /**
     ** Additional Functions
     **/

    m.def(
        "println",
        println,
        py::arg("text"),
        "Print a line of text to stdout.\n"
        "\n"
        ":param text: The text to print.");

    m.def(
        "raise_error",
        raise_error,
        py::arg("message"),
        "Raise a module error, immediately stopping the current run.\n"
        "\n"
        ":param message: Human-readable error description.");

    m.def(
        "time_since_start_msec",
        time_since_start_msec,
        "Return the time elapsed since the experiment started.\n"
        "\n"
        ":return: Elapsed time in milliseconds.\n"
        ":rtype: int");

    m.def(
        "time_since_start_usec",
        time_since_start_usec,
        "Return the time elapsed since the experiment started.\n"
        "\n"
        ":return: Elapsed time in microseconds.\n"
        ":rtype: int");

    m.def(
        "wait",
        wait,
        py::arg("msec"),
        "Sleep for approximately the given number of milliseconds.\n"
        "\n"
        "Unlike a plain ``time.sleep()``, this keeps the communication channel to Syntalos\n"
        "alive so that control messages (e.g. stop requests) are still handled.\n"
        "\n"
        ":param msec: Duration to wait in milliseconds.");

    m.def(
        "wait_sec",
        wait_sec,
        py::arg("sec"),
        "Sleep for approximately the given number of seconds.\n"
        "\n"
        "Unlike a plain ``time.sleep()``, this keeps the communication channel to Syntalos\n"
        "alive so that control messages (e.g. stop requests) are still handled.\n"
        "\n"
        ":param sec: Duration to wait in seconds.");

    m.def(
        "await_data",
        await_data,
        py::arg("timeout_usec") = -1,
        "Wait for incoming data and dispatch it to registered ``on_data`` callbacks.\n"
        "\n"
        "Also services the IPC channel to the Syntalos process. Call this regularly\n"
        "inside a ``run()`` loop to keep the module responsive.\n"
        "\n"
        ":param timeout_usec: Maximum time to block in microseconds. Pass ``-1`` (default) to\n"
        "    wait until the module is no longer in ``RUNNING`` state.");

    m.def(
        "is_running",
        is_running,
        "Check whether the experiment is still active.\n"
        "\n"
        ":return: ``True`` while the run is in progress, ``False`` once a stop has been requested.\n"
        ":rtype: bool");

    m.def(
        "schedule_delayed_call",
        &schedule_delayed_call,
        py::arg("delay_msec"),
        py::arg("callable_fn"),
        "Schedule a callable to be invoked after a delay.\n"
        "\n"
        "The call is executed on the module's event loop, so it is safe to interact\n"
        "with ports and other module state from the callback.\n"
        "\n"
        ":param delay_msec: Delay before the call is made, in milliseconds. Must be ≥ 0.\n"
        ":param callable_fn: Zero-argument callable to invoke.\n"
        ":raises SyntalosPyError: If ``delay_msec`` is negative.");

    m.def(
        "register_input_port",
        register_input_port,
        py::arg("id"),
        py::arg("title"),
        py::arg("data_type"),
        "Declare a new input port for this module.\n"
        "\n"
        "Must be called at module level so that Syntalos can discover the port topology and\n"
        "restore project connections before the first run is prepared.\n"
        "\n"
        ":param id: Unique port identifier used in :func:`get_input_port`.\n"
        ":param title: Human-readable port label shown in the flow-graph editor.\n"
        ":param data_type: Data type name, e.g. ``'Frame'``, ``'TableRow'``, ``'IntSignalBlock'``, etc.\n"
        ":return: An :class:`InputPort` handle, or ``None`` if registration failed (e.g. duplicate ID).\n"
        ":rtype: InputPort or None");

    m.def(
        "register_output_port",
        register_output_port,
        py::arg("id"),
        py::arg("title"),
        py::arg("data_type"),
        "Declare a new output port for this module.\n"
        "\n"
        "Must be called at module level (top-level script code, not inside a function)\n"
        "so that Syntalos can discover the port topology and restore project connections\n"
        "before the first run is prepared.\n"
        "\n"
        ":param id: Unique port identifier used in :func:`get_output_port`.\n"
        ":param title: Human-readable port label shown in the flow-graph editor.\n"
        ":param data_type: Data type name, e.g. ``'Frame'``, ``'TableRow'``, etc.\n"
        ":return: An :class:`OutputPort` handle, or ``None`` if registration failed (e.g. duplicate ID).\n"
        ":rtype: OutputPort or None");

    m.def(
        "get_input_port",
        get_input_port,
        py::arg("id"),
        "Retrieve a reference to an input port by its ID.\n"
        "\n"
        ":param id: The port ID passed to :func:`register_input_port`.\n"
        ":return: An :class:`InputPort` handle, or ``None`` if no port with that ID exists.\n"
        ":rtype: InputPort or None");

    m.def(
        "get_output_port",
        get_output_port,
        py::arg("id"),
        "Retrieve a reference to an output port by its ID.\n"
        "\n"
        ":param id: The port ID passed to :func:`register_output_port`.\n"
        ":return: An :class:`OutputPort` handle, or ``None`` if no port with that ID exists.\n"
        ":rtype: OutputPort or None");

    m.def(
        "call_on_show_settings",
        &call_on_show_settings,
        py::arg("callable_fn"),
        "Register a callback to be invoked when the user opens the module's settings dialog.\n"
        "\n"
        ":param callable_fn: Callable with signature ``fn(old_settings: bytes)``.");

    m.def(
        "call_on_show_display",
        &call_on_show_display,
        py::arg("callable_fn"),
        "Register a callback to be invoked when the user opens the module's display window.\n"
        "\n"
        ":param callable_fn: Zero-argument callable.");

    m.def(
        "save_settings",
        &save_settings,
        py::arg("settings_data"),
        "Persist the module's settings with Syntalos.\n"
        "\n"
        "The saved blob is passed back to the module as ``old_settings`` the next time the\n"
        "settings callback (see :func:`call_on_show_settings`) is invoked, and also delivered\n"
        "via ``set_settings()`` before each run.\n"
        "\n"
        ":param settings_data: Arbitrary settings payload serialized as ``bytes``.");

    // Firmata helpers
    m.def(
        "new_firmatactl_with_id_name",
        new_firmatactl_with_id_name,
        py::arg("kind"),
        py::arg("pin_id"),
        py::arg("name"),
        "Create a :class:`FirmataControl` command identified by both numeric ID and name.\n"
        "\n"
        ":param kind: The :class:`FirmataCommandKind` to execute.\n"
        ":param pin_id: Numeric pin identifier.\n"
        ":param name: Registered pin name.\n"
        ":return: A new :class:`FirmataControl` instance.\n"
        ":rtype: FirmataControl");

    m.def(
        "new_firmatactl_with_id",
        new_firmatactl_with_id,
        py::arg("kind"),
        py::arg("pin_id"),
        "Create a :class:`FirmataControl` command identified by numeric pin ID only.\n"
        "\n"
        ":param kind: The :class:`FirmataCommandKind` to execute.\n"
        ":param pin_id: Numeric pin identifier.\n"
        ":return: A new :class:`FirmataControl` instance.\n"
        ":rtype: FirmataControl");

    m.def(
        "new_firmatactl_with_name",
        new_firmatactl_with_name,
        py::arg("kind"),
        py::arg("name"),
        "Create a :class:`FirmataControl` command identified by a registered pin name.\n"
        "\n"
        "The pin must have been previously registered with :meth:`OutputPort.firmata_register_digital_pin`.\n"
        "\n"
        ":param kind: The :class:`FirmataCommandKind` to execute.\n"
        ":param name: Registered pin name.\n"
        ":return: A new :class:`FirmataControl` instance.\n"
        ":rtype: FirmataControl");
};
#pragma GCC visibility pop

void pythonRegisterSyioModule()
{
    PyImport_AppendInittab("syntalos_mlink", &PyInit_syntalos_mlink);
}

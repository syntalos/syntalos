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

#include <glib.h>

#include <chrono>
#include <iostream>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/stl/filesystem.h>
#include <pybind11/eigen.h>
#include <stdexcept>

#include <syntalos-mlink>
#include "cvnp/cvnp.h"
#include "datactl/datatypes.h"
#include "datactl/frametype.h"
#include "sydatatopy.h" // needed for stream data type conversion

namespace py = pybind11;

class PySyLinkManager;
static SyntalosLink *getActiveLink();

// Global Python interface object for the Syntalos link
static PySyLinkManager *g_pslMgr = nullptr;

using PyNewDataFn = std::function<void(const py::object &obj)>;

SyntalosPyError::SyntalosPyError(const char *what_arg)
    : std::runtime_error(what_arg) {};
SyntalosPyError::SyntalosPyError(const std::string &what_arg)
    : std::runtime_error(what_arg) {};

/**
 * Handle interpreter errors properly, and forward them to the frontend if sensible.
 *
 * This helper function should be called in a catch() scope.
 * @returns True if the error was handled, False if it should be rethrown.
 */
static bool handlePyError(SyntalosLink *m_slink, const py::error_already_set &e)
{
    // always gracefully handle errors by sending them to the frontend, if we can
    if (m_slink == nullptr)
        return false;
    if (m_slink->state() != ModuleState::ERROR)
        m_slink->raiseError(e.what());

    // let Python/interpreter shutdown semantics propagate (we also treat any assertion failure as fatal)
    if (e.matches(PyExc_SystemExit) || e.matches(PyExc_KeyboardInterrupt) || e.matches(PyExc_GeneratorExit)
        || e.matches(PyExc_AssertionError)) {
        m_slink->setShutdownPending(true);
        return false;
    }

    return true;
}

/**
 * Python binding for a Syntalos input port.
 */
struct InputPort {
    InputPort(const std::shared_ptr<InputPortInfo> &iport)
        : _iport(iport)
    {
        _id = _iport->id();
        _dataTypeId = _iport->dataTypeId();
    }

    void set_on_data(PyNewDataFn fn)
    {
        _on_data_cb = std::move(fn);
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
                default:
                    throw SyntalosPyError(std::format("Received data of unknown type on input port: {}", _id));
                }
            } catch (py::error_already_set &e) {
                if (!handlePyError(getActiveLink(), e))
                    throw;
            }
        });
    }

    [[nodiscard]] PyNewDataFn get_on_data() const
    {
        return _on_data_cb;
    }

    [[nodiscard]] MetaStringMap metadata() const
    {
        return _iport->metadata();
    }

    void set_throttle_items_per_sec(uint itemsPerSec)
    {
        _iport->setThrottleItemsPerSec(itemsPerSec);
        getActiveLink()->updateInputPort(_iport);
    }

    std::string _id;
    int _dataTypeId;
    const std::shared_ptr<InputPortInfo> _iport;
    PyNewDataFn _on_data_cb;
};

/**
 * Python binding for a Syntalos output port.
 */
struct OutputPort {
    OutputPort(const std::shared_ptr<OutputPortInfo> &oport)
        : _oport(oport)
    {
        _id = _oport->id();
        _dataTypeId = _oport->dataTypeId();
    }

    bool _submit_output_private(const py::object &pyObj)
    {
        auto slink = getActiveLink();
        switch (_oport->dataTypeId()) {
        case syDataTypeId<ControlCommand>():
            return slink->submitOutput(_oport, py::cast<const ControlCommand &>(pyObj));
        case syDataTypeId<TableRow>(): {
            // value-cast for sequence-construction path from Python list-like objects
            auto row = py::cast<TableRow>(pyObj);
            return slink->submitOutput(_oport, row);
        }
        case syDataTypeId<Frame>():
            return slink->submitOutput(_oport, py::cast<const Frame &>(pyObj));
        case syDataTypeId<FirmataControl>():
            return slink->submitOutput(_oport, py::cast<const FirmataControl &>(pyObj));
        case syDataTypeId<FirmataData>():
            return slink->submitOutput(_oport, py::cast<const FirmataData &>(pyObj));
        case syDataTypeId<IntSignalBlock>():
            return slink->submitOutput(_oport, py::cast<const IntSignalBlock &>(pyObj));
        case syDataTypeId<FloatSignalBlock>():
            return slink->submitOutput(_oport, py::cast<const FloatSignalBlock &>(pyObj));
        default:
            return false;
        }
    }

    [[noreturn]] static void _throw_submit_failed()
    {
        throw SyntalosPyError(
            "Data submission failed: "
            "Tried to send data via output port that can't carry it (sent data and port type are mismatched, or "
            "data can't be serialized).");
    }

    template<typename T>
    void _submit_typed_or_throw(const T &obj)
    {
        if (!getActiveLink()->submitOutput(_oport, obj))
            _throw_submit_failed();
    }

    void submit(const py::object &pyObj)
    {
        if (!_submit_output_private(pyObj))
            _throw_submit_failed();
    }

    void _set_metadata_value_private(const std::string &key, const MetaValue &value)
    {
        auto slink = getActiveLink();
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

        _submit_typed_or_throw(ctl);
        return ctl;
    }

    FirmataControl firmata_submit_digital_value(const std::string &name, bool value)
    {
        FirmataControl ctl;
        ctl.command = FirmataCommandKind::WRITE_DIGITAL;
        ctl.pinName = name;
        ctl.value = value;

        _submit_typed_or_throw(ctl);
        return ctl;
    }

    FirmataControl firmata_submit_digital_pulse(const std::string &name, int duration_msec = 50)
    {
        FirmataControl ctl;
        ctl.command = FirmataCommandKind::WRITE_DIGITAL_PULSE;
        ctl.pinName = name;
        ctl.value = duration_msec;

        _submit_typed_or_throw(ctl);
        return ctl;
    }

    std::string _id;
    int _dataTypeId;
    const std::shared_ptr<OutputPortInfo> _oport;
};

using PySaveSettingsFn = std::function<ByteVector(const fs::path &baseDir)>;

/**
 * Central link object for Syntalos Python modules.
 *
 * Returned by init_link() and exposed to Python as syntalos_mlink.SyntalosLink.
 * It serves as both the connection to Syntalos and the registry for all
 * lifecycle callbacks.
 */
class PySyLinkManager
{
private:
    SyntalosLink *m_slink;
    std::unique_ptr<SyntalosLink> m_ownedSLink;

    PrepareRunFn m_prepareFn;
    StartFn m_startFn;
    StopFn m_stopFn;

    PySaveSettingsFn m_saveSettingsFn;
    LoadSettingsFn m_loadSettingsFn;

    ShowSettingsFn m_showSettingsFn;
    ShowDisplayFn m_showDisplayFn;

public:
    /**
     * Standalone mode: creates and owns a new SyntalosLink.
     */
    PySyLinkManager(const ModuleInitOptions &optn = {})
    {
        ModuleInitOptions newOptn = optn;
        py::module_ sptMod;
        if (newOptn.renameThread) {
            // To fully rename the process, we would need access to the original
            // argv[], which Python does not provide. Rather than doing the horrendous
            // memory-scanning of setproctitle ourselves, we might as well call the real
            // thing, if it is installed.
            try {
                sptMod = py::module_::import("setproctitle");
            } catch (py::error_already_set &e) {
                if (!e.matches(PyExc_ImportError))
                    throw;
            }

            // if we have setproctitle, we prefer it over Syntalos' simple thread-renaming
            if (sptMod)
                newOptn.renameThread = false;
        }

        // connect
        m_ownedSLink = initSyntalosModuleLink(newOptn);
        m_slink = m_ownedSLink.get();

        // do the renaming, if the user wanted it
        if (optn.renameThread && sptMod)
            sptMod.attr("setproctitle")(m_slink->instanceId());
    }

    /**
     * PyWorker mode: borrows an existing SyntalosLink (caller retains ownership).
     */
    PySyLinkManager(SyntalosLink *borrowedLink)
        : m_slink(borrowedLink)
    {
    }

    PySyLinkManager(const PySyLinkManager &) = delete;
    PySyLinkManager &operator=(const PySyLinkManager &) = delete;
    ~PySyLinkManager()
    {
        cleanup();
    }

    /**
     * Drops all Python-capturing callbacks and (in standalone mode) destroys
     * the underlying SyntalosLink. Safe to call multiple times, but the object
     * must not be used anymore after calling this function.
     * /!\ This must only ever be called by Python when the object will be destroyed anyway!
     */
    void cleanup()
    {
        if (!m_slink)
            return;

        // Drop all C++ callbacks that capture Python objects while the interpreter
        // is still in a valid state.
        for (const auto &iport : m_slink->inputPorts())
            iport->setNewDataRawCallback(nullptr);

        m_slink->setPrepareRunCallback(nullptr);
        m_slink->setStartCallback(nullptr);
        m_slink->setStopCallback(nullptr);

        m_slink->setSaveSettingsCallback(nullptr);
        m_slink->setLoadSettingsCallback(nullptr);

        m_slink->setShowSettingsCallback(nullptr);
        m_slink->setShowDisplayCallback(nullptr);

        // In standalone mode, destroy the SyntalosLink now (while interpreter is valid).
        // In pyworker mode m_ownedSLink is empty, so this is a no-op.
        m_ownedSLink.reset();
        m_slink = nullptr;

        if (g_pslMgr == this)
            g_pslMgr = nullptr;
    }

    [[nodiscard]] SyntalosLink *link() const
    {
        return m_slink;
    }

    void setOnPrepare(PrepareRunFn fn)
    {
        m_prepareFn = std::move(fn);
        if (!m_prepareFn) {
            m_slink->setPrepareRunCallback([this]() {
                m_slink->setState(ModuleState::READY);
                return true;
            });
            return;
        }

        m_slink->setPrepareRunCallback([this]() {
            try {
                bool result = m_prepareFn();
                if (result) {
                    m_slink->setState(ModuleState::READY);
                    return true;
                }
                if (m_slink->state() != ModuleState::ERROR)
                    m_slink->raiseError("Module preparation failed (prepare() returned False).");
            } catch (py::error_already_set &e) {
                if (!handlePyError(m_slink, e))
                    throw;
            }

            return false;
        });
    }

    auto onPrepare()
    {
        return m_prepareFn;
    }

    void setOnStart(StartFn fn)
    {
        m_startFn = std::move(fn);
        if (!m_startFn) {
            m_slink->setStartCallback(nullptr);
            return;
        }

        m_slink->setStartCallback([this]() {
            m_slink->setState(ModuleState::RUNNING);
            try {
                m_startFn();
            } catch (py::error_already_set &e) {
                if (!handlePyError(m_slink, e))
                    throw;
            }
        });
    }

    auto onStart()
    {
        return m_startFn;
    }

    void setOnStop(StopFn fn)
    {
        m_stopFn = std::move(fn);
        if (!m_stopFn) {
            m_slink->setStopCallback(nullptr);
            return;
        }

        m_slink->setStopCallback([this]() {
            try {
                m_stopFn();
            } catch (py::error_already_set &e) {
                if (!handlePyError(m_slink, e))
                    throw;
            }
            m_slink->setState(ModuleState::IDLE);
        });
    }

    auto onStop()
    {
        return m_stopFn;
    }

    void setOnShowSettings(ShowSettingsFn fn)
    {
        m_showSettingsFn = std::move(fn);
        if (!m_showSettingsFn) {
            m_slink->setShowSettingsCallback(nullptr);
            return;
        }

        m_slink->setShowSettingsCallback([this]() {
            try {
                m_showSettingsFn();
            } catch (py::error_already_set &e) {
                if (!handlePyError(m_slink, e))
                    throw;
            }
        });
    }

    auto onShowSettings()
    {
        return m_showSettingsFn;
    }

    void setOnShowDisplay(ShowDisplayFn fn)
    {
        m_showDisplayFn = std::move(fn);
        if (!m_showDisplayFn) {
            m_slink->setShowDisplayCallback(nullptr);
            return;
        }

        m_slink->setShowDisplayCallback([this]() {
            try {
                m_showDisplayFn();
            } catch (py::error_already_set &e) {
                if (!handlePyError(m_slink, e))
                    throw;
            }
        });
    }

    auto onShowDisplay()
    {
        return m_showDisplayFn;
    }

    void setOnSaveSettings(PySaveSettingsFn fn)
    {
        m_saveSettingsFn = std::move(fn);
        if (!m_saveSettingsFn) {
            m_slink->setSaveSettingsCallback(nullptr);
            return;
        }

        m_slink->setSaveSettingsCallback([this](ByteVector &settings, const fs::path &baseDir) {
            try {
                settings = m_saveSettingsFn(baseDir);
                return true;
            } catch (py::error_already_set &e) {
                if (!handlePyError(m_slink, e))
                    throw;
                return false;
            }
        });
    }

    auto onSaveSettings()
    {
        return m_saveSettingsFn;
    }

    void setOnLoadSettings(LoadSettingsFn fn)
    {
        m_loadSettingsFn = std::move(fn);
        if (!m_loadSettingsFn) {
            m_slink->setLoadSettingsCallback(nullptr);
            return;
        }

        m_slink->setLoadSettingsCallback([this](const ByteVector &settings, const fs::path &baseDir) {
            try {
                return m_loadSettingsFn(settings, baseDir);
            } catch (py::error_already_set &e) {
                if (!handlePyError(m_slink, e))
                    throw;
                return false;
            }
        });
    }

    auto onLoadSettings()
    {
        return m_loadSettingsFn;
    }

    /**
     * Declare a new input port for this module.
     */
    InputPort registerInputPort(const std::string &id, const std::string &title, BaseDataType::TypeId data_type)
    {
        if (auto res = m_slink->registerInputPort(id, title, data_type); res.has_value())
            return {*res};
        else
            throw std::runtime_error(res.error());
    }

    /**
     * Declare a new output port for this module.
     */
    OutputPort registerOutputPort(const std::string &id, const std::string &title, BaseDataType::TypeId data_type)
    {
        if (auto res = m_slink->registerOutputPort(id, title, data_type); res.has_value())
            return {*res};
        else
            throw std::runtime_error(res.error());
    }

    /**
     * Signal IDLE without entering the event loop (for custom loops).
     */
    void signalIdle()
    {
        m_slink->setState(ModuleState::IDLE);
    }

    /**
     * Returns true if the experiment is currently running.
     */
    bool isRunning()
    {
        return m_slink->state() == ModuleState::RUNNING;
    }

    /**
     * Single-shot data poll (for custom event loops).
     */
    void awaitData(int timeoutUsec)
    {
        m_slink->awaitData(timeoutUsec);
    }

    /**
     * Signal IDLE (initialization complete) and run the built-in event loop.
     */
    void awaitDataForever(std::function<void()> eventFn)
    {
        signalIdle();

        std::function<void()> evtFn = nullptr;
        if (eventFn) {
            evtFn = [eventFn = std::move(eventFn), this]() {
                try {
                    eventFn();
                } catch (py::error_already_set &e) {
                    if (!handlePyError(m_slink, e))
                        throw;
                }
            };
        }

        m_slink->awaitDataForever(evtFn, 25 * 1000); // 25ms interval keeps GLib sources responsive
    }

    bool allowAsyncStart()
    {
        return m_slink->allowAsyncStart();
    }

    void setAllowAsyncStart(bool allow)
    {
        m_slink->setAllowAsyncStart(allow);
    }

    py::bytes runUuid()
    {
        const auto &uuid = m_slink->runInfo().uuid.bytes;
        return py::bytes(reinterpret_cast<const char *>(uuid.data()), uuid.size());
    }

    std::shared_ptr<EDLGroup> rootGroup()
    {
        auto g = m_slink->runInfo().rootGroup;
        if (!g)
            throw SyntalosPyError("No EDL root group available (PrepareRun not received yet).");
        return g;
    }

    std::shared_ptr<EDLDataset> createDefaultDataset(const std::string &preferredName)
    {
        auto res = m_slink->createDefaultDataset(preferredName);
        if (!res)
            throw SyntalosPyError(res.error());
        return *res;
    }

    std::shared_ptr<EDLGroup> createStorageGroup(const std::string &name)
    {
        auto res = m_slink->createStorageGroup(name);
        if (!res)
            throw SyntalosPyError(res.error());
        return *res;
    }

    std::shared_ptr<EDLDataset> createDatasetInGroup(const std::shared_ptr<EDLGroup> &parent, const std::string &name)
    {
        auto res = m_slink->reserveEdlDataset(parent, name);
        if (!res)
            throw SyntalosPyError(res.error());
        return *res;
    }
};

static SyntalosLink *getActiveLink()
{
    if (g_pslMgr == nullptr)
        throw SyntalosPyError("Syntalos Module Link was not initialized. Call `syntalos_mlink.init_link()` first!");
    return g_pslMgr->link();
}

static PySyLinkManager *getActiveManager()
{
    if (g_pslMgr == nullptr)
        throw SyntalosPyError("Syntalos Module Link was not initialized. Call `syntalos_mlink.init_link()` first!");
    return g_pslMgr;
}

static uint64_t time_since_start_msec()
{
    return getActiveLink()->timer()->timeSinceStartMsec().count();
}

static uint64_t time_since_start_usec()
{
    return getActiveLink()->timer()->timeSinceStartUsec().count();
}

static void println(const std::string &text)
{
    std::cout << text << std::endl;
}

static void raise_error(const std::string &message)
{
    getActiveLink()->raiseError(message);
}

static void wait(uint msec)
{
    auto slink = getActiveLink();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(msec);
    while (std::chrono::steady_clock::now() < deadline)
        slink->awaitData(5 * 1000);
}

static void wait_sec(uint sec)
{
    auto slink = getActiveLink();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(sec);
    while (std::chrono::steady_clock::now() < deadline)
        slink->awaitData(50 * 1000);
}

static bool is_running()
{
    return getActiveManager()->isRunning();
}

static void await_data(int timeout_usec)
{
    getActiveLink()->awaitData(timeout_usec);
}

struct DelayedCallPayload {
    std::function<void()> fn;
};

static gboolean dispatch_delayed_call(gpointer userData)
{
    std::unique_ptr<DelayedCallPayload> payload(static_cast<DelayedCallPayload *>(userData));
    try {
        payload->fn();
    } catch (py::error_already_set &e) {
        if (!handlePyError(getActiveLink(), e))
            throw;
    }
    return G_SOURCE_REMOVE;
}

static void schedule_delayed_call(int delay_msec, const std::function<void()> &fn)
{
    if (delay_msec < 0)
        throw SyntalosPyError("Delay must be positive or zero.");

    auto payload = std::make_unique<DelayedCallPayload>();
    payload->fn = fn;

    g_autoptr(GSource) source = g_timeout_source_new(static_cast<guint>(delay_msec));
    g_source_set_callback(source, &dispatch_delayed_call, payload.release(), nullptr);
    g_source_attach(source, g_main_context_default());
}

static std::optional<InputPort> get_input_port(const std::string &id)
{
    std::shared_ptr<InputPortInfo> res = nullptr;
    for (auto &iport : getActiveLink()->inputPorts()) {
        if (iport->id() == id) {
            res = iport;
            break;
        }
    }
    if (!res)
        return std::nullopt;

    return InputPort(res);
}

static std::optional<OutputPort> get_output_port(const std::string &id)
{
    std::shared_ptr<OutputPortInfo> res = nullptr;
    for (auto &oport : getActiveLink()->outputPorts()) {
        if (oport->id() == id) {
            res = oport;
            break;
        }
    }
    if (!res)
        return std::nullopt;

    return OutputPort(res);
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

static PySyLinkManager *init_link_impl(const ModuleInitOptions &optn, SyntalosLink *slink = nullptr)
{
    if (g_pslMgr != nullptr)
        throw SyntalosPyError(
            "Syntalos Module Link was already initialized. It is not allowed to run `init_link()` twice!");

    g_pslMgr = (slink != nullptr) ? new PySyLinkManager(slink) : new PySyLinkManager(optn);

    if (slink == nullptr) {
        // Hold a Python-side reference so the wrapper survives until atexit.
        py::object pyObj = py::cast(g_pslMgr, py::return_value_policy::reference);
        // Standalone mode: register a Python atexit handler that calls cleanup().
        // We capture pyObj (not a raw pointer) so that the lambda holds a Python reference to
        // the PySyLinkManager wrapper. Without this, Python GC could collect the wrapper before
        // atexit fires (e.g. when main() returns and the last Python-side reference is released),
        // turning rawMgr into a dangling pointer and causing a crash on cleanup.
        py::module_::import("atexit").attr("register")(py::cpp_function([pyObj]() {
            (void)pyObj; // keep a Python reference alive until atexit invocation
            if (g_pslMgr)
                g_pslMgr->cleanup();
        }));
    }

    return g_pslMgr;
}

static PySyLinkManager *init_link(bool rename_process = false)
{
    return init_link_impl({.renameThread = rename_process}, nullptr);
}

static PySyLinkManager *_init_link_with_handle(SyntalosLink *slink)
{
    if (slink == nullptr)
        throw SyntalosPyError("_init_link_with_handle() requires a valid Syntalos link handle.");
    return init_link_impl({}, slink);
}

#pragma GCC visibility push(default)
PYBIND11_MODULE(syntalos_mlink, m)
{
    m.doc() = "Syntalos Python Module Interface";

    pydef_cvnp(m);
    py::register_exception<SyntalosPyError>(m, "SyntalosPyError");

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
        .def("__eq__", &MetaSize::operator==, py::arg("other"))
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
     ** Data Type IDs
     **/

    py::enum_<BaseDataType::TypeId>(m, "DataType", "Identifies the data type of a port / stream.")
        .value("ControlCommand", BaseDataType::TypeId::ControlCommand, "Module control command.")
        .value("TableRow", BaseDataType::TypeId::TableRow, "A row of tabular data.")
        .value("Frame", BaseDataType::TypeId::Frame, "A video frame.")
        .value("FirmataControl", BaseDataType::TypeId::FirmataControl, "Firmata device control message.")
        .value("FirmataData", BaseDataType::TypeId::FirmataData, "Data received from a Firmata device.")
        .value("IntSignalBlock", BaseDataType::TypeId::IntSignalBlock, "A block of integer signal samples.")
        .value("FloatSignalBlock", BaseDataType::TypeId::FloatSignalBlock, "A block of float signal samples.");

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
            "(e.g. :class:`Frame`, :class:`TableRow`). Set to ``None`` to remove the callback.\n"
            "\n"
            "Type: ``Callable[[object], None] | None``.")
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
            py::arg("data"),
            "Send a data item to all modules connected to this port.\n"
            "\n"
            ":param data: Data item matching this port's type (e.g. :class:`Frame`, :class:`TableRow`).\n"
            ":raises SyntalosPyError: If the item type does not match the port's declared data type.")
        .def_readonly("name", &OutputPort::_id, "The unique port ID string.")
        .def(
            "set_metadata_value",
            &OutputPort::set_metadata_value,
            py::arg("key"),
            py::arg("value"),
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
            py::arg("key"),
            py::arg("value"),
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
            py::return_value_policy::move,
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
            py::return_value_policy::move,
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
            py::return_value_policy::move,
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
        "schedule_delayed_call",
        &schedule_delayed_call,
        py::arg("delay_msec"),
        py::arg("callable_fn"),
        "Schedule a callable to be invoked after a delay.\n"
        "\n"
        "The call is executed on the module's event loop, so it is safe to interact\n"
        "with ports and other module state from the callback.\n"
        "\n"
        "Signature: ``schedule_delayed_call(delay_msec: int, callable_fn: Callable[[], None]) -> None``.\n"
        "\n"
        ":param delay_msec: Delay before the call is made, in milliseconds. Must be >= 0.\n"
        ":param callable_fn: Zero-argument callable to invoke.\n"
        ":raises SyntalosPyError: If ``delay_msec`` is negative.");

    /**
     ** Functions for the PyScript module (where ports are defined externally, in GUI)
     **/

    m.def(
        "get_input_port",
        get_input_port,
        py::arg("id"),
        "Retrieve a reference to an input port by its ID.\n"
        "\n"
        ":param id: The port ID (assigned via :meth:`SyntalosLink.register_input_port` or via the PyScript GUI "
        "editor).\n"
        ":return: An :class:`InputPort` handle, or ``None`` if no port with that ID exists.\n"
        ":rtype: InputPort or None");

    m.def(
        "get_output_port",
        get_output_port,
        py::arg("id"),
        "Retrieve a reference to an output port by its ID.\n"
        "\n"
        ":param id: The port ID (assigned via :meth:`SyntalosLink.register_output_port` or via the PyScript GUI "
        "editor).\n"
        ":return: An :class:`OutputPort` handle, or ``None`` if no port with that ID exists.\n"
        ":rtype: OutputPort or None");

    m.def(
        "is_running",
        is_running,
        "Check whether the experiment is still active.\n"
        "\n"
        ":return: ``True`` while the run is in progress, ``False`` once a stop has been requested.\n"
        ":rtype: bool");

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

    /**
     ** Firmata helpers
     **/

    m.def(
        "new_firmatactl_with_id_name",
        new_firmatactl_with_id_name,
        py::return_value_policy::move,
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
        py::return_value_policy::move,
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
        py::return_value_policy::move,
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

    /**
     * Module registration (for standalone Python modules)
     **/

    py::class_<PySyLinkManager>(m, "SyntalosLink", "Manages the connection to Syntalos.")
        .def_property(
            "on_prepare",
            &PySyLinkManager::onPrepare,
            &PySyLinkManager::setOnPrepare,
            "Register a callback invoked when Syntalos prepares a new run.\n"
            "\n"
            "The callback must return ``True`` to signal that preparation succeeded,\n"
            "or ``False`` / raise an exception to abort.\n"
            "If not registered the module automatically signals readiness.\n"
            "\n"
            "Type: ``Callable[[], bool] | None``.")
        .def_property(
            "on_start",
            &PySyLinkManager::onStart,
            &PySyLinkManager::setOnStart,
            "Register a callback invoked when Syntalos starts a run.\n"
            "\n"
            "Type: ``Callable[[], None] | None``.")
        .def_property(
            "on_stop",
            &PySyLinkManager::onStop,
            &PySyLinkManager::setOnStop,
            "Register a callback invoked when Syntalos stops a run.\n"
            "\n"
            "Type: ``Callable[[], None] | None``.")
        .def_property(
            "on_show_settings",
            &PySyLinkManager::onShowSettings,
            &PySyLinkManager::setOnShowSettings,
            "Register a callback invoked when the user opens the module settings dialog.\n"
            "\n"
            "Type: ``Callable[[], None] | None``.")
        .def_property(
            "on_show_display",
            &PySyLinkManager::onShowDisplay,
            &PySyLinkManager::setOnShowDisplay,
            "Register a callback invoked when the user opens the module display window.\n"
            "\n"
            "Type: ``Callable[[], None] | None``.")
        .def_property(
            "on_save_settings",
            &PySyLinkManager::onSaveSettings,
            &PySyLinkManager::setOnSaveSettings,
            "Register a callback invoked when Syntalos saves settings for this module.\n"
            "\n"
            "The callback receives the base project directory as a ``pathlib.Path`` and must return a\n"
            "bytes-like payload (``bytes`` or ``bytearray``). If not registered, no module settings are saved.\n"
            "\n"
            "Type: ``Callable[[pathlib.Path], bytes | bytearray] | None``.")
        .def_property(
            "on_load_settings",
            &PySyLinkManager::onLoadSettings,
            &PySyLinkManager::setOnLoadSettings,
            "Register a callback invoked when Syntalos loads settings for this module.\n"
            "\n"
            "The callback receives the loaded settings payload first (``bytes`` / ``bytearray``), followed by\n"
            "the base project directory as ``pathlib.Path``.\n"
            "The callback must return ``True`` to signal that the settings were loaded successfully, or"
            " ``False`` / use ``raise_error`` to signal a loading failure.\n"
            "If not registered, incoming settings are ignored.\n"
            "\n"
            "Type: ``Callable[[bytes | bytearray, pathlib.Path], bool] | None``.")
        .def(
            "register_input_port",
            &PySyLinkManager::registerInputPort,
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
            ":param data_type: :class:`DataType` value, e.g. ``DataType.Frame``, ``DataType.TableRow``.\n"
            ":return: An :class:`InputPort` handle.\n"
            ":rtype: InputPort")
        .def(
            "register_output_port",
            &PySyLinkManager::registerOutputPort,
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
            ":param data_type: :class:`DataType` value, e.g. ``DataType.Frame``, ``DataType.TableRow``.\n"
            ":return: An :class:`OutputPort` handle.\n"
            ":rtype: OutputPort")

        .def_property_readonly(
            "is_running",
            &PySyLinkManager::isRunning,
            "Check whether the experiment is still active.\n"
            "\n"
            ":return: ``True`` while the run is in progress, ``False`` once a stop has been requested.\n"
            ":rtype: bool")
        .def_property(
            "allow_async_start",
            &PySyLinkManager::allowAsyncStart,
            &PySyLinkManager::setAllowAsyncStart,
            "Whether start() is run in parallel with other modules for this module.\n"
            "\n"
            "On startup, Syntalos can send START to all external modules in parallel,"
            "not waiting for them to finish individual START actions.\n"
            "This is the async-start method, and it results in much better aligned start times of modules."
            "However, some modules may need to modify stream metadata in START and must ensure it is received"
            "before their streams are started.\n"
            "Those modules may want to disable async-start for themselves, so they run exclusively and the engine"
            "waits for them to complete START actions.")

        .def(
            "await_data",
            &PySyLinkManager::awaitData,
            py::arg("timeout_usec") = -1,
            "Single-shot data poll for use inside a custom event loop.\n"
            "\n"
            ":param timeout_usec: Maximum time to block in µs; ``-1`` waits indefinitely.")
        .def(
            "await_data_forever",
            &PySyLinkManager::awaitDataForever,
            py::arg("event_fn") = py::none(),
            "Signal initialization complete and run the module event loop.\n"
            "\n"
            "Signals IDLE to Syntalos then blocks until a shutdown is requested.\n"
            "\n"
            ":param event_fn: Optional zero-argument callable, invoked regularly to pump an external event loop.")
        .def(
            "signal_idle",
            &PySyLinkManager::signalIdle,
            "Signal IDLE (initialization complete) without entering the built-in event loop.\n"
            "\n"
            "Call this before starting a custom loop with :meth:`await_data`.")

        // ---- EDL storage access ----

        .def_property_readonly(
            "run_uuid",
            &PySyLinkManager::runUuid,
            "The run's collection UUID as 16 raw bytes (UUIDv7). Available after prepare().")
        .def_property_readonly(
            "root_group",
            &PySyLinkManager::rootGroup,
            "The local EDL root group (:class:`EdlGroup`). Available after prepare().")
        .def(
            "create_default_dataset",
            &PySyLinkManager::createDefaultDataset,
            py::arg("preferred_name") = std::string{},
            "Create the default dataset for this module under the assigned EDL root group.\n"
            "\n"
            "Uses MUST_CREATE semantics: fails if a dataset with the same name already exists.\n"
            "Must be called inside :attr:`on_prepare`.\n"
            "\n"
            ":param preferred_name: Dataset name; defaults to the module's group name.\n"
            ":return: The new :class:`EdlDataset`.\n"
            ":rtype: EdlDataset")
        .def(
            "create_storage_group",
            &PySyLinkManager::createStorageGroup,
            py::arg("name"),
            "Create (or open) a sub-group under the module's assigned EDL root group.\n"
            "\n"
            ":param name: Group name.\n"
            ":return: The :class:`EdlGroup`.\n"
            ":rtype: EdlGroup")
        .def(
            "create_dataset_in_group",
            &PySyLinkManager::createDatasetInGroup,
            py::arg("group"),
            py::arg("name"),
            "Create a new dataset inside the given group with MUST_CREATE semantics.\n"
            "\n"
            ":param group: Parent :class:`EdlGroup`.\n"
            ":param name: Dataset name.\n"
            ":return: The new :class:`EdlDataset`.\n"
            ":rtype: EdlDataset");

    py::class_<EDLGroup, std::shared_ptr<EDLGroup>>(
        m, "EdlGroup", "An EDL group that can contain datasets and sub-groups.")
        .def_property_readonly("path", &EDLGroup::path, "Filesystem path of this group (pathlib.Path).")
        .def_property_readonly("name", &EDLGroup::name, "Name of this group (str).")
        .def(
            "create_group",
            [](std::shared_ptr<EDLGroup> g, const std::string &name) {
                auto res = getActiveLink()->reserveEdlGroup(g, name);
                if (!res)
                    throw SyntalosPyError(res.error());
                return *res;
            },
            py::arg("name"),
            "Create (or open) a sub-group with the given name.\n"
            ":rtype: EdlGroup")
        .def(
            "create_dataset",
            [](std::shared_ptr<EDLGroup> g, const std::string &name) {
                auto res = getActiveLink()->reserveEdlDataset(g, name);
                if (!res)
                    throw SyntalosPyError(res.error());
                return *res;
            },
            py::arg("name"),
            "Create a dataset with the given name (MUST_CREATE semantics).\n"
            ":rtype: EdlDataset")
        .def(
            "set_attribute",
            &EDLGroup::insertAttribute,
            py::arg("key"),
            py::arg("value"),
            "Set an attribute on this group.");

    py::class_<EDLDataset, std::shared_ptr<EDLDataset>>(
        m, "EdlDataset", "An EDL dataset that holds data files for a modality.")
        .def_property_readonly("path", &EDLDataset::path, "Filesystem path of this dataset's directory (pathlib.Path).")
        .def_property_readonly("name", &EDLDataset::name, "Name of this dataset (str).")
        .def_property_readonly("is_empty", &EDLDataset::isEmpty, "True if no data file has been registered yet.")
        .def(
            "set_data_file",
            [](EDLDataset &ds, const std::string &fileName, const std::string &summary, bool scanPattern) {
                if (scanPattern) {
                    ds.setDataScanPattern(fileName, summary);
                    return ds.path();
                }
                return ds.setDataFile(fileName, summary);
            },
            py::arg("file_name"),
            py::arg("summary") = std::string{},
            py::arg("scan") = false,
            "Register the primary data file (or a scan pattern).\n"
            "\n"
            "Returns the path where the data should be written (pathlib.Path).\n"
            ":param scan: If True, treat ``file_name`` as a glob pattern scanned at save time.")
        .def(
            "add_aux_file",
            [](EDLDataset &ds,
               const std::string &fileName,
               const std::string &key,
               const std::string &summary,
               bool scanPattern) {
                if (scanPattern) {
                    ds.addAuxDataScanPattern(fileName, summary);
                    return ds.path();
                }
                auto res = ds.addAuxDataFile(fileName, key, summary);
                if (!res)
                    throw SyntalosPyError(res.error());
                return *res;
            },
            py::arg("file_name"),
            py::arg("key") = std::string{},
            py::arg("summary") = std::string{},
            py::arg("scan") = false,
            "Register an auxiliary data file (or scan pattern).\n"
            "\n"
            "Returns the path where the data should be written (pathlib.Path).")
        .def(
            "path_for_basename",
            &EDLDataset::pathForDataBasename,
            py::arg("basename"),
            "Return the path for a data file with the given basename (pathlib.Path).\n"
            "\n"
            "The file is *not* registered. Use a scan pattern to include it in the manifest.")
        .def(
            "set_attribute",
            &EDLDataset::insertAttribute,
            py::arg("key"),
            py::arg("value"),
            "Set an attribute on this dataset.");

    m.def(
        "init_link",
        &init_link,
        py::kw_only(),
        py::arg("rename_process") = false,
        py::return_value_policy::take_ownership,
        "Initialize a connection with Syntalos.\n"
        "\n"
        "This function must be called only once at program startup, before invoking any\n"
        "other methods on this module.\n"
        "\n"
        ":param rename_process: Rename the current process to match the module identifier."
        ":return: The active :class:`SyntalosLink` registry object.\n"
        ":rtype: SyntalosLink");

    // Register SyntalosLink as an opaque handle so pyworker can pass its pointer
    // to init_link() via py::cast without exposing any of its C++ API to Python.
    py::class_<SyntalosLink>(m, "_SyntalosLinkHandle");

    // Internal entry point for the embedded PyWorker runtime.
    m.def("_init_link_with_handle", &_init_link_with_handle, py::return_value_policy::take_ownership, py::arg("slink"));
};
#pragma GCC visibility pop

void pythonRegisterSyioModule()
{
    PyImport_AppendInittab("syntalos_mlink", &PyInit_syntalos_mlink);
}

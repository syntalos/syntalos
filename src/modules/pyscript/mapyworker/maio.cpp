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

#include <QJsonArray>
#include <QStringList>
#include <boost/python.hpp>
#include <boost/python/list.hpp>
#include <boost/python/extract.hpp>
#include <stdexcept>

#include "zmqclient.h"

using namespace boost::python;

// error variable for Python interface
static PyObject *MaIOError;

#if 0
static PyObject *maio_new_digital_pin(PyObject* self, PyObject* args)
{
    const char *pinName;
    const char *kindStr;
    int pinId;

    Q_UNUSED(self);
    if (!PyArg_ParseTuple(args, "iss", &pinId, &pinName, &kindStr))
        return NULL;

    auto maio = MaIO::instance();
    maio->newDigitalPin(pinId, QString::fromUtf8(pinName), QString::fromUtf8(kindStr));

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *maio_set_events_header(PyObject* self, PyObject* args)
{
    Q_UNUSED(self);
    PyObject *obj;

    if (!PyArg_ParseTuple(args, "O", &obj))
        return NULL;

    PyObject *iter = PyObject_GetIter(obj);
    if (!iter) {
        PyErr_SetString(MaIOError, "Expected an interable type (e.g. a list) as parameter.");
        return NULL;
    }

    QStringList header;
    while (true) {
        PyObject *next = PyIter_Next(iter);
        if (!next)
            break;

        if (!PyUnicode_Check(next)) {
            PyErr_SetString(MaIOError, "Expected a list of strings as parameter.");
            return NULL;
        }

        header << QString::fromUtf8(PyUnicode_AsUTF8(next));
    }

    auto maio = MaIO::instance();
    maio->setEventsHeader(header);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *maio_fetch_digital_input(PyObject* self, PyObject* args)
{
    Q_UNUSED(self);
    Q_UNUSED(args);
    PyObject *res;

    QPair<QString, bool> pair;
    if (MaIO::instance()->fetchDigitalInput(&pair)) {
        res = Py_BuildValue("OsO", Py_True, qPrintable(pair.first), pair.second? Py_True : Py_False);
    } else {
        res = Py_BuildValue("OOO", Py_False, Py_None, Py_False);
    }

    return res;
}

static PyObject *maio_save_event(PyObject* self, PyObject* args)
{
    Q_UNUSED(self);
    PyObject *obj;

    if (!PyArg_ParseTuple(args, "O", &obj))
        return NULL;

    // if we got passed a string, take it directly
    if (PyUnicode_Check(obj)) {
        const char *bytes = PyUnicode_AsUTF8(obj);
        MaIO::instance()->saveEvent(QString::fromUtf8(bytes));

        Py_INCREF(Py_None);
        return Py_None;
    }

    PyObject *iter = PyObject_GetIter(obj);
    if (!iter) {
        PyErr_SetString(MaIOError, "Expected an interable type (e.g. a list) as parameter.");
        return NULL;
    }

    QStringList values;
    while (true) {
        PyObject *next = PyIter_Next(iter);
        if (!next)
            break;

        if (!PyUnicode_Check(next)) {
            PyErr_SetString(MaIOError, "Expected a list of strings as parameter.");
            return NULL;
        }

        values << QString::fromUtf8(PyUnicode_AsUTF8(next));
    }

    MaIO::instance()->saveEvent(values);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *maio_pin_set_value(PyObject* self, PyObject* args)
{
    const char *pinName;
    PyObject *state;

    Q_UNUSED(self);
    if (!PyArg_ParseTuple(args, "sO", &pinName, &state))
        return NULL;

    MaIO::instance()->pinSetValue(QString::fromUtf8(pinName), state == Py_True);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *maio_pin_signal_pulse(PyObject* self, PyObject* args)
{
    const char *pinName;

    Q_UNUSED(self);
    if (!PyArg_ParseTuple(args, "s", &pinName))
        return NULL;

    MaIO::instance()->pinSignalPulse(QString::fromUtf8(pinName));

    Py_INCREF(Py_None);
    return Py_None;
}

#endif

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
    auto zc = ZmqClient::instance();
    return zc->runRpc(MaPyFunction::G_timeSinceStartMsec).toLongLong();
}

struct EventTable
{
    EventTable(std::string name)
        : _name(name)
    {
        auto zc = ZmqClient::instance();
        QJsonArray params;
        params.append(QString::fromStdString(name));
        _inst_id = zc->runRpc(MaPyFunction::T_newEventTable, params).toInt();
        if (_inst_id < 0)
            throw MazeAmazePyError("Could not create new event table with this name.");
    }

    void set_header(boost::python::list header)
    {
        auto zc = ZmqClient::instance();
        QJsonArray params;
        params.append(_inst_id);

        QJsonArray list;
        for (long i = 0; i < len(header); ++i)
            list.append(QString::fromStdString(boost::python::extract<std::string>(header[i])));
        params.append(list);

        zc->runRpc(MaPyFunction::T_setHeader, params);
    }

    void add_event(boost::python::list values)
    {
        auto zc = ZmqClient::instance();
        QJsonArray params;
        params.append(_inst_id);

        QJsonArray list;
        for (long i = 0; i < len(values); ++i) {
            list.append(QString::fromStdString(boost::python::extract<std::string>(values[i])));
        }
        params.append(list);

        zc->runRpc(MaPyFunction::T_addEvent, params);
    }

    std::string _name;
    int _inst_id;
};

enum PinType {
    PT_INPUT  = 0,
    PT_OUTPUT = 1,
    PT_PULLUP = 2
};

struct FirmataInterface
{
    bool new_digital_pin(int id, const std::string& name, PinType ptype)
    {
        auto zc = ZmqClient::instance();
        QJsonArray params;
        params.append(_mod_id);
        params.append(id);
        params.append(QString::fromStdString(name));
        params.append(ptype);
        return zc->runRpc(MaPyFunction::F_newDigitalPin, params).toBool();
    }

    boost::python::tuple fetch_digital_input()
    {
        auto zc = ZmqClient::instance();
        auto res = zc->runRpc(MaPyFunction::F_fetchDigitalInput).toJsonArray();
        if (res[0].toBool())
            return boost::python::make_tuple(true, res[1].toString(), res[2].toInt());
        else
            return boost::python::make_tuple(false, "", 0);
    }

    int _mod_id;
};

static FirmataInterface get_firmata_interface(const std::string& name)
{
    auto zc = ZmqClient::instance();

    QJsonArray params;
    params.append(QString::fromStdString(name));
    auto id = zc->runRpc(MaPyFunction::F_getFirmataModuleId, params).toInt();
    if (id < 0)
        throw MazeAmazePyError("Unable to find the requested Firmata module.");

    FirmataInterface intf;
    intf._mod_id = id;

    return intf;
}


BOOST_PYTHON_MODULE(maio)
{
    register_exception_translator<MazeAmazePyError>(&translateException);

    def("time_since_start_msec", time_since_start_msec, "Get time since experiment started in milliseconds.");
    def("get_firmata_interface", get_firmata_interface, "Retrieve the Firmata interface with the given name.");

    class_<EventTable>("EventTable", init<std::string>())
            .def("set_header", &EventTable::set_header)
            .def("add_event", &EventTable::add_event)
    ;

    enum_<PinType>("PinType")
            .value("INPUT", PT_INPUT)
            .value("OUTPUT", PT_OUTPUT)
            .value("PULLUP", PT_OUTPUT);

    class_<FirmataInterface>("FirmataInterface")
        .def("new_digital_pin", &FirmataInterface::new_digital_pin, "Register a new digital pin.")
        .def("fetch_digital_input", &FirmataInterface::fetch_digital_input, "Retreive digital input from the queue.")
    ;
};


#if 0
static struct PyMethodDef methods[] = {
    { "time_since_start_msec", maio_time_since_start_msec,     METH_VARARGS, "Get time since experiment started in milliseconds." },

    { "new_digital_pin",           maio_new_digital_pin,     METH_VARARGS, ""},
    { "maio_new_analog_pin",       maio_new_analog_pin,     METH_VARARGS, "Register a new analog pin."},
    { "set_events_header",         maio_set_events_header,   METH_VARARGS, "Set header of the recorded events table."},
    { "fetch_digital_input",       maio_fetch_digital_input, METH_VARARGS, "Retreive digital input from the queue."},
    { "save_event",                maio_save_event,          METH_VARARGS, "Save an event to the log."},
    { "pin_set_value",             maio_pin_set_value,       METH_VARARGS, "Set a digital output pin to a boolean value."},
    { "pin_signal_pulse",          maio_pin_signal_pulse,    METH_VARARGS, "Emit a digital siganl on the specific pin."},

    { NULL, NULL, 0, NULL }
};

static struct PyModuleDef modDef = {
    PyModuleDef_HEAD_INIT, "maio", NULL, -1, methods,
    NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC
PyInit_maio(void)
{
    PyObject *m = PyModule_Create(&modDef);
    if (m == NULL)
        return NULL;

    MaIOError = PyErr_NewException("maio.error", NULL, NULL);
    Py_INCREF(MaIOError);
    PyModule_AddObject(m, "error", MaIOError);

    return m;
}
#endif

void pythonRegisterMaioModule()
{
    PyImport_AppendInittab("maio", &PyInit_maio);
}

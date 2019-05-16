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
#include <QEventLoop>
#include <QTimer>
#include <QTime>
#include <QCoreApplication>

#include "zmqclient.h"

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

static PyObject *maio_new_analog_pin(PyObject* self, PyObject* args)
{
    const char *pinName;
    const char *kindStr;
    int pinId;

    Q_UNUSED(self);
    if (!PyArg_ParseTuple(args, "iss", &pinId, &pinName, &kindStr))
        return NULL;

    auto maio = MaIO::instance();
    maio->newAnalogPin(pinId, QString::fromUtf8(pinName), QString::fromUtf8(kindStr));

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

static PyObject *maio_time_since_start_msec(PyObject* self, PyObject* args)
{
    Q_UNUSED(self);
    Q_UNUSED(args);

    auto zc = ZmqClient::instance();

    auto time = zc->runRpc(MaPyFunction::G_timeSinceStartMsec).toLongLong();
    return PyLong_FromLongLong(time);
}

static struct PyMethodDef methods[] = {
    { "time_since_start_msec", maio_time_since_start_msec,     METH_VARARGS, "Get time since experiment started in milliseconds." },

#if 0
    { "new_digital_pin",           maio_new_digital_pin,     METH_VARARGS, "Register a new digital pin."},
    { "maio_new_analog_pin",       maio_new_analog_pin,     METH_VARARGS, "Register a new analog pin."},
    { "set_events_header",         maio_set_events_header,   METH_VARARGS, "Set header of the recorded events table."},
    { "fetch_digital_input",       maio_fetch_digital_input, METH_VARARGS, "Retreive digital input from the queue."},
    { "save_event",                maio_save_event,          METH_VARARGS, "Save an event to the log."},
    { "pin_set_value",             maio_pin_set_value,       METH_VARARGS, "Set a digital output pin to a boolean value."},
    { "pin_signal_pulse",          maio_pin_signal_pulse,    METH_VARARGS, "Emit a digital siganl on the specific pin."},
#endif

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

void pythonRegisterMaioModule()
{
    PyImport_AppendInittab("maio", &PyInit_maio);
}

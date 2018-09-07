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

#include <QMessageBox>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QTime>
#include <QCoreApplication>

#include "firmata/serialport.h"

std::mutex MaIO::_mutex;
MaIO::MaIO(QObject *parent)
    : QObject(parent),
      m_firmata(nullptr)
{
}

void MaIO::setFirmata(SerialFirmata *firmata)
{
    m_firmata = firmata;
    connect(m_firmata, &SerialFirmata::digitalRead, this, &MaIO::onDigitalRead, Qt::DirectConnection);
    connect(m_firmata, &SerialFirmata::digitalPinRead, this, &MaIO::onDigitalPinRead, Qt::DirectConnection);

    reset();
}

SerialFirmata *MaIO::firmata() const
{
    return m_firmata;
}

void MaIO::reset()
{
    m_changedValuesQueue.clear();
    m_namePinMap.clear();
    m_pinNameMap.clear();
}

void MaIO::newDigitalPin(int pinID, const QString& pinName, bool output, bool pullUp)
{
    FmPin pin;
    pin.kind = PinKind::Digital;
    pin.id = pinID;
    pin.output = output;

    if (pin.output) {
        // initialize output pin
        m_firmata->setPinMode(pinID, IoMode::Output);
        m_firmata->writeDigitalPin(pinID, false);
        qDebug() << "Pin" << pinID << "set as output";
    } else {
        // connect input pin
        if (pullUp)
            m_firmata->setPinMode(pinID, IoMode::PullUp);
        else
            m_firmata->setPinMode(pinID, IoMode::Input);

        auto port = pin.id >> 3;
        m_firmata->reportDigitalPort(port, true);

        qDebug() << "Pin" << pinID << "set as input";
    }

    m_namePinMap.insert(pinName, pin);
    m_pinNameMap.insert(pin.id, pinName);
}

void MaIO::newDigitalPin(int pinID, const QString& pinName, const QString& kind)
{
    if (kind == "output")
        newDigitalPin(pinID, pinName, true, false);
    else if (kind == "input")
        newDigitalPin(pinID, pinName, false, false);
    else if (kind == "input-pullup")
        newDigitalPin(pinID, pinName, false, true);
    else
        qCritical() << "Invalid pin kind:" << kind;
}

bool MaIO::fetchDigitalInput(QPair<QString, bool> *result)
{
    if (m_changedValuesQueue.empty())
        return false;
    if (result == nullptr)
        return false;

    m_mutex.lock();
    *result = m_changedValuesQueue.dequeue();
    m_mutex.unlock();

    return true;
}

void MaIO::pinSetValue(const QString& pinName, bool value)
{
    auto pin = m_namePinMap.value(pinName);
    if (pin.kind == PinKind::Unknown) {
        QMessageBox::critical(nullptr, "MaIO Error",
                              QString("Unable to deliver message to pin '%1' (pin does not exist)").arg(pinName));
        return;
    }
    m_firmata->writeDigitalPin(pin.id, value);
}

void MaIO::pinSignalPulse(const QString& pinName)
{
    pinSetValue(pinName, true);
    this->sleep(50);
    pinSetValue(pinName, false);
}

void MaIO::onDigitalRead(uint8_t port, uint8_t value)
{
    qDebug() << "Firmata digital port read:" << int(value);

    // value of a digital port changed: 8 possible pin changes
    const int first = port * 8;
    const int last = first + 7;

    for (const FmPin p : m_namePinMap.values()) {
        if ((!p.output) && (p.kind != PinKind::Unknown)) {
            if ((p.id >= first) && (p.id <= last)) {
                QPair<QString, bool> pair;
                pair.first = m_pinNameMap.value(p.id);
                pair.second = value & (1 << (p.id - first));

                m_mutex.lock();
                m_changedValuesQueue.append(pair);
                m_mutex.unlock();
            }
        }
    }
}

void MaIO::onDigitalPinRead(uint8_t pin, bool value)
{
    qDebug("Firmata digital pin read: %d=%d", pin, value);

    auto pinName = m_pinNameMap.value(pin);
    if (pinName.isEmpty()) {
        qWarning() << "Received state change for unknown pin:" << pin;
        return;
    }

    m_changedValuesQueue.append(qMakePair(pinName, value));
}

void MaIO::saveEvent(const QString &message)
{
    QStringList msgs;
    msgs << message;
    emit eventSaved(msgs);
}

void MaIO::saveEvent(const QStringList& messages)
{
    emit eventSaved(messages);
}

void MaIO::setEventsHeader(const QStringList &headers)
{
    emit headersSet(headers);
}

void MaIO::sleep(uint msecs)
{
    QEventLoop loop;
    QTimer timer;
    timer.setInterval(msecs);
    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    timer.start();
    loop.exec();
}

void MaIO::wait(uint msecs)
{
    auto waitTime = QTime::currentTime().addMSecs(msecs);
    auto etime = waitTime.msec();
    etime = etime / 4;
    while (QTime::currentTime() < waitTime)
        QCoreApplication::processEvents(QEventLoop::AllEvents, etime);
}


/**
 *
 * Python interface section
 *
**/

// error variable for Python interface
static PyObject *MaIOError;

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

static struct PyMethodDef methods[] = {
    { "new_digital_pin",     maio_new_digital_pin,     METH_VARARGS, "Register a new digital pin."},
    { "set_events_header",   maio_set_events_header,   METH_VARARGS, "Set header of the recorded events table."},
    { "fetch_digital_input", maio_fetch_digital_input, METH_VARARGS, "Retreive digital input from the queue."},
    { "save_event",          maio_save_event,          METH_VARARGS, "Save an event to the log."},
    { "pin_set_value",       maio_pin_set_value,       METH_VARARGS, "Set a digital output pin to a boolean value."},
    { "pin_signal_pulse",    maio_pin_signal_pulse,    METH_VARARGS, "Emit a digital siganl on the specific pin."},


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

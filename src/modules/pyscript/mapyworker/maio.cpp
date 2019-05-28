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

#include "zmqclient.h"

using namespace boost::python;

#pragma GCC diagnostic ignored "-Wweak-vtables"
struct MazeAmazePyError : std::runtime_error {
    explicit MazeAmazePyError(const char* what_arg);
    explicit MazeAmazePyError(const std::string& what_arg);
};
MazeAmazePyError::MazeAmazePyError(const char* what_arg)
    : std::runtime_error(what_arg) {};
MazeAmazePyError::MazeAmazePyError(const std::string& what_arg)
    : std::runtime_error(what_arg) {};
#pragma GCC diagnostic pop

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
        QJsonArray params;
        params.append(_mod_id);
        auto res = zc->runRpc(MaPyFunction::F_fetchDigitalInput, params).toJsonArray();
        if (res[0].toBool())
            return boost::python::make_tuple(true, res[1].toString().toStdString(), res[2].toBool());
        else
            return boost::python::make_tuple(false, "", 0);
    }

    void pin_set_value(const std::string& name, bool value)
    {
        auto zc = ZmqClient::instance();
        QJsonArray params;
        params.append(_mod_id);
        params.append(QString::fromStdString(name));
        params.append(value);
        zc->runRpc(MaPyFunction::F_pinSetValue, params);
    }

    void pin_signal_pulse(const std::string& name)
    {
        auto zc = ZmqClient::instance();
        QJsonArray params;
        params.append(_mod_id);
        params.append(QString::fromStdString(name));
        zc->runRpc(MaPyFunction::F_pinSignalPulse, params);
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
        .def("pin_set_value", &FirmataInterface::pin_set_value, "Set a digital output pin to a boolean value.")
        .def("pin_signal_pulse", &FirmataInterface::pin_set_value, "Emit a digital siganl on the specific pin.")
    ;
};

void pythonRegisterMaioModule()
{
    PyImport_AppendInittab("maio", &PyInit_maio);
}

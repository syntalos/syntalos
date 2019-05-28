/*
 * Copyright (C) 2019 Matthias Klumpp <matthias@tenstral.net>
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
#include "pycontroller.h"

#include <QCoreApplication>
#include <QThread>
#include <QDebug>
#include <iostream>

#include "zmqclient.h"
#include "maio.h"

PyController::PyController(QObject *parent)
    : QObject(parent)
{
    m_conn = ZmqClient::instance(this);
    pythonRegisterMaioModule();
}

void PyController::run()
{
    const auto args = QCoreApplication::arguments();
    if (args.length() <= 1) {
        qCritical() << "No socket passed as parameter.";
        emit finished(4);
        return;
    }
    const auto socketName = args[1];

    m_conn->connect(socketName);

    const auto pyScript = m_conn->runRpc(MaPyFunction::G_getPythonScript).toString();

    while (!m_conn->runRpc(MaPyFunction::G_canStart).toBool()) {
        QThread::usleep(1 * 1000);
    }

    runScript(pyScript);
}

void PyController::exitError(const QString& msg)
{
    std::cerr << msg.toStdString() << std::endl;
    QCoreApplication::exit(6);
}

#pragma GCC diagnostic ignored "-Wold-style-cast"
void PyController::runScript(const QString &scriptContent)
{
    //! Py_SetProgramName("mazeamaze-script");

    // initialize Python in the thread
    Py_Initialize();

    PyObject *mainModule = PyImport_AddModule("__main__");
    if (mainModule == nullptr) {
        exitError("Can not execute Python code: No __main__module.");

        Py_Finalize();
        return;
    }
    PyObject *mainDict = PyModule_GetDict(mainModule);

    // run script
    auto res = PyRun_String(qPrintable(scriptContent), Py_file_input, mainDict, mainDict);

    if (res == nullptr) {
        if (PyErr_Occurred()) {
            PyObject *excType, *excValue, *excTraceback;
            PyErr_Fetch(&excType, &excValue, &excTraceback);
            PyErr_NormalizeException(&excType, &excValue, &excTraceback);

            QString message;
            if (excType) {
                PyObject* str = PyObject_Str(excType);
                if (str != nullptr) {
                    const char *bytes = PyUnicode_AsUTF8(str);
                    message = QString::fromUtf8(bytes);
                    Py_XDECREF(str);
                }
            }
            if (excValue) {
                PyObject* str = PyObject_Str(excValue);
                if (str != nullptr) {
                    const char *bytes = PyUnicode_AsUTF8(str);
                    message = QString::fromUtf8("%1\n%2").arg(message).arg(QString::fromUtf8(bytes));
                    Py_XDECREF(str);
                }
            }
            if (excTraceback) {
                PyObject* str = PyObject_Str(excTraceback);
                if (str != nullptr) {
                    const char *bytes = PyUnicode_AsUTF8(str);
                    message = QString::fromUtf8("%1\n%2").arg(message).arg(QString::fromUtf8(bytes));
                    Py_XDECREF(str);
                }
            }

            if (message.isEmpty())
                message = QStringLiteral("An unknown Python error occured.");

            exitError(QStringLiteral("Python error: %1").arg(message));

            Py_XDECREF(excTraceback);
            Py_XDECREF(excType);
            Py_XDECREF(excValue);
        }
    } else {
        Py_XDECREF(res);
    }

    Py_Finalize();
}
#pragma GCC diagnostic pop

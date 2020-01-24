/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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
#include "worker.h"

#include "maio.h"

#include <iostream>

#include "../oop/cvmatshm.h"
#include <opencv2/highgui/highgui.hpp>

OOPWorker::OOPWorker(QObject *parent)
    : OOPWorkerSource(parent)
{
    m_pyb = PyBridge::instance(this);
    pythonRegisterMaioModule();
    qDebug() << "PyWorker created!";
}

OOPWorker::~OOPWorker()
{
}

bool OOPWorker::ready() const
{
    return true;
}

std::optional<InputPortInfo> OOPWorker::inputPortInfoByIdString(const QString &idstr)
{
    std::optional<InputPortInfo> res;
    for (const auto &port : m_inPortInfo) {
        if (port.idstr() == idstr) {
            res = port;
            break;
        }
    }
    return res;
}

bool OOPWorker::initializeFromData(const QString &script, const QString &env)
{
    if (!env.isEmpty())
        QDir::setCurrent(env);

    m_script = script;
    qDebug() << "Initialized from Python script";
    QTimer::singleShot(0, this, &OOPWorker::runScript);
    return true;
}

bool OOPWorker::initializeFromFile(const QString &fname, const QString &env)
{
    Q_UNUSED(env)
    Q_UNUSED(fname)

    QTimer::singleShot(0, this, &OOPWorker::runScript);
    return true;
}

void OOPWorker::setInputPortInfo(QList<InputPortInfo> ports)
{
    m_inPortInfo = ports;
    m_shmRecv.clear();

    // set up our incoming shared memory links
    for (int i = 0; i < m_inPortInfo.size(); i++)
        m_shmRecv.push_back(std::unique_ptr<SharedMemory>(new SharedMemory));

    for (int i = 0; i < m_inPortInfo.size(); i++) {
        if (i >= m_inPortInfo.size()) {
            raiseError("Invalid data sent for input port information!");
            return;
        }
        auto port = m_inPortInfo[i];

        m_shmRecv[port.id()]->setShmKey(port.shmKeyRecv());
    }
}

void OOPWorker::setOutputPortInfo(QList<OutputPortInfo> ports)
{
    m_outPortInfo = ports;

    // set up our outgoing shared memory links
    for (int i = 0; i < m_outPortInfo.size(); i++)
        m_shmSend.push_back(std::unique_ptr<SharedMemory>(new SharedMemory));

    for (int i = 0; i < m_outPortInfo.size(); i++) {
        if (i >= m_outPortInfo.size()) {
            raiseError("Invalid data sent for output port information!");
            return;
        }
        auto port = m_outPortInfo[i];

        m_shmSend[port.id()]->setShmKey(port.shmKeySend());
    }
}

void OOPWorker::start(long startTimestampMsec)
{
    auto timePoint = steady_hr_timepoint(std::chrono::duration<long, std::milli>(startTimestampMsec));
    m_pyb->timer()->startAt(timePoint);

    m_running = true;
}

void OOPWorker::shutdown()
{
    qDebug() << "PyWorker Shutdown";
    m_running = false;
    QCoreApplication::processEvents();

    // give other events a bit of time (10ms) to react to the fact that we are no longer running
    QTimer::singleShot(10, this, &QCoreApplication::quit);
}

void OOPWorker::emitPyError()
{

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

        raiseError(QStringLiteral("Python script failed:\n%1").arg(message));

        Py_XDECREF(excTraceback);
        Py_XDECREF(excType);
        Py_XDECREF(excValue);
}

#pragma GCC diagnostic ignored "-Wold-style-cast"
void OOPWorker::runScript()
{
    //! Py_SetProgramName("mazeamaze-script");

    // initialize Python in the thread
    Py_Initialize();

    PyObject *mainModule = PyImport_AddModule("__main__");
    if (mainModule == nullptr) {
        raiseError("Can not execute Python code: No __main__ module.");

        Py_Finalize();
        return;
    }
    PyObject *mainDict = PyModule_GetDict(mainModule);

    // run script
    auto res = PyRun_String(qPrintable(m_script), Py_file_input, mainDict, mainDict);

    if (res != nullptr) {
        auto pyMain = PyImport_ImportModule("__main__");
        auto pFunc = PyObject_GetAttrString(pyMain, "loop");

        if (pFunc && PyCallable_Check(pFunc)) {
            bool callEventLoop = true;

            // while we are not running, ait for the start signal
            while (!m_running) { QCoreApplication::processEvents(); }

            do {
                QCoreApplication::processEvents();

                auto loopRes = PyObject_CallObject(pFunc, nullptr);
                if (loopRes == nullptr) {
                    if (PyErr_Occurred())
                        emitPyError();
                    callEventLoop = false;
                } else {
                    if (PyBool_Check (loopRes))
                        callEventLoop = loopRes == Py_True;
                    else
                        callEventLoop = false;
                    Py_XDECREF(loopRes);
                }
            } while (callEventLoop && m_running);
        } else {
            raiseError("Could not find loop() function entrypoint in Python script.");
        }
    }

    if (res == nullptr) {
        if (PyErr_Occurred())
            emitPyError();
    } else {
        Py_XDECREF(res);
    }

    Py_Finalize();
    m_running = false;
}
#pragma GCC diagnostic pop

std::optional<bool> OOPWorker::waitForInput()
{
    std::optional<bool> res;

    // TODO: Wait for events properly here.
    QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);

    return res;
}

bool OOPWorker::receiveInput(int inPortId, QVariantHash data)
{
    qDebug() << "Received input on" << inPortId << "data:" << data;

    auto mat = shm_to_cvmat(m_shmRecv[inPortId]);

    return true;
}

void OOPWorker::raiseError(const QString &message)
{
    m_running = false;
    std::cerr << message.toStdString() << std::endl;
    emit error(message);
}

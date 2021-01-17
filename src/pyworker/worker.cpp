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

#define QT_NO_KEYWORDS
#include <iostream>
#include <QMetaType>
#include "worker.h"

#include "rtkit.h"
#include "cpuaffinity.h"
#include "syio.h"
#include "pyipcmarshal.h"
#include "streams/datatypes.h"

OOPWorker::OOPWorker(QObject *parent)
    : OOPWorkerSource(parent),
      m_stage(OOPWorker::IDLE),
      m_running(false),
      m_maxRTPriority(0)
{
    m_pyb = PyBridge::instance(this);
    pythonRegisterSyioModule();

    registerStreamMetaTypes();
}

OOPWorker::~OOPWorker()
{
}

OOPWorker::Stage OOPWorker::stage() const
{
    return m_stage;
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

std::optional<OutputPortInfo> OOPWorker::outputPortInfoByIdString(const QString &idstr)
{
    std::optional<OutputPortInfo> res;
    for (const auto &port : m_outPortInfo) {
        if (port.idstr() == idstr) {
            res = port;
            break;
        }
    }
    return res;
}

bool OOPWorker::initializeFromData(const QString &script, const QString &wdir)
{
    if (!wdir.isEmpty())
        QDir::setCurrent(wdir);

    m_script = script;
    qDebug() << "Initialized from Python script";
    QTimer::singleShot(0, this, &OOPWorker::runScript);
    return true;
}

bool OOPWorker::initializeFromFile(const QString &fname, const QString &wdir)
{
    Q_UNUSED(wdir)
    Q_UNUSED(fname)

    QTimer::singleShot(0, this, &OOPWorker::runScript);
    return true;
}

void OOPWorker::setInputPortInfo(const QList<InputPortInfo> &ports)
{
    m_inPortInfo = ports;
    m_shmRecv.clear();
    m_pyb->incomingData.clear();

    // set up our incoming shared memory links
    for (int i = 0; i < m_inPortInfo.size(); i++)
        m_shmRecv.push_back(std::unique_ptr<SharedMemory>(new SharedMemory));

    for (int i = 0; i < m_inPortInfo.size(); i++) {
        if (i >= m_inPortInfo.size()) {
            raiseError("Invalid data sent for input port information!");
            return;
        }
        auto port = m_inPortInfo[i];
        port.setWorkerDataTypeId(QMetaType::type(qPrintable(port.dataTypeName())));
        m_inPortInfo[i] = port;

        m_shmRecv[port.id()]->setShmKey(port.shmKeyRecv());
        m_pyb->incomingData.append(QQueue<py::object>());
    }
}

void OOPWorker::setOutputPortInfo(const QList<OutputPortInfo> &ports)
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
        port.setWorkerDataTypeId(QMetaType::type(qPrintable(port.dataTypeName())));
        m_outPortInfo[i] = port;

        m_shmSend[port.id()]->setShmKey(port.shmKeySend());
    }
}

void OOPWorker::start(long startTimestampUsec)
{
    const auto timePoint = symaster_timepoint(microseconds_t(startTimestampUsec));
    m_pyb->timer()->startAt(timePoint);

    m_running = true;
}

void OOPWorker::shutdown()
{
    m_running = false;
    QCoreApplication::processEvents();

    // give other events a bit of time (10ms) to react to the fact that we are no longer running
    QTimer::singleShot(10, this, &QCoreApplication::quit);
    qDebug() << "Shutting down Python worker.";
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
                message = QString::fromUtf8(PyUnicode_AsUTF8(str));
                Py_XDECREF(str);
            }
        }

        if (excValue) {
            PyObject* str = PyObject_Str(excValue);
            if (str != nullptr) {
                message = QString::fromUtf8("%1\n%2").arg(message)
                                                     .arg(QString::fromUtf8(PyUnicode_AsUTF8(str)));
                Py_XDECREF(str);
            }
        }

        if (excTraceback) {
            // let's try to generate a useful traceback
                auto pyTbModName = PyUnicode_FromString("traceback");
                auto pyTbMod = PyImport_Import(pyTbModName);
                Py_DECREF(pyTbModName);

                if (pyTbModName == nullptr) {
                    // we can't create a good backtrace, just print the thing as string as a fallback
                    auto str = PyObject_Str(excTraceback);
                    if (str != nullptr) {
                        message = QString::fromUtf8("%1\n%2").arg(message)
                                                             .arg(QString::fromUtf8(PyUnicode_AsUTF8(str)));
                        Py_XDECREF(str);
                    }
                } else {
                    const auto pyFnFormatE = PyObject_GetAttrString(pyTbMod, "format_exception");
                    if (pyFnFormatE && PyCallable_Check(pyFnFormatE)) {
                        auto pyTbVal = PyObject_CallFunctionObjArgs(pyFnFormatE,
                                                                    excType,
                                                                    excValue,
                                                                    excTraceback,
                                                                    nullptr);
                        auto str = PyObject_Str(pyTbVal);
                        if (str != nullptr) {
                            message = QString::fromUtf8("%1\n%2").arg(message)
                                                                 .arg(QString::fromUtf8(PyUnicode_AsUTF8(str)));
                            Py_XDECREF(str);
                        }
                    } else {
                        message = QString::fromUtf8("%1\n<<Unable to format traceback.>>").arg(message);
                    }
                }
        }

        if (message.isEmpty())
            message = QStringLiteral("An unknown Python error occured.");

        raiseError(QStringLiteral("Python:\n%1").arg(message));

        Py_XDECREF(excTraceback);
        Py_XDECREF(excType);
        Py_XDECREF(excValue);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
void OOPWorker::runScript()
{
    Py_SetProgramName(QCoreApplication::arguments()[0].toStdWString().c_str());

    // HACK: make Python thing *we* are the Python interpreter, so it finds
    // all modules correctly when we are in a virtual environment.
    const auto venvDir = QString::fromUtf8(qgetenv("VIRTUAL_ENV"));
    if (!venvDir.isEmpty())
        Py_SetProgramName(QDir(venvDir).filePath("bin/python").toStdWString().c_str());

    // initialize Python in this process
    Py_Initialize();

    PyObject *mainModule = PyImport_AddModule("__main__");
    if (mainModule == nullptr) {
        raiseError("Can not execute Python code: No __main__ module.");

        Py_Finalize();
        return;
    }
    PyObject *mainDict = PyModule_GetDict(mainModule);

    // run script
    setStage(OOPWorker::PREPARING);
    auto res = PyRun_String(qPrintable(m_script), Py_file_input, mainDict, mainDict);

    // check if we already failed
    if (m_stage == OOPWorker::ERROR)
        goto finalize;

    if (res != nullptr) {
        // everything is good, we can run some Python functions
        // explicitly now
        auto pyMain = PyImport_ImportModule("__main__");

        // run prepare function if it exists for initial setup
        if (PyObject_HasAttrString(pyMain, "prepare")) {
            auto pFnPrep = PyObject_GetAttrString(pyMain, "prepare");
            if (pFnPrep && PyCallable_Check(pFnPrep)) {
                const auto pyRes = PyObject_CallObject(pFnPrep, nullptr);
                if (pyRes == nullptr) {
                    if (PyErr_Occurred()) {
                        emitPyError();
                        goto finalize;
                    }
                } else {
                    Py_XDECREF(pyRes);
                }
            }
            Py_XDECREF(pFnPrep);
        }

        // check if have failed, and quit in that case
        if (m_stage == OOPWorker::ERROR)
            goto finalize;

        // the script may have changed output port metadata, so we send all of that back to
        // the master process
        for (int portId = 0; portId < m_outPortInfo.size(); ++portId)
            Q_EMIT outPortMetadataUpdated(portId, m_outPortInfo[portId].metadata());

        // signal that we are ready now, preparations are done
        setStage(OOPWorker::READY);

        // find the start function if it exists
        PyObject *pFnStart = nullptr;
        if (PyObject_HasAttrString(pyMain, "start")) {
            pFnStart = PyObject_GetAttrString(pyMain, "start");
            if (!pFnStart || !PyCallable_Check(pFnStart)) {
                Py_XDECREF(pFnStart);
                pFnStart = nullptr;
            }
        }

        // find the loop function - this function *must* exists,
        // and unlike the other functions isn't optional, so GetAttrString
        // is allowed to throw an error here
        auto pFnLoop = PyObject_GetAttrString(pyMain, "loop");
        if (!pFnLoop || !PyCallable_Check(pFnLoop)) {
            raiseError("Could not find loop() function entrypoint in Python script.");
            Py_XDECREF(pFnLoop);
            goto finalize;
        }

        // while we are not running, wait for the start signal
        while (!m_running) { QCoreApplication::processEvents(); }
        setStage(OOPWorker::RUNNING);

        // run the start function first, if we have it
        if (pFnStart != nullptr) {
            const auto pyRes = PyObject_CallObject(pFnStart, nullptr);
            if (pyRes == nullptr) {
                if (PyErr_Occurred()) {
                    emitPyError();
                    goto finalize;
                }
            } else {
                Py_XDECREF(pyRes);
            }
            Py_XDECREF(pFnStart);
        }

        // maybe start() failed? Immediately exit in that case
        if (m_stage == OOPWorker::ERROR) {
            Py_XDECREF(pFnLoop);
            goto finalize;
        }

        if (pFnLoop != nullptr) {
            bool callEventLoop = true;

            // we are running! - loop() until we are stopped
            do {
                QCoreApplication::processEvents();

                auto loopRes = PyObject_CallObject(pFnLoop, nullptr);
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

            Py_XDECREF(pFnLoop);
        }

        // we have stopped, so call the stop function if one exists
        if (PyObject_HasAttrString(pyMain, "stop")) {
            auto pFnStop = PyObject_GetAttrString(pyMain, "stop");
            if (pFnStop && PyCallable_Check(pFnStop)) {
                const auto pyRes = PyObject_CallObject(pFnStop, nullptr);
                if (pyRes == nullptr) {
                    if (PyErr_Occurred()) {
                        emitPyError();
                        goto finalize;
                    }
                } else {
                    Py_XDECREF(pyRes);
                }
            }
            Py_XDECREF(pFnStop);
        }
    }

finalize:
    if (res == nullptr) {
        if (PyErr_Occurred())
            emitPyError();
    } else {
        Py_XDECREF(res);
    }

    Py_Finalize();

    // we aren't ready anymore,
    // and also stopped running the loop
    setStage(OOPWorker::IDLE);
    m_running = false;
}
#pragma GCC diagnostic pop

std::optional<bool> OOPWorker::waitForInput()
{
    std::optional<bool> res = false;

    while (true) {
        for (const auto &q : m_pyb->incomingData) {
            if (!q.isEmpty()) {
                res = true;
                break;
            }
        }
        if (res.value())
            break;

        if (!m_running) {
            res.reset();
            break;
        }

        QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
    }

    return res;
}

bool OOPWorker::receiveInput(int inPortId, const QVariant &argData)
{
    const auto typeId = m_inPortInfo[inPortId].workerDataTypeId();
    auto pyObj = unmarshalDataToPyObject(typeId, argData, m_shmRecv[inPortId]);
    m_pyb->incomingData[inPortId].append(pyObj);

    return true;
}

bool OOPWorker::submitOutput(int outPortId, py::object pyObj)
{
    // don't send anything if nothing is connected to this port
    if (!m_outPortInfo[outPortId].connected())
        return true;

    const auto typeId = m_outPortInfo[outPortId].workerDataTypeId();
    QVariant argData;
    const auto ret = marshalPyDataElement(typeId, pyObj, argData, m_shmSend[outPortId]);
    if (ret)
        Q_EMIT sendOutput(outPortId, argData);
    return ret;
}

void OOPWorker::setOutPortMetadataValue(int outPortId, const QString &key, const QVariant &value)
{
    auto portInfo = m_outPortInfo[outPortId];
    auto mdata = portInfo.metadata();
    mdata.insert(key, value);
    portInfo.setMetadata(mdata);
    m_outPortInfo[outPortId] = std::move(portInfo);
}

void OOPWorker::setInputThrottleItemsPerSec(int inPortId, uint itemsPerSec, bool allowMore)
{
    Q_EMIT inputThrottleItemsPerSecRequested(inPortId, itemsPerSec, allowMore);
}

void OOPWorker::setStage(OOPWorker::Stage stage)
{
    m_stage = stage;
    Q_EMIT stageChanged(m_stage);
}

void OOPWorker::raiseError(const QString &message)
{
    m_running = false;
    std::cerr << "ERROR: " << message.toStdString() << std::endl;
    setStage(OOPWorker::ERROR);
    Q_EMIT error(message);
}

void OOPWorker::makeDocFileAndQuit(const QString &fname)
{
    // FIXME: We ignore Python warnings for now, as we otherwise get lots of
    // "Couldn't read PEP-224 variable docstrings from <Class X>: <class  X> is a built-in class"
    // messages that - currently - we can't do anything about
    qputenv("PYTHONWARNINGS", "ignore");

    Py_Initialize();
    PyRun_SimpleString(qPrintable(QStringLiteral(
                       "import pdoc\n"
                       "\n"
                       "with open('%1', 'w') as f:\n"
                       "    f.write(pdoc.text('syio'))\n"
                       "    f.write('\\n')\n"
                       "\n").arg(QString(fname).replace("'", "\\'"))));
    if (Py_FinalizeEx() < 0)
        exit(9);

    // documentation generated successfully, we can quit now
    exit(0);
}

bool OOPWorker::setNiceness(int nice)
{
    return setCurrentThreadNiceness(nice);
}

void OOPWorker::setMaxRealtimePriority(int priority)
{
    // we just store this value here in case the script wants to go into
    // realtime mode later for some reason
    m_maxRTPriority = priority;
}

void OOPWorker::setCPUAffinity(QVector<uint> cores)
{
    if (cores.empty())
        return;
    thread_set_affinity_from_vec(pthread_self(), std::vector<uint>(cores.begin(), cores.end()));
}

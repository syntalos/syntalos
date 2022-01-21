/*
 * Copyright (C) 2019-2022 Matthias Klumpp <matthias@tenstral.net>
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

#define QT_NO_KEYWORDS
#include <iostream>
#include <QMetaType>
#include "worker.h"

#include "rtkit.h"
#include "cpuaffinity.h"
#include "syio.h"
#include "pyipcmarshal.h"
#include "streams/datatypes.h"

using namespace Syntalos;
namespace Syntalos {
    Q_LOGGING_CATEGORY(logPyWorker, "pyworker")
}

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
    if (m_pyInitialized)
        Py_Finalize();
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

QByteArray OOPWorker::changeSettings(const QByteArray &oldSettings)
{
    if (!m_pyInitialized)
        return oldSettings;

    // check if we even have a function to change settings
    if (!PyObject_HasAttrString(m_pyMain, "change_settings"))
        return oldSettings;

    m_running = true;

    auto pFnSettings = PyObject_GetAttrString(m_pyMain, "change_settings");
    if (!pFnSettings || !PyCallable_Check(pFnSettings)) {
        // change_settings was not a callable, we ignore this
        Py_XDECREF(pFnSettings);
        return oldSettings;
    }

    auto pyOldSettings = PyBytes_FromStringAndSize(oldSettings.data(), oldSettings.size());
    QByteArray settings = oldSettings;
    const auto pyRes = PyObject_CallFunctionObjArgs(pFnSettings, pyOldSettings, nullptr);
    if (pyRes == nullptr) {
        if (PyErr_Occurred()) {
            emitPyError();
        } else {
            raiseError(QStringLiteral("Did not receive settings output from Python script!"));
        }
    } else {
        if (pyRes != Py_None && !PyBytes_Check(pyRes)) {
            raiseError(QStringLiteral("Did not receive settings output from Python script!"));
        } else {
            char *bytes;
            ssize_t bytes_len;
            PyBytes_AsStringAndSize(pyRes, &bytes, &bytes_len);
            settings = QByteArray::fromRawData(bytes, bytes_len);
        }

        Py_XDECREF(pyRes);
    }

    Py_XDECREF(pFnSettings);
    Py_XDECREF(pyOldSettings);
    return settings;
}

void OOPWorker::start(long startTimestampUsec)
{
    const auto timePoint = symaster_timepoint(microseconds_t(startTimestampUsec));
    m_pyb->timer()->startAt(timePoint);

    m_running = true;
}

bool OOPWorker::prepareShutdown()
{
    m_running = false;
    QCoreApplication::processEvents();
    return true;
}

void OOPWorker::shutdown()
{
    m_running = false;
    QCoreApplication::processEvents();

    // give other events a bit of time (10ms) to react to the fact that we are no longer running
    QTimer::singleShot(10, this, &QCoreApplication::quit);
    qCDebug(logPyWorker).noquote() << "Shutting down.";
}

bool OOPWorker::loadPythonScript(const QString &script, const QString &wdir)
{
    if (!wdir.isEmpty())
        QDir::setCurrent(wdir);

    Py_SetProgramName(QCoreApplication::arguments()[0].toStdWString().c_str());

    // HACK: make Python think *we* are the Python interpreter, so it finds
    // all modules correctly when we are in a virtual environment.
    const auto venvDir = QString::fromUtf8(qgetenv("VIRTUAL_ENV"));
    qCDebug(logPyWorker).noquote() << "Using virtual environment:" << venvDir;
    if (!venvDir.isEmpty())
        Py_SetProgramName(QDir(venvDir).filePath("bin/python").toStdWString().c_str());

    // initialize Python in this process
    Py_Initialize();
    m_pyInitialized = true;

    PyObject *mainModule = PyImport_AddModule("__main__");
    if (mainModule == nullptr) {
        raiseError("Can not execute Python code: No __main__ module.");

        Py_Finalize();
        return false;
    }
    PyObject *mainDict = PyModule_GetDict(mainModule);

    // load script
    auto res = PyRun_String(qPrintable(script), Py_file_input, mainDict, mainDict);
    if (res != nullptr) {
        // everything is good, we can run some Python functions
        // explicitly now
        m_pyMain = PyImport_ImportModule("__main__");
        Py_XDECREF(res);
        qCDebug(logPyWorker).noquote() << "Script loaded.";
        return true;
    } else {
        if (PyErr_Occurred())
            emitPyError();
        qCDebug(logPyWorker).noquote() << "Failed to load Python script data.";
        return false;
    }
}

bool OOPWorker::prepareStart(const QByteArray &settings)
{
    m_settings = settings;
    QTimer::singleShot(0, this, &OOPWorker::prepareAndRun);
    return m_pyInitialized;
}

static QString pyObjectToQStr(PyObject *pyObj)
{
    if (!PyList_Check(pyObj)) {
        // not a list
        auto pyStr = PyObject_Str(pyObj);
        if (pyStr == nullptr)
            return QString();
        const auto qStr = QString::fromUtf8(PyUnicode_AsUTF8(pyStr));
        Py_XDECREF(pyStr);
        return qStr;
    }

    const auto listLen = PyList_Size(pyObj);
    if (listLen < 0)
        return QString();

    QString qResStr("");
    for (Py_ssize_t i = 0; i < listLen; i++) {
        auto pyItem = PyList_GetItem(pyObj, i);
        auto pyStr = PyObject_Str(pyItem);
        if (pyStr == nullptr)
            continue;
        qResStr.append(QString::fromUtf8(PyUnicode_AsUTF8(pyStr)));
        Py_XDECREF(pyStr);
    }
    return qResStr;
}

void OOPWorker::emitPyError()
{
        PyObject *excType, *excValue, *excTraceback;
        PyErr_Fetch(&excType, &excValue, &excTraceback);
        PyErr_NormalizeException(&excType, &excValue, &excTraceback);

        QString message;
        if (excType)
            message = pyObjectToQStr(excType);

        if (excValue) {
            const auto str = pyObjectToQStr(excValue);
            if (!str.isEmpty())
                message = QString::fromUtf8("%1\n%2").arg(message).arg(str);
        }

        if (excTraceback) {
            // let's try to generate a useful traceback
                auto pyTbModName = PyUnicode_FromString("traceback");
                auto pyTbMod = PyImport_Import(pyTbModName);
                Py_DECREF(pyTbModName);

                if (pyTbModName == nullptr) {
                    // we can't create a good backtrace, just print the thing as string as a fallback
                    const auto str = pyObjectToQStr(excTraceback);
                    if (!str.isEmpty())
                        message = QString::fromUtf8("%1\n%2").arg(message).arg(str);
                } else {
                    const auto pyFnFormatE = PyObject_GetAttrString(pyTbMod, "format_exception");
                    if (pyFnFormatE && PyCallable_Check(pyFnFormatE)) {
                        auto pyTbVal = PyObject_CallFunctionObjArgs(pyFnFormatE,
                                                                    excType,
                                                                    excValue,
                                                                    excTraceback,
                                                                    nullptr);
                        const auto str = pyObjectToQStr(pyTbVal);
                        if (str != nullptr)
                            message = QString::fromUtf8("%1\n%2").arg(message).arg(str);
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

        if (m_pyInitialized) {
            Py_Finalize();
            m_pyInitialized = false;
        }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
void OOPWorker::prepareAndRun()
{
    // don't attempt to run if we have already failed
    if (m_stage == OOPWorker::ERROR)
        return;

    if (!m_pyInitialized) {
        raiseError(QStringLiteral("Can not run module: Python was not initialized."));
        return;
    }

    {
        // pass selected settings to the current run
        if (PyObject_HasAttrString(m_pyMain, "set_settings")) {
            auto pFnSettings = PyObject_GetAttrString(m_pyMain, "set_settings");
            if (pFnSettings && PyCallable_Check(pFnSettings)) {
                auto pySettings = PyBytes_FromStringAndSize(m_settings.data(), m_settings.size());
                const auto pyRes = PyObject_CallFunctionObjArgs(pFnSettings, pySettings, nullptr);
                if (pyRes == nullptr) {
                    if (PyErr_Occurred()) {
                        emitPyError();
                        goto finalize;
                    }
                } else {
                    Py_XDECREF(pyRes);
                }
            }
            Py_XDECREF(pFnSettings);
        }

        // run prepare function if it exists for initial setup
        if (PyObject_HasAttrString(m_pyMain, "prepare")) {
            auto pFnPrep = PyObject_GetAttrString(m_pyMain, "prepare");
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
        if (PyObject_HasAttrString(m_pyMain, "start")) {
            pFnStart = PyObject_GetAttrString(m_pyMain, "start");
            if (!pFnStart || !PyCallable_Check(pFnStart)) {
                Py_XDECREF(pFnStart);
                pFnStart = nullptr;
            }
        }

        // find the loop function - this function *must* exists,
        // and unlike the other functions isn't optional, so GetAttrString
        // is allowed to throw an error here
        auto pFnLoop = PyObject_GetAttrString(m_pyMain, "loop");
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
        if (PyObject_HasAttrString(m_pyMain, "stop")) {
            auto pFnStop = PyObject_GetAttrString(m_pyMain, "stop");
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
    // we aren't ready anymore,
    // and also stopped running the loop
    setStage(OOPWorker::IDLE);
    m_running = false;

    // ensure any pending emitted events are processed
    qApp->processEvents();
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

bool OOPWorker::checkRunning()
{
    QCoreApplication::processEvents();
    return m_running;
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

    prepareShutdown();
    shutdown();
}

void OOPWorker::makeDocFileAndQuit(const QString &fname)
{
    // FIXME: We ignore Python warnings for now, as we otherwise get lots of
    // "Couldn't read PEP-224 variable docstrings from <Class X>: <class  X> is a built-in class"
    // messages that - currently - we can't do anything about
    qputenv("PYTHONWARNINGS", "ignore");

    Py_Initialize();
    PyRun_SimpleString(qPrintable(
                           QStringLiteral(
                               "import pdoc\n"
                               "\n"
                               "context = pdoc.Context()\n"
                               "mod = pdoc.Module('syio', context=context)\n"
                               "pdoc.link_inheritance(context)\n"
                               "\n"
                               "with open('%1', 'w') as f:\n"
                               "    f.write(mod.text())\n"
                               "    f.write('\\n')\n"
                               "\n"
                               "with open('%1.html', 'w') as f:\n"
                               "    f.write(mod.html())\n"
                               "    f.write('\\n')\n"
                               "\n")
                           .arg(QString(fname).replace("'", "\\'"))));
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

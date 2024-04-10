/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
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
#include "pyworker.h"
#include <QDir>
#include <QCoreApplication>
#include <iostream>

#include "cpuaffinity.h"
#include "rtkit.h"
#include "streams/datatypes.h"
#include "streams/frametype.h"

#include "syio.h"
#include "qstringtopy.h" // needed for QString registration
#include "sydatatopy.h"  // needed for stream data type conversion

using namespace Syntalos;
namespace Syntalos
{
Q_LOGGING_CATEGORY(logPyWorker, "pyworker")
}

PyWorker::PyWorker(SyntalosLink *slink, QObject *parent)
    : QObject(parent),
      m_link(slink),
      m_running(false)
{
    m_pyb = PyBridge::instance(this);
    pythonRegisterSyioModule();

    // set up callbacks
    m_link->setLoadScriptCallback([this](const QString &script, const QString &wdir) {
        return loadPythonScript(script, wdir);
    });
    m_link->setPrepareStartCallback([this](const QByteArray &settings) {
        return prepareStart(settings);
    });
    m_link->setStartCallback([this]() {
        start();
    });
    m_link->setStopCallback([this]() {
        return stop();
    });
    m_link->setShutdownCallback([this]() {
        shutdown();
    });

    // process incoming data forever
    m_evTimer = new QTimer(this);
    m_evTimer->setInterval(0);
    connect(m_evTimer, &QTimer::timeout, this, [this]() {
        m_link->awaitData(500 * 1000);
    });
    m_evTimer->start();
}

PyWorker::~PyWorker()
{
    if (m_pyInitialized)
        Py_Finalize();
}

ModuleState PyWorker::state() const
{
    return m_link->state();
}

SyncTimer *PyWorker::timer() const
{
    return m_link->timer();
}

bool PyWorker::isRunning() const
{
    return m_running;
}

void PyWorker::awaitData(int timeoutUsec)
{
    m_link->awaitData(timeoutUsec);
}

std::shared_ptr<InputPortInfo> PyWorker::inputPortById(const QString &idstr)
{
    for (auto &iport : m_link->inputPorts()) {
        if (iport->id() == idstr)
            return iport;
    }

    return nullptr;
}

std::shared_ptr<OutputPortInfo> PyWorker::outputPortById(const QString &idstr)
{
    for (auto &oport : m_link->outputPorts()) {
        if (oport->id() == idstr)
            return oport;
    }

    return nullptr;
}

bool PyWorker::submitOutput(const std::shared_ptr<OutputPortInfo> &oport, const py::object &pyObj)
{
    switch (oport->dataTypeId()) {
    case syDataTypeId<ControlCommand>():
        return m_link->submitOutput(oport, py::cast<ControlCommand>(pyObj));
    case syDataTypeId<TableRow>():
        return m_link->submitOutput(oport, py::cast<TableRow>(pyObj));
    case syDataTypeId<Frame>():
        return m_link->submitOutput(oport, py::cast<Frame>(pyObj));
    case syDataTypeId<FirmataControl>():
        return m_link->submitOutput(oport, py::cast<FirmataControl>(pyObj));
    case syDataTypeId<FirmataData>():
        return m_link->submitOutput(oport, py::cast<FirmataData>(pyObj));
    case syDataTypeId<IntSignalBlock>():
        return m_link->submitOutput(oport, py::cast<IntSignalBlock>(pyObj));
    case syDataTypeId<FloatSignalBlock>():
        return m_link->submitOutput(oport, py::cast<FloatSignalBlock>(pyObj));
    default:
        return false;
    }
}

void PyWorker::setOutPortMetadataValue(
    const std::shared_ptr<OutputPortInfo> &oport,
    const QString &key,
    const QVariant &value)
{
    oport->setMetadataVar(key, value);
    m_link->updateOutputPort(oport);
}

void PyWorker::setInputThrottleItemsPerSec(const std::shared_ptr<InputPortInfo> &iport, uint itemsPerSec)
{
    iport->setThrottleItemsPerSec(itemsPerSec);
    m_link->updateInputPort(iport);
}

void PyWorker::raiseError(const QString &message)
{
    m_running = false;
    std::cerr << "PyWorker-ERROR: " << message.toStdString() << std::endl;
    m_link->raiseError(message);

    stop();
    shutdown();
}

bool PyWorker::loadPythonScript(const QString &script, const QString &wdir)
{
    if (!wdir.isEmpty())
        QDir::setCurrent(wdir);

    PyConfig config;
    PyConfig_InitPythonConfig(&config);

    auto status = PyConfig_SetString(
        &config, &config.program_name, QCoreApplication::arguments()[0].toStdWString().c_str());
    if (PyStatus_Exception(status)) {
        raiseError(QStringLiteral("Unable to set Python program name: %1").arg(status.err_msg));
        PyConfig_Clear(&config);
        return false;
    }

    // HACK: make Python think *we* are the Python interpreter, so it finds
    // all modules correctly when we are in a virtual environment.
    const auto venvDir = QString::fromUtf8(qgetenv("VIRTUAL_ENV"));
    if (!venvDir.isEmpty()) {
        qCDebug(logPyWorker).noquote() << "Using virtual environment:" << venvDir;
        status = PyConfig_SetString(
            &config, &config.program_name, QDir(venvDir).filePath("bin/python").toStdWString().c_str());
        if (PyStatus_Exception(status)) {
            raiseError(QStringLiteral("Unable to set Python program name: %1").arg(status.err_msg));
            PyConfig_Clear(&config);
            return false;
        }
    }

    // initialize Python in this process
    status = Py_InitializeFromConfig(&config);
    m_pyInitialized = true;
    PyConfig_Clear(&config);

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

QByteArray PyWorker::changeSettings(const QByteArray &oldSettings)
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

bool PyWorker::prepareStart(const QByteArray &settings)
{
    m_settings = settings;
    QTimer::singleShot(0, this, &PyWorker::prepareAndRun);
    return m_pyInitialized;
}

void PyWorker::start()
{
    m_running = true;
}

bool PyWorker::stop()
{
    m_running = false;
    QCoreApplication::processEvents();
    return true;
}

void PyWorker::shutdown()
{
    m_running = false;
    QCoreApplication::processEvents();

    // give other events a bit of time (10ms) to react to the fact that we are no longer running
    QTimer::singleShot(10, this, &QCoreApplication::quit);
    qCDebug(logPyWorker).noquote() << "Shutting down.";
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

void PyWorker::emitPyError()
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
                auto pyTbVal = PyObject_CallFunctionObjArgs(pyFnFormatE, excType, excValue, excTraceback, nullptr);
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
void PyWorker::prepareAndRun()
{
    // don't attempt to run if we have already failed
    if (m_link->state() == ModuleState::ERROR)
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
        if (m_link->state() == ModuleState::ERROR)
            goto finalize;

        // signal that we are ready now, preparations are done
        m_link->setState(ModuleState::READY);

        // find the start function if it exists
        PyObject *pFnStart = nullptr;
        if (PyObject_HasAttrString(m_pyMain, "start")) {
            pFnStart = PyObject_GetAttrString(m_pyMain, "start");
            if (!pFnStart || !PyCallable_Check(pFnStart)) {
                Py_XDECREF(pFnStart);
                pFnStart = nullptr;
            }
        }

        // find the "run" function - if it does not exists, we will create
        // our own run function that does only listen for messages.
        auto pFnRun = PyObject_GetAttrString(m_pyMain, "run");
        if (!PyCallable_Check(pFnRun)) {
            Py_XDECREF(pFnRun);
            pFnRun = nullptr;
        }

        // while we are not running, wait for the start signal
        m_evTimer->stop();
        while (!m_running) {
            m_link->awaitData(1 * 1000); // 1ms timeout
            QCoreApplication::processEvents();
        }
        m_link->setState(ModuleState::RUNNING);

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
        if (m_link->state() == ModuleState::ERROR) {
            Py_XDECREF(pFnRun);
            goto finalize;
        }

        if (pFnRun == nullptr) {
            // we have no run function, so we just listen for events implicitly
            while (m_running) {
                m_link->awaitData(500 * 1000); // 500ms timeout
                QCoreApplication::processEvents();
            }
        } else {
            // call the run function
            auto runRes = PyObject_CallObject(pFnRun, nullptr);
            if (runRes == nullptr) {
                if (PyErr_Occurred())
                    emitPyError();
            } else {
                Py_XDECREF(runRes);
            }

            Py_XDECREF(pFnRun);
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
    m_link->setState(ModuleState::IDLE);
    m_running = false;

    // ensure any pending emitted events are processed
    m_evTimer->start();
    qApp->processEvents();
}
#pragma GCC diagnostic pop

void PyWorker::setState(ModuleState state)
{
    m_link->setState(state);
}

void PyWorker::makeDocFileAndQuit(const QString &fname)
{
    pythonRegisterSyioModule();

    // FIXME: We ignore Python warnings for now, as we otherwise get lots of
    // "Couldn't read PEP-224 variable docstrings from <Class X>: <class  X> is a built-in class"
    // messages that - currently - we can't do anything about
    qputenv("PYTHONWARNINGS", "ignore");

    QString jinjaTemplate = R""""(
<div>
    {% block content %}{% endblock %}

    {% filter minify_css %}
        {% block style %}
            <style>{% include "syntax-highlighting.css" %}</style>
            <style>{% include "theme.css" %}</style>
            <style>{% include "content.css" %}</style>
        {% endblock %}
    {% endfilter %}
</div>
)"""";

    QString jinjaTemplatePyLiteral = "\"\"\"" + jinjaTemplate + "\n\"\"\"";

    Py_Initialize();
    PyRun_SimpleString(qPrintable(
        QStringLiteral("import os\n"
                       "import tempfile\n"
                       "import pdoc\n"
                       "import syio\n"
                       "\n"
                       "jinjaTmpl = ")
        + jinjaTemplatePyLiteral
        + QStringLiteral("\n"
                         "\n"
                         "doc = pdoc.doc.Module(syio)\n"
                         "with tempfile.TemporaryDirectory() as tmp_dir:\n"
                         "    with open(os.path.join(tmp_dir, 'frame.html.jinja2'), 'w') as f:\n"
                         "        f.write(jinjaTmpl)\n"
                         "    pdoc.render.configure(template_directory=tmp_dir)\n"
                         "    html_data = pdoc.render.html_module(module=doc, all_modules={'syio': doc})\n"
                         "    with open('%1', 'w') as f:\n"
                         "        f.write(html_data)\n"
                         "        f.write('\\n')\n"
                         "\n")
              .arg(QString(fname).replace("'", "\\'"))));
    if (Py_FinalizeEx() < 0)
        exit(9);

    // documentation generated successfully, we can quit now
    exit(0);
}

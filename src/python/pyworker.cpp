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
#include "pyw-config.h"

#include <QDir>
#include <QCoreApplication>
#include <iostream>
#include <pybind11/embed.h>

#include "cpuaffinity.h"
#include "rtkit.h"

using namespace Syntalos;
namespace Syntalos
{
Q_LOGGING_CATEGORY(logPyWorker, "pyworker")
}

PyWorker::PyWorker(SyntalosLink *slink, QObject *parent)
    : QObject(parent),
      m_link(slink),
      m_scriptLoaded(false),
      m_running(false)
{
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

    // set up embedded Python interpreter
    initPythonInterpreter();

    // signal that we are ready and done with initialization
    m_link->setState(ModuleState::IDLE);

    // process incoming data, so we can react to incoming requests
    m_evTimer = new QTimer(this);
    m_evTimer->setInterval(0);
    connect(m_evTimer, &QTimer::timeout, this, [this]() {
        m_link->awaitData(125 * 1000);
    });
    m_evTimer->start();

    // switch to unbuffered mode so our parent receives Python output
    // (e.g. from print() & Co.) faster.
    setenv("PYTHONUNBUFFERED", "1", 1);
}

void PyWorker::resetPyCallbacks()
{
    m_link->setShowDisplayCallback(nullptr);
    m_link->setShowSettingsCallback(nullptr);

    for (const auto &iport : m_link->inputPorts())
        iport->setNewDataRawCallback(nullptr);
}

PyWorker::~PyWorker()
{
    // Reset any callback that calls into the current Python script directly
    // before tearing down the interpreter.
    resetPyCallbacks();

    py::finalize_interpreter();
}

static void ensureModuleImportPaths()
{
    py::exec("import sys");
    py::exec(QStringLiteral("sys.path.insert(0, '%1')")
                 .arg(QStringLiteral(SY_PYTHON_MOD_DIR).replace("'", "\\'"))
                 .toStdString());
    py::exec(
        QStringLiteral("sys.path.insert(0, '%1')").arg(qApp->applicationDirPath().replace("'", "\\'")).toStdString());
}

bool PyWorker::initPythonInterpreter()
{
    m_scriptLoaded = false;

    // Reset any callback that calls into the current Python script directly
    // before tearing down the interpreter.
    resetPyCallbacks();

    if (Py_IsInitialized())
        py::finalize_interpreter();

    PyConfig config;
    PyConfig_InitPythonConfig(&config);
    auto status = PyConfig_SetString(
        &config, &config.program_name, QCoreApplication::arguments()[0].toStdWString().c_str());
    if (PyStatus_Exception(status)) {
        QTimer::singleShot(0, this, [this, status]() {
            raiseError(QStringLiteral("Unable to set Python program name: %1").arg(status.err_msg));
        });

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
            QTimer::singleShot(0, this, [this, status]() {
                raiseError(QStringLiteral("Unable to set Python program name: %1").arg(status.err_msg));
            });

            PyConfig_Clear(&config);
            return false;
        }
    }

    py::initialize_interpreter(&config);

    // make sure we find the syntalos_mlink module even if it isn't installed yet
    try {
        ensureModuleImportPaths();
    } catch (py::error_already_set &e) {
        raiseError(QStringLiteral("%1").arg(e.what()));
        PyConfig_Clear(&config);
        return false;
    }

    // pass our Syntalos link to the Python code
    {
        auto mlink_mod = py::module_::import("syntalos_mlink");
        auto pySLink = py::cast(m_link, py::return_value_policy::reference);
        mlink_mod.attr("init_link")(pySLink);
    }

    PyConfig_Clear(&config);
    return true;
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

void PyWorker::raiseError(const QString &message)
{
    m_running = false;
    std::cerr << "PyWorker-ERROR: " << message.toStdString() << std::endl;
    m_link->raiseError(message);

    if (m_running)
        stop();
    shutdown();
}

bool PyWorker::loadPythonScript(const QString &script, const QString &wdir)
{
    if (!wdir.isEmpty())
        QDir::setCurrent(wdir);

    // create a clean slate to load the new script
    py::globals().clear();

    try {
        // execute the script
        py::exec(script.toStdString(), py::globals());
    } catch (py::error_already_set &e) {
        raiseError(QStringLiteral("%1").arg(e.what()));
        return false;
    }

    m_scriptLoaded = true;
    return true;
}

bool PyWorker::prepareStart(const QByteArray &settings)
{
    m_settings = settings;

    if (!m_scriptLoaded) {
        raiseError(QStringLiteral("No Python script loaded."));
        return false;
    }

    bool success = true;
    try {
        // pass selected settings to the current run
        if (py::globals().contains("set_settings")) {
            auto pyFnSetSettings = py::globals()["set_settings"];
            if (!pyFnSetSettings.is_none())
                pyFnSetSettings(py::bytes(settings));
        }

        // run prepare function if it exists for initial setup
        if (py::globals().contains("prepare")) {
            auto pyFnPrepare = py::globals()["prepare"];
            if (!pyFnPrepare.is_none()) {
                auto res = pyFnPrepare();
                success = res.cast<bool>();
            }
        }

    } catch (py::error_already_set &e) {
        raiseError(QStringLiteral("%1").arg(e.what()));
        return false;
    }

    if (!success)
        return false;

    // signal that we are ready now, preparations are done
    m_link->setState(ModuleState::READY);

    // get the main processing loop of this run ready and have it wait for the start signal
    QTimer::singleShot(0, this, &PyWorker::executePythonRunFn);

    return true;
}

void PyWorker::start()
{
    try {
        if (py::globals().contains("start")) {
            auto pyFnStart = py::globals()["start"];
            if (!pyFnStart.is_none())
                pyFnStart();
        }
    } catch (py::error_already_set &e) {
        raiseError(QStringLiteral("%1").arg(e.what()));
        return;
    }

    m_running = true;
}

bool PyWorker::stop()
{
    m_running = false;
    QCoreApplication::processEvents();

    try {
        if (py::globals().contains("stop")) {
            auto pyFnStop = py::globals()["stop"];
            if (!pyFnStop.is_none())
                pyFnStop();
        }
    } catch (py::error_already_set &e) {
        raiseError(QStringLiteral("%1").arg(e.what()));
        return false;
    }

    return true;
}

void PyWorker::shutdown()
{
    m_running = false;
    qCDebug(logPyWorker).noquote() << "Shutting down.";
    QCoreApplication::processEvents();
    awaitData(1000);
    exit(0);
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
}

void PyWorker::executePythonRunFn()
{
    // don't attempt to run if we have already failed
    if (m_link->state() == ModuleState::ERROR)
        return;

    // find the "run" function - if it does not exists, we will create
    // our own run function that does only listen for messages.
    py::object pyFnRun = py::none();
    if (py::globals().contains("run"))
        pyFnRun = py::globals()["run"];

    // while we are not running, wait for the start signal
    m_evTimer->stop();
    while (!m_running) {
        m_link->awaitData(1 * 1000); // 1ms timeout
        QCoreApplication::processEvents();

        if (m_link->state() == ModuleState::ERROR) {
            // bail out if any error was raised
            m_evTimer->start();
            return;
        }
    }

    m_link->setState(ModuleState::RUNNING);
    if (pyFnRun.is_none()) {
        // we have no run function, so we just listen for events implicitly
        while (m_running) {
            m_link->awaitData(250 * 1000); // 250ms timeout
            QCoreApplication::processEvents();
        }
    } else {
        // call the run function
        try {
            pyFnRun();
        } catch (py::error_already_set &e) {
            raiseError(QStringLiteral("%1").arg(e.what()));
        }
    }

    // we aren't ready anymore,
    // and also stopped running the loop
    m_link->setState(ModuleState::IDLE);
    m_running = false;

    // ensure any pending emitted events are processed
    m_evTimer->start();
    qApp->processEvents();
}

void PyWorker::setState(ModuleState state)
{
    m_link->setState(state);
}

void PyWorker::makeDocFileAndQuit(const QString &fname)
{
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

    py::scoped_interpreter guard{};

    // make sure we find the syntalos_mlink module even if it isn't installed yet
    ensureModuleImportPaths();

    try {
        py::exec(qPrintable(
            QStringLiteral("import os\n"
                           "import tempfile\n"
                           "import pdoc\n"
                           "import syntalos_mlink\n"
                           "\n"
                           "jinjaTmpl = ")
            + jinjaTemplatePyLiteral
            + QStringLiteral(
                  "\n"
                  "\n"
                  "doc = pdoc.doc.Module(syntalos_mlink)\n"
                  "with tempfile.TemporaryDirectory() as tmp_dir:\n"
                  "    with open(os.path.join(tmp_dir, 'frame.html.jinja2'), 'w') as f:\n"
                  "        f.write(jinjaTmpl)\n"
                  "    pdoc.render.configure(template_directory=tmp_dir)\n"
                  "    html_data = pdoc.render.html_module(module=doc, all_modules={'syntalos_mlink': doc})\n"
                  "    with open('%1', 'w') as f:\n"
                  "        for line in html_data.split('\\n'):\n"
                  "            f.write(line.strip() + '\\n')\n"
                  "        f.write('\\n')\n"
                  "\n")
                  .arg(QString(fname).replace("'", "\\'"))));
    } catch (py::error_already_set &e) {
        std::cerr << "Failed to generate syntalos_mlink Python docs: " << e.what() << std::endl;
    }

    // documentation generated successfully, we can quit now
    exit(0);
}

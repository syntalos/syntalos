
#include <Python.h>
#include "pythread.h"

#include <QCoreApplication>
#include <QDebug>

#include "firmata/serialport.h"
#include "maio.h"

PyThread::PyThread(QObject *parent)
    : QThread(parent)
{
    m_firmata = new SerialFirmata(this);
    m_firmata->moveToThread(this);

    // the MaIO interface an all Python stuff belongs to this thread and
    // must not ever be touched directly from the outside
    MaIO::instance()->moveToThread(this);

    MaIO::instance()->setFirmata(m_firmata);

    pythonRegisterMaioModule();
}

MaIO *PyThread::maio() const
{
    return MaIO::instance();
}

void PyThread::initFirmata(const QString &serialDevice)
{
    qDebug() << "Loading Firmata interface (" << serialDevice << ")";
    if (m_firmata->device().isEmpty()) {
        if (!m_firmata->setDevice(serialDevice)) {
            emit firmataError(m_firmata->statusText());
            return;
        }
    }

    if (!m_firmata->waitForReady(10000) || m_firmata->statusText().contains("Error")) {
        emit firmataError(QString("Unable to open serial interface: %1").arg(m_firmata->statusText()));
        m_firmata->setDevice(QString());
        return;
    }
}

void PyThread::start(QThread::Priority priority)
{
    // ensure we have quit before we start running again
    this->wait();

    m_initializing = true;
    QThread::start(priority);
}

static int python_call_quit(void *)
{
    PyErr_SetInterrupt();
    return -1;
}

void PyThread::quit()
{
    // do nothing if we are already terminating (e.g. due to a previous error)
    if (m_terminating)
        return;

    // tell that we are about to intentionally terminate the script
    m_terminating = true;

    // When trying to abort the script immediately after launching it, we can run into crashes
    // withing CPython (adding a pending call prior to initializing is a bad idea)
    // Therefore we wait here until basic initialization of the interpreter is done.
    while (m_initializing)
        QThread::usleep(100);

    Py_AddPendingCall(&python_call_quit, NULL);

    QThread::quit();

    while (!this->wait(20000)) {
        qWarning() << "PyThread quit wait time ran out, attempting to terminate thread now.";
        Py_AddPendingCall(&python_call_quit, NULL);
        this->terminate();
    }

    MaIO::instance()->reset();
}

void PyThread::setScriptContent(const QString &script)
{
    m_script = script;
}

void PyThread::run()
{
    m_terminating = false;
    m_initializing = true;
    m_firmata->moveToThread(this);
    MaIO::instance()->moveToThread(this);
    //! Py_SetProgramName("mazeamaze-script");

    // initialize Python in the thread
    Py_Initialize();

    PyObject *mainModule = PyImport_AddModule("__main__");
    if (mainModule == NULL) {
        emit scriptError("Can not execute Python code: No __main__module.");

        Py_Finalize();
        m_initializing = false;
        return;
    }
    PyObject *mainDict = PyModule_GetDict(mainModule);

    // initialization phase completed
    m_initializing = false;

    // run script
    auto res = PyRun_String(qPrintable(m_script), Py_file_input, mainDict, mainDict);

    // quit without any error handling when we are terminating script execution
    if (m_terminating) {
        Py_XDECREF(res);

        Py_Finalize();
        return;
    }

    m_terminating = true;
    if (res == NULL) {
        if (PyErr_Occurred()) {
            PyObject *excType, *excValue, *excTraceback;
            PyErr_Fetch(&excType, &excValue, &excTraceback);
            PyErr_NormalizeException(&excType, &excValue, &excTraceback);

            QString message;
            if (excType) {
                PyObject* str = PyObject_Str(excType);
                if (str != NULL) {
                    const char *bytes = PyUnicode_AsUTF8(str);
                    message = QString::fromUtf8(bytes);
                    Py_XDECREF(str);
                }
            }
            if (excValue) {
                PyObject* str = PyObject_Str(excValue);
                if (str != NULL) {
                    const char *bytes = PyUnicode_AsUTF8(str);
                    message = QString::fromUtf8("%1\n%2").arg(message).arg(QString::fromUtf8(bytes));
                    Py_XDECREF(str);
                }
            }
            if (excTraceback) {
                PyObject* str = PyObject_Str(excTraceback);
                if (str != NULL) {
                    const char *bytes = PyUnicode_AsUTF8(str);
                    message = QString::fromUtf8("%1\n%2").arg(message).arg(QString::fromUtf8(bytes));
                    Py_XDECREF(str);
                }
            }

            if (message.isEmpty())
                message = QStringLiteral("An unknown Python error occured.");

            qDebug() << "Python error:" << message;
            emit scriptError(message);

            Py_XDECREF(excTraceback);
            Py_XDECREF(excType);
            Py_XDECREF(excValue);
        }
    } else {
        Py_XDECREF(res);
    }

    Py_Finalize();

    m_terminating = false;
}

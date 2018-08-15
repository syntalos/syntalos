
#include "pyhelper.h"
#include "pythread.h"

#include <QCoreApplication>
#include <QDebug>

#include "firmata/serialport.h"
#include "maio.h"

PyThread::PyThread(QObject *parent)
    : QThread(parent)
{
    // the MaIO interface an all Python stuff belongs to this thread and
    // must not ever be touched directly from the outside
    MaIO::instance()->moveToThread(this);
}

void PyThread::setFirmata(SerialFirmata *firmata)
{
    MaIO::instance()->setFirmata(firmata);
    firmata->moveToThread(this);
}

MaIO *PyThread::maio()
{
    return MaIO::instance();
}

void PyThread::runScript()
{
    this->start();
}

static int python_call_quit(void *)
{
    PyErr_SetInterrupt();
    return -1;
}

void PyThread::terminate()
{
    m_terminating = true;
    Py_AddPendingCall(&python_call_quit, NULL);

    this->quit();
    while (this->isRunning()) {
        // busy wait
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
    //! Py_SetProgramName("mazeamaze-script");

    pythonRegisterMaioModule();
    CPyInstance hInstance;

    PyObject *mainModule = PyImport_AddModule("__main__");
    if (mainModule == NULL) {
        emit errorReceived("Can not execute Python code: No __main__module.");
        return;
    }
    PyObject *mainDict = PyModule_GetDict(mainModule);

    auto res = PyRun_String(qPrintable(m_script), Py_file_input, mainDict, mainDict);

    // quit without any error handling when we are terminating script execution
    if (m_terminating) {
        Py_XDECREF(res);
        return;
    }

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
            emit errorReceived(message);

            Py_XDECREF(excTraceback);
            Py_XDECREF(excType);
            Py_XDECREF(excValue);
        }
    } else {
        Py_XDECREF(res);
    }
}

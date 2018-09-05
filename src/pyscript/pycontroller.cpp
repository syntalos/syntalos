
#include <Python.h>
#include "pycontroller.h"

#include <QCoreApplication>
#include <QThread>
#include <QDebug>

#include "firmata/serialport.h"
#include "maio.h"


static int python_call_quit(void *)
{
    PyErr_SetInterrupt();
    return -1;
}

class PyWorker : public QObject
{
    Q_OBJECT
public:
    explicit PyWorker(QObject *parent = nullptr)
        : QObject(parent),
          m_state(PyState::STOPPED)
    {
    }

public slots:
    void runScript()
    {
        m_state = PyState::INITIALIZING;
        //! Py_SetProgramName("mazeamaze-script");

        // initialize Python in the thread
        Py_Initialize();

        PyObject *mainModule = PyImport_AddModule("__main__");
        if (mainModule == NULL) {
            emit scriptError("Can not execute Python code: No __main__module.");

            Py_Finalize();
            m_state = PyState::STOPPED;
            return;
        }
        PyObject *mainDict = PyModule_GetDict(mainModule);

        // initialization phase completed
        m_state = PyState::RUNNING;

        // run script
        auto res = PyRun_String(qPrintable(scriptContent), Py_file_input, mainDict, mainDict);

        // quit without any error handling when we are already terminating script execution
        // (in this case, termination was intentional)
        if (m_state == PyState::TERMINATING) {
            Py_XDECREF(res);

            Py_Finalize();
            return;
        }

        m_state = PyState::TERMINATING;
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

        // move singletons back to main thread
        MaIO::instance()->reset();
        MaIO::instance()->moveToThread(QCoreApplication::instance()->thread());

        m_state = PyState::STOPPED;
    }

    void terminateScript()
    {
        // When trying to abort the script immediately after launching it, we can run into crashes
        // withing CPython (adding a pending call prior to initializing is a bad idea)
        // Therefore we wait here until basic initialization of the interpreter is done.
        while (m_state == PyState::INITIALIZING)
            QThread::usleep(100);

        // do nothing if we are not running
        if (m_state != PyState::RUNNING)
            return;

        // tell that we are about to intentionally terminate the script
        m_state = PyState::TERMINATING;

        // terminate script execution as soon as possible
        Py_AddPendingCall(&python_call_quit, NULL);
    }

signals:
    void scriptError(const QString& message);
    void firmataError(const QString& message);

public:
    QString scriptContent;

private:
    enum PyState
    {
        STOPPED,
        INITIALIZING,
        RUNNING,
        TERMINATING
    };

    PyState m_state;
};



PyController::PyController(QObject *parent)
    : QObject(parent),
      m_pyThread(nullptr),
      m_running(false)
{
    // The SerialFirmata instance need to reside in the program's main thread - it communicates
    // with the Python thread via a queued connection.
    m_firmata = new SerialFirmata(this);

    MaIO::instance()->setFirmata(m_firmata);

    pythonRegisterMaioModule();
}

MaIO *PyController::maio() const
{
    return MaIO::instance();
}

void PyController::initFirmata(const QString &serialDevice)
{
    qDebug() << "Loading Firmata interface (" << serialDevice << ")";
    if (m_firmata->device().isEmpty()) {
        if (!m_firmata->setDevice(serialDevice)) {
            emit firmataError(m_firmata->statusText());
            return;
        }
    }

    if (!m_firmata->waitForReady(20000) || m_firmata->statusText().contains("Error")) {
        emit firmataError(QString("Unable to open serial interface: %1").arg(m_firmata->statusText()));
        m_firmata->setDevice(QString());
        return;
    }
}

void PyController::setScriptContent(const QString &script)
{
    m_script = script;
}

void PyController::startScript()
{
    if (m_running) {
        qCritical("Can not re-launch an already running maze script!");
        return;
    }

    m_pyThread = new QThread;
    m_worker = new PyWorker;
    m_worker->moveToThread(m_pyThread);

    // the MaIO interface an all Python stuff belongs to this thread and
    // must not ever be touched directly from the outside
    MaIO::instance()->moveToThread(m_pyThread);

    m_worker->scriptContent = m_script;

    connect(m_pyThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_pyThread, &QThread::finished, m_pyThread, &QObject::deleteLater);

    connect(m_pyThread, &QThread::started, m_worker, &PyWorker::runScript);
    connect(m_pyThread, &QThread::finished, this, &PyController::pyThreadFinished);

    connect(m_worker, &PyWorker::firmataError, this, &PyController::firmataError);
    connect(m_worker, &PyWorker::scriptError, this, &PyController::scriptError);

    // the MaIO interface an all Python stuff belongs to this thread and
    // must not ever be touched directly from the outside
    MaIO::instance()->moveToThread(m_pyThread);

    m_running = true;
    m_pyThread->start();
}

void PyController::terminateScript()
{
    // we hijack the worker thread here by calling into it from the outside.
    // this feels wrong, better suggestions are welcome!
    // (all Python functions called in this function are threadsafe)

    m_worker->terminateScript();

    if (m_pyThread != nullptr) {
        m_pyThread->quit();
        while ((m_pyThread != nullptr) && (!m_pyThread->wait(20000))) {
            qWarning() << "PyWorker quit wait time ran out, attempting to terminate thread now.";
            Py_AddPendingCall(&python_call_quit, NULL);
            m_pyThread->terminate();
        }
    }
}

bool PyController::isRunning() const
{
    return m_running;
}

void PyController::pyThreadFinished()
{
    // the object are deleted automatically, we set this to NULL to be able to check for
    // them later and make error detection easier (nullpointer dereferences say more than invalid memory access)
    m_pyThread = nullptr;
    m_worker = nullptr;

    m_running = false;

    qDebug() << "Python script execution ended.";
}

#include "pycontroller.moc"

/*
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
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

#include "pyscriptmodule.h"

#include <QMessageBox>
#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QTextBrowser>
#include <QDebug>
#include <QHBoxLayout>
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>

#include "zmqserver.h"

PyScriptModule::PyScriptModule(QObject *parent)
    : AbstractModule(parent)
{
    m_name = QStringLiteral("Python Script");
    m_pyoutWindow = nullptr;
    m_scriptWindow = nullptr;

    m_workerBinary = QStringLiteral("%1/modules/pyscript/mapyworker/mapyworker").arg(QCoreApplication::applicationDirPath());
    QFileInfo checkBin(m_workerBinary);
    if (!checkBin.exists()) {
        m_workerBinary = QStringLiteral("%1/../lib/mazeamaze/mapyworker").arg(QCoreApplication::applicationDirPath());
        QFileInfo fi(m_workerBinary);
        m_workerBinary = fi.canonicalFilePath();
    }

    m_zserver = nullptr;
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    m_funcRelay = new MaFuncRelay(this);
}

PyScriptModule::~PyScriptModule()
{
    if (m_zserver == nullptr) {
        delete m_zserver;
        m_zserver = nullptr;
    }
    if (m_pyoutWindow != nullptr)
        delete m_pyoutWindow;
    if (m_scriptWindow != nullptr)
        delete m_scriptWindow;
}

QString PyScriptModule::id() const
{
    return QStringLiteral("pyscript");
}

QString PyScriptModule::description() const
{
    return QStringLiteral("Control certain aspects of MazeAmaze (most notably Firmata I/O) using a Python script.");
}

QPixmap PyScriptModule::pixmap() const
{
    return QPixmap(":/module/python");
}

bool PyScriptModule::initialize(ModuleManager *manager)
{
    Q_UNUSED(manager)
    assert(!initialized());
    setState(ModuleState::INITIALIZING);

    if (m_workerBinary.isEmpty()) {
        raiseError("Unable to find Python worker binary. Is MazeAmaze installed correctly?");
        return false;
    }

    m_pyoutWindow = new QTextBrowser;
    m_pyoutWindow->setFontFamily(QStringLiteral("Monospace"));
    m_pyoutWindow->setFontPointSize(10);
    m_pyoutWindow->setWindowTitle(QStringLiteral("Python Console Output"));
    m_pyoutWindow->setWindowIcon(QIcon(":/icons/generic-view"));
    m_pyoutWindow->resize(540, 210);
    m_displayWindows.append(m_pyoutWindow);

    // set up code editor
    auto editor = KTextEditor::Editor::instance();
    // create a new document
    auto pyDoc = editor->createDocument(this);

    QFile samplePyRc(QStringLiteral(":/texts/pyscript-sample.py"));
    if(samplePyRc.open(QIODevice::ReadOnly)) {
        pyDoc->setText(samplePyRc.readAll());
    }
    samplePyRc.close();

    m_scriptWindow = new QWidget;
    m_scriptWindow->setWindowIcon(QIcon(":/icons/generic-config"));
    m_scriptWindow->setWindowTitle(QStringLiteral("Python Code"));
    auto scriptLayout = new QHBoxLayout(m_scriptWindow);
    m_scriptWindow->setLayout(scriptLayout);
    scriptLayout->setMargin(2);
    m_scriptWindow->resize(680, 780);
    m_settingsWindows.append(m_scriptWindow);

    m_scriptView = pyDoc->createView(m_scriptWindow);
    scriptLayout->addWidget(m_scriptView);
    pyDoc->setHighlightingMode("python");

    setState(ModuleState::READY);
    setInitialized();
    return true;
}

bool PyScriptModule::prepare(const QString &storageRootDir, const TestSubject &testSubject, HRTimer *timer)
{
    Q_UNUSED(storageRootDir);
    Q_UNUSED(testSubject);
    Q_UNUSED(timer);
    setState(ModuleState::PREPARING);

    m_pyoutWindow->clear();

    delete m_funcRelay;
    m_funcRelay = new MaFuncRelay(this);
    m_funcRelay->setPyScript(m_scriptView->document()->text());

    delete m_zserver;
    m_zserver = new ZmqServer(m_funcRelay);
    m_zserver->start(timer);

    QStringList args;
    args.append(m_zserver->socketName());

    m_process->start(m_workerBinary, args, QProcess::Unbuffered | QProcess::ReadWrite);
    if (!m_process->waitForStarted(10)) {
        raiseError("Unable to launch worker process for Python code.");
        m_pyoutWindow->setText(m_process->readAllStandardOutput());
        return false;
    }
    if (m_process->waitForFinished(100)) {
        // the process terminated prematurely
        raiseError("Unable to launch worker process for Python code.");
        m_pyoutWindow->setText(m_process->readAllStandardOutput());
        return false;
    }

    m_running = true;
    setState(ModuleState::WAITING);
    return true;
}

void PyScriptModule::start()
{
    m_funcRelay->setCanStartScript(true);
    setState(ModuleState::RUNNING);
}

bool PyScriptModule::runCycle()
{
    // if the script exited without error, we just continue to run
    // everything else and don't execute useless operations anymore
    // in a cycle.
    if (!m_running)
        return true;

    if (m_process->waitForFinished(0)) {
        if (m_process->exitCode() != 0) {
            raiseError(QStringLiteral("Python code terminated with an error (%1) - Please check the console output for details.").arg(m_process->exitCode()));
            return false;
        }
        m_running = false;
        return true;
    }

    const auto data = m_process->readAllStandardOutput();
    if (data.isEmpty())
        return true;
    m_pyoutWindow->setText(data);

    return true;
}

void PyScriptModule::stop()
{
    if (m_zserver == nullptr) {
        delete m_zserver;
        m_zserver = nullptr;
    }
    m_process->kill();
}

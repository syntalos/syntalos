/*
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
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

#include "mazescript.h"

#include <QScriptEngine>
#include <QDebug>
#include <QEventLoop>
#include <QFile>
#include <QScriptEngineDebugger>
#include <QMessageBox>
#include <QProcess>

#include "firmata/serialport.h"

#include "mazeio.h"


const QString defaultSampleScript = QStringLiteral("\n//\n"
                                           "// Configure the pins we want to use\n"
                                           "//\n"
                                           "io.newDigitalPin(0, 'armLeft',  'input');\n"
                                           "io.newDigitalPin(2, 'armRight', 'input');\n"
                                           "\n"
                                           "io.newDigitalPin(6, 'dispLeft',  'output');\n"
                                           "io.newDigitalPin(8, 'dispRight', 'output');\n"
                                           "\n"
                                           "io.newDigitalPin(2, 'pinSignal', 'output');\n"
                                           "\n"
                                           "lastArm = \"unknown\"\n"
                                           "\n"
                                           "io.setEventsHeader([\"State\"]);\n"
                                           "io.setTimeout(function() {\n"
                                           "    // light LED on port 2 briefly after 3 seconds\n"
                                           "    io.pinSetValue('pinSignal', true);\n"
                                           "    io.sleep(500); // wait 500 msec\n"
                                           "    io.pinSetValue('pinSignal', false);\n"
                                           "}, 3000);\n"
                                           "\n"
                                           "onDigitalInput = function inputReceived(pinName, value)\n"
                                           "{\n"
                                           "    if (!value)\n"
                                           "        return;\n"
                                           "\n"
                                           "    if (pinName == lastArm)\n"
                                           "        return;\n"
                                           "    lastArm = pinName;\n"
                                           "\n"
                                           "    io.saveEvent('success');\n"
                                           "\n"
                                           "    if (pinName == 'armLeft')\n"
                                           "        io.pinSignalPulse('dispLeft');\n"
                                           "    else if (pinName == 'armRight')\n"
                                           "        io.pinSignalPulse('dispRight');\n"
                                           "}\n"
                                           "\n"
                                           "io.valueChanged.connect(onDigitalInput);\n");

MazeScript::MazeScript(QObject *parent)
    : QObject(parent),
      m_externalProcess(nullptr)
{
    m_firmata = new SerialFirmata(this);

    m_script = defaultSampleScript;

    m_eventFile = new QFile;

    m_running = false;
    
    // initialize the JS engine
    m_jseng = nullptr;
    m_mazeio = nullptr;
    resetEngine();
}

MazeScript::~MazeScript()
{
    if (m_jseng)
        delete m_jseng;
    delete m_eventFile;
}

void MazeScript::initFirmata(const QString &serialDevice)
{
    // FIXME: Eeek...
    if (m_useExternalScript)
        return;

    qDebug() << "Loading Firmata interface (" << serialDevice << ")";
    if (m_firmata->device().isEmpty()) {
        if (!m_firmata->setDevice(serialDevice)) {
            emit firmataError(m_firmata->statusText());
            emit finished();
            return;
        }
    }

    if (!m_firmata->waitForReady(4000) || m_firmata->statusText().contains("Error")) {
        emit firmataError(QString("Unable to open serial interface: %1").arg(m_firmata->statusText()));
        m_firmata->setDevice(QString());
        emit finished();
        return;
    }
}

void MazeScript::setScript(const QString &script)
{
    m_script = script;
}

QString MazeScript::script() const
{
    return m_script;
}

void MazeScript::setEventFile(const QString &fname)
{
    m_eventFileName = fname;
}

void MazeScript::setExternalScript(QString path)
{
    m_externalScript = path;
}

QString MazeScript::externalScript() const
{
    return m_externalScript;
}

void MazeScript::setUseExternalScript(bool value)
{
    m_useExternalScript = value;
}

bool MazeScript::useExternalScript() const
{
    return m_useExternalScript;
}

void MazeScript::resetEngine()
{
    if (m_jseng)
        delete m_jseng;
    
    m_jseng = new QScriptEngine;
    QScriptEngineDebugger debugger;
    debugger.attachTo(m_jseng);

    if (m_mazeio)
        delete m_mazeio;

    m_mazeio = new MazeIO(m_firmata, this);
    connect(m_mazeio, &MazeIO::eventSaved, this, &MazeScript::eventReceived);
    connect(m_mazeio, &MazeIO::headersSet, this, &MazeScript::headersReceived);
}

void MazeScript::run()
{
    if (m_running) {
        qWarning() << "Can not start an already active MazeScript.";
        return;
    }

    // FIXME: temporary hack
    if (m_useExternalScript) {
        if (m_externalProcess != nullptr)
            delete m_externalProcess;
        m_externalProcess = new QProcess(this);
        m_externalProcess->start(m_externalScript);

        if (!m_externalProcess->waitForStarted()) {
            emit evalError(0, "Unable to launch external script!");
            return;
        }

        m_running = true;
        return;
    }


    // prepare event log file
    if (!m_eventFileName.isEmpty()) {
        m_eventFile->setFileName(m_eventFileName);
        if (!m_eventFile->open(QFile::WriteOnly | QFile::Truncate)) {
            emit evalError(0, "Unable to open events file");
            return;
        }
    }

    qDebug() << "Evaluating Maze Script";

    // we don't have any events yet
    m_haveEvents = false;
    
    // start timer for event log
    m_timer.start();

    // set context
    auto context = m_jseng->pushContext();
    auto mazeIOVal = m_jseng->newQObject(m_mazeio);
    context->activationObject().setProperty("io", mazeIOVal);
    context->activationObject().setProperty("mazeIO", mazeIOVal); // backwards compatibility

    // run script
    m_running = true;
    auto result = m_jseng->evaluate(m_script);
    if (m_jseng->hasUncaughtException())
        emit evalError(m_jseng->uncaughtExceptionLineNumber(), result.toString());
}

void MazeScript::headersReceived(const QStringList& headers)
{
    // make copy
    QStringList hdrs = headers;
    hdrs.prepend("Time");
    
    if (m_haveEvents) {
        QMessageBox::warning(nullptr, "Script Error", "Can not change headers after already receiving events.");
        return;
    }

    emit headersSet(hdrs);
    
    // write headers
    if (!m_eventFile->isOpen())
        return;

    m_eventFile->write(qPrintable(hdrs.join(";") + "\n"));
}

void MazeScript::eventReceived(const QStringList &messages)
{
    QStringList msgs = messages;
    msgs.prepend(QString::number(m_timer.elapsed()));

    emit mazeEvent(msgs);

    m_haveEvents = true;
    // write to file if file is opened
    if (!m_eventFile->isOpen())
        return;

    m_eventFile->write(qPrintable(msgs.join(";") + "\n"));
}

void MazeScript::stop()
{
    if (!m_running)
        return;

    // temporary hack
    if (m_useExternalScript) {
        if (m_externalProcess == nullptr)
            return;

        m_running = false;
        m_externalProcess->terminate();
        if (!m_externalProcess->waitForFinished(4000))
            m_externalProcess->kill();
        return;
    }

    m_jseng->abortEvaluation();
    m_eventFile->close();
    m_jseng->popContext();
    resetEngine();

    m_running = false;
    emit finished();
}

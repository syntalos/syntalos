/*
 * Copyright (C) 2016-2018 Matthias Klumpp <matthias@tenstral.net>
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

#include <QDebug>
#include <QEventLoop>
#include <QFile>
#include <QMessageBox>

#include "firmata/serialport.h"

#include "pythread.h"
#include "maio.h"


const QString defaultSampleScript = QStringLiteral("import maio as io\n"
                                                   "import time\n"
                                                   "from threading import Timer\n\n"
                                                   "#\n"
                                                   "# Configure the pins we want to use\n"
                                                   "#\n"
                                                   "io.new_digital_pin(0, 'armLeft',  'input')\n"
                                                   "io.new_digital_pin(2, 'armRight', 'input')\n"
                                                   "\n"
                                                   "io.new_digital_pin(6, 'dispLeft',  'output')\n"
                                                   "io.new_digital_pin(8, 'dispRight', 'output')\n"
                                                   "\n"
                                                   "io.new_digital_pin(2, 'pinSignal', 'output')\n"
                                                   "\n"
                                                   "lastArm = 'unknown'\n"
                                                   "\n\n"
                                                   "def signal_led_blink():\n"
                                                   "    io.pin_set_value('pinSignal', True)\n"
                                                   "    time.sleep(.5) # wait 500 msec\n"
                                                   "    io.pin_set_value('pinSignal', False)\n"
                                                   "\n\n"
                                                   "def digital_input_received(pinName, value):\n"
                                                   "    if not value:\n"
                                                   "        return\n"
                                                   "\n"
                                                   "    if pinName == lastArm:\n"
                                                   "        return\n"
                                                   "    lastArm = pinName\n"
                                                   "\n"
                                                   "    io.save_event('success')\n"
                                                   "\n"
                                                   "    if pinName == 'armLeft':\n"
                                                   "        io.pin_signal_pulse('dispLeft')\n"
                                                   "    elif (pinName == 'armRight'):\n"
                                                   "        io.pin_signal_pulse('dispRight')\n"
                                                   "\n\n"
                                                   "def main():\n"
                                                   "    io.set_events_header(['State'])\n"
                                                   "    # light LED on port 2 briefly after 3 seconds\n"
                                                   "    timer = Timer(3, signal_led_blink)\n"
                                                   "    timer.start()\n"
                                                   "\n"
                                                   "    while True:\n"
                                                   "        r, pinName, value = io.receive_digital_input()\n"
                                                   "        if r:\n"
                                                   "            digital_input_received(pinName, value)\n"
                                                  "\n\nmain()\n");

MazeScript::MazeScript(QObject *parent)
    : QObject(parent),
      m_pythread(nullptr)
{
    m_firmata = new SerialFirmata(this);
    resetEngine();

    m_script = defaultSampleScript;

    m_eventFile = new QFile;

    m_running = false;
}

MazeScript::~MazeScript()
{
    delete m_eventFile;
}

void MazeScript::initFirmata(const QString &serialDevice)
{
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

void MazeScript::resetEngine()
{
    if (m_pythread != nullptr) {
        m_pythread->terminate();
        delete m_pythread;
    }
    m_pythread = new PyThread(this);
    connect(m_pythread->maio(), &MaIO::eventSaved, this, &MazeScript::eventReceived, Qt::QueuedConnection);
    connect(m_pythread->maio(), &MaIO::headersSet, this, &MazeScript::headersReceived, Qt::QueuedConnection);
    connect(m_pythread, &PyThread::errorReceived, this, [=](const QString& message) {
        emit evalError(message);
    });
}

void MazeScript::run()
{
    if (m_running) {
        qWarning() << "Can not start an already active MazeScript.";
        return;
    }

    // prepare event log file
    if (!m_eventFileName.isEmpty()) {
        m_eventFile->setFileName(m_eventFileName);
        if (!m_eventFile->open(QFile::WriteOnly | QFile::Truncate)) {
            emit evalError("Unable to open events file");
            return;
        }
    }

    // ensure the Python interface can actually interface with the Firmata serial device
    m_pythread->setFirmata(m_firmata);

    qDebug() << "Evaluating Maze Script";
    m_pythread->setScriptContent(m_script);

    // we don't have any events yet
    m_haveEvents = false;
    
    // start timer for event log
    m_timer.start();

    // run script
    m_running = true;
    m_pythread->runScript();
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

    m_pythread->quit();
    m_eventFile->close();
    resetEngine();

    m_running = false;
    emit finished();
}

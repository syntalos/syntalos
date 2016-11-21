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

#include "mazeio.h"

#include <QMessageBox>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QTime>
#include <QScriptEngine>
#include <QCoreApplication>

#include "firmata/firmata.h"
#include "firmata/pins/digitalpin.h"

MazeIO::MazeIO(Firmata *firmata, QObject *parent)
    : QObject(parent),
      m_firmata(firmata)
{
}

void MazeIO::newDigitalPin(int pinID, const QString& pinName, bool output)
{
    auto pin = new DigitalPin(this);
    pin->setPin(pinID);
    pin->setOutput(output);
    m_firmata->addPin(pin);
    if (output) {
        // initialize output pin
        pin->setValue(false);
    } else {
        // connect input pin
        connect(pin, &DigitalPin::valueChanged, this, &MazeIO::pinChangeReceived);
    }

    m_namePinMap.insert(pinName, pin);
    m_pinNameMap.insert(pin, pinName);

    if (output)
        qDebug() << "Pin" << pinID << "set as output";
    else
        qDebug() << "Pin" << pinID << "set as input";
}

void MazeIO::newDigitalPin(int pinID, const QString& pinName, const QString& kind)
{
    if (kind == "output")
        newDigitalPin(pinID, pinName, true);
    else if (kind == "input")
        newDigitalPin(pinID, pinName, false);
    else
        qCritical() << "Invalid pin kind:" << kind;
}

void MazeIO::pinSetValue(const QString& pinName, bool value)
{
    auto pin = m_namePinMap.value(pinName);
    if (pin == nullptr) {
        QMessageBox::critical(nullptr, "MazeIO Error",
                              QString("Unable to deliver message to pin '%1' (pin does not exist)").arg(pinName));
        return;
    }
    pin->setValue(value);
}

void MazeIO::pinSignalPulse(const QString& pinName)
{
    pinSetValue(pinName, true);
    this->sleep(2);
    pinSetValue(pinName, false);
}

void MazeIO::pinChangeReceived(bool value)
{
    qDebug() << "Received pin value change.";
    auto sender = QObject::sender();
    if (sender == nullptr)
        return;
    auto pin = dynamic_cast<DigitalPin*>(sender);

    auto pinName = m_pinNameMap.value(pin);
    if (pinName.isEmpty()) {
        qWarning() << "Ignored message from unregistered pin " << pin->pin();
        return;
    }

    emit valueChanged(pinName, value);
}

void MazeIO::saveEvent(const QString &message)
{
    QStringList msgs;
    msgs << message;
    emit eventSaved(msgs);
}

void MazeIO::saveEvent(const QStringList& messages)
{
    emit eventSaved(messages);
}

void MazeIO::sleep(uint msecs)
{
    QEventLoop loop;
    QTimer timer;
    timer.setInterval(msecs);
    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    timer.start();
    loop.exec();
}

void MazeIO::wait(uint msecs)
{
    auto waitTime = QTime::currentTime().addMSecs(msecs);
    auto etime = waitTime.msec();
    etime = etime / 4;
    while (QTime::currentTime() < waitTime)
        QCoreApplication::processEvents(QEventLoop::AllEvents, etime);
}

void MazeIO::setTimeout(QScriptValue fn, int msec)
{
  if (!fn.isFunction()) {
      qWarning() << QString("QScript parameter '%1' is not a function.").arg(fn.toString());
      return;
  }

  QTimer *timer = new QTimer;
  qScriptConnect(timer, SIGNAL(timeout()), QScriptValue(), fn);
  connect(timer, SIGNAL(timeout()), timer, SLOT(deleteLater()));
  timer->setSingleShot(true);
  timer->start(msec);
}

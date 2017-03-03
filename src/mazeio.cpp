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

#include "firmata/serialport.h"

MazeIO::MazeIO(SerialFirmata *firmata, QObject *parent)
    : QObject(parent),
      m_firmata(firmata)
{
    connect(m_firmata, &SerialFirmata::digitalRead, this, &MazeIO::onDigitalRead);
    connect(m_firmata, &SerialFirmata::digitalPinRead, this, &MazeIO::onDigitalPinRead);
}

void MazeIO::newDigitalPin(int pinID, const QString& pinName, bool output)
{
    FmPin pin;
    pin.kind = PinKind::Digital;
    pin.id = pinID;
    pin.output = output;

    if (pin.output) {
        // initialize output pin
        m_firmata->setPinMode(pinID, IoMode::Output);
        m_firmata->writeDigitalPin(pinID, false);
        qDebug() << "Pin" << pinID << "set as output";
    } else {
        // connect input pin
        m_firmata->setPinMode(pinID, IoMode::Input);

        auto port = pin.id >> 3;
        m_firmata->reportDigitalPort(port, true);

        qDebug() << "Pin" << pinID << "set as input";
    }

    m_namePinMap.insert(pinName, pin);
    m_pinNameMap.insert(pin.id, pinName);
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
    if (pin.kind == PinKind::Unknown) {
        QMessageBox::critical(nullptr, "MazeIO Error",
                              QString("Unable to deliver message to pin '%1' (pin does not exist)").arg(pinName));
        return;
    }
    m_firmata->writeDigitalPin(pin.id, value);
}

void MazeIO::pinSignalPulse(const QString& pinName)
{
    pinSetValue(pinName, true);
    this->sleep(2);
    pinSetValue(pinName, false);
}

void MazeIO::onDigitalRead(uint8_t port, uint8_t value)
{
    qDebug() << "Firmata digital port read:" << int(value);
    auto sender = QObject::sender();
    if (sender == nullptr)
        return;

    // value of a digital port changed: 8 possible pin changes
    const int first = port * 8;
    const int last = first + 7;

    for (const FmPin p : m_namePinMap.values()) {
        if ((!p.output) && (p.kind != PinKind::Unknown)) {
            if ((p.id >= first) && (p.id <= last)) {
                bool boolVal = value & (1 << (p.id - first));
                auto pinName = m_pinNameMap.value(p.id);
                emit valueChanged(pinName, boolVal);
            }
        }
    }
}

void MazeIO::onDigitalPinRead(uint8_t pin, bool value)
{
    qDebug("Firmata digital pin read: %d=%d", pin, value);
    auto sender = QObject::sender();
    if (sender == nullptr)
        return;

    auto pinName = m_pinNameMap.value(pin);
    if (pinName.isEmpty()) {
        qWarning() << "Received state change for unknown pin:" << pin;
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

void MazeIO::setEventsHeader(const QStringList &headers)
{
    emit headersSet(headers);
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

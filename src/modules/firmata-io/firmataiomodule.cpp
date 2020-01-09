/**
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

#include "firmataiomodule.h"

#include <QDebug>
#include <QThread>
#include <QMessageBox>
#include "firmatasettingsdialog.h"
#include "firmata/serialport.h"

QString FirmataIOModuleInfo::id() const
{
    return QStringLiteral("firmata-io");
}

QString FirmataIOModuleInfo::name() const
{
    return QStringLiteral("Firmata I/O");
}

QString FirmataIOModuleInfo::description() const
{
    return QStringLiteral("Control input/output of a devive (i.e. an Arduino) via the Firmata protocol.");
}

QString FirmataIOModuleInfo::license() const
{
    return QStringLiteral("Qt Firmata implementation (c) 2016 Calle Laakkonen [GPLv3+]");
}

QPixmap FirmataIOModuleInfo::pixmap() const
{
    return QPixmap(":/module/firmata-io");
}

AbstractModule *FirmataIOModuleInfo::createModule(QObject *parent)
{
    return new FirmataIOModule(parent);
}

FirmataIOModule::FirmataIOModule(QObject *parent)
    : AbstractModule(parent),
      m_settingsDialog(nullptr)
{
    m_firmata = new SerialFirmata(this);
    addSettingsWindow(new FirmataSettingsDialog);
    m_settingsDialog->setWindowTitle(QStringLiteral("%1 - Settings").arg(name()));
}

FirmataIOModule::~FirmataIOModule()
{
    if (m_settingsDialog != nullptr)
        delete m_settingsDialog;
}

ModuleFeatures FirmataIOModule::features() const
{
    return ModuleFeature::SHOW_SETTINGS;
}

bool FirmataIOModule::prepare(const QString &storageRootDir, const TestSubject &testSubject)
{
    Q_UNUSED(storageRootDir)
    Q_UNUSED(testSubject)
    setState(ModuleState::PREPARING);

    // cleanup
    m_changedValuesQueue.clear();
    m_namePinMap.clear();
    m_pinNameMap.clear();

    delete m_firmata;
    m_firmata = new SerialFirmata(this);
    connect(m_firmata, &SerialFirmata::digitalRead, this, &FirmataIOModule::recvDigitalRead);
    connect(m_firmata, &SerialFirmata::digitalPinRead, this, &FirmataIOModule::recvDigitalPinRead);

    auto serialDevice = m_settingsDialog->serialPort();
    if (serialDevice.isEmpty()) {
        raiseError("Unable to find a Firmata serial device for programmable I/O to connect to. Can not continue.");
        return false;
    }

    qDebug() << "Loading Firmata interface" << serialDevice;
    if (m_firmata->device().isEmpty()) {
        if (!m_firmata->setDevice(serialDevice)) {
            raiseError(QStringLiteral("Unable to open serial interface: %1").arg(m_firmata->statusText()));
            return false;
        }
    }

    if (!m_firmata->waitForReady(20000) || m_firmata->statusText().contains("Error")) {
        QString msg;
        if (m_firmata->statusText().contains("Error"))
            msg = m_firmata->statusText();
        else
            msg = QStringLiteral("Does the selected serial device use the Firmata protocol?");

        raiseError(QStringLiteral("Unable to initialize Firmata: %1").arg(msg));
        m_firmata->setDevice(QString());
        return false;
    }

    setState(ModuleState::READY);
    return true;
}

void FirmataIOModule::stop()
{
    setState(ModuleState::IDLE);
}

QByteArray FirmataIOModule::serializeSettings(const QString &confBaseDir)
{
    Q_UNUSED(confBaseDir);
    QJsonObject jsettings;
    jsettings.insert("serialPort", m_settingsDialog->serialPort());

    return jsonObjectToBytes(jsettings);
}

bool FirmataIOModule::loadSettings(const QString &confBaseDir, const QByteArray &data)
{
    Q_UNUSED(confBaseDir);
    auto jsettings = jsonObjectFromBytes(data);
    m_settingsDialog->setSerialPort(jsettings.value("serialPort").toString());

    return true;
}

bool FirmataIOModule::fetchDigitalInput(QPair<QString, bool> *result)
{
    if (result == nullptr)
        return false;

    if (m_changedValuesQueue.empty())
        return false;

    m_mutex.lock();
    *result = m_changedValuesQueue.dequeue();
    m_mutex.unlock();

    return true;
}

void FirmataIOModule::newDigitalPin(int pinID, const QString &pinName, bool output, bool pullUp)
{
    FmPin pin;
    pin.kind = PinKind::Digital;
    pin.id = static_cast<uint8_t>(pinID);
    pin.output = output;

    if (pin.output) {
        // initialize output pin
        m_firmata->setPinMode(pin.id, IoMode::Output);
        m_firmata->writeDigitalPin(pin.id, false);
        qDebug() << "Pin" << pinID << "set as output";
    } else {
        // connect input pin
        if (pullUp)
            m_firmata->setPinMode(pin.id, IoMode::PullUp);
        else
            m_firmata->setPinMode(pin.id, IoMode::Input);

        uint8_t port = pin.id >> 3;
        m_firmata->reportDigitalPort(port, true);

        qDebug() << "Pin" << pinID << "set as input";
    }

    m_namePinMap.insert(pinName, pin);
    m_pinNameMap.insert(pin.id, pinName);
}

void FirmataIOModule::pinSetValue(const QString &pinName, bool value)
{
    auto pin = m_namePinMap.value(pinName);
    if (pin.kind == PinKind::Unknown) {
        qCritical() << QStringLiteral("Unable to deliver message to pin '%1' (pin does not exist, it needs to be registered first)").arg(pinName);
        return;
    }
    m_firmata->writeDigitalPin(pin.id, value);
}

void FirmataIOModule::pinSignalPulse(const QString &pinName)
{
    pinSetValue(pinName, true);
    QThread::usleep(50 * 1000); // sleep 50msec
    pinSetValue(pinName, false);
}

void FirmataIOModule::recvDigitalRead(uint8_t port, uint8_t value)
{
    qDebug() << "Firmata digital port read:" << int(value);

    // value of a digital port changed: 8 possible pin changes
    const int first = port * 8;
    const int last = first + 7;

    for (const FmPin p : m_namePinMap.values()) {
        if ((!p.output) && (p.kind != PinKind::Unknown)) {
            if ((p.id >= first) && (p.id <= last)) {
                QPair<QString, bool> pair;
                pair.first = m_pinNameMap.value(p.id);
                pair.second = value & (1 << (p.id - first));

                m_mutex.lock();
                m_changedValuesQueue.append(pair);
                m_mutex.unlock();
            }
        }
    }
}

void FirmataIOModule::recvDigitalPinRead(uint8_t pin, bool value)
{
    qDebug("Firmata digital pin read: %d=%d", pin, value);

    auto pinName = m_pinNameMap.value(pin);
    if (pinName.isEmpty()) {
        qWarning() << "Received state change for unknown pin:" << pin;
        return;
    }

    m_mutex.lock();
    m_changedValuesQueue.append(qMakePair(pinName, value));
    m_mutex.unlock();
}

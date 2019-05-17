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
#include <QMessageBox>
#include "firmatasettingsdialog.h"
#include "firmata/serialport.h"

FirmataIOModule::FirmataIOModule(QObject *parent)
    : AbstractModule(parent),
      m_settingsDialog(nullptr)
{
    m_name = QStringLiteral("Firmata I/O");
    m_firmata = new SerialFirmata(this);
}

FirmataIOModule::~FirmataIOModule()
{
    if (m_settingsDialog != nullptr)
        delete m_settingsDialog;
}

QString FirmataIOModule::id() const
{
    return QStringLiteral("firmata-io");
}

QString FirmataIOModule::description() const
{
    return QStringLiteral("Control input/output of a devive (i.e. an Arduino) via the Firmata protocol.");
}

QPixmap FirmataIOModule::pixmap() const
{
    return QPixmap(":/module/firmata-io");
}

ModuleFeatures FirmataIOModule::features() const
{
    return ModuleFeature::SETTINGS;
}

bool FirmataIOModule::initialize(ModuleManager *manager)
{
    Q_UNUSED(manager);
    assert(!initialized());
    setState(ModuleState::INITIALIZING);

    m_settingsDialog = new FirmataSettingsDialog;

    setState(ModuleState::READY);
    setInitialized();
    return true;
}

bool FirmataIOModule::prepare(const QString &storageRootDir, const TestSubject &testSubject, HRTimer *timer)
{
    Q_UNUSED(storageRootDir);
    Q_UNUSED(testSubject);
    Q_UNUSED(timer);
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
        raiseError("Unable to find a Firmata serial device to connect to. Can not continue.");
        return false;
    }

    qDebug() << "Loading Firmata interface" << serialDevice;
    if (m_firmata->device().isEmpty()) {
        if (!m_firmata->setDevice(serialDevice)) {
            raiseError(QStringLiteral("Firmata initialization error: %1").arg(m_firmata->statusText()));
            return false;
        }
    }

    if (!m_firmata->waitForReady(20000) || m_firmata->statusText().contains("Error")) {
        raiseError(QStringLiteral("Unable to open serial interface: %1").arg(m_firmata->statusText()));
        m_firmata->setDevice(QString());
        return false;
    }

    setState(ModuleState::WAITING);
    return true;
}

void FirmataIOModule::stop()
{

}

void FirmataIOModule::showSettingsUi()
{
    assert(initialized());
    m_settingsDialog->show();
}

void FirmataIOModule::hideSettingsUi()
{
    assert(initialized());
    m_settingsDialog->hide();
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

                m_changedValuesQueue.append(pair);
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

    m_changedValuesQueue.append(qMakePair(pinName, value));
}

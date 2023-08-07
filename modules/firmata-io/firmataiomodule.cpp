/**
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include "firmata/serialport.h"
#include "firmatasettingsdialog.h"
#include <QDebug>
#include <QThread>
#include <QTimer>

#include "utils/misc.h"

SYNTALOS_MODULE(FirmataIOModule)

namespace Syntalos
{
Q_LOGGING_CATEGORY(logFmMod, "mod.firmata")
}

enum class PinKind {
    Unknown,
    Digital,
    Analog
};

struct FmPin {
    PinKind kind;
    bool output;
    uint8_t id;
};

class FirmataIOModule : public AbstractModule
{
    Q_OBJECT

private:
    FirmataSettingsDialog *m_settingsDialog;
    SerialFirmata *m_firmata;

    QHash<QString, FmPin> m_namePinMap;
    QHash<int, QString> m_pinNameMap;

    QTimer *m_evTimer;
    std::shared_ptr<StreamInputPort<FirmataControl>> m_inFmCtl;
    std::shared_ptr<DataStream<FirmataData>> m_fmStream;
    std::shared_ptr<StreamSubscription<FirmataControl>> m_fmCtlSub;

public:
    explicit FirmataIOModule(QObject *parent = nullptr)
        : AbstractModule(parent),
          m_settingsDialog(nullptr)
    {
        m_firmata = new SerialFirmata(this);
        m_settingsDialog = new FirmataSettingsDialog;
        addSettingsWindow(m_settingsDialog);
        m_settingsDialog->setWindowTitle(QStringLiteral("%1 - Settings").arg(name()));

        m_evTimer = new QTimer(this);
        m_evTimer->setInterval(0);
        connect(m_evTimer, &QTimer::timeout, this, &FirmataIOModule::checkControlCommands);

        m_inFmCtl = registerInputPort<FirmataControl>(QStringLiteral("fmctl"), QStringLiteral("Firmata Control"));
        m_fmStream = registerOutputPort<FirmataData>(QStringLiteral("fmdata"), QStringLiteral("Firmata Data"));
    }

    ~FirmataIOModule() override {}

    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_SETTINGS;
    }

    bool prepare(const TestSubject &) override
    {
        // cleanup
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

        // start event stream and see if we should listen to control commands
        m_fmStream->start();
        m_fmCtlSub.reset();
        if (m_inFmCtl->hasSubscription())
            m_fmCtlSub = m_inFmCtl->subscription();

        return true;
    }

    void start() override
    {
        if (m_fmCtlSub.get() != nullptr)
            m_evTimer->start();
    }

    void stop() override
    {
        m_evTimer->stop();
    }

    void checkControlCommands()
    {
        const auto maybeCtl = m_fmCtlSub->peekNext();
        if (!maybeCtl.has_value())
            return;
        const auto ctl = maybeCtl.value();
        switch (ctl.command) {
        case FirmataCommandKind::NEW_DIG_PIN:
            newDigitalPin(ctl.pinId, ctl.pinName, ctl.isOutput, ctl.isPullUp);
            if (ctl.pinName.isEmpty())
                pinSetValue(ctl.pinId, ctl.value);
            else
                pinSetValue(ctl.pinName, ctl.value);
            break;
        case FirmataCommandKind::WRITE_DIGITAL:
            if (ctl.pinName.isEmpty())
                pinSetValue(ctl.pinId, ctl.value);
            else
                pinSetValue(ctl.pinName, ctl.value);
            break;
        case FirmataCommandKind::WRITE_DIGITAL_PULSE:
            if (ctl.pinName.isEmpty())
                pinSignalPulse(ctl.pinId, ctl.value);
            else
                pinSignalPulse(ctl.pinName, ctl.value);
            break;
        default:
            qCWarning(logFmMod) << "Received not-implemented Firmata instruction of type"
                                << QString::number(static_cast<int>(ctl.command));
            break;
        }
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert("serial_port", m_settingsDialog->serialPort());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_settingsDialog->setSerialPort(settings.value("serial_port").toString());
        return true;
    }

    void newDigitalPin(int pinId, const QString &pinName, bool output, bool pullUp)
    {
        FmPin pin;
        pin.kind = PinKind::Digital;
        pin.id = static_cast<uint8_t>(pinId);
        pin.output = output;

        if (pin.output) {
            // initialize output pin
            m_firmata->setPinMode(pin.id, IoMode::Output);
            m_firmata->writeDigitalPin(pin.id, false);
            qCDebug(logFmMod) << "Firmata: Pin" << pinId << "set as output";
        } else {
            // connect input pin
            if (pullUp)
                m_firmata->setPinMode(pin.id, IoMode::PullUp);
            else
                m_firmata->setPinMode(pin.id, IoMode::Input);

            uint8_t port = pin.id >> 3;
            m_firmata->reportDigitalPort(port, true);

            qCDebug(logFmMod) << "Firmata: Pin" << pinId << "set as input";
        }

        auto pname = pinName;
        if (pname.isEmpty())
            pname = QStringLiteral("pin-%1").arg(pinId);

        m_namePinMap.insert(pname, pin);
        m_pinNameMap.insert(pin.id, pname);
    }

    FmPin findPin(const QString &pinName)
    {
        auto pin = m_namePinMap.value(pinName);
        if (pin.kind == PinKind::Unknown)
            qCCritical(logFmMod)
                << QStringLiteral(
                       "Unable to deliver message to pin '%1' (pin does not exist, it needs to be registered first)")
                       .arg(pinName);
        return pin;
    }

    void pinSetValue(int pinId, bool value)
    {
        m_firmata->writeDigitalPin(pinId, value);
    }

    void pinSetValue(const QString &pinName, bool value)
    {
        auto pin = findPin(pinName);
        if (pin.kind == PinKind::Unknown)
            return;
        pinSetValue(pin.id, value);
    }

    void pinSignalPulse(int pinId, int pulseDuration = 0)
    {
        if (pulseDuration <= 0)
            pulseDuration = 50; // 50 msec is our default pulse length
        else if (pulseDuration > 4000)
            pulseDuration = 4000; // clamp pulse length at 4 sec max
        pinSetValue(pinId, true);
        delay(pulseDuration);
        pinSetValue(pinId, false);
    }

    void pinSignalPulse(const QString &pinName, int pulseDuration = 0)
    {
        auto pin = findPin(pinName);
        if (pin.kind == PinKind::Unknown)
            return;
        pinSignalPulse(pin.id, pulseDuration);
    }

private slots:
    void recvDigitalRead(uint8_t port, uint8_t value)
    {
        // value of a digital port changed: 8 possible pin changes
        const int first = port * 8;
        const int last = first + 7;
        const auto timestamp = m_syTimer->timeSinceStartMsec();

        qCDebug(logFmMod, "Digital port read: %d (%d - %d)", value, first, last);
        for (const FmPin p : m_namePinMap.values()) {
            if ((!p.output) && (p.kind != PinKind::Unknown)) {
                if ((p.id >= first) && (p.id <= last)) {
                    FirmataData fdata;
                    fdata.time = timestamp;
                    fdata.isDigital = true;
                    fdata.pinId = p.id;
                    fdata.pinName = m_pinNameMap.value(p.id);
                    fdata.value = (value & (1 << (p.id - first))) ? 1 : 0;

                    m_fmStream->push(fdata);
                }
            }
        }
    }

    void recvDigitalPinRead(uint8_t pin, bool value)
    {
        FirmataData fdata;
        fdata.time = m_syTimer->timeSinceStartMsec();
        fdata.isDigital = true;
        fdata.pinId = pin;
        fdata.pinName = m_pinNameMap.value(pin);
        fdata.value = value;
        if (fdata.pinName.isEmpty()) {
            qWarning() << "Received state change for unknown pin:" << pin;
            return;
        }

        qCDebug(logFmMod, "Digital pin read: %d=%d", pin, value);
        m_fmStream->push(fdata);
    }
};

QString FirmataIOModuleInfo::id() const
{
    return QStringLiteral("firmata-io");
}

QString FirmataIOModuleInfo::name() const
{
    return QStringLiteral("Firmata IO");
}

QString FirmataIOModuleInfo::description() const
{
    return QStringLiteral("Control input/output of a serial device (i.e. an Arduino) via the Firmata protocol.");
}

QString FirmataIOModuleInfo::license() const
{
    return QStringLiteral(
        "Module licensed under GPL-3.0+, uses the Qt Firmata implementation © 2016 Calle Laakkonen [GPL-3.0+]");
}

AbstractModule *FirmataIOModuleInfo::createModule(QObject *parent)
{
    return new FirmataIOModule(parent);
}

#include "firmataiomodule.moc"

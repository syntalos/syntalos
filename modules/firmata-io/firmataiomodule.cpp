/**
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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
#include <QEventLoop>

#include "streams/subscriptionnotifier.h"
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
    std::atomic_bool m_stopped;

    QHash<QString, FmPin> m_namePinMap;
    QHash<int, QString> m_pinNameMap;

    std::shared_ptr<StreamInputPort<FirmataControl>> m_inFmCtl;
    std::shared_ptr<DataStream<FirmataData>> m_fmStream;
    std::shared_ptr<StreamSubscription<FirmataControl>> m_fmCtlSub;

public:
    explicit FirmataIOModule(QObject *parent = nullptr)
        : AbstractModule(parent),
          m_settingsDialog(nullptr),
          m_stopped(true)
    {
        m_settingsDialog = new FirmataSettingsDialog;
        addSettingsWindow(m_settingsDialog);
        m_settingsDialog->setWindowTitle(QStringLiteral("%1 - Settings").arg(name()));

        m_inFmCtl = registerInputPort<FirmataControl>(QStringLiteral("fmctl"), QStringLiteral("Firmata Control"));
        m_fmStream = registerOutputPort<FirmataData>(QStringLiteral("fmdata"), QStringLiteral("Firmata Data"));
    }

    ~FirmataIOModule() override {}

    ModuleFeatures features() const final
    {
        return ModuleFeature::SHOW_SETTINGS;
    }

    ModuleDriverKind driver() const final
    {
        return ModuleDriverKind::THREAD_DEDICATED;
    }

    void usbHotplugEvent(UsbHotplugEventKind) override
    {
        if (m_running)
            return;
        m_settingsDialog->updatePortList();
    }

    bool prepare(const TestSubject &) final
    {
        // cleanup
        m_namePinMap.clear();
        m_pinNameMap.clear();

        // start event stream and see if we should listen to control commands
        m_fmStream->start();
        m_fmCtlSub.reset();
        if (m_inFmCtl->hasSubscription())
            m_fmCtlSub = m_inFmCtl->subscription();

        return true;
    }

    void start() final
    {
        AbstractModule::start();
    }

    void runThread(OptionalWaitCondition *waitCondition) final
    {
        // event loop for this thread
        QEventLoop loop;

        // setup Firmata serial connection to the device
        auto firmata = std::make_unique<SerialFirmata>(nullptr);

        auto serialDevice = m_settingsDialog->serialPort();
        if (serialDevice.isEmpty()) {
            raiseError("Unable to find a Firmata serial device for programmable I/O to connect to. Can not continue.");
            return;
        }

        qCDebug(logFmMod) << "Loading Firmata interface" << serialDevice;
        if (firmata->device().isEmpty()) {
            if (!firmata->setDevice(serialDevice)) {
                raiseError(QStringLiteral("Unable to open serial interface: %1").arg(firmata->statusText()));
                return;
            }
        }

        // check if we can communicate with the Firmata serial device
        firmata->reportProtocolVersion();
        for (uint i = 0; i < 1000; i++) {
            firmata->readAndParseData(10);
            if (firmata->isReady())
                break;
        }
        if (!firmata->isReady() || firmata->statusText().contains("Error")) {
            QString msg;
            if (firmata->statusText().contains("Error"))
                msg = firmata->statusText();
            else
                msg = QStringLiteral("Does the selected serial device use the Firmata protocol?");

            raiseError(QStringLiteral("Unable to initialize Firmata: %1").arg(msg));
            return;
        }

        // connect to data received events
        connect(
            firmata.get(), &SerialFirmata::digitalRead, this, &FirmataIOModule::recvDigitalRead, Qt::DirectConnection);
        connect(
            firmata.get(),
            &SerialFirmata::digitalPinRead,
            this,
            &FirmataIOModule::recvDigitalPinRead,
            Qt::DirectConnection);

        // trigger if we have new input data
        std::unique_ptr<SubscriptionNotifier> notifier;
        if (m_fmCtlSub) {
            notifier = std::make_unique<SubscriptionNotifier>(m_fmCtlSub);
            connect(notifier.get(), &SubscriptionNotifier::dataReceived, [&loop, &firmata, this]() {
                for (uint i = 0; i < 4; i++) {
                    if (!checkFirmataControlCmdReceived(firmata.get()))
                        break;
                }

                // quit the loop if we stopped running
                if (!m_running)
                    loop.quit();
            });
        }

        // periodically check if we have to quit
        QTimer quitTimer;
        quitTimer.setInterval(200);
        quitTimer.setSingleShot(false);
        quitTimer.start();
        connect(&quitTimer, &QTimer::timeout, [&loop, this]() {
            if (!m_running)
                loop.quit();
        });

        // wait until we actually start acquiring data
        m_stopped = false;
        waitCondition->wait(this);

        // run our internal event loop
        loop.exec();

        m_fmCtlSub->disableNotify();
        m_stopped = true;
    }

    bool checkFirmataControlCmdReceived(SerialFirmata *firmata)
    {
        const auto maybeCtl = m_fmCtlSub->peekNext();
        if (!maybeCtl.has_value())
            return false;
        const auto ctl = maybeCtl.value();
        switch (ctl.command) {
        case FirmataCommandKind::NEW_DIG_PIN:
            newDigitalPin(firmata, ctl.pinId, ctl.pinName, ctl.isOutput, ctl.isPullUp);
            if (ctl.pinName.isEmpty())
                pinSetValue(firmata, ctl.pinId, ctl.value);
            else
                pinSetValue(firmata, ctl.pinName, ctl.value);
            break;
        case FirmataCommandKind::WRITE_DIGITAL:
            if (ctl.pinName.isEmpty())
                pinSetValue(firmata, ctl.pinId, ctl.value);
            else
                pinSetValue(firmata, ctl.pinName, ctl.value);
            break;
        case FirmataCommandKind::WRITE_DIGITAL_PULSE:
            if (ctl.pinName.isEmpty())
                pinSignalPulse(firmata, ctl.pinId, ctl.value);
            else
                pinSignalPulse(firmata, ctl.pinName, ctl.value);
            break;
        default:
            qCWarning(logFmMod) << "Received not-implemented Firmata instruction of type"
                                << QString::number(static_cast<int>(ctl.command));
            break;
        }

        return true;
    }

    void newDigitalPin(SerialFirmata *firmata, int pinId, const QString &pinName, bool output, bool pullUp)
    {
        FmPin pin;
        pin.kind = PinKind::Digital;
        pin.id = static_cast<uint8_t>(pinId);
        pin.output = output;

        if (pin.output) {
            // initialize output pin
            firmata->setPinMode(pin.id, IoMode::Output);
            firmata->writeDigitalPin(pin.id, false);
            qCDebug(logFmMod) << "Firmata: Pin" << pinId << "set as output";
        } else {
            // connect input pin
            if (pullUp)
                firmata->setPinMode(pin.id, IoMode::PullUp);
            else
                firmata->setPinMode(pin.id, IoMode::Input);

            uint8_t port = pin.id >> 3;
            firmata->reportDigitalPort(port, true);

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

    void pinSetValue(SerialFirmata *firmata, int pinId, bool value)
    {
        firmata->writeDigitalPin(pinId, value);
    }

    void pinSetValue(SerialFirmata *firmata, const QString &pinName, bool value)
    {
        auto pin = findPin(pinName);
        if (pin.kind == PinKind::Unknown)
            return;
        pinSetValue(firmata, pin.id, value);
    }

    void pinSignalPulse(SerialFirmata *firmata, int pinId, int pulseDuration = 0)
    {
        if (pulseDuration <= 0)
            pulseDuration = 50; // 50 msec is our default pulse length
        else if (pulseDuration > 4000)
            pulseDuration = 4000; // clamp pulse length at 4 sec max
        pinSetValue(firmata, pinId, true);
        delay(pulseDuration);
        pinSetValue(firmata, pinId, false);
    }

    void pinSignalPulse(SerialFirmata *firmata, const QString &pinName, int pulseDuration = 0)
    {
        auto pin = findPin(pinName);
        if (pin.kind == PinKind::Unknown)
            return;
        pinSignalPulse(firmata, pin.id, pulseDuration);
    }

    void stop() final
    {
        AbstractModule::stop();

        // wait for our thread to finish
        while (!m_stopped)
            appProcessEvents();
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) final
    {
        settings.insert("serial_port", m_settingsDialog->serialPort());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) final
    {
        m_settingsDialog->setSerialPort(settings.value("serial_port").toString());
        return true;
    }

    void recvDigitalRead(uint8_t port, uint8_t value)
    {
        // WARNING: This method is called from a different thread!

        // value of a digital port changed: 8 possible pin changes
        const int first = port * 8;
        const int last = first + 7;
        const auto timestamp = m_syTimer->timeSinceStartMsec();

        qCDebug(logFmMod, "Digital port read: %d (%d - %d)", value, first, last);
        for (const FmPin &p : m_namePinMap.values()) {
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
        // WARNING: This method is called from a different thread!

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

ModuleCategories FirmataIOModuleInfo::categories() const
{
    return ModuleCategory::DEVICES;
}

AbstractModule *FirmataIOModuleInfo::createModule(QObject *parent)
{
    return new FirmataIOModule(parent);
}

#include "firmataiomodule.moc"

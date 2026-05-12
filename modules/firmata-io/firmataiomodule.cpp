/**
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include <QThread>
#include <QTimer>
#include <QEventLoop>

#include "streams/subscriptionnotifier.h"
#include "utils/misc.h"

#include "firmata/serialport.h"
#include "firmatasettingsdialog.h"

SYNTALOS_MODULE(FirmataIOModule)

enum class PinKind {
    Unknown,
    Digital,
    Analog
};

struct FmPin {
    PinKind kind{PinKind::Unknown};
    bool output{false};
    uint8_t id{0};
};

class FirmataIOModule : public AbstractModule
{
    Q_OBJECT

private:
    FirmataSettingsDialog *m_settingsDialog;
    std::atomic_bool m_stopped;

    QHash<int, FmPin> m_pinMap;

    std::shared_ptr<StreamInputPort<LineCommand>> m_inLineCtl;
    std::shared_ptr<DataStream<LineReading>> m_outStream;
    std::shared_ptr<StreamSubscription<LineCommand>> m_lineCtlSub;

public:
    explicit FirmataIOModule(QObject *parent = nullptr)
        : AbstractModule(parent),
          m_settingsDialog(nullptr),
          m_stopped(true)
    {
        m_settingsDialog = new FirmataSettingsDialog;
        addSettingsWindow(m_settingsDialog);
        m_settingsDialog->setWindowTitle(QStringLiteral("%1 - Settings").arg(name()));

        m_inLineCtl = registerInputPort<LineCommand>(QStringLiteral("fmctl"), QStringLiteral("Line Control"));
        m_outStream = registerOutputPort<LineReading>(QStringLiteral("fmdata"), QStringLiteral("Line Readings"));
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
        m_pinMap.clear();

        // start event stream and see if we should listen to control commands
        m_outStream->start();
        m_lineCtlSub.reset();
        if (m_inLineCtl->hasSubscription())
            m_lineCtlSub = m_inLineCtl->subscription();

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
        firmata->setLogger(m_log);

        auto serialDevice = m_settingsDialog->serialPort();
        if (serialDevice.isEmpty()) {
            raiseError("Unable to find a Firmata serial device for programmable I/O to connect to. Can not continue.");
            return;
        }

        LOG_INFO(m_log, "Loading Firmata interface: {}", serialDevice);
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
        if (m_lineCtlSub) {
            notifier = std::make_unique<SubscriptionNotifier>(m_lineCtlSub);
            connect(notifier.get(), &SubscriptionNotifier::dataReceived, [&loop, &firmata, this]() {
                for (uint i = 0; i < 4; i++) {
                    if (!checkLineCommandReceived(firmata.get()))
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

        m_lineCtlSub->disableNotify();
        m_stopped = true;
    }

    bool checkLineCommandReceived(SerialFirmata *firmata)
    {
        const auto maybeCtl = m_lineCtlSub->peekNext();
        if (!maybeCtl.has_value())
            return false;
        const auto &ctl = maybeCtl.value();
        switch (ctl.kind) {
        case LineCommandKind::SetMode:
            configureDigitalPin(firmata, ctl.lineId, ctl.flags);
            break;
        case LineCommandKind::WriteDigital:
            pinSetValue(firmata, ctl.lineId, ctl.value != 0);
            break;
        case LineCommandKind::WriteDigitalPulse: {
            const auto durMs = std::chrono::duration_cast<milliseconds_t>(ctl.duration).count();
            pinSignalPulse(firmata, ctl.lineId, static_cast<int>(durMs));
            break;
        }
        default:
            LOG_WARNING(m_log, "Received unsupported LineCommand kind for Firmata: {}", static_cast<int>(ctl.kind));
            break;
        }

        return true;
    }

    void configureDigitalPin(SerialFirmata *firmata, int lineId, LineModeFlags flags)
    {
        const bool isOutput = hasFlag(flags, LineModeFlags::IsOutput);
        const bool isPullUp = hasFlag(flags, LineModeFlags::PullUp);

        FmPin pin;
        pin.kind = PinKind::Digital;
        pin.id = static_cast<uint8_t>(lineId);
        pin.output = isOutput;

        if (pin.output) {
            // initialize output pin
            firmata->setPinMode(pin.id, IoMode::Output);
            firmata->writeDigitalPin(pin.id, false);
            LOG_INFO(m_log, "Firmata: Pin {} set as output", lineId);
        } else {
            // connect input pin
            if (isPullUp)
                firmata->setPinMode(pin.id, IoMode::PullUp);
            else
                firmata->setPinMode(pin.id, IoMode::Input);

            uint8_t port = pin.id >> 3;
            firmata->reportDigitalPort(port, true);

            LOG_INFO(m_log, "Firmata: Pin {} set as input", lineId);
        }

        m_pinMap.insert(lineId, pin);
    }

    void pinSetValue(SerialFirmata *firmata, int pinId, bool value)
    {
        firmata->writeDigitalPin(pinId, value);
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
        const auto timestamp = m_syTimer->timeSinceStartUsec();

        // value of a digital port changed: 8 possible pin changes
        const int first = port * 8;
        const int last = first + 7;

        LOG_DEBUG(m_log, "Digital port read: {} ({} - {})", value, first, last);
        for (const FmPin &p : m_pinMap.values()) {
            if ((!p.output) && (p.kind != PinKind::Unknown)) {
                if ((p.id >= first) && (p.id <= last)) {
                    LineReading r;
                    r.time = timestamp;
                    r.lineId = p.id;
                    r.value = (value & (1 << (p.id - first))) ? 1 : 0;

                    m_outStream->push(r);
                }
            }
        }
    }

    void recvDigitalPinRead(uint8_t pin, bool value)
    {
        // WARNING: This method is called from a different thread!
        const auto timestamp = m_syTimer->timeSinceStartUsec();

        if (!m_pinMap.contains(pin)) {
            LOG_WARNING(m_log, "Received state change for unregistered pin: {}", pin);
            return;
        }

        LineReading r;
        r.time = timestamp;
        r.lineId = pin;
        r.value = value ? 1 : 0;

        LOG_DEBUG(m_log, "Digital pin read: {}={}", pin, value);
        m_outStream->push(r);
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

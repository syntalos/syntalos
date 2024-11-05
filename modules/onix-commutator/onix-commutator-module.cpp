/*
 * Copyright (C) 2023-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "onix-commutator-module.h"

#include <QDebug>
#include <QThread>
#include <QSerialPort>

#include "onix-commutator-settingsdialog.h"

SYNTALOS_MODULE(ONIXCommutatorModule)

class ONIXCommutatorModule : public AbstractModule
{
    Q_OBJECT

private:
    std::shared_ptr<StreamInputPort<FloatSignalBlock>> m_qIn;
    ONIXCommutatorSettingsDialog *m_settingsDlg;

public:
    explicit ONIXCommutatorModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_qIn = registerInputPort<FloatSignalBlock>(QStringLiteral("quaternion-in"), QStringLiteral("Quaternions"));

        m_settingsDlg = new ONIXCommutatorSettingsDialog;
        addSettingsWindow(m_settingsDlg);
    }

    ~ONIXCommutatorModule() override {}

    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_SETTINGS;
    }

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::THREAD_DEDICATED;
    }

    void usbHotplugEvent(UsbHotplugEventKind) override
    {
        // ensure the currently displayed device info is accurate
        m_settingsDlg->scanDevices();
    }

    bool prepare(const TestSubject &) override
    {
        m_settingsDlg->setRunning(true);

        setStateReady();
        return true;
    }

    bool writeSerialCommand(
        QSerialPort &serial,
        const QByteArray &data,
        bool replyExpected = true,
        bool errorOnTimeout = true)
    {
        serial.write(data + "\n");
        if (!serial.waitForBytesWritten(4 * 1000)) {
            if (!errorOnTimeout)
                return false;
            raiseError(QStringLiteral("Timed out while trying to write data to commutator %1 (%2)")
                           .arg(serial.portName())
                           .arg(serial.errorString()));
            return false;
        }

        if (replyExpected) {
            QByteArray result;
            while (serial.waitForReadyRead(500)) {
                result += serial.readAll();
                if (result.length() > 1024)
                    break;
            }

            if (!result.contains("C:" + data)) {
                raiseError(QStringLiteral(
                               "Command \"%1\" was not acknowledged by the device %2. Please check your connection!")
                               .arg(QString::fromUtf8(data))
                               .arg(serial.portName()));
                return false;
            }
        }

        return true;
    }

    static double computeTurns(double currentAngle, double previousAngle)
    {
        double dAngle = currentAngle - previousAngle;

        // calculate the rotation
        double rotation = -dAngle / (2 * M_PI);

        // adjust the rotation if it's greater than 0.5 (account for wrap-around)
        if (std::abs(rotation) > 0.5)
            rotation = std::abs(rotation) - 1;

        return rotation;
    }

    void runThread(OptionalWaitCondition *waitCondition) override
    {
        // do nothing if we do not have data input
        if (!m_qIn->hasSubscription()) {
            setStateDormant();
            return;
        }

        auto qSub = m_qIn->subscription();

        const auto signalNames = qSub->metadataValue("signal_names", QStringList()).toStringList();
        const auto expectedSignalNames = QStringList() << "qw"
                                                       << "qx"
                                                       << "qy"
                                                       << "qz";
        if (signalNames != expectedSignalNames) {
            raiseError(QStringLiteral("Unexpected signal labels for quaternion input: %1. Expected %2.")
                           .arg(signalNames.join(", "))
                           .arg(expectedSignalNames.join(", ")));
            return;
        }

        // configure serial device
        QSerialPort serial;
        serial.close();
        serial.setBaudRate(QSerialPort::Baud115200);
        serial.setStopBits(QSerialPort::OneStop);
        serial.setPortName(m_settingsDlg->serialPort());

        if (!serial.open(QIODevice::ReadWrite)) {
            raiseError(QStringLiteral("Can't open %1, error code %2").arg(serial.portName()).arg(serial.error()));
            return;
        }

        // configure settings
        statusMessage("Configuring...");

        // configure commutator
        if (!writeSerialCommand(
                serial,
                QStringLiteral("{enable: true, led: %1, speed: %2, accel: %3}")
                    .arg(m_settingsDlg->statusLedEnabled() ? "true" : "false")
                    .arg(m_settingsDlg->speed(), 0, 'f', 3)
                    .arg(m_settingsDlg->acceleration(), 0, 'f', 3)
                    .toUtf8(),
                false))
            return;

        // wait until experiment start, in case we haven't started yet
        waitCondition->wait(this);

        statusMessage("Ready.");

        double prevYaw2Pi = 0;
        while (m_running) {
            auto sblock = qSub->next();
            if (!sblock.has_value())
                continue;

            const auto qw = sblock->data(0, 0);
            const auto qx = sblock->data(0, 1);
            const auto qy = sblock->data(0, 2);
            const auto qz = sblock->data(0, 3);

            double yawEuler = atan2(2 * (qw * qz + qx * qy), 1 - 2 * (qy * qy + qz * qz));
            double yaw2Pi = fmod(yawEuler + 2 * M_PI, 2 * M_PI);

            auto turns = computeTurns(yaw2Pi, prevYaw2Pi);
            if (std::abs(turns) < 0.001)
                continue;

            prevYaw2Pi = yaw2Pi;
            if (!writeSerialCommand(serial, QStringLiteral("{turns: %1}").arg(turns, 0, 'f', 3).toUtf8(), false))
                break;

            // display the change as status - potentially we should rate-limit this display in future?
            statusMessage(QStringLiteral("Turned %1 turns.").arg(turns, 0, 'f', 3));
        }
    }

    void stop() override
    {
        m_settingsDlg->setRunning(false);
        statusMessage("Device stopped.");
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert("device", m_settingsDlg->serialPort());
        settings.insert("status_led_enabled", m_settingsDlg->statusLedEnabled());
        settings.insert("speed", m_settingsDlg->speed());
        settings.insert("acceleration", m_settingsDlg->acceleration());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_settingsDlg->setSerialPort(settings.value("device").toString());
        m_settingsDlg->setStatusLedEnabled(settings.value("status_led_enabled", true).toBool());
        m_settingsDlg->setSpeed(settings.value("speed", 100.0).toDouble());
        m_settingsDlg->setAcceleration(settings.value("acceleration", 200.0).toDouble());

        return true;
    }
};

QString ONIXCommutatorModuleInfo::id() const
{
    return QStringLiteral("onix-commutator");
}

QString ONIXCommutatorModuleInfo::name() const
{
    return QStringLiteral("ONIX Coax Commutator");
}

QString ONIXCommutatorModuleInfo::description() const
{
    return QStringLiteral("Support for the ONIX coaxial commutator, accepts BNO055 quaterions as input.");
}

ModuleCategories ONIXCommutatorModuleInfo::categories() const
{
    return ModuleCategory::DEVICES;
}

AbstractModule *ONIXCommutatorModuleInfo::createModule(QObject *parent)
{
    return new ONIXCommutatorModule(parent);
}

#include "onix-commutator-module.moc"

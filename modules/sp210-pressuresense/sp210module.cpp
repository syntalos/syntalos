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

#include "sp210module.h"

#include <QDebug>
#include <QThread>
#include <QSerialPort>

#include "sp210settingsdialog.h"

SYNTALOS_MODULE(SP210Module)

enum class PinKind {
    Unknown,
    Digital,
    Analog
};

class SP210Module : public AbstractModule
{
    Q_OBJECT

private:
    std::shared_ptr<DataStream<FloatSignalBlock>> m_paStream;
    std::shared_ptr<DataStream<FloatSignalBlock>> m_tempStream;
    SP210SettingsDialog *m_settingsDlg;

    std::unique_ptr<SecondaryClockSynchronizer> m_clockSync;

public:
    explicit SP210Module(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_paStream = registerOutputPort<FloatSignalBlock>(
            QStringLiteral("sensor-data-pressure"), QStringLiteral("Pressure Data"));
        m_tempStream = registerOutputPort<FloatSignalBlock>(
            QStringLiteral("sensor-data-temperature"), QStringLiteral("Temperature Data"));

        m_settingsDlg = new SP210SettingsDialog;
        addSettingsWindow(m_settingsDlg);
    }

    ~SP210Module() override {}

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

        m_paStream->setMetadataValue("signal_names", QStringList() << "Pressure");
        m_paStream->setMetadataValue("time_unit", "milliseconds");
        m_paStream->setMetadataValue("data_unit", "mPa");

        m_tempStream->setMetadataValue("signal_names", QStringList() << "Temperature");
        m_tempStream->setMetadataValue("time_unit", "milliseconds");
        m_tempStream->setMetadataValue("data_unit", "°C");

        m_paStream->start();
        m_tempStream->start();

        // set up clock synchronizer
        m_clockSync = initClockSynchronizer(m_settingsDlg->samplingRate());
        m_clockSync->setStrategies(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD | TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD);

        // start the synchronizer
        if (!m_clockSync->start()) {
            raiseError(QStringLiteral("Unable to set up clock synchronizer!"));
            return false;
        }

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
            raiseError(QStringLiteral("Timed out while trying to write data to device %1 (%2)")
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

    void runThread(OptionalWaitCondition *waitCondition) override
    {
        // do nothing if nobody consumes our data
        if (!m_paStream->hasSubscribers() && m_tempStream->hasSubscribers())
            return;

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
        if (!writeSerialCommand(
                serial,
                QStringLiteral("ZERO_NOISE_SUPPRESSION=%1")
                    .arg(m_settingsDlg->zeroNoiseSuppression() ? "true" : "false")
                    .toUtf8()))
            return;
        if (!writeSerialCommand(serial, QStringLiteral("ZERO_MODE=%1").arg(m_settingsDlg->zeroMode()).toUtf8()))
            return;
        if (!writeSerialCommand(serial, QStringLiteral("RATE=%1").arg(m_settingsDlg->samplingRate()).toUtf8()))
            return;

        const int blockSize = m_settingsDlg->samplingRate() / 10;
        if (blockSize < 2) {
            raiseError(QStringLiteral("Invalid data block size."));
            return;
        }

        // wait until we actually start acquiring data
        waitCondition->wait(this);

        // start measuring
        if (!writeSerialCommand(serial, "START", false))
            return;
        statusMessage("Reading data...");

        FloatSignalBlock paBlock(blockSize, 1);
        FloatSignalBlock cBlock(blockSize, 1);
        int blockSampleIdx = 0;
        while (m_running) {
            if (!serial.waitForReadyRead(10 * 1000)) {
                // try again if we timed out
                continue;
            }

            QByteArray sensorDataRaw;
            auto dataRecvTime = FUNC_DONE_TIMESTAMP(
                m_syTimer->startTime(), sensorDataRaw = serial.readLine().trimmed());
            if (sensorDataRaw.isEmpty() || !sensorDataRaw.startsWith("D:"))
                continue;

            const auto parts = sensorDataRaw.mid(2).split(';');
            if (parts.length() != 3)
                continue;

            // timestap by the device
            const auto deviceTimestamp = microseconds_t(parts[0].toULong() * 1000);

            // convert Millikelvin to °C
            double temperatureC = (parts[1].toUInt() / 1000.0) - 273.15;

            // convert pressure from µPa to mPa
            double pressureMilliPa = parts[2].toLong() / 1000.0;

            // adjust the received time if necessary, gather clock sync information
            m_clockSync->processTimestamp(dataRecvTime, deviceTimestamp);
            const uint dpTimestampMs = dataRecvTime.count() / 1000;

            // write data to block
            cBlock.data(blockSampleIdx, 0) = temperatureC;
            cBlock.timestamps(blockSampleIdx, 0) = dpTimestampMs;
            paBlock.data(blockSampleIdx, 0) = pressureMilliPa;
            paBlock.timestamps(blockSampleIdx, 0) = dpTimestampMs;

            blockSampleIdx++;
            if (blockSampleIdx >= blockSize) {
                blockSampleIdx = 0;

                // submit data
                m_paStream->push(paBlock);
                m_tempStream->push(cBlock);
            }
        }

        // stop measuring
        writeSerialCommand(serial, "STOP", false, false);

        // clear remaining output from the serial buffer
        while (serial.waitForReadyRead(500))
            serial.readAll();
    }

    void stop() override
    {
        safeStopSynchronizer(m_clockSync);
        m_settingsDlg->setRunning(false);
        statusMessage("Device stopped.");
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert("device", m_settingsDlg->serialPort());
        settings.insert("zero_mode", m_settingsDlg->zeroMode());
        settings.insert("zero_noise_suppression", m_settingsDlg->zeroNoiseSuppression());
        settings.insert("sampling_rate", m_settingsDlg->samplingRate());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_settingsDlg->setSerialPort(settings.value("device").toString());
        m_settingsDlg->setZeroMode(settings.value("zero_mode").toString());
        m_settingsDlg->setZeroNoiseSuppression(settings.value("zero_noise_suppression").toBool());
        m_settingsDlg->setSamplingRate(settings.value("sampling_rate").toInt());

        return true;
    }
};

QString SP210ModuleInfo::id() const
{
    return QStringLiteral("sp210-pressuresense");
}

QString SP210ModuleInfo::name() const
{
    return QStringLiteral("Pico Pi SP210 Pressure Sensor");
}

QString SP210ModuleInfo::description() const
{
    return QStringLiteral(
        "Support for the Superior Sensor SP210 differential pressure sensor driven by a Raspberry Pi Pico.");
}

AbstractModule *SP210ModuleInfo::createModule(QObject *parent)
{
    return new SP210Module(parent);
}

#include "sp210module.moc"

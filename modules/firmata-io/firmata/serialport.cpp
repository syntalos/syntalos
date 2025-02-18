// QFirmata - a Firmata library for QML
//
// Copyright 2016 - Calle Laakkonen
//
// QFirmata is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// QFirmata is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Foobar.  If not, see <http://www.gnu.org/licenses/>.

#include "serialport.h"

#include <QDebug>
#include <QMetaEnum>
#include <QScopedPointer>
#include <QSerialPort>

struct SerialFirmata::Private {
    QScopedPointer<QSerialPort> port;
    QString device;
    int baudRate;
    bool manualReadTrigger;

    Private()
        : baudRate(57600)
    {
    }
};

SerialFirmata::SerialFirmata(QObject *parent, bool manualReadTrigger)
    : FirmataBackend(parent),
      d(new Private)
{
    d->manualReadTrigger = manualReadTrigger;
}

SerialFirmata::~SerialFirmata()
{
    delete d;
}

QString SerialFirmata::device() const
{
    return d->device;
}

bool SerialFirmata::setDevice(const QString &device)
{
    bool success = true;
    if (device != d->device) {
        d->device = device;
        if (device.isEmpty()) {
            d->port.reset();
            setStatusText(QStringLiteral("Device not set"));

        } else {
            d->port.reset(new QSerialPort(device));
            d->port->setBaudRate(d->baudRate);
            connect(d->port.data(), &QSerialPort::errorOccurred, [this](QSerialPort::SerialPortError e) {
                QString msg;
                switch (e) {
                case QSerialPort::NoError:
                    msg = QStringLiteral("No error");
                    break;
                case QSerialPort::DeviceNotFoundError:
                    msg = QStringLiteral("Device not found");
                    break;
                case QSerialPort::PermissionError:
                    msg = QStringLiteral("Permission denied");
                    break;
                case QSerialPort::OpenError:
                    msg = QStringLiteral("Device already opened");
                    break;
                case QSerialPort::ResourceError:
                    msg = QStringLiteral("Unable to communicate with the device. Is it plugged in?");
                    break;
                case QSerialPort::TimeoutError:
                    msg = QStringLiteral("Request timed out");
                    break;
                default:
                    msg = QString("Error code %1").arg(e);
                    break;
                }

                setStatusText(msg);
            });

            if (!d->port->open(QIODevice::ReadWrite)) {
                qWarning() << "Error opening" << device << d->port->error();

                auto metaEnum = QMetaEnum::fromType<QSerialPort::SerialPortError>();
                setStatusText("Error opening " + device + " " + metaEnum.valueToKey(d->port->error()));
                return false;
            } else {
                if (!d->manualReadTrigger)
                    connect(
                        d->port.data(),
                        &QSerialPort::readyRead,
                        this,
                        &SerialFirmata::onReadyRead,
                        Qt::DirectConnection);
                setAvailable(true);
                setStatusText(QStringLiteral("Serial port opened"));
            }
        }

        emit deviceChanged(device);
    }

    return success;
}

int SerialFirmata::baudRate() const
{
    return d->baudRate;
}

void SerialFirmata::setBaudRate(int baudRate)
{
    if (baudRate != d->baudRate) {
        d->baudRate = baudRate;
        if (d->port) {
            if (!d->port->setBaudRate(baudRate)) {
                qWarning() << "Error setting baud rate" << baudRate << d->port->error();
            }
        }

        emit baudRateChanged(baudRate);
    }
}

void SerialFirmata::writeBuffer(const uint8_t *buffer, int len)
{
    if (!d->port) {
        qWarning() << "Device" << d->device << "not open!";
        return;
    }
    qint64 written = d->port->write((const char *)buffer, len);
    if (written != len) {
        qWarning() << d->device << "error while writing buffer" << d->port->error();
    }

    d->port->flush();
}

void SerialFirmata::readAndParseData(int waitMsecs)
{
    Q_ASSERT(d->port);
    if (waitMsecs > 0) {
        if (!d->port->waitForReadyRead(waitMsecs))
            return;
    }

    char buffer[256];
    int len;
    do {
        len = d->port->read(buffer, sizeof(buffer));
        if (len > 0)
            bytesRead(buffer, len);
    } while (len > 0);
}

void SerialFirmata::onReadyRead()
{
    readAndParseData();
}

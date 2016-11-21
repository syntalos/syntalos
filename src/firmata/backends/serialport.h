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

#ifndef QFIRMATA_BACKEND_SERIAL_H
#define QFIRMATA_BACKEND_SERIAL_H

#include "backend.h"

#include <cstdint>

//! A serial port based Firmata backend

//! This backend connects to a device using a serial port.
//! It will automatically open the port when the device property is set.
//!
//! Example use:
//! <pre><code>SerialFirmata {
//!     device: "/dev/ttyUSB0"
//!     baudrate: 115200 // default is 57600
//! }</code></pre>
class SerialFirmata : public FirmataBackend
{
	Q_OBJECT
	//! The serial device path
	Q_PROPERTY(QString device READ device WRITE setDevice NOTIFY deviceChanged)

	//! Serial port baud rate. Default is 57600
	Q_PROPERTY(int baudRate READ baudRate WRITE setBaudRate NOTIFY baudRateChanged)

public:
	SerialFirmata(QObject *parent=nullptr);
	~SerialFirmata();

	QString device() const;
    bool setDevice(const QString &device);

	int baudRate() const;
	void setBaudRate(int br);

signals:
	void deviceChanged(const QString &device);
	void baudRateChanged(int baudRate);

protected:
	void writeBuffer(const uint8_t *buffer, int len) override;

private slots:
	void onReadyRead();

private:
	struct Private;
	Private *d;
};

#endif


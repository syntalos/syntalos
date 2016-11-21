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

#ifndef Q_FIRMATA_H
#define Q_FIRMATA_H

#include "backends/backend.h"
#include "pins/pin.h"

#include <QObject>

//! A high level Firmata API for QML

//! Example use:
//! <pre><code>import Firmata 1.0
//!
//! Firmata {
//!     backend: SerialFirmata { device: "/dev/ttyUSB0" }
//!     initPins: false // custom firmware needs no configuration
//!     DigitalPin {
//!         pin: 2
//!         id: pin2
//!     }
//!     ... more pin configuration ...
//! }
//! </code></pre>
class Firmata : public QObject
{
	Q_OBJECT
public:
	Firmata(QObject *parent=nullptr);
	~Firmata();

	FirmataBackend *backend() const;
	void setBackend(FirmataBackend *backend);

	bool isReady() const;

	QString statusText() const;

	bool isInitPins() const;
	void setInitPins(bool);

	int samplingInterval() const;
	void setSamplingInterval(int si);

    QList<Pin*> pins();
    void addPin(Pin *p);

signals:
	void backendChanged();
	void initPinsChanged(bool);
	void readyChanged(bool);
	void samplingIntervalChanged(int);
	void statusTextChanged();

	//! Device connection became ready to use
	void ready();

	//! Sysex string received
	void stringReceived(const QString&);

private slots:
	void onAnalogRead(uint8_t channel, uint16_t value);
	void onDigitalRead(uint8_t port, uint8_t value);
	void onDigitalPinRead(uint8_t pin, bool value);
	void onSysexRead(const QByteArray &data);
	void onBackendAvailable(bool ready);
	void onProtocolVersion(int major, int minor);

	void updateDigitalReport();

private:
	void doInitPins();

	void requestAnalogMappingIfNeeded();
	void sysexFirmwareName(const QByteArray &data);
	void sysexAnalogMapping(const QByteArray &data);
	void sysexString(const QByteArray &data);
	void sendSamplingInterval();

	struct Private;
	Private *d;
};

#endif

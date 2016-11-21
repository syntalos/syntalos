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

#ifndef SERVOPIN_H
#define SERVOPIN_H

#include "pin.h"

//! A (RC hobby) servo pin

class ServoPin : public Pin
{
	Q_OBJECT
	//! The target angle. Range is [0, 200]
	Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)

	//! Minimum pulse duration in microseconds. Default is 1000 (corresponds to 0 deg)
	Q_PROPERTY(int minPulse READ minPulse WRITE setMinPulse NOTIFY minPulseChanged)

	//! Maximum pulse duration in microseconds. Default is 2000 (corresponds to 180 deg)
	Q_PROPERTY(int maxPulse READ maxPulse WRITE setMaxPulse NOTIFY maxPulseChanged)

public:
	ServoPin(QObject *parent=nullptr);
	~ServoPin();

	void setValue(int);
	int value() const;

	void setMinPulse(int p);
	int minPulse() const;

	void setMaxPulse(int p);
	int maxPulse() const;

protected:
	void writeInit(FirmataBackend&) override;
	void writeValue(FirmataBackend&) override;
	void readSysex(const QByteArray &data) override;

signals:
	void valueChanged(int);
	void minPulseChanged(int);
	void maxPulseChanged(int);

private:
	struct Private;
	Private *d;
};

#endif


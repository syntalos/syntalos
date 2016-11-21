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

#include "servo.h"
#include "../backends/backend.h"
#include "../utils.h"

struct ServoPin::Private {
	uint16_t value;
	uint16_t minPulse;
	uint16_t maxPulse;

	Private() : value(90), minPulse(1000), maxPulse(2000)
	{ }
};

ServoPin::ServoPin(QObject *parent)
	: Pin(parent), d(new Private)
{

}

ServoPin::~ServoPin()
{
	delete d;
}

void ServoPin::setValue(int a)
{
	a = qBound(0, a, 200); // 200 is the largest angle supported by arduino servo library
	if(a != d->value) {
		d->value = a;
		send();
		emit valueChanged(a);
	}
}

int ServoPin::value() const
{
	return d->value;
}

void ServoPin::setMinPulse(int p)
{
	p = qBound(0, p, 0x7ff);
	if(p != d->minPulse) {
		d->minPulse = p;
		emit minPulseChanged(p);
		// TODO update configuration
	}
}

int ServoPin::minPulse() const
{
	return d->minPulse;
}

void ServoPin::setMaxPulse(int p)
{
	p = qBound(0, p, 0x7ff);
	if(p != d->maxPulse) {
		d->maxPulse = p;
		emit maxPulseChanged(p);
		// TODO update configuration
	}
}

int ServoPin::maxPulse() const
{
	return d->maxPulse;
}

void ServoPin::writeInit(FirmataBackend &b)
{
	const uint8_t cfg[] {
		0x70, // servo config message
		uint8_t(pin()),
		lsb14(d->minPulse),
		msb14(d->minPulse),
		lsb14(d->maxPulse),
		msb14(d->maxPulse)
	};
	b.writeSysex(cfg, sizeof(cfg));
	b.setPinMode(pin(), IoMode::Servo);
}

void ServoPin::writeValue(FirmataBackend &b)
{
	b.writeAnalogPin(pin(), d->value);
}

void ServoPin::readSysex(const QByteArray &data)
{
	Q_UNUSED(data);
}


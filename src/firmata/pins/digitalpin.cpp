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

#include "digitalpin.h"
#include "../backends/backend.h"

struct DigitalPin::Private {
	bool value;
	bool output;
	bool pullup;

	Private() : value(false), output(false), pullup(false) { }
};

DigitalPin::DigitalPin(QObject *parent)
	: Pin(parent), d(new Private)
{
}

void DigitalPin::setValue(bool v)
{
	if(v != d->value) {
		d->value = v;
		if(d->output)
			send();
		emit valueChanged(v);
	}
}

bool DigitalPin::value() const
{
	return d->value;
}

void DigitalPin::setOutput(bool out)
{
	if(out != d->output) {
		d->output = out;
		// TODO Firmata should reinitialize (if autoInit is enabled) the port on output mode change
		emit outputModeChanged(out);
	}
}

bool DigitalPin::isOutput() const
{
	return d->output;
}

void DigitalPin::setPullup(bool pullup)
{
	if(pullup != d->pullup) {
		d->pullup = pullup;
		emit pullupChanged(pullup);
		// TODO reinitialize
	}
}

bool DigitalPin::isPullup() const
{
	return d->pullup;
}

void DigitalPin::writeInit(FirmataBackend &b)
{
	IoMode mode;
	if(d->output) {
		mode = IoMode::Output;
	} else {
		mode = d->pullup ? IoMode::PullUp : IoMode::Input;
	}
	b.setPinMode(pin(), mode);
}

void DigitalPin::writeValue(FirmataBackend &b)
{
	if(d->output)
		b.writeDigitalPin(pin(), d->value);
}

void DigitalPin::readSysex(const QByteArray &data)
{
	Q_UNUSED(data);
}


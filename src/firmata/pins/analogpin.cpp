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

#include "analogpin.h"
#include "../backends/backend.h"

struct AnalogPin::Private{
	int channel;
	int value;
	double scale;

	Private() : channel(-1), value(0), scale(1.0 / 1024.0)
	{ }
};

AnalogPin::AnalogPin(QObject *parent)
	: Pin(parent), d(new Private)
{

}

AnalogPin::~AnalogPin()
{
	delete d;
}

bool AnalogPin::isConfigured() const
{
	return Pin::isConfigured() &&
		d->channel >= 0;
}

void AnalogPin::setChannel(int c)
{
	if(c>=0 && c<16 && d->channel != c) {
		d->channel = c;
		emit channelChanged(c);
	}
}

int AnalogPin::channel() const
{
	return d->channel;
}

void AnalogPin::setScale(double s)
{
	if(s>0 && !qFuzzyCompare(s+1.0, d->scale+1.0)) {
		d->scale = s;
		emit scaleChanged(s);
		emit valueChanged();
	}
}

double AnalogPin::scale() const
{
	return d->scale;
}

void AnalogPin::setRawValue(int v)
{
	if(v != d->value) {
		d->value = v;
		emit valueChanged();
	}
	emit sampled();
}

int AnalogPin::rawValue() const
{
	return d->value;
}

double AnalogPin::value() const
{
	return qBound(0.0, d->value * d->scale, 1.0);
}

void AnalogPin::writeInit(FirmataBackend &b)
{
	b.setPinMode(pin(), IoMode::Analog);
	b.reportAnalogPin(channel(), true);
}

void AnalogPin::readSysex(const QByteArray &data)
{
	Q_UNUSED(data);
}


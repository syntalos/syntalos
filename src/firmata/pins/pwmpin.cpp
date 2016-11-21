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

#include "pwmpin.h"
#include "../backends/backend.h"

PwmPin::PwmPin(QObject *parent)
	: Pin(parent), m_value(0), m_scale(255)
{

}

void PwmPin::setValue(double v)
{
	v = qBound(0.0, v, 1.0);
	if(!qFuzzyCompare(v+1.0, m_value+1.0)) {
		m_value = v;
		send();
		emit valueChanged(v);
	}
}

void PwmPin::setScale(double s)
{
	if(!qFuzzyCompare(s+1.0, m_scale+1.0)) {
		const uint16_t oldval = m_value*m_scale;
		const uint16_t newval = s*m_scale;
		m_scale = s;

		if(oldval != newval)
			send();
		emit scaleChanged(s);
	}
}

void PwmPin::writeInit(FirmataBackend &b)
{
	b.setPinMode(pin(), IoMode::PWM);
}

void PwmPin::writeValue(FirmataBackend &b)
{
	b.writeAnalogPin(pin(), uint16_t(m_value * m_scale));
}

void PwmPin::readSysex(const QByteArray &data)
{
	Q_UNUSED(data);
}


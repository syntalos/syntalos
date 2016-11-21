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

#include "pin.h"
#include "../firmata.h"
#include "../backends/backend.h"

Pin::Pin(QObject *parent)
	: QObject(parent), m_firmata(nullptr), m_pin(-1)
{
}

void Pin::setFirmata(Firmata *firmata)
{
	Q_ASSERT(m_firmata == nullptr);
	 m_firmata = firmata;
}

void Pin::setPin(int pin)
{
	if(pin<0 || pin>127) {
		qWarning("Invalid pin number: %d", pin);
		return;
	}
	if(pin != m_pin) {
		m_pin = pin;
		emit pinChanged(pin);
	}
}

bool Pin::isConfigured() const
{
	return pin() >= 0 && pin() < 128;
}

bool Pin::canSend(const char *warning_msg) const
{
	if(!m_firmata) {
		qWarning("[pin %d] %s: not yet attached to a QFirmata instance!", pin(), warning_msg);
		return false;
	}

	if(!isConfigured()) {
		qWarning("[pin %d] %s: not yet fully configured!", pin(), warning_msg);
		return false;
	}

	const FirmataBackend *b = m_firmata->backend();
	if(!b) {
		qWarning("[pin %d] %s: no backend set!", pin(), warning_msg);
		return false;
	}

	if(!b->isAvailable()) {
		qWarning("[pin %d] %s: backend is not ready!", pin(), warning_msg);
		return false;
	}

	return true;
}

void Pin::initialize()
{
	if(canSend("initialize")) {
		FirmataBackend &b = *firmata()->backend();
		writeInit(b);
		writeValue(b);
	}
}

void Pin::send()
{
	if(canSend("send value")) {
		writeValue(*firmata()->backend());
	}
}

void Pin::sysex(const QByteArray &data)
{
	if(data.isEmpty())
		return;

	readSysex(data);
}


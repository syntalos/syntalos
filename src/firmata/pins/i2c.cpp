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

#include "i2c.h"
#include "../backends/backend.h"
#include "../firmata.h"
#include "../utils.h"

#include <QVarLengthArray>
#include <QDebug>

static const uint8_t
	SYSEX_I2C_REQUEST = 0x76,
	SYSEX_I2C_REPLY = 0x77,
	SYSEX_I2C_CONFIG = 0x78
	;

struct I2C::Private {
	int delay;

	Private() : delay(0)
	{ }
};

I2C::I2C(QObject *parent)
	: Pin(parent), d(new Private)
{

}

I2C::~I2C()
{
	delete d;
}

void I2C::setDelay(int delay)
{
	if(delay>0 && delay != d->delay) {
		d->delay = delay;
		emit delayChanged(delay);
	}
}

int I2C::delay() const
{
	return d->delay;
}

bool I2C::isConfigured() const
{
	// No configuration needed at the moment
	return true;
}

void I2C::write(int address, QList<int> data)
{
	sendRequest(address, false, false, rwmode::Write, data);
}

void I2C::read(int address, int bytes, int reg, bool autoRestart)
{
	sendReadRequest(address, bytes, reg, false, autoRestart);
}

void I2C::autoRead(int address, int bytes, int reg, bool autoRestart)
{
	sendReadRequest(address, bytes, reg, true, autoRestart);
}

void I2C::sendReadRequest(int address, int bytes, int reg, bool autoread, bool autoRestart)
{
	QList<int> query;
	if(reg>=0)
		query.append(reg);
	query.append(bytes);

	sendRequest(address, autoRestart, false, autoread ? rwmode::AutoRead : rwmode::Read, query);
}

void I2C::stopAutoRead(int address)
{
	sendRequest(address, false, false, rwmode::StopRead, QList<int>());
}

void I2C::sendRequest(int address, bool autoRestart, bool tenbit, rwmode mode, const QList<int> &data)
{
	if(!canSend("i2c"))
		return;

	if(address>127 && !tenbit) {
		qWarning("Tried to send I2C request with address 0x%x in 7-bit mode", address);
		return;
	}

	QVarLengthArray<uint8_t, 256> cmd;
	cmd.reserve(3+data.length()*2);

	cmd.append(SYSEX_I2C_REQUEST);
	cmd.append(lsb14(address));
	cmd.append(
			(autoRestart ? 0x40 : 0) |
			(tenbit ? 0x20 : 0) |
			(uint8_t(mode) << 3) |
			((address >> 7) & 0x07) // used only in 10-bit mode
			);

	// Note: QList<int> is used for simple interop with QML/JavaScript
	for(int i=0;i<data.length();++i) {
#ifndef NDEBUG
		if(data[i]<0 || data[i]>255)
			qFatal("I2C byte (0x%x) out of range", data[i]);
#endif
		cmd.append(lsb14(data[i]));
		cmd.append(msb14(data[i]));
	}

	firmata()->backend()->writeSysex(cmd.constData(), cmd.length());
}

void I2C::writeInit(FirmataBackend &b)
{
#if 0 // explicitly setting the pin modes does not seem to be necessary
	b.setPinMode(18, IoMode::I2C);
	b.setPinMode(19, IoMode::I2C);
#endif
	const uint8_t cmd[] {
		SYSEX_I2C_CONFIG,
		lsb14(d->delay),
		msb14(d->delay)
	};

	b.writeSysex(cmd, sizeof(cmd));
}

void I2C::readSysex(const QByteArray &sedata)
{
	if(sedata.at(0) != SYSEX_I2C_REPLY)
		return;

	if(sedata.length() < 5 || sedata.length()%2 == 0) {
		qWarning("Unexpected I2C reply length %d", sedata.length());
		return;
	}

	const uint8_t *data = reinterpret_cast<const uint8_t*>(sedata.constData());
	const int address = unpack14(data+1);
	const int reg = unpack14(data+3);

	const int bytes = sedata.length() - 5;
	QList<int> payload;
	for(int i=0;i<bytes;i+=2) {
		payload.append(unpack14(data+5+i));
	}

	emit reply(address, reg, payload);
}


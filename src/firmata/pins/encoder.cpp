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

#include "encoder.h"
#include "../backends/backend.h"
#include "../utils.h"
#include "../firmata.h"

static const uint8_t
	ENCODER_DATA = 0x61,
	ENCODER_ATTACH = 0x00,
	ENCODER_REPORT_POSITION = 0x01,
	ENCODER_RESET_POSITION = 0x03,
	ENCODER_REPORT_AUTO = 0x04,
	ENCODER_DETACH = 0x05
	;

struct EncoderPins::Private {
	int32_t value;
	uint8_t pin2;
	uint8_t number;

	bool emitDelta;

	Private() : value(0), pin2(255), number(0), emitDelta(false)
	{ }
};

EncoderPins::EncoderPins(QObject *parent)
	: Pin(parent), d(new Private)
{

}

EncoderPins::~EncoderPins()
{
	delete d;
}

bool EncoderPins::isConfigured() const
{
	return Pin::isConfigured() &&
		d->pin2 < 128
		;
}

int EncoderPins::value() const
{
	return d->value;
}

void EncoderPins::setPin2(int p)
{
	if(p>=0 && p<128 && p != d->pin2) {
		d->pin2 = p;
		emit pin2Changed(p);
	}
}

int EncoderPins::number() const
{
	return d->number;
}

void EncoderPins::setNumber(int n)
{
	// Note: actual encoder number limit is typically very small
	// (like 5), but this is firmware dependant. 63 is the protocol
	// maximum imposed by the number of available bits.
	if(n>=0 && n<64 && n != d->number) {
		d->number = n;
		emit numberChanged(n);
	}
}

int EncoderPins::pin2() const
{
	return d->pin2;
}

void EncoderPins::writeInit(FirmataBackend &b)
{
	const uint8_t cfg[] {
		ENCODER_DATA,
		ENCODER_ATTACH,
		d->number,
		uint8_t(pin()),
		d->pin2
	};
	b.writeSysex(cfg, sizeof(cfg));
	b.setPinMode(pin(), IoMode::Encoder);

	// TODO make this configurable
	setAutoReporting(b, true);
}

void EncoderPins::setAutoReporting(FirmataBackend &backend, bool enable)
{
	const uint8_t cfg[] {
		ENCODER_DATA,
		ENCODER_REPORT_AUTO,
		enable
	};
	backend.writeSysex(cfg, sizeof(cfg));
}

void EncoderPins::readSysex(const QByteArray &data)
{
	if(data.at(0) != ENCODER_DATA)
		return;

	const int encoders = (data.length()-1) / 5;
	if(data.length() != 1+encoders*5) {
		qWarning("Invalid encoder data: expected length %d, was %d", 1+encoders*5, data.length());
		return;
	}

	const uint8_t *enc = (const uint8_t*)data.constData() + 1;
	for(int i=0;i<encoders;++i,enc+=5) {
		const uint8_t encnum = *enc & 0x2f; // The first 6 bits make up the encoder number

		if(encnum == d->number) {
			const int32_t sign = (*enc & 0x40) ? -1 : 1; // The seventh bit is the direction
			const int32_t pos = sign * unpack28(enc+1); // The next four bytes make up the 28bit value value

			if(pos != d->value) {
				if(d->emitDelta)
					emit delta(pos - d->value);
				else
					d->emitDelta = true;

				d->value = pos;
				emit valueChanged(pos);
			}
		}
	}
}

void EncoderPins::reset()
{
	if(canSend("reset")) {
		const uint8_t cmd[] {
			ENCODER_DATA,
			ENCODER_RESET_POSITION,
			d->number
		};
		firmata()->backend()->writeSysex(cmd, sizeof(cmd));
		d->emitDelta = false;
	}
}

void EncoderPins::queryPosition()
{
	if(canSend("query")) {
		const uint8_t cmd[] {
			ENCODER_DATA,
			ENCODER_REPORT_POSITION,
			d->number
		};
		firmata()->backend()->writeSysex(cmd, sizeof(cmd));
	}
}


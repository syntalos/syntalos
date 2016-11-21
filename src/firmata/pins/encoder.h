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

#ifndef ENCODER_PIN_H
#define ENCODER_PIN_H

#include "pin.h"

//! An encoder input pin

class EncoderPins : public Pin
{
	Q_OBJECT

	//! Second encoder pin number
	Q_PROPERTY(int pin2 READ pin2 WRITE setPin2 NOTIFY pin2Changed)

	//! Encoder number

	//! Defaults to zero. Remember to set this if you have more than one encoder!
	Q_PROPERTY(int number READ number WRITE setNumber NOTIFY numberChanged)

	//! The current value
	Q_PROPERTY(int value READ value NOTIFY valueChanged)

public:
	EncoderPins(QObject *parent=nullptr);
	~EncoderPins();

	bool isConfigured() const override;

	int value() const;

	void setPin2(int);
	int pin2() const;

	void setNumber(int);
	int number() const;

	//! Enable/disable autoreporting

	//! At the moment, autoreporting is be enabled when an encoder pin is initialized.
	static void setAutoReporting(FirmataBackend &backend, bool enable);

public slots:
	//! Reset encoder value to zero
	void reset();

	//! Request current value
	void queryPosition();

protected:
	void writeInit(FirmataBackend&) override;
	void readSysex(const QByteArray &data) override;

signals:
	void valueChanged(int);
	void pin2Changed(int);
	void autoReportChanged(bool);
	void numberChanged(int);

	//! Position delta change
	void delta(int);

private:
	struct Private;
	Private *d;
};

#endif



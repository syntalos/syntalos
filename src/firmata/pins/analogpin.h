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

#ifndef ANALOGPIN_H
#define ANALOGPIN_H

#include "pin.h"

//! Analog input pin

//! Note: analog pins are configured and address in a slightly odd way.
//! To configure a pin as an analog input, the actual pin number must be used,
//! but when reading from it, a channel number is used instead.
//!
//! When configuring analog pins, you can leave out the pin number and
//! use just the channel. QFirmata will make an analog mapping query
//! and fill in missing pin numbers itself.
//!
//! If autoInit is on, reporting will be enabled for analog pins.
//! TODO extendend analog to support more than 16 channels
//! TODO set scale automatically by resolution query
class AnalogPin : public Pin
{
	Q_OBJECT
	//! Analog channel (must be set)
	Q_PROPERTY(int channel READ channel WRITE setChannel NOTIFY channelChanged)

	//! Scaled analog value
	Q_PROPERTY(double value READ value NOTIFY valueChanged)

	//! The raw integer analog value
	Q_PROPERTY(int rawValue READ rawValue NOTIFY valueChanged)

	//! Scaling factor (by default maps 10-bit range to [0.0, 1.0])
	Q_PROPERTY(double scale READ scale WRITE setScale NOTIFY scaleChanged)

public:
	AnalogPin(QObject *parent=nullptr);
	~AnalogPin();

	bool isConfigured() const override;

	double value() const;

	// Note: setRawValue is used internally only
	void setRawValue(int val);
	int rawValue() const;

	double scale() const;
	void setScale(double scale);

	int channel() const;
	void setChannel(int c);

signals:
	void valueChanged();
	void scaleChanged(double);
	void channelChanged(int);

	//! This signal is emitted when the analog pin is sampled, even if the value does not change
	void sampled();

protected:
	void writeInit(FirmataBackend&) override;
	void readSysex(const QByteArray &data) override;

private:
	struct Private;
	Private *d;
};

#endif // ANALOGPIN_H

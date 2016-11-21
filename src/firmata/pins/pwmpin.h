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

#ifndef PWMPIN_H
#define PWMPIN_H

#include "pin.h"

//! A PWM ("analog output") pin

class PwmPin : public Pin
{
	Q_OBJECT
	//! The value to output. Range is [0.0, 1.0]
	Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)

	//! Scaling factor.

	//! By default maps 1.0 to 0xff.
	//! Set this to 0xffff for 16-bit resolution
	Q_PROPERTY(double scale READ scale WRITE setScale NOTIFY scaleChanged)

public:
	PwmPin(QObject *parent=nullptr);

	void setValue(double v);
	double value() const { return m_value; }

	void setScale(double s);
	double scale() const { return m_scale; }

protected:
	void writeInit(FirmataBackend&) override;
	void writeValue(FirmataBackend&) override;
	void readSysex(const QByteArray &data) override;

signals:
	void valueChanged(double);
	void scaleChanged(double);

private:
	double m_value;
	double m_scale;
};

#endif // PWMPIN_H

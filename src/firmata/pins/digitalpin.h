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

#ifndef QFIRMATA_DIGITALPIN_H
#define QFIRMATA_DIGITALPIN_H

#include "pin.h"

//! Digital input/output pin

//! The digital pin works in both input (default) and output modes
//! Set the <b>output</b> property to true to use it as an output.
//!
//! Changes to pin value are set using the "set digital pin value" message
//! rather than the "digital I/O message", so make sure your firmware supports
//! this command.
//!
//! If autoInit is on, reporting for digital input pins will be enabled.
class DigitalPin : public Pin
{
	Q_OBJECT
	//! The value of the pin

	//! When in output mode, setting this will send the pin value change message.
	//! In input mode, the value is changed by digital io messages
	Q_PROPERTY(bool value READ value WRITE setValue NOTIFY valueChanged)

	//! Is this pin in output mode
	Q_PROPERTY(bool output READ isOutput WRITE setOutput NOTIFY outputModeChanged)

	//! Are internal pull-ups enabled (input mode only)
	Q_PROPERTY(bool pullup READ isPullup WRITE setPullup NOTIFY pullupChanged)

public:
	DigitalPin(QObject *parent=nullptr);

	void setValue(bool v);
	bool value() const;

	void setOutput(bool);
	bool isOutput() const;

	void setPullup(bool);
	bool isPullup() const;

protected:
	void writeInit(FirmataBackend&) override;
	void writeValue(FirmataBackend&) override;
	void readSysex(const QByteArray &data) override;

signals:
	void valueChanged(bool);
	void outputModeChanged(bool);
	void pullupChanged(bool);

private:
	struct Private;
	Private *d;
};

#endif // DIGITALPIN_H

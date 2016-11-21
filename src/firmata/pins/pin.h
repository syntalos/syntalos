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

#ifndef Q_FIRMATA_PIN_H
#define Q_FIRMATA_PIN_H

#include <QObject>

class Firmata;
class FirmataBackend;

//! Base class for Pins

//! A pin represents a device connected to a specific pin
class Pin : public QObject
{
	Q_OBJECT
	//! The pin number
	
	//! This must be set to a valid number (0-127) for the pin to work.
	//! If autoInit is enabled, pinmode commands will be automatically
	//! sent to configure the pin.
	Q_PROPERTY(int pin READ pin WRITE setPin NOTIFY pinChanged)

	//Q_ENUMS(IoMode)
public:
	/*
	enum IoMode {
		Input,
		Output
	};
	*/

	explicit Pin(QObject *parent);

	void setFirmata(Firmata *firmata);

	int pin() const { return m_pin; }
	void setPin(int pin);

	//! Is this pin fully configured?
	virtual bool isConfigured() const;

	//! Initialize this pin
	
	//! This usually called automatically when needed
	Q_SLOT void initialize();

	//! Send the value of this pin (output type only)
	
	//! This is called automatically when the pin value changes
	Q_SLOT void send();

	// Handle a sysex message.
	// These are sent to all pins. Each pin should decide if
	// the message is relevant and ignore it if its not.
	// There is no need to call this function manually
	void sysex(const QByteArray &data);

signals:
	void pinChanged(int);

protected:
	//! Write the commands to initialize this pin

	//! Typically the pin mode is set here, but for more advanced devices
	//! (e.g. servos), extra configuration sysex messages may be sent too.
	virtual void writeInit(FirmataBackend &backend) = 0;

	//! Write the current value of the pin (if this is an output pin)
	virtual void writeValue(FirmataBackend &backend) { Q_UNUSED(backend); /* only output type pins need to implement this */ }

	//! Handle sysex
	
	//! Messages not addressed to this pin or not supported by this type should be ignored
	virtual void readSysex(const QByteArray &data) = 0;

	//! Check if everything is ready for sending
	bool canSend(const char *warning_msg) const;

	//! Get the attached Firmata instance
	Firmata *firmata() const { return m_firmata; }

private:
	Firmata *m_firmata;
	int m_pin;
};

#endif // PIN_H

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

#ifndef QFIRMATA_BACKEND_H
#define QFIRMATA_BACKEND_H

#include <QObject>

//! Pin input/output mode
enum class IoMode {
	Input = 0x00,   //! Digital input
	Output = 0x01,  //! Digital output
	Analog = 0x02,  //! Analog input
	PWM = 0x03,     //! PWM output
	Servo = 0x04,   //! RC servo output
	Shift = 0x05,   //! Shift register
	I2C = 0x06,     //! I²C bus
	OneWire = 0x07, //! OneWire bus,
	Stepper = 0x08, //! Stepper motor
	Encoder = 0x09, //! Encoder input
	Serial = 0x0a,  //! Serial port
	PullUp = 0x0b   //! Digital input with internal pull-ups
};

//! Abstract base class for Firmata backends

//! This class provides a low level Firmata API.
//! You would not typically use it directly, but through
//! the more high level interface offered by QFirmata and
//! the Pin implementations.
//!
//! Concrete implementations must add the properties for
//! selecting the device and the logic for opening the connection
//! (typically in response to setting the device parameters.)
//! The virtual function \a writeBuffer must be implemented
//! to actually write data to the device. Received data can
//! be parsed by calling the function \a bytesRead
class FirmataBackend : public QObject {
	Q_OBJECT
	//! Is the backend available?
	
	//! When the backend becomes available, QFirmata will try to contact
	//! the device. When contact is established, QFirmata consideres the
	//! connection ready for use and (optionally) performs automatic configuration.
	Q_PROPERTY(bool available READ isAvailable NOTIFY availabilityChanged)

	//! Current status of the backend in human-readable format
	Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
	FirmataBackend(QObject *parent=nullptr);
	~FirmataBackend();

	bool isAvailable() const;

	QString statusText() const;

    // Low level Firmata messages.
	
	//! Write a 14bit analog value
	void writeAnalogPin(uint8_t pin, uint16_t value);

	//! Write the value of a digital pin
	void writeDigitalPin(uint8_t pin, bool value);

	//! Enable/disable analog pin value reporting
	void reportAnalogPin(uint8_t pin, bool enable);

	//! Enable disable digital port value reporting
	void reportDigitalPort(uint8_t port, bool enable);

	//! Request the device to report its protocol version
	void reportProtocolVersion();

	//! Set the mode of a Pin
	void setPinMode(uint8_t pin, IoMode mode);

	//! Send a SysEx command
	void writeSysex(const uint8_t *data, int len);

signals:
	//! An analog message was just received
	void analogRead(uint8_t channel, uint16_t value);

	//! A digital message was just received
	void digitalRead(uint8_t port, uint8_t value);

	//! Individual digital pin value changed
	void digitalPinRead(uint8_t pin, bool value);

	//! A SysEx command was just received
	void sysexRead(const QByteArray &data);

	//! Protocol version was just received
	void protocolVersion(int major, int minor);

	void availabilityChanged(bool);
	void statusTextChanged(const QString &);

protected:
	virtual void writeBuffer(const uint8_t *buffer, int len) = 0;

	void bytesRead(const char *data, int len);

	void setAvailable(bool available);
	void setStatusText(const QString &text);

private:
	struct Private;
	Private *d;
};

#endif


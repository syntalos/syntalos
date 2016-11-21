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

#ifndef I2CPIN_H
#define I2CPIN_H

#include "pin.h"

//! I²C bus

//! Note. Firmata currently only supports one I²C bus, so the pins
//! are not configurable.
class I2C : public Pin
{
	Q_OBJECT

	//! Delay between read and write (needed by some devices)
	Q_PROPERTY(int delay READ delay WRITE setDelay NOTIFY delayChanged)

public:
	I2C(QObject *parent=nullptr);
	~I2C();

	bool isConfigured() const override;

	int delay() const;
	void setDelay(int);

public slots:
	//! Write data to the given I2C address
	void write(int address, QList<int> data);

	//! Request a read from the given address

	//! \param address target device address
	//! \param bytes number of bytes to read
	//! \param reg register to read (optional)
	//! \param autoRestart if true, bus is not released after read
	void read(int address, int bytes, int reg=-1, bool autoRestart=false);

	//! Like \a read, but the read is automatically repeated when inputs are sampled
	void autoRead(int address, int bytes, int reg=-1, bool autoRestart=false);

	//! Stop autoreading the given address
	void stopAutoRead(int address);

signals:
	//! I2C reply received
	void reply(int address, int reg, const QList<int> &data);

	void delayChanged(int);

protected:
	void writeInit(FirmataBackend&) override;
	void readSysex(const QByteArray &data) override;

private:
	enum class rwmode { Write=0, Read=1, AutoRead=2, StopRead=3 };

	void sendReadRequest(int address, int bytes, int reg, bool autoread, bool autoRestart);
	void sendRequest(int address, bool autoRestart, bool tenbit, rwmode mode, const QList<int> &data);

	struct Private;
	Private *d;
};

#endif


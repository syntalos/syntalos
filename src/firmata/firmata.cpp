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

#include "firmata.h"
#include "utils.h"
#include "pins/digitalpin.h"
#include "pins/analogpin.h"

#include <QBitArray>

struct Firmata::Private {
	FirmataBackend *backend;
	QList<Pin*> pins;
	int samplingInterval;
	bool initPins;
	bool isReady;


	QBitArray reportDigital;

	Private()
		: backend(nullptr), samplingInterval(0), initPins(true), isReady(false), reportDigital(128/8)
	{ }
	~Private() {
		delete backend;
		for(Pin * p : pins)
			delete p;
	}
};

Firmata::Firmata(QObject *parent)
	: QObject(parent), d(new Private)
{
}

Firmata::~Firmata()
{
	delete d;
}

FirmataBackend *Firmata::backend() const
{
	return d->backend;
}

void Firmata::setBackend(FirmataBackend *backend)
{
	if(backend == d->backend)
		return;

	delete d->backend;
	d->backend = backend;
	if(d->isReady) {
		d->isReady = false;
		emit readyChanged(false);
	}

	if(backend) {
		connect(d->backend, &FirmataBackend::analogRead, this, &Firmata::onAnalogRead);
		connect(d->backend, &FirmataBackend::digitalRead, this, &Firmata::onDigitalRead);
		connect(d->backend, &FirmataBackend::digitalPinRead, this, &Firmata::onDigitalPinRead);
		connect(d->backend, &FirmataBackend::digitalPinRead, this, &Firmata::onDigitalPinRead);
		connect(d->backend, &FirmataBackend::protocolVersion, this, &Firmata::onProtocolVersion);
		connect(d->backend, &FirmataBackend::sysexRead, this, &Firmata::onSysexRead);
		connect(d->backend, &FirmataBackend::availabilityChanged, this, &Firmata::onBackendAvailable);
		connect(d->backend, &FirmataBackend::statusTextChanged, this, &Firmata::statusTextChanged);
	}

	emit backendChanged();
	emit statusTextChanged();
	onBackendAvailable(backend && backend->isAvailable());
}

int Firmata::samplingInterval() const
{
	return d->samplingInterval;
}

void Firmata::setSamplingInterval(int si)
{
	if(si>0x3fff)
		si = 0x3fff;

	if(si>0 && si != d->samplingInterval) {
		d->samplingInterval = si;
		if(isReady())
			sendSamplingInterval();
		emit samplingIntervalChanged(si);
	}
}

bool Firmata::isReady() const
{
	return d->isReady;
}

QString Firmata::statusText() const
{
	if(isReady())
		return QStringLiteral("Ready");
	else if(d->backend)
		return d->backend->statusText();
	else
		return QStringLiteral("Backend not set");
}

bool Firmata::isInitPins() const
{
	return d->initPins;
}

void Firmata::setInitPins(bool ip)
{
	if(d->initPins != ip) {
		d->initPins = ip;
		if(ip && isReady())
			doInitPins();
		emit initPinsChanged(ip);
	}
}

QList<Pin *> Firmata::pins()
{
    return d->pins;
}

void Firmata::onAnalogRead(uint8_t channel, uint16_t value)
{
	// Analog pin value change
	for(Pin *p : d->pins) {
		AnalogPin *ap = qobject_cast<AnalogPin*>(p);
		if(ap && ap->channel()==channel) {
			ap->setRawValue(value);
			break;
		}
	}
}

void Firmata::onDigitalRead(uint8_t port, uint8_t value)
{
	const int first = port*8;
	const int last = first+7;

	qDebug("onDigitalRead %d", int(value));
	// Value of a digital port changed: 8 possible pin changes
	for(Pin *p : d->pins) {
		DigitalPin *dp = qobject_cast<DigitalPin*>(p);
		if(dp && dp->pin()>=first && dp->pin()<=last && !dp->isOutput()) {
			dp->setValue(value & (1<<(dp->pin()-first)));
		}
	}
}

void Firmata::onDigitalPinRead(uint8_t pin, bool value)
{
	// Value of a single pin changed
	qDebug("onDigitalPinRead %d=%d", pin, value);
	for(Pin *p : d->pins) {
		DigitalPin *dp = qobject_cast<DigitalPin*>(p);
		if(dp && dp->pin()==pin) {
			if(!dp->isOutput())
				dp->setValue(value);
			break;
		}
	}
}

void Firmata::onSysexRead(const QByteArray &data)
{
	if(data.isEmpty())
		return;

	const uint8_t cmd = data.at(0);

	switch(cmd) {
	case 0x79: sysexFirmwareName(data); break;
	case 0x62: sysexAnalogMapping(data); break;
	case 0x71: sysexString(data); break;
	default: 
		qDebug("onSysex 0x%x", cmd);
		for(Pin *pin : d->pins) {
			pin->sysex(data);
		}
	}
}

void Firmata::onBackendAvailable(bool available)
{
	if(!available) {
		if(d->isReady) {
			d->isReady = false;
			emit statusTextChanged();
		}
        emit readyChanged(false);
		return;
	}
	if(d->isReady)
		return;

	// Request protocol version number. When we receive the reply, we
	// know the connection is up.
	Q_ASSERT(d->backend);
	d->backend->reportProtocolVersion();
}

void Firmata::onProtocolVersion(int major, int minor)
{
	Q_ASSERT(d->backend);
	qDebug("Device protocol version: %d.%d", major, minor);
	if(!d->isReady) {
		// Protocol version received: we now know the device connection is working. 
		d->isReady = true;
		doInitPins();
		emit readyChanged(true);
		emit ready();
		emit statusTextChanged();
	}
}

void Firmata::doInitPins()
{
	requestAnalogMappingIfNeeded();
	sendSamplingInterval();

	// Initialiez all the pins we can.
	// Unmapped analog pins will be initialized when the mapping
	// reply is received.
	if(d->initPins) {
		for(Pin *p : d->pins) {
			p->initialize();
		}

		updateDigitalReport();
	}
}

void Firmata::updateDigitalReport()
{
	if(!isReady())
		return;

	// Gather a list of all digital ports that have input pins
	QBitArray inputs(128/8);
	for(const Pin *p : d->pins) {
		const DigitalPin *dp = qobject_cast<const DigitalPin*>(p);
		if(dp && !dp->isOutput() && dp->pin()>=0) {
			Q_ASSERT(dp->pin() < 128);
			inputs.setBit(dp->pin()/8);
		}
	}

	// Update the digital report status for each changed port
	QBitArray changed = inputs ^ d->reportDigital;
	for(int i=0;i<changed.size();++i) {
		if(changed.testBit(i))
			d->backend->reportDigitalPort(i, inputs.testBit(i));
	}

	d->reportDigital = inputs;
}

void Firmata::requestAnalogMappingIfNeeded()
{
	if(!isReady())
		return;

	// Check if we have any analog pins missing pin numbers
	bool needMapping = false;
	for(const Pin *p : d->pins) {
		const AnalogPin *ap = qobject_cast<const AnalogPin*>(p);
		if(ap && ap->pin()<0 && ap->channel()>=0) {
			needMapping = true;
			break;
		}
	}

	if(needMapping) {
		qDebug("Requesting analog channel mappings...");
		const uint8_t cmd[] {
			0x69
		};
		d->backend->writeSysex(cmd, sizeof(cmd));
	}
}

void Firmata::sendSamplingInterval()
{
	if(d->samplingInterval<=0)
		return;

	qDebug("Setting sampling interval to %d ms", d->samplingInterval);
	Q_ASSERT(isReady());
	const uint8_t cmd[] {
		0x7a,
		lsb14(d->samplingInterval),
		msb14(d->samplingInterval)
	};
	d->backend->writeSysex(cmd, sizeof(cmd));
}

void Firmata::sysexFirmwareName(const QByteArray &data)
{
	if(data.length() < 3) {
		qWarning("Too short extended firmware name message!");
	} else {
		int major = data.at(1);
		int minor = data.at(2);
		QString name = QString::fromLatin1(data.constData()+3, data.length()-3);

		qDebug("Firmware version %d.%d \"%s\"", major, minor, qPrintable(name));
	}
}

void Firmata::sysexAnalogMapping(const QByteArray &data)
{
	for(int i=1;i<data.length();++i) {
		const uint8_t pin = i-1;
		const uint8_t channel = data.at(i);

		qDebug("Pin %d to channel %d", pin, channel);
		// Channel 127 means no analog channel assigned to this pin
		if(channel>=127)
			continue;

		// Find an AnalogPin configured with the given channel and assign pin number
		for(Pin *p : d->pins) {
			AnalogPin *ap = qobject_cast<AnalogPin*>(p);
			if(ap->channel() == channel) {
				qDebug("Found pin %d for analog channel %d", pin, channel);
				ap->setPin(pin);
				if(d->initPins)
					ap->initialize();
				break;
			}
		}
	}
}

void Firmata::sysexString(const QByteArray &data)
{
	QString str = QString::fromLatin1(data.constData()+1, data.length()-1);
	qDebug("Received string \"%s\"", qPrintable(str));
	emit stringReceived(str);
}

void Firmata::addPin(Pin *p)
{
    p->setFirmata(this);
    d->pins.append(p);
    if(d->initPins && isReady()) {
        p->initialize();
        updateDigitalReport();
    }
    requestAnalogMappingIfNeeded();
}

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

#include "backend.h"
#include "fmutils.h"

#include <QDebug>
#include <QtEndian>

#include <QCoreApplication>
#include <QEventLoop>
#include <QTime>

enum class ParserState {
    ExpectNothing,
    ExpectParam1,
    ExpectParam2,
    ExpectSysexData
};

//! Standard commands
static const uint8_t CMD_ANALOG_IO = 0xe0, CMD_DIGITAL_IO = 0x90, CMD_ANALOG_REPORT = 0xc0, CMD_DIGITAL_REPORT = 0xd0,
                     CMD_SYSEX_START = 0xf0, CMD_SYSEX_END = 0xF7, CMD_SET_PINMODE = 0xf4, CMD_SET_DIGITAL_PIN = 0xf5,
                     CMD_PROTOCOL_VERSION = 0xf9,

                     SYSEX_EXTENDED_ANALOG = 0x6f;

//! Combine pin/port number with a command
inline uint8_t cmdPin(uint8_t cmd, uint8_t pin)
{
    Q_ASSERT((cmd & 0x0f) == 0);
    Q_ASSERT((pin & 0xf0) == 0);
    return cmd | pin;
}

struct FirmataBackend::Private {
    FirmataBackend *const b;
    QString statusText;
    bool available;
    bool ready;

    // Parser
    ParserState parserState;
    uint8_t currentCommand;
    uint8_t currentChannel;
    uint8_t params[2];
    QByteArray sysexdata;

    Private(FirmataBackend *backend)
        : b(backend),
          statusText(QStringLiteral("Not configured")),
          available(false),
          ready(false),
          parserState(ParserState::ExpectNothing)
    {
    }

    void parse(uint8_t val)
    {
        if ((val & 0x80)) {
            // high bit set: this is a command
            parseCommand(val);
            return;
        }

        // bit 7 zero: parameter data
        switch (parserState) {
        case ParserState::ExpectNothing:
            break;
        case ParserState::ExpectParam1:
            params[0] = val;
            parserState = ParserState::ExpectParam2;
            break;
        case ParserState::ExpectParam2:
            params[1] = val;
            parserState = ParserState::ExpectNothing;
            executeCommand();
            break;
        case ParserState::ExpectSysexData:
            sysexdata.append(val);
            break;
        }
    }

    void parseCommand(uint8_t cmd)
    {
        const uint8_t nib = cmd & 0xf0;
        switch (nib) {
        case CMD_ANALOG_IO:
        case CMD_DIGITAL_IO:
            currentCommand = nib;
            currentChannel = cmd & 0x0f;
            parserState = ParserState::ExpectParam1;
            return;
        }

        switch (cmd) {
        case CMD_SET_DIGITAL_PIN:
        case CMD_PROTOCOL_VERSION:
            currentCommand = cmd;
            parserState = ParserState::ExpectParam1;
            break;
        case CMD_SYSEX_START:
            sysexdata.clear();
            parserState = ParserState::ExpectSysexData;
            break;
        case CMD_SYSEX_END:
            emit b->sysexRead(sysexdata);
            parserState = ParserState::ExpectNothing;
            break;
        default:
            qWarning("Unknown command 0x%x", cmd);
            parserState = ParserState::ExpectNothing;
        }
    }

    void executeCommand()
    {
        switch (currentCommand) {
        case CMD_SET_DIGITAL_PIN:
            emit b->digitalPinRead(params[0], params[1]);
            break;
        case CMD_ANALOG_IO:
            emit b->analogRead(currentChannel, unpack14(params));
            break;
        case CMD_DIGITAL_IO:
            emit b->digitalRead(currentChannel, unpack14(params));
            break;
        case CMD_PROTOCOL_VERSION:
            emit b->protocolVersion(params[0], params[1]);
            qDebug("Firmata v%i.%i", params[0], params[1]);
            ready = true;
            break;
        default:
            qWarning("Unknown command 0x%x", currentCommand);
        }
    }
};

FirmataBackend::FirmataBackend(QObject *parent)
    : QObject(parent),
      d(new Private(this))
{
}

FirmataBackend::~FirmataBackend()
{
    delete d;
}

bool FirmataBackend::isAvailable() const
{
    return d->available;
}

void FirmataBackend::setAvailable(bool a)
{
    if (a != d->available) {
        d->available = a;
        emit availabilityChanged(a);
    }
}

bool FirmataBackend::isReady() const
{
    return d->ready;
}

bool FirmataBackend::waitForReady(time_t timeoutMsec)
{
    // check if the connection is working by requesting
    // the firmata protocol version.
    d->ready = false;
    reportProtocolVersion();

    auto waitTime = QTime::currentTime().addMSecs(timeoutMsec);
    auto etime = waitTime.msec();
    etime = etime / 4;
    while (QTime::currentTime() < waitTime) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, etime);
        if (isReady())
            break;
    }

    return isReady();
}

QString FirmataBackend::statusText() const
{
    return d->statusText;
}

void FirmataBackend::setStatusText(const QString &text)
{
    if (d->statusText != text) {
        d->statusText = text;
        emit statusTextChanged(text);
    }
}

void FirmataBackend::writeAnalogPin(uint8_t pin, uint16_t value)
{
    qDebug("Write analog pin %d <- %d", pin, value);
    if (pin < 0x10) {
        const uint8_t buffer[]{cmdPin(CMD_ANALOG_IO, pin), lsb14(value), msb14(value)};
        writeBuffer(buffer, sizeof(buffer));

    } else if (pin < 0x80) {
        const uint8_t buffer[]{
            CMD_SYSEX_START,
            SYSEX_EXTENDED_ANALOG,
            pin,
            lsb14(value),
            msb14(value), // TODO support >14bit resolutions?
            CMD_SYSEX_END};
        writeBuffer(buffer, sizeof(buffer));

    } else {
        qFatal("Analog pin %d not supported. Max is 127", pin);
    }
}

void FirmataBackend::writeDigitalPin(uint8_t pin, bool value)
{
    qDebug("Write digital pin %d <- %d", pin, value);
    if (pin < 0x80) {
        const uint8_t buffer[]{CMD_SET_DIGITAL_PIN, pin, value};
        writeBuffer(buffer, sizeof(buffer));

    } else {
        qFatal("Pin %d not supported (max is 127)", pin);
    }
}

void FirmataBackend::reportAnalogPin(uint8_t pin, bool enable)
{
    qDebug("Report analog pin %d = %s", pin, enable ? "on" : "off");
    if (pin < 0x10) {
        const uint8_t buffer[]{cmdPin(CMD_ANALOG_REPORT, pin), enable};
        writeBuffer(buffer, sizeof(buffer));

    } else {
        qFatal("Reporting analog channel %d is not supported (max is 15)", pin);
    }
}

void FirmataBackend::reportDigitalPort(uint8_t port, bool enable)
{
    qDebug("Report digital port %d = %s", port, enable ? "on" : "off");
    if (port < 0x10) {
        const uint8_t buffer[]{cmdPin(CMD_DIGITAL_REPORT, port), enable};
        writeBuffer(buffer, sizeof(buffer));

    } else {
        qFatal("Digital port %d not supported (max is 15)", port);
    }
}

void FirmataBackend::reportProtocolVersion()
{
    qDebug("Requested protocol version");
    writeBuffer(&CMD_PROTOCOL_VERSION, 1);
}

void FirmataBackend::setPinMode(uint8_t pin, IoMode mode)
{
    qDebug("Set pin mode %d = %d", pin, int(mode));
    if (pin < 0x80) {
        const uint8_t buffer[]{CMD_SET_PINMODE, pin, uint8_t(mode)};
        writeBuffer(buffer, sizeof(buffer));

    } else {
        qFatal("Pins over 127 (%d) not supported", pin);
    }
}

void FirmataBackend::writeSysex(const uint8_t *data, int len)
{
    Q_ASSERT(len > 0);
#ifndef NDEBUG
    for (int i = 0; i < len; ++i) {
        if (data[i] & 0x80) {
            qFatal("writeSysex: data must be 7-bit!");
        }
    }
#endif
    qDebug("Writing sysex 0x%x (payload len=%d)", data[0], len - 1);
    writeBuffer(&CMD_SYSEX_START, 1);
    writeBuffer(data, len);
    writeBuffer(&CMD_SYSEX_END, 1);
}

void FirmataBackend::bytesRead(const char *data, int len)
{
    while (len-- > 0) {
        d->parse(*(data++));
    }
}

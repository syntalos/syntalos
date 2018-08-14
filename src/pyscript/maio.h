/*
 * Copyright (C) 2016-2018 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MAIO_H
#define MAIO_H

#include <QObject>
#include <QHash>
#include <mutex>

class SerialFirmata;

enum PinKind
{
    Unknown,
    Digital,
    Analog
};

struct FmPin
{
    PinKind kind;
    bool output;
    uint8_t id;
};

class MaIO : public QObject
{
    Q_OBJECT
public:
    static MaIO *instance() {
        std::lock_guard<std::mutex> lock(_mutex);
        static MaIO *_instance = nullptr;
        if (_instance == nullptr) {
            _instance = new MaIO();
        }
        return _instance;
    }

    void setFirmata(SerialFirmata *firmata);

    void newDigitalPin(int pinID, const QString& pinName, bool output, bool pullUp);
    void newDigitalPin(int pinID, const QString& pinName, const QString& kind);

    void pinSetValue(const QString& pinName, bool value);
    void pinSignalPulse(const QString& pinName);

    void sleep(uint msecs);
    void wait(uint msecs);

    void saveEvent(const QString& message);
    void saveEvent(const QStringList& messages);

    void setEventsHeader(const QStringList& headers);

signals:
    void valueChanged(const QString& pinName, bool value);

    void eventSaved(const QStringList& messages);
    void headersSet(const QStringList& headers);

private slots:
    void onDigitalRead(uint8_t port, uint8_t value);
    void onDigitalPinRead(uint8_t pin, bool value);

private:
    static std::mutex _mutex;
    explicit MaIO(QObject *parent = 0);
    Q_DISABLE_COPY(MaIO)

    SerialFirmata *m_firmata;

    QHash<QString, FmPin> m_namePinMap;
    QHash<int, QString> m_pinNameMap;
};

void pythonRegisterMaioModule();

#endif // MAIO_H

/*
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef MAZEIO_H
#define MAZEIO_H

#include <QObject>
#include <QHash>
#include <QScriptValue>
#include <QScriptable>

class SerialFirmata;

class MazeIO : public QObject, protected QScriptable
{
    Q_OBJECT
public:
    explicit MazeIO(SerialFirmata *firmata, QObject *parent = 0);

    void newDigitalPin(int pinID, const QString& pinName, bool output);

public slots:
    void newDigitalPin(int pinID, const QString& pinName, const QString& kind);

    void pinSetValue(const QString& pinName, bool value);
    void pinSignalPulse(const QString& pinName);

    void sleep(uint msecs);
    void wait(uint msecs);
    void setTimeout(QScriptValue fn, int msec);

    void saveEvent(const QString& message);
    void saveEvent(const QStringList& messages);

    void setEventsHeader(const QStringList& headers);

signals:
    void valueChanged(const QString& pinName, bool value);

    void eventSaved(const QStringList& messages);
    void headersSet(const QStringList& headers);

private slots:
    void onDigitalPinRead(uint8_t pin, bool value);

private:
    SerialFirmata *m_firmata;

    QHash<QString, int> m_namePinMap;
    QHash<int, QString> m_pinNameMap;
};

#endif // MAZEIO_H

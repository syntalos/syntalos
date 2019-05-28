/*
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef FIRMATAIOMODULE_H
#define FIRMATAIOMODULE_H

#include <QObject>
#include <chrono>
#include <QQueue>
#include <QMutex>
#include "abstractmodule.h"

class SerialFirmata;
class FirmataSettingsDialog;

enum class PinKind
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

class FirmataIOModule : public AbstractModule
{
    Q_OBJECT
public:
    explicit FirmataIOModule(QObject *parent = nullptr);
    ~FirmataIOModule() override;

    QString id() const override;
    QString description() const override;
    QPixmap pixmap() const override;
    ModuleFeatures features() const override;

    bool initialize(ModuleManager *manager) override;
    bool prepare(const QString& storageRootDir, const TestSubject& testSubject, HRTimer *timer) override;
    void stop() override;

    bool fetchDigitalInput(QPair<QString, bool> *result);
    void newDigitalPin(int pinID, const QString& pinName, bool output, bool pullUp);
    void pinSetValue(const QString& pinName, bool value);
    void pinSignalPulse(const QString& pinName);

private slots:
    void recvDigitalRead(uint8_t port, uint8_t value);
    void recvDigitalPinRead(uint8_t pin, bool value);

private:
    FirmataSettingsDialog *m_settingsDialog;
    SerialFirmata *m_firmata;

    QMutex m_mutex;
    QQueue<QPair<QString, bool>> m_changedValuesQueue;
    QHash<QString, FmPin> m_namePinMap;
    QHash<int, QString> m_pinNameMap;
};

#endif // FIRMATAIOMODULE_H

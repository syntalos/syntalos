/*
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "moduleapi.h"
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <chrono>

SYNTALOS_DECLARE_MODULE

namespace Syntalos
{
Q_DECLARE_LOGGING_CATEGORY(logFmMod)
}

class SerialFirmata;
class FirmataSettingsDialog;

class FirmataIOModuleInfo : public ModuleInfo
{
public:
    QString id() const override;
    QString name() const override;
    QString description() const override;
    QString license() const override;
    ModuleCategories categories() const override;
    AbstractModule *createModule(QObject *parent = nullptr) override;
};

#endif // FIRMATAIOMODULE_H

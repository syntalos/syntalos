/*
 * Copyright (C) 2019-2021 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QObject>
#include "moduleapi.h"

SYNTALOS_DECLARE_MODULE

class IntanRhxModuleInfo : public ModuleInfo
{
public:
    QString id() const override;
    QString name() const override;
    QString description() const override;
    QString license() const override;
    QIcon icon() const override;
    bool singleton() const;
    AbstractModule *createModule(QObject *parent = nullptr) override;
};

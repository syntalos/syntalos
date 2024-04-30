/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "moduleapi.h"
#include <QObject>

using namespace Syntalos;

SYNTALOS_DECLARE_MODULE

class PyOOPTestModuleInfo : public ModuleInfo
{
public:
    QString id() const final;
    QString name() const final;
    QString description() const final;
    QIcon icon() const final;
    ModuleCategories categories() const final;
    AbstractModule *createModule(QObject *parent = nullptr) final;
};

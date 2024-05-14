/**
 * Copyright (C) 2020-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include <QObject>

#include "moduleapi.h"

SYNTALOS_DECLARE_MODULE
Q_DECLARE_LOGGING_CATEGORY(logModAudio)

class AudioSourceModuleInfo : public ModuleInfo
{
public:
    QString id() const final;
    QString name() const final;
    QString description() const final;
    ModuleCategories categories() const final;
    void refreshIcon() override;
    AbstractModule *createModule(QObject *parent = nullptr) final;
};

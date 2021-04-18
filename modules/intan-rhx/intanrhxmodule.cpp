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

#include "intanrhxmodule.h"

SYNTALOS_MODULE(IntanRhxModule)

class IntanRhxModule : public AbstractModule
{
private:

public:
    explicit IntanRhxModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {

    }

    ~IntanRhxModule() override
    {

    }

    ModuleFeatures features() const override
    {
        ModuleFeatures flags = ModuleFeature::SHOW_SETTINGS |
                                ModuleFeature::CORE_AFFINITY |
                                ModuleFeature::REALTIME;
        return flags;
    }

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::THREAD_DEDICATED;
    }

    bool prepare(const TestSubject &) override
    {
        return false;
    }

private:

};

QString IntanRhxModuleInfo::id() const
{
    return QStringLiteral("intan-rhx");
}

QString IntanRhxModuleInfo::name() const
{
    return QStringLiteral("Intan RHX");
}

QString IntanRhxModuleInfo::description() const
{
    return QStringLiteral("Record electrophysiological signals from any Intan RHD or RHS system using "
                          "an RHD USB interface board, RHD recording controller, or RHS stim/recording controller.");
}

QIcon IntanRhxModuleInfo::icon() const
{
    return QIcon(":/module/intan-rhx");
}

AbstractModule *IntanRhxModuleInfo::createModule(QObject *parent)
{
    return new IntanRhxModule(parent);
}

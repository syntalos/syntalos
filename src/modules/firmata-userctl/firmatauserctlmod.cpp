/*
 * Copyright (C) 2010 Matthias Klumpp <matthias@tenstral.net>
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

#include "firmatauserctlmod.h"

#include "firmatactldialog.h"

class FirmataUserCtlModule : public AbstractModule
{
private:
    std::shared_ptr<StreamInputPort<FirmataData>> m_fmInPort;
    std::shared_ptr<DataStream<FirmataControl>> m_fmCtlStream;
    FirmataCtlDialog *m_ctlDialog;

public:
    explicit FirmataUserCtlModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_fmInPort = registerInputPort<FirmataData>(QStringLiteral("firmata-in"), QStringLiteral("Firmata Input"));
        m_fmCtlStream = registerOutputPort<FirmataControl>(QStringLiteral("firmata-out"), QStringLiteral("Firmata Control"));
        m_ctlDialog = new FirmataCtlDialog(m_fmCtlStream);
        addDisplayWindow(m_ctlDialog);
    }

    ~FirmataUserCtlModule() override
    {}

    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_DISPLAY;
    }

    bool prepare(const TestSubject &) override
    {
        return true;
    }

private:

};

QString FirmataUserCtlModuleInfo::id() const
{
    return QStringLiteral("firmatauserctl");
}

QString FirmataUserCtlModuleInfo::name() const
{
    return QStringLiteral("Firmata Manual I/O");
}

QString FirmataUserCtlModuleInfo::description() const
{
    return QStringLiteral("Have the user control and view Firmata input/output manually.");
}

QPixmap FirmataUserCtlModuleInfo::pixmap() const
{
    return QPixmap(":/module/firmatauserctl");
}

AbstractModule *FirmataUserCtlModuleInfo::createModule(QObject *parent)
{
    return new FirmataUserCtlModule(parent);
}

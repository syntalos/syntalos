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

#include "runcmdmodule.h"

#include <QMessageBox>

RunCmdModule::RunCmdModule(QObject *parent)
    : AbstractModule(parent)
{
    m_name = QStringLiteral("Run Command");
}

RunCmdModule::~RunCmdModule()
{

}

QString RunCmdModule::id() const
{
    return QStringLiteral("runcmd");
}

QString RunCmdModule::description() const
{
    return QStringLiteral("Run external commands at various stages of the experiment.");
}

QPixmap RunCmdModule::pixmap() const
{
    return QPixmap(":/module/runcmd");
}

bool RunCmdModule::initialize(ModuleManager *manager)
{
    Q_UNUSED(manager);
    assert(!initialized());
    setState(ModuleState::INITIALIZING);

    setState(ModuleState::READY);
    setInitialized();
    return true;
}

bool RunCmdModule::prepare(const QString &storageRootDir, const TestSubject &testSubject, HRTimer *timer)
{
    Q_UNUSED(storageRootDir);
    Q_UNUSED(testSubject);
    Q_UNUSED(timer);
    setState(ModuleState::PREPARING);


    setState(ModuleState::WAITING);
    return true;
}

void RunCmdModule::stop()
{

}

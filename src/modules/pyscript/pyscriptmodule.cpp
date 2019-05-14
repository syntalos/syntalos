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

#include "pyscriptmodule.h"

#include <QMessageBox>

PyScriptModule::PyScriptModule(QObject *parent)
    : AbstractModule(parent)
{
    m_name = QStringLiteral("Python Script");
}

PyScriptModule::~PyScriptModule()
{

}

QString PyScriptModule::id() const
{
    return QStringLiteral("pyscript");
}

QString PyScriptModule::description() const
{
    return QStringLiteral("Control certain aspects of MazeAmaze (most notably Firmata I/O) using a Python script.");
}

QPixmap PyScriptModule::pixmap() const
{
    return QPixmap(":/module/python");
}

bool PyScriptModule::initialize(ModuleManager *manager)
{
    assert(!initialized());
    setState(ModuleState::INITIALIZING);

    setState(ModuleState::READY);
    setInitialized();
    return true;
}

bool PyScriptModule::prepare(const QString &storageRootDir, const TestSubject &testSubject, HRTimer *timer)
{
    Q_UNUSED(storageRootDir);
    Q_UNUSED(testSubject);
    Q_UNUSED(timer);
    setState(ModuleState::PREPARING);


    setState(ModuleState::WAITING);
    return true;
}

void PyScriptModule::stop()
{

}

void PyScriptModule::showDisplayUi()
{

}

void PyScriptModule::hideDisplayUi()
{

}

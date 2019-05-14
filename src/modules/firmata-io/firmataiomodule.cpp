/**
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

#include "firmataiomodule.h"

#include <QMessageBox>
#include "firmatasettingsdialog.h"

FirmataIOModule::FirmataIOModule(QObject *parent)
    : AbstractModule(parent),
      m_settingsDialog(nullptr)
{
    m_name = QStringLiteral("Firmata I/O");
}

FirmataIOModule::~FirmataIOModule()
{
    if (m_settingsDialog != nullptr)
        delete m_settingsDialog;
}

QString FirmataIOModule::id() const
{
    return QStringLiteral("firmata-io");
}

QString FirmataIOModule::description() const
{
    return QStringLiteral("Control input/output of a devive (i.e. an Arduino) via the Firmata protocol.");
}

QPixmap FirmataIOModule::pixmap() const
{
    return QPixmap(":/module/firmata-io");
}

bool FirmataIOModule::initialize(ModuleManager *manager)
{
    assert(!initialized());
    setState(ModuleState::INITIALIZING);

    m_settingsDialog = new FirmataSettingsDialog;

    setState(ModuleState::READY);
    setInitialized();
    return true;
}

bool FirmataIOModule::prepare(const QString &storageRootDir, const TestSubject &testSubject, HRTimer *timer)
{
    Q_UNUSED(storageRootDir);
    Q_UNUSED(testSubject);
    Q_UNUSED(timer);
    setState(ModuleState::PREPARING);


    setState(ModuleState::WAITING);
    return true;
}

void FirmataIOModule::stop()
{

}

void FirmataIOModule::showSettingsUi()
{
    assert(initialized());
    m_settingsDialog->show();
}

void FirmataIOModule::hideSettingsUi()
{
    assert(initialized());
    m_settingsDialog->hide();
}

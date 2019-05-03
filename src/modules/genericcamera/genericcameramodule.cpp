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

#include "genericcameramodule.h"

#include <QMessageBox>

GenericCameraModule::GenericCameraModule(QObject *parent)
    : ImageSourceModule(parent)
{
}

GenericCameraModule::~GenericCameraModule()
{
}

QString GenericCameraModule::id() const
{
    return QStringLiteral("generic-camera");
}

QString GenericCameraModule::displayName() const
{
    return QStringLiteral("Generic Camera");
}

QString GenericCameraModule::description() const
{
    return QStringLiteral("Record a video with a regular, Linux-compatible camera.");
}

QPixmap GenericCameraModule::pixmap() const
{
    return QPixmap(":/module/generic-camera");
}

bool GenericCameraModule::initialize(ModuleManager *manager)
{
    assert(!initialized());

    setState(ModuleState::READY);
    setInitialized();
    return true;
}

bool GenericCameraModule::prepare(const QString &storageRootDir, const QString &subjectId)
{
    Q_UNUSED(storageRootDir);
    Q_UNUSED(subjectId);

    return true;
}

void GenericCameraModule::stop()
{

}

void GenericCameraModule::showDisplayUi()
{
}

void GenericCameraModule::hideDisplayUi()
{
}

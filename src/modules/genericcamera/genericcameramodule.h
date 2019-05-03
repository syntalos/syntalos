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

#ifndef GENERICCAMERAMODULE_H
#define GENERICCAMERAMODULE_H

#include <QObject>
#include <chrono>

#include "imagesourcemodule.h"
#include "abstractmodule.h"

class GenericCameraModule : public ImageSourceModule
{
    Q_OBJECT
public:
    explicit GenericCameraModule(QObject *parent = nullptr);
    ~GenericCameraModule();

    QString id() const override;
    QString displayName() const;
    QString description() const;
    QPixmap pixmap() const;

    bool initialize(ModuleManager *manager);
    bool prepare(const QString& storageRootDir, const QString& subjectId);
    void stop();

    void showDisplayUi();
    void hideDisplayUi();

private:

};

#endif // GENERICCAMERAMODULE_H

/*
 * Copyright (C) 2019 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#include <QObject>
#include <QList>
#include <QPixmap>
#include <QScopedPointer>

namespace Syntalos {
class ModuleInfo;
}
using namespace Syntalos;

/**
 * @brief The ModuleLibrary class
 * Manage the lifecycle of MazeAmaze modules.
 */
class ModuleLibrary : public QObject
{
    Q_OBJECT
public:
    explicit ModuleLibrary(QObject *parent = nullptr);
    ~ModuleLibrary();

    QList<QSharedPointer<ModuleInfo>> moduleInfo() const;
    QSharedPointer<ModuleInfo> moduleInfo(const QString &id);

private:
    Q_DISABLE_COPY(ModuleLibrary)
    class Private;
    QScopedPointer<Private> d;

    template<typename T> void registerModuleInfo();
};

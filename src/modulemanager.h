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

#ifndef MODULEMANAGER_H
#define MODULEMANAGER_H

#include <QObject>
#include <QList>
#include <QPixmap>
#include <QSharedDataPointer>

class ModuleInfo;
class AbstractModule;
class Engine;

/**
 * @brief The ModuleManager class
 * Manage the lifecycle of MazeAmaze modules.
 */
class ModuleManager : public QObject
{
    Q_OBJECT
public:
    explicit ModuleManager(Engine *engine);
    ~ModuleManager();

    QList<QSharedPointer<ModuleInfo>> moduleInfo() const;

    AbstractModule *createModule(const QString& id);
    bool removeModule(AbstractModule *mod);

    QList<AbstractModule*> activeModules() const;

    void removeAll();

signals:
    void moduleCreated(ModuleInfo *info, AbstractModule *mod);
    void modulePreRemove(AbstractModule *mod);

private:
    Q_DISABLE_COPY(ModuleManager)
    class MMData;
    QSharedDataPointer<MMData> d;

    bool removeModuleImmediately(AbstractModule *mod);
    template<typename T> void registerModuleInfo();
};

#endif // MODULEMANAGER_H

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

class AbstractModule;
class AbstractModuleCreator;

class ModuleInfo {
public:
    QString id;
    QString displayName;
    QString description;
    QString license;
    QPixmap pixmap;
    bool singleton;
    int count;
};

/**
 * @brief The ModuleManager class
 * Manage the lifecycle of MazeAmaze modules.
 */
class ModuleManager : public QObject
{
    Q_OBJECT
public:
    explicit ModuleManager(QObject *parent = nullptr, QWidget *parentWidget = nullptr);

    QList<QSharedPointer<ModuleInfo>> moduleInfo() const;

    AbstractModule *createModule(const QString& id);
    bool removeModule(AbstractModule *mod);

    QList<AbstractModule*> activeModules() const;

    void removeAll();

signals:
    void moduleCreated(AbstractModule *mod);
    void modulePreRemove(AbstractModule *mod);
    void moduleError(AbstractModule *mod, const QString& message);

private slots:
    void receiveModuleError(const QString& message);

private:
    class MMData;
    QSharedPointer<MMData> d; // FIXME

    bool removeModuleImmediately(AbstractModule *mod);
    template<typename T> void registerModuleInfo();
};

#endif // MODULEMANAGER_H

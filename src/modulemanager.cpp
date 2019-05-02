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

#include "modulemanager.h"

#include <QMessageBox>

#include "modules/rhd2000/rhd2000module.h"
#include "modules/traceplot/traceplotmodule.h"

ModuleManager::ModuleManager(QObject *parent)
    : QObject(parent)
{
    registerModuleInfo<Rhd2000Module>();
    registerModuleInfo<TracePlotModule>();
}

AbstractModule *ModuleManager::createModule(const QString &id)
{
    AbstractModule *mod = nullptr;
    if (id == Rhd2000Module::id())
        mod = new Rhd2000Module(this);
    if (id == TracePlotModule::id())
        mod = new TracePlotModule(this);

    // Ensure we don't register a module twice that should only exist once
    if (mod->singleton()) {
        Q_FOREACH(auto emod, m_modules) {
            if (emod->id() == id) {
                delete mod;
                return nullptr;
            }
        }
    }

    // Update module info
    Q_FOREACH(auto info, m_modInfo) {
        if (info.id == id)
            info.count++;
    }

    m_modules.append(mod);
    emit moduleCreated(mod);

    // the module has been created and registered, we can
    // safely initialize it now.
    if (!mod->initialize()) {
        QMessageBox::critical(nullptr, QStringLiteral("Module initialization failed"),
                              QStringLiteral("Failed to initialize module %1, it can not be added. Message: %2").arg(id).arg(mod->lastError()),
                              QMessageBox::Ok);
        removeModule(mod);
    }

    return mod;
}

bool ModuleManager::removeModule(AbstractModule *mod)
{
    auto id = mod->id();
    if (m_modules.removeOne(mod)) {
        Q_FOREACH(auto info, m_modInfo) {
            if (info.id == id)
                info.count--;
        }

        emit modulePreRemove(mod);
        delete mod;
        return true;
    }

    return false;
}

QList<AbstractModule *> ModuleManager::activeModules() const
{
    return m_modules;
}

QList<ModuleInfo> ModuleManager::moduleInfo() const
{
    return m_modInfo;
}

template<typename T>
void ModuleManager::registerModuleInfo()
{
    auto tmp = new T;
    ModuleInfo info;
    info.id = T::id();
    info.pixmap = tmp->pixmap();
    info.displayName = tmp->displayName();
    info.description = tmp->description();
    info.singleton = tmp->singleton();
    info.count = 0;
    delete tmp;

    m_modInfo.append(info);
}

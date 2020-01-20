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

#include "config.h"
#include "modulemanager.h"

#include <QMessageBox>
#include <QDebug>

#include "engine.h"

#include "modules/rhd2000/rhd2000module.h"
#include "modules/traceplot/traceplotmodule.h"
#include "modules/videorecorder/videorecordmodule.h"
#include "modules/genericcamera/genericcameramodule.h"
#ifdef HAVE_UEYE_CAMERA
#include "modules/ueyecamera/ueyecameramodule.h"
#endif
#ifdef HAVE_MINISCOPE
#include "modules/miniscope/miniscopemodule.h"
#endif
//#include "modules/triled-tracker/triledtrackermodule.h"
#include "modules/firmata-io/firmataiomodule.h"
#include "modules/pyscript/pyscriptmodule.h"
#include "modules/runcmd/runcmdmodule.h"

#pragma GCC diagnostic ignored "-Wpadded"
class ModuleManager::MMData : public QSharedData
{
public:
    MMData() { }
    ~MMData() { }

    QMap<QString, QSharedPointer<ModuleInfo>> modInfos;

    Engine *engine;
    QList<AbstractModule*> modules;
};
#pragma GCC diagnostic pop

ModuleManager::ModuleManager(Engine *engine)
    : QObject(engine),
      d(new MMData)
{
    d->engine = engine;

    registerModuleInfo<Rhd2000ModuleInfo>();
    registerModuleInfo<TracePlotModuleInfo>();
    registerModuleInfo<VideoRecorderModuleInfo>();
    registerModuleInfo<GenericCameraModuleInfo>();
#ifdef HAVE_UEYE_CAMERA
    registerModuleInfo<UEyeCameraModuleInfo>();
#endif
#ifdef HAVE_MINISCOPE
    registerModuleInfo<MiniscopeModuleInfo>();
#endif
    //registerModuleInfo<TriLedTrackerModuleInfo>();
    registerModuleInfo<FirmataIOModuleInfo>();
    registerModuleInfo<PyScriptModuleInfo>();
    registerModuleInfo<RunCmdModuleInfo>();
}

ModuleManager::~ModuleManager()
{
    removeAll();
}

template<typename T>
void ModuleManager::registerModuleInfo()
{
    QSharedPointer<ModuleInfo> info(new T);
    info->setCount(0);
    d->modInfos.insert(info->id(), info);
}

AbstractModule *ModuleManager::createModule(const QString &id)
{
    auto modInfo = d->modInfos.value(id);
    if (modInfo == nullptr)
        return nullptr;

    // Ensure we don't register a module twice that should only exist once
    if (modInfo->singleton()) {
        for (auto &emod : d->modules) {
            if (emod->id() == id)
                return nullptr;
        }
    }

    auto mod = modInfo->createModule();
    assert(mod);
    mod->setId(modInfo->id());
    modInfo->setCount(modInfo->count() + 1);
    if (modInfo->count() > 1)
        mod->setName(QStringLiteral("%1 - %2").arg(modInfo->name()).arg(modInfo->count()));
    else
        mod->setName(modInfo->name());

    d->modules.append(mod);
    emit moduleCreated(modInfo.get(), mod);

    // the module has been created and registered, we can
    // safely initialize it now.
    mod->setState(ModuleState::INITIALIZING);
    if (!d->engine->initializeModule(mod)) {
        removeModuleImmediately(mod);
        return nullptr;
    }

    return mod;
}

bool ModuleManager::removeModuleImmediately(AbstractModule *mod)
{
    auto id = mod->id();
    if (d->modules.removeOne(mod)) {
        // Update module info
        auto modInfo = d->modInfos.value(id);
        modInfo->setCount(modInfo->count() - 1);

        emit modulePreRemove(mod);
        delete mod;
        return true;
    }

    return false;
}

bool ModuleManager::removeModule(AbstractModule *mod)
{
    return removeModuleImmediately(mod);
}

QList<AbstractModule *> ModuleManager::activeModules() const
{
    return d->modules;
}

void ModuleManager::removeAll()
{
    foreach (auto mod, d->modules)
        removeModuleImmediately(mod);
}

QList<QSharedPointer<ModuleInfo> > ModuleManager::moduleInfo() const
{
    return d->modInfos.values();
}

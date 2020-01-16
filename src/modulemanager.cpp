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

    QWidget *parentWidget;
    QList<AbstractModule*> modules;
};
#pragma GCC diagnostic pop

ModuleManager::ModuleManager(QWidget *parentWidget)
    : QObject(parentWidget),
      d(new MMData)
{
    d->parentWidget = parentWidget;

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
        Q_FOREACH(auto emod, d->modules) {
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
    if (!mod->initialize(this)) {
        QMessageBox::critical(d->parentWidget, QStringLiteral("Module initialization failed"),
                              QStringLiteral("Failed to initialize module %1, it can not be added. Message: %2").arg(id).arg(mod->lastError()),
                              QMessageBox::Ok);
        removeModule(mod);
        return nullptr;
    }

    connect(mod, &AbstractModule::error, this, &ModuleManager::receiveModuleError);

    return mod;
}

void ModuleManager::receiveModuleError(const QString& message)
{
    auto mod = qobject_cast<AbstractModule*>(sender());
    if (mod != nullptr)
        emit moduleError(mod, message);
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
    Q_FOREACH(auto emod, d->modules) {
        if (!emod->canRemove(mod)) {
            // oh no! Another module tries to prevent the removal of the current module.
            // Let's notify about that, then stop removing the module.
            QMessageBox::information(d->parentWidget, QStringLiteral("Can not remove module"),
                                  QStringLiteral("The '%1' module can not be removed, because the '%2' module depends on it. Please remove '%2' first!").arg(mod->name()).arg(emod->name()),
                                  QMessageBox::Ok);
            return false;
        }
    }

    return removeModuleImmediately(mod);
}

QList<AbstractModule *> ModuleManager::activeModules() const
{
    return d->modules;
}

QList<AbstractModule *> ModuleManager::createOrderedModuleList()
{
    // we need to do some ordering so modules which consume data from other modules get
    // activated last.
    // this implicit sorting isn't great, and we might - if MazeAmaze becomes more complex than
    // it already is - replace this with declarative ordering a proper dependency resolution
    // at some point.
    // we don't use Qt sorting facilities here, because we want at least some of the original
    // module order to be kept.
    QList<AbstractModule*> orderedActiveModules;
    QList<AbstractModule*> scriptModList;
    int firstImgSinkModIdx = -1;

    orderedActiveModules.reserve(d->modules.length());
    Q_FOREACH(auto mod, d->modules) {
        if (qobject_cast<ImageSourceModule*>(mod) != nullptr) {
            if (firstImgSinkModIdx >= 0) {
                // put in before the first sink
                orderedActiveModules.insert(firstImgSinkModIdx, mod);
                continue;
            } else {
                // just add the module
                orderedActiveModules.append(mod);
                continue;
            }
        }
#if 0
        else if (qobject_cast<ImageSinkModule*>(mod) != nullptr) {
            if (firstImgSinkModIdx < 0) {
                // we have the first sink module
                orderedActiveModules.append(mod);
                firstImgSinkModIdx = orderedActiveModules.length() - 1;
                continue;
            } else {
                // put in after the first sink
                orderedActiveModules.insert(firstImgSinkModIdx + 1, mod);
                continue;
            }
        }
#endif
        else if (qobject_cast<PyScriptModule*>(mod) != nullptr) {
            // scripts are always initialized last, as they may arbitrarily connect
            // to the other modules to control them.
            // and we rather want that to happen when everything is prepared to run.
            scriptModList.append(mod);
            continue;
        } else {
            orderedActiveModules.append(mod);
        }
    }

    Q_FOREACH(auto mod, scriptModList)
        orderedActiveModules.append(mod);


    auto debugText = QStringLiteral("Running modules in order: ");
    Q_FOREACH(auto mod, orderedActiveModules)
        debugText.append(mod->name() + QStringLiteral("; "));
    qDebug().noquote() << debugText;

    return orderedActiveModules;
}

void ModuleManager::removeAll()
{
    Q_FOREACH(auto mod, d->modules) {
        removeModuleImmediately(mod);
    }
}

QWidget *ModuleManager::parentWidget() const
{
    return d->parentWidget;
}

QList<QSharedPointer<ModuleInfo> > ModuleManager::moduleInfo() const
{
    return d->modInfos.values();
}

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
#include "modules/triled-tracker/triledtrackermodule.h"
#include "modules/firmata-io/firmataiomodule.h"
#include "modules/pyscript/pyscriptmodule.h"
#include "modules/runcmd/runcmdmodule.h"

class AbstractModuleCreator
{
public:
    AbstractModuleCreator() { }
    virtual ~AbstractModuleCreator();

    virtual AbstractModule *createModule() = 0;
};
AbstractModuleCreator::~AbstractModuleCreator() {};

template <typename T>
class ModuleCreator : public AbstractModuleCreator
{
public:
    ModuleCreator() { }
    ~ModuleCreator() override { }

    T *createModule() override
    {
        return new T;
    }
};

#pragma GCC diagnostic ignored "-Wpadded"
class ModuleManager::MMData : public QSharedData
{
public:
    MMData() { }
    ~MMData() { }

    QWidget *parentWidget;
    QList<QSharedPointer<ModuleInfo>> modInfo;
    QList<AbstractModule*> modules;

    QHash<QString, QSharedPointer<AbstractModuleCreator>> creators;
};
#pragma GCC diagnostic pop

ModuleManager::ModuleManager(QObject *parent, QWidget *parentWidget)
    : QObject(parent),
      d(new MMData)
{
    d->parentWidget = parentWidget;

    registerModuleInfo<Rhd2000Module>();
    registerModuleInfo<TracePlotModule>();
    registerModuleInfo<VideoRecorderModule>();
    registerModuleInfo<GenericCameraModule>();
#ifdef HAVE_UEYE_CAMERA
    registerModuleInfo<UEyeCameraModule>();
#endif
    registerModuleInfo<TriLedTrackerModule>();
    registerModuleInfo<FirmataIOModule>();
    registerModuleInfo<PyScriptModule>();
    registerModuleInfo<RunCmdModule>();
}

AbstractModule *ModuleManager::createModule(const QString &id)
{
    AbstractModule *mod = nullptr;
    auto creator = d->creators.value(id);
    if (creator == nullptr)
        return nullptr;
    mod = creator->createModule();

    // Ensure we don't register a module twice that should only exist once
    if (mod->singleton()) {
        Q_FOREACH(auto emod, d->modules) {
            if (emod->id() == id) {
                delete mod;
                return nullptr;
            }
        }
    }

    // Update module info
    Q_FOREACH(auto info, d->modInfo) {
        if (info->id == id) {
            info->count++;
            if (info->count > 1)
                mod->setName(QStringLiteral("%1 - %2").arg(mod->name()).arg(info->count));
            break;
        }
    }

    d->modules.append(mod);
    emit moduleCreated(mod);

    // the module has been created and registered, we can
    // safely initialize it now.
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
        Q_FOREACH(auto info, d->modInfo) {
            if (info->id == id) {
                info->count--;
                break;
            }
        }

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

void ModuleManager::removeAll()
{
    Q_FOREACH(auto mod, d->modules) {
        removeModuleImmediately(mod);
    }
}

QList<QSharedPointer<ModuleInfo> > ModuleManager::moduleInfo() const
{
    return d->modInfo;
}

template<typename T>
void ModuleManager::registerModuleInfo()
{
    auto tmp = new T;
    QSharedPointer<ModuleInfo> info(new ModuleInfo);
    info->id = tmp->id();
    info->pixmap = tmp->pixmap();
    info->displayName = tmp->name();
    info->description = tmp->description();
    info->license = tmp->license();
    info->singleton = tmp->singleton();
    info->count = 0;
    delete tmp;

    QSharedPointer<ModuleCreator<T>> creator(new ModuleCreator<T>);
    d->creators.insert(info->id, creator);
    d->modInfo.append(info);
}

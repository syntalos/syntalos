/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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
#include "modulelibrary.h"

#include <QMessageBox>
#include <QDebug>

#include "engine.h"

#if 0
#include "modules/devel.clock/clockmodule.h"
#include "modules/devel.datasource/datasourcemodule.h"
#include "modules/devel.datasst/datasstmodule.h"
#include "modules/devel.pyooptest/pyooptestmodule.h"

#include "modules/canvas/canvasmodule.h"
#include "modules/table/tablemodule.h"

#include "modules/videorecorder/videorecordmodule.h"
#include "modules/videotransform/videotransformmodule.h"
#include "modules/camera-generic/genericcameramodule.h"
#ifdef HAVE_TIS_CAMERA
#include "modules/camera-tis/tiscameramodule.h"
#endif
#ifdef HAVE_FLIR_CAMERA
#include "modules/camera-flir/flircameramod.h"
#endif
#ifdef HAVE_UEYE_CAMERA
#include "modules/camera-ueye/ueyecameramodule.h"
#endif
#ifdef HAVE_MINISCOPE
#include "modules/miniscope/miniscopemodule.h"
#endif

#include "modules/triled-tracker/triledtrackermodule.h"

#include "modules/firmata-io/firmataiomodule.h"
#include "modules/firmata-userctl/firmatauserctlmod.h"
#include "modules/pyscript/pyscriptmodule.h"

//#include "modules/rhd2000/rhd2000module.h"
#include "modules/traceplot/traceplotmodule.h"

#include "modules/runcmd/runcmdmodule.h"
#endif

#include <QLibrary>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
class ModuleLibrary::Private
{
public:
    Private() { }
    ~Private() { }

    QMap<QString, QSharedPointer<ModuleInfo>> modInfos;
};
#pragma GCC diagnostic pop

ModuleLibrary::ModuleLibrary(QObject *parent)
    : QObject(parent),
      d(new ModuleLibrary::Private)
{
#if 0
    registerModuleInfo<DevelClockModuleInfo>();
    registerModuleInfo<DevelDataSourceModuleInfo>();
    registerModuleInfo<DevelDataSSTModuleInfo>();
    registerModuleInfo<PyOOPTestModuleInfo>();

    registerModuleInfo<CanvasModuleInfo>();
    registerModuleInfo<TableModuleInfo>();

    registerModuleInfo<VideoRecorderModuleInfo>();
    registerModuleInfo<VideoTransformModuleInfo>();
    registerModuleInfo<GenericCameraModuleInfo>();
#ifdef HAVE_TIS_CAMERA
    registerModuleInfo<TISCameraModuleInfo>();
#endif
#ifdef HAVE_FLIR_CAMERA
    registerModuleInfo<FLIRCameraModuleInfo>();
#endif
#ifdef HAVE_UEYE_CAMERA
    registerModuleInfo<UEyeCameraModuleInfo>();
#endif
#ifdef HAVE_MINISCOPE
    registerModuleInfo<MiniscopeModuleInfo>();
#endif

    registerModuleInfo<TriLedTrackerModuleInfo>();

    registerModuleInfo<FirmataIOModuleInfo>();
    registerModuleInfo<FirmataUserCtlModuleInfo>();
    registerModuleInfo<PyScriptModuleInfo>();

    //registerModuleInfo<Rhd2000ModuleInfo>();
    registerModuleInfo<TracePlotModuleInfo>();
    registerModuleInfo<RunCmdModuleInfo>();

#endif

}

ModuleLibrary::~ModuleLibrary()
{
}

template<typename T>
void ModuleLibrary::registerModuleInfo()
{
    QSharedPointer<ModuleInfo> info(new T);
    d->modInfos.insert(info->id(), info);
}

QList<QSharedPointer<ModuleInfo> > ModuleLibrary::moduleInfo() const
{
    return d->modInfos.values();
}

QSharedPointer<ModuleInfo> ModuleLibrary::moduleInfo(const QString &id)
{
    return d->modInfos.value(id);
}

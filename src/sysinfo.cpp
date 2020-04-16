/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "sysinfo.h"

#include <QSysInfo>
#include <QFile>
#include <QTextStream>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>
#include <opencv2/core.hpp>
#include <Eigen/Core>
extern "C" {
#include <libavutil/avutil.h>
}

using namespace Syntalos;

#pragma GCC diagnostic ignored "-Wpadded"
class SysInfo::Private
{
public:
    Private() { }
    ~Private() { }

    QString currentClocksource;
    QString availableClocksources;
    QString initName;
    int usbFsMemoryMb;

    QString glVersion;
    QString glExtensions;
};
#pragma GCC diagnostic pop

SysInfo::SysInfo(QObject *parent)
    : QObject(parent),
      d(new SysInfo::Private)
{
#ifndef Q_OS_LINUX
    return;
#endif

    d->currentClocksource = readSysFsValue("/sys/devices/system/clocksource/clocksource0/current_clocksource").replace("\n", "");
    d->availableClocksources = readSysFsValue("/sys/devices/system/clocksource/clocksource0/available_clocksource").replace("\n", "");
    d->initName = readSysFsValue("/proc/1/comm").replace("\n", "");
    d->usbFsMemoryMb = readSysFsValue("/sys/module/usbcore/parameters/usbfs_memory_mb").replace("\n", "").toInt();

    // try to determine OpenGL version
    QOffscreenSurface surf;
    surf.create();
    QOpenGLContext ctx;
    ctx.create();
    ctx.makeCurrent(&surf);
    d->glVersion = QString::fromUtf8((const char*)ctx.functions()->glGetString(GL_VERSION));
    d->glExtensions = QString::fromUtf8((const char*) ctx.functions()->glGetString(GL_EXTENSIONS));
}

SysInfo::~SysInfo()
{}

QString SysInfo::prettyProductName() const
{
    return QSysInfo::prettyProductName();
}

QString SysInfo::currentArchitecture() const
{
    return QSysInfo::currentCpuArchitecture();
}

QString SysInfo::kernelInfo() const
{
    return QStringLiteral("%1 %2").arg(QSysInfo::kernelType())
            .arg(QSysInfo::kernelVersion());
}

QString SysInfo::initName() const
{
    return d->initName;
}

SysInfoCheckResult SysInfo::checkInitSystem()
{
    // we communicate with systemd in some occasions,
    // and no tests have been done with other init systems
    if (d->initName.startsWith("systemd"))
        return SysInfoCheckResult::OK;
    return SysInfoCheckResult::ISSUE;
}

int SysInfo::usbFsMemoryMb() const
{
    return d->usbFsMemoryMb;
}

SysInfoCheckResult SysInfo::checkUsbFsMemory()
{
    // some cameras need a really huge buffer to function properly,
    // ideally around 1000Mb even.
    if (d->usbFsMemoryMb < 640)
        return SysInfoCheckResult::SUSPICIOUS;
    return SysInfoCheckResult::OK;
}

QString SysInfo::glVersion() const
{
    return d->glVersion;
}

QString SysInfo::glExtensions() const
{
    return d->glExtensions;
}

QString SysInfo::currentClocksource() const
{
    return d->currentClocksource;
}

QString SysInfo::availableClocksources() const
{
    return d->availableClocksources;
}

SysInfoCheckResult SysInfo::checkClocksource()
{
    // we want the CPU timestamp-counter to be default, and not
    // any of the inferior clocksources
    if (d->currentClocksource == QStringLiteral("tsc"))
        return SysInfoCheckResult::OK;
    return SysInfoCheckResult::ISSUE;
}

QString SysInfo::qtVersion() const
{
    return qVersion();
}

QString SysInfo::openCVVersionString() const
{
    return QString::fromStdString(cv::getVersionString());
}

QString SysInfo::eigenVersionString() const
{
    return QStringLiteral("%1.%2.%3").arg(EIGEN_WORLD_VERSION)
                                     .arg(EIGEN_MAJOR_VERSION)
                                     .arg(EIGEN_MINOR_VERSION);
}

QString SysInfo::ffmpegVersionString() const
{
    return QString::fromUtf8(av_version_info());
}

QString SysInfo::readSysFsValue(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    QTextStream in(&file);
    return in.readAll().simplified();
}

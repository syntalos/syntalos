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

#include <QDebug>
#include <QSysInfo>
#include <QFile>
#include <QTextStream>
#include <QThread>
#include <QSet>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>
#include <opencv2/core.hpp>
#include <Eigen/Core>
extern "C" {
#include <libavutil/avutil.h>
}

#include "utils.h"

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
    bool tscIsConstant;
    QString supportedAVXInstructions;
    QString cpu0ModelName;

    int cpuCount;
    int cpuPhysicalCoreCount;

    QString glVersion;
    QString glExtensions;
};
#pragma GCC diagnostic pop

SysInfo::SysInfo(QObject *parent)
    : QObject(parent),
      d(new SysInfo::Private)
{
    d->cpuCount = QThread::idealThreadCount();
    d->cpuPhysicalCoreCount = d->cpuCount;
#ifndef Q_OS_LINUX
    qCritical() << "We are not running on Linux - please make sure to adjust the SysInfo code when porting to other systems!";
    return;
#endif

    d->tscIsConstant = false;
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

    readCPUInfo();
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

bool SysInfo::tscIsConstant() const
{
    return d->tscIsConstant;
}

SysInfoCheckResult SysInfo::checkTSCConstant()
{
    return d->tscIsConstant? SysInfoCheckResult::OK : SysInfoCheckResult::ISSUE;
}

QString SysInfo::supportedAVXInstructions() const
{
    return d->supportedAVXInstructions;
}

SysInfoCheckResult SysInfo::checkAVXInstructions()
{
    if (d->supportedAVXInstructions.isEmpty())
        return SysInfoCheckResult::ISSUE;
    return d->supportedAVXInstructions.contains("avx2")? SysInfoCheckResult::OK : SysInfoCheckResult::SUSPICIOUS;
}

QString SysInfo::cpu0ModelName() const
{
    return d->cpu0ModelName;
}

int SysInfo::cpuCount() const
{
    return d->cpuCount;
}

int SysInfo::cpuPhysicalCoreCount() const
{
    return d->cpuPhysicalCoreCount;
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

void SysInfo::readCPUInfo()
{
    QFile file("/proc/cpuinfo");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "Unable to open /proc/cpuinfo for reading. This may be a system configuration issue.";
        return;
    }

    d->cpuCount = 0;
    d->cpuPhysicalCoreCount = 0;
    d->tscIsConstant = false;

    bool firstCPU = false;
    QSet<QString> seenCoreIds;
    QString currentPhysicalId;

    QTextStream in(&file);
    while (true) {
        const auto line = in.readLine();
        if (line.isNull()) {
            // we reached the end of this file
            break;
        }
        const auto parts = qStringSplitLimit(line, ':', 1);
        if (parts.length() < 2)
            continue;
        const auto key = parts[0].simplified().trimmed();
        const auto value = parts[1].simplified().replace("\n", "");

        if (key == "processor") {
            firstCPU = (value == "0");
            d->cpuCount++;
            continue;
        }

        if (key == "physical id") {
            currentPhysicalId = value;
            continue;
        }

        // ugly way to count physical CPU cores
        if (key == "core id") {
            const auto coreId = QStringLiteral("%1_%2").arg(currentPhysicalId).arg(value);
            if (seenCoreIds.contains(coreId))
                continue;

            d->cpuPhysicalCoreCount++;
            seenCoreIds.insert(coreId);
            continue;
        }

        if (firstCPU) {
            // for simplification, we just (inefficiently) read flags
            // from the first CPU core.
            // if there are multiple CPUs with different specs in this computer,
            // we may show wrong values due to that (but if we support that case,
            // the UI displaying these values as well as the checks themselves would
            // have to be much more complicated, and this is a niche case to support)
            if (key == "flags") {
                const auto flags = value.split(' ');

                for (const auto &flag : flags) {
                    if (flag == "constant_tsc")
                        d->tscIsConstant = true;
                    if (flag.startsWith("avx"))
                        d->supportedAVXInstructions.append(flag + " ");
                }

                continue;
            }

            if (key == "model name") {
                d->cpu0ModelName = value;
                continue;
            }
        }
    }

    // safeguard in case we failed to determine proper information for any reason
    if (d->cpuCount <= 0) {
        qCritical() << "Unable to read CPU information. Is /proc/cpuinfo accessible?";
        d->cpuCount = QThread::idealThreadCount();
        d->cpuPhysicalCoreCount = d->cpuCount;
    }
}

/*
 * Copyright (C) 2019-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include "config.h"
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
#include <sys/utsname.h>
#include <stdlib.h>
extern "C" {
#include <libavutil/avutil.h>
}

#include "rtkit.h"
#include "utils/misc.h"

using namespace Syntalos;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
class SysInfo::Private
{
public:
    Private() { }
    ~Private() { }

    QString osId;
    QString osName;
    QString osVersion;

    QString runtimeName;
    QString runtimeVersion;

    QString currentClocksource;
    QString availableClocksources;
    QString initName;
    int usbFsMemoryMb;
    bool tscIsConstant;
    QString supportedAVXInstructions;
    QString cpu0ModelName;

    int cpuCount;
    int cpuPhysicalCoreCount;

    int rtkitMaxRealtimePriority;
    int rtkitMinNiceLevel;
    long long rtkitMaxRTTimeUsec;

    QString glVersion;
    QString glExtensions;

    bool inFlatpakSandbox;
};
#pragma GCC diagnostic pop

static QString unquote(const char *begin, const char *end)
{
    // man os-release says:
    // Variable assignment values must be enclosed in double
    // or single quotes if they include spaces, semicolons or
    // other special characters outside of A–Z, a–z, 0–9. Shell
    // special characters ("$", quotes, backslash, backtick)
    // must be escaped with backslashes, following shell style.
    // All strings should be in UTF-8 format, and non-printable
    // characters should not be used. It is not supported to
    // concatenate multiple individually quoted strings.
    if (*begin == '"') {
        Q_ASSERT(end[-1] == '"');
        return QString::fromUtf8(begin + 1, end - begin - 2);
    }
    return QString::fromUtf8(begin, end - begin);
}

typedef struct
{
    QString id;
    QString name;
    QString version;
} OsReleaseInfo;

static OsReleaseInfo readOsRelease(const char *filename)
{

    const QByteArray idKey = QByteArrayLiteral("ID=");
    const QByteArray versionKey = QByteArrayLiteral("VERSION_ID=");
    const QByteArray prettyNameKey = QByteArrayLiteral("PRETTY_NAME=");
    OsReleaseInfo relInfo;

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return relInfo;

    const auto buffer = file.readAll();

    const char *ptr = buffer.constData();
    const char *end = buffer.constEnd();
    const char *eol;
    QByteArray line;
    for (; ptr != end; ptr = eol + 1) {
        // find the end of the line after ptr
        eol = static_cast<const char *>(memchr(ptr, '\n', end - ptr));
        if (!eol)
            eol = end - 1;
        line.setRawData(ptr, eol - ptr);

        if (line.startsWith(idKey)) {
            ptr += idKey.length();
            relInfo.id = unquote(ptr, eol);
            continue;
        }

        if (line.startsWith(prettyNameKey)) {
            ptr += prettyNameKey.length();
            relInfo.name = unquote(ptr, eol);
            continue;
        }

        if (line.startsWith(versionKey)) {
            ptr += versionKey.length();
            relInfo.version = unquote(ptr, eol);
            continue;
        }
    }

    return relInfo;
}

SysInfo::SysInfo()
    : QObject(nullptr),
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

    // get realtime scheduling limits set by RealtimeKit (the user may tweak those)
    RtKit rtkit;
    d->rtkitMaxRealtimePriority = rtkit.queryMaxRealtimePriority();
    d->rtkitMinNiceLevel = rtkit.queryMinNiceLevel();
    d->rtkitMaxRTTimeUsec = rtkit.queryRTTimeUSecMax();

    // try to determine OpenGL version
    QOffscreenSurface surf;
    surf.create();
    QOpenGLContext ctx;
    ctx.create();
    ctx.makeCurrent(&surf);
    d->glVersion = QString::fromUtf8((const char*)ctx.functions()->glGetString(GL_VERSION));
    d->glExtensions = QString::fromUtf8((const char*) ctx.functions()->glGetString(GL_EXTENSIONS));

    // test if we are sandboxed in a Flatpak environment
    d->inFlatpakSandbox = isInFlatpakSandbox();
    if (d->inFlatpakSandbox) {
        // we're in a Flatpak sandbox, so special rules apply to get some information about the host
        // as well as the Flatpak runtime that we are using.
        const auto osInfo = readOsRelease("/run/host/etc/os-release");
        if (osInfo.id.isEmpty()) {
            d->osId = QSysInfo::productType();
            d->osName = QSysInfo::prettyProductName();
            d->osVersion = QSysInfo::productVersion();
        } else {
            d->osId = osInfo.id;
            d->osName = osInfo.name;
            d->osVersion = osInfo.version;
        }
        d->runtimeName = QSysInfo::prettyProductName();
        d->runtimeVersion = QSysInfo::productVersion();
    } else {
        // we're not in a sandbox, so we can just take the native OS values
        d->osId = QSysInfo::productType();
        d->osName = QSysInfo::prettyProductName();
        d->osVersion = QSysInfo::productVersion();
        d->runtimeName = QSysInfo::prettyProductName();
        d->runtimeVersion = d->osVersion;
    }

    // load CPU data
    readCPUInfo();
}

SysInfo::~SysInfo()
{}

QString SysInfo::machineHostName() const
{
    return QSysInfo::machineHostName();
}

QString SysInfo::osId() const
{
    return d->osId;
}

QString SysInfo::prettyOSName() const
{
    return d->osName;
}

QString SysInfo::osVersion() const
{
    return QSysInfo::productVersion();
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

SysInfoCheckResult SysInfo::checkKernel()
{
    struct utsname buffer;
    long ver[16] = {0};

    errno = 0;
    if (uname(&buffer) != 0)
        qFatal("Call to uname failed: %s", std::strerror(errno));

    auto p = buffer.release;
    int i = 0;
    while (*p != '\0') {
        if (isdigit(*p)) {
            ver[i] = strtol(p, &p, 10);
            i++;
        } else {
            p++;
        }
        if (i >= 16)
            break;
    }

    if (ver[0] < 3)
        return SysInfoCheckResult::ISSUE;
    if ((ver[0] == 3) && (ver[1] < 14))
        return SysInfoCheckResult::ISSUE;
    if (ver[0] < 5)
        return SysInfoCheckResult::SUSPICIOUS;
    if ((ver[0] == 5) && (ver[1] < 4))
        return SysInfoCheckResult::SUSPICIOUS;

    return SysInfoCheckResult::OK;
}

QString SysInfo::initName() const
{
    return d->initName;
}

SysInfoCheckResult SysInfo::checkInitSystem()
{
    // if we are in Flatpak, we ignore this check for now
    if (inFlatpakSandbox())
        return SysInfoCheckResult::OK;

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

int SysInfo::rtkitMaxRealtimePriority() const
{
    return d->rtkitMaxRealtimePriority;
}

SysInfoCheckResult SysInfo::checkRtkitMaxRealtimePriority()
{
    if (d->rtkitMaxRealtimePriority < 20)
        return SysInfoCheckResult::ISSUE;
    return SysInfoCheckResult::OK;
}

int SysInfo::rtkitMinNiceLevel() const
{
    return d->rtkitMinNiceLevel;
}

SysInfoCheckResult SysInfo::checkRtkitMinNiceLevel()
{
    if (d->rtkitMinNiceLevel > -14)
        return SysInfoCheckResult::ISSUE;
    return SysInfoCheckResult::OK;
}

long long SysInfo::rtkitMaxRTTimeUsec() const
{
    return d->rtkitMaxRTTimeUsec;
}

SysInfoCheckResult SysInfo::checkRtkitMaxRTTimeUsec()
{
    if (d->rtkitMaxRTTimeUsec < 200000)
        return SysInfoCheckResult::ISSUE;
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
    // we ideally want the CPU timestamp-counter or HPET to be default
    if (d->currentClocksource == QStringLiteral("tsc") || d->currentClocksource == QStringLiteral("hpet"))
        return SysInfoCheckResult::OK;
    if (d->currentClocksource == QStringLiteral("acpi_pm"))
        return SysInfoCheckResult::ISSUE;
    return SysInfoCheckResult::SUSPICIOUS;
}

bool SysInfo::tscIsConstant() const
{
    return d->tscIsConstant;
}

SysInfoCheckResult SysInfo::checkTSCConstant()
{
    return d->tscIsConstant? SysInfoCheckResult::OK : SysInfoCheckResult::ISSUE;
}

bool SysInfo::inFlatpakSandbox() const
{
    return d->inFlatpakSandbox;
}

QString SysInfo::runtimeName() const
{
    return d->runtimeName;
}

QString SysInfo::runtimeVersion() const
{
    return d->runtimeVersion;
}

QString SysInfo::sandboxAppId() const
{
    return QStringLiteral("io.github.bothlab.syntalos");
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

bool SysInfo::syntalosHWSupportInstalled() const
{
    return !findHostFile("/lib/udev/rules.d/90-syntalos-intan.rules").isEmpty() ||
           !findHostFile("/usr/lib/udev/rules.d/90-syntalos-intan.rules").isEmpty() ||
           !findHostFile("/etc/udev/rules.d/90-syntalos-intan.rules").isEmpty();
}

QString SysInfo::syntalosVersion() const
{
    return syntalosVersionFull();
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

QString SysInfo::pythonApiVersion() const
{
    return QStringLiteral(PYTHON_LANG_VERSION);
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

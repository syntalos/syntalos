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

#pragma once

#include <QObject>

namespace Syntalos {

enum class SysInfoCheckResult {
    UNKNOWN,
    OK,
    SUSPICIOUS,
    ISSUE
};

class SysInfo : public QObject
{
    Q_OBJECT
public:
    explicit SysInfo(QObject *parent = nullptr);
    ~SysInfo();

    QString machineHostName() const;
    QString prettyOSName() const;
    QString osId() const;
    QString osVersion() const;
    QString kernelInfo() const;
    SysInfoCheckResult checkKernel();

    QString initName() const;
    SysInfoCheckResult checkInitSystem();

    int usbFsMemoryMb() const;
    SysInfoCheckResult checkUsbFsMemory();

    int rtkitMaxRealtimePriority() const;
    SysInfoCheckResult checkRtkitMaxRealtimePriority();

    int rtkitMinNiceLevel() const;
    SysInfoCheckResult checkRtkitMinNiceLevel();

    long long rtkitMaxRTTimeUsec() const;
    SysInfoCheckResult checkRtkitMaxRTTimeUsec();

    QString glVersion() const;
    QString glExtensions() const;

    QString currentArchitecture() const;
    QString currentClocksource() const;
    QString availableClocksources() const;
    SysInfoCheckResult checkClocksource();

    bool tscIsConstant() const;
    SysInfoCheckResult checkTSCConstant();

    bool inFlatpakSandbox() const;

    QString supportedAVXInstructions() const;
    SysInfoCheckResult checkAVXInstructions();

    QString cpu0ModelName() const;
    int cpuCount() const;
    int cpuPhysicalCoreCount() const;

    QString syntalosVersion() const;
    QString qtVersion() const;
    QString openCVVersionString() const;
    QString eigenVersionString() const;
    QString ffmpegVersionString() const;
    QString pythonApiVersion() const;

private:
    class Private;
    Q_DISABLE_COPY(SysInfo)
    QScopedPointer<Private> d;

    QString readSysFsValue(const QString &path);
    void readCPUInfo();
};

} // end of namespace

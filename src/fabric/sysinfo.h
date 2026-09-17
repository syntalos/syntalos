/*
 * Copyright (C) 2019-2026 Matthias Klumpp <matthias@tenstral.net>
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
#include <optional>

namespace Syntalos
{

enum class SysInfoCheckResult {
    UNKNOWN,    /// We don't know.
    OK,         /// We're good.
    SUSPICIOUS, /// Something might be off and is not ideal for some experiments.
    ISSUE       /// Something is really wrong and should be fixed.
};

class SysInfo : public QObject
{
    Q_OBJECT
public:
    static SysInfo *get()
    {
        static SysInfo instance;
        return &instance;
    }
    SysInfo(SysInfo const &) = delete;
    void operator=(SysInfo const &) = delete;

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

    QString guiPlatform() const;

    QString glVersion() const;
    QString glExtensions() const;

    QString currentArchitecture() const;
    QString currentClocksource() const;
    QString availableClocksources() const;
    SysInfoCheckResult checkClocksource();

    bool tscIsConstant() const;
    SysInfoCheckResult checkTSCConstant();

    bool inFlatpakSandbox() const;
    QString runtimeName() const;
    QString runtimeVersion() const;
    QString sandboxAppId() const;

    QString supportedAVXInstructions() const;
    SysInfoCheckResult checkAVXInstructions();

    QString cpu0ModelName() const;
    int cpuCount() const;
    int cpuPhysicalCoreCount() const;

    /**
     * @brief The CPU frequency scaling governor(s) currently in use, summarized over all cores.
     *
     * Read live, as the governor can change at any time (power profiles, AC/battery, ...).
     * Empty if the system does not expose frequency scaling (e.g. in a VM).
     */
    QString cpuGovernor() const;

    /**
     * @brief The CPU frequency scaling driver, e.g. "intel_pstate" or "amd-pstate-epp".
     */
    QString cpuScalingDriver() const;

    /**
     * @brief The energy-performance preference(s) of the CPU, summarized over all cores.
     *
     * Only available with drivers where the hardware picks the frequency by itself
     * (intel_pstate / amd-pstate in active mode), empty otherwise.
     */
    QString cpuEnergyPerfPreference() const;

    /**
     * @brief Whether CPU frequency boost ("turbo") is enabled, nothing if the system does not tell.
     */
    std::optional<bool> cpuBoostEnabled() const;

    /**
     * @brief Human-readable combination of governor, driver, energy-performance preference and boost state.
     */
    QString cpuGovernorInfo() const;

    /**
     * @brief Rate the frequency scaling setup by the worst setting of any core.
     */
    SysInfoCheckResult checkCpuGovernor() const;

    bool syntalosHWSupportInstalled() const;
    QString syntalosVersion() const;
    QString qtVersion() const;
    QString openCVVersionString() const;
    QString eigenVersionString() const;
    QString ffmpegVersionString() const;
    QString pythonApiVersion() const;

private:
    class Private;
    QScopedPointer<Private> d;

    explicit SysInfo();
    ~SysInfo();

    static QString readSysFsValue(const QString &path);
    void readCPUInfo();
};

} // namespace Syntalos

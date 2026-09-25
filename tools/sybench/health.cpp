/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "health.h"

#include <QJsonObject>

using namespace Syntalos;

namespace SyBench
{

QString healthStatusString(SysInfoCheckResult status)
{
    switch (status) {
    case SysInfoCheckResult::OK:
        return QStringLiteral("ok");
    case SysInfoCheckResult::SUSPICIOUS:
        return QStringLiteral("suspicious");
    case SysInfoCheckResult::ISSUE:
        return QStringLiteral("issue");
    case SysInfoCheckResult::UNKNOWN:
        break;
    }
    return QStringLiteral("unknown");
}

QList<HealthItem> collectHealthItems()
{
    auto *si = SysInfo::get();
    QList<HealthItem> items;

    items.append({QStringLiteral("Kernel"), si->kernelInfo(), si->checkKernel()});
    items.append({QStringLiteral("Init system"), si->initName(), si->checkInitSystem()});
    items.append({QStringLiteral("CPU governor"), si->cpuGovernorInfo(), si->checkCpuGovernor()});
    items.append({QStringLiteral("Clocksource"), si->currentClocksource(), si->checkClocksource()});
    items.append(
        {QStringLiteral("Constant TSC"),
         si->tscIsConstant() ? QStringLiteral("yes") : QStringLiteral("no"),
         si->checkTSCConstant()});
    items.append(
        {QStringLiteral("RtKit max. realtime priority"),
         QString::number(si->rtkitMaxRealtimePriority()),
         si->checkRtkitMaxRealtimePriority()});
    items.append(
        {QStringLiteral("RtKit min. nice level"),
         QString::number(si->rtkitMinNiceLevel()),
         si->checkRtkitMinNiceLevel()});
    items.append(
        {QStringLiteral("RtKit max. RT time"),
         QStringLiteral("%1 µs").arg(si->rtkitMaxRTTimeUsec()),
         si->checkRtkitMaxRTTimeUsec()});
    items.append(
        {QStringLiteral("USB filesystem memory"),
         QStringLiteral("%1 MiB").arg(si->usbFsMemoryMb()),
         si->checkUsbFsMemory()});
    items.append({QStringLiteral("AVX instructions"), si->supportedAVXInstructions(), si->checkAVXInstructions()});
    items.append(
        {QStringLiteral("Sandbox"),
         si->inFlatpakSandbox() ? QStringLiteral("Flatpak (%1)").arg(si->sandboxAppId()) : QStringLiteral("none"),
         SysInfoCheckResult::OK});
    return items;
}

QJsonArray healthToJson(const QList<HealthItem> &items)
{
    QJsonArray arr;
    for (const auto &it : items) {
        QJsonObject o;
        o.insert(QStringLiteral("name"), it.name);
        o.insert(QStringLiteral("value"), it.value);
        o.insert(QStringLiteral("status"), healthStatusString(it.status));
        arr.append(o);
    }
    return arr;
}

} // namespace SyBench

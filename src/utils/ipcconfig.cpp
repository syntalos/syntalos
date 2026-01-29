/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
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
#include "ipcconfig.h"

#include <QDebug>
#include <QSettings>

#include "meminfo.h"

using namespace Syntalos;

namespace Syntalos
{
Q_LOGGING_CATEGORY(logIPCConfig, "global.ipcconfig")
}

IPCConfig::IPCConfig(QObject *parent)
    : QObject(parent)
{
    // The IPC config information is stored in Syntalos' global configuration file
    // It exists in a separate class so we do not need to link RouDi against syfabric
    m_s = new QSettings("Syntalos", "Syntalos", this);
}

void IPCConfig::sync()
{
    m_s->sync();
}

bool IPCConfig::roudiMonitoringEnabled() const
{
    return m_s->value("ipc/roudi_monitoring", true).toBool();
}

void IPCConfig::setRoudiMonitoringEnabled(bool enabled)
{
    m_s->setValue("ipc/roudi_monitoring", enabled);
}

static IPCMemPoolInfo getMemPoolInfo(QSettings *settings, const QString &poolName, bool defaultSettings = false)
{
    QVariant defaultValue;
    if (poolName == "mempool1") {
        defaultValue = QVariant::fromValue(
            QHash<QString, QVariant>({
                {"chunk_size_mb", 6 },
                {"chunk_count",   20}
        }));
    } else if (poolName == "mempool2") {
        defaultValue = QVariant::fromValue(
            QHash<QString, QVariant>({
                {"chunk_size_mb", 24},
                {"chunk_count",   10}
        }));
    } else {
        qCCritical(logIPCConfig).noquote() << "Unknown IPC mempool name" << poolName;
    }

    // return our default values
    if (defaultSettings) {
        return IPCMemPoolInfo{
            defaultValue.toHash().value("chunk_size_mb", 6).toUInt(),
            defaultValue.toHash().value("chunk_count", 20).toUInt()};
    }

    // fetch user configured values
    const auto pool = settings->value(QStringLiteral("ipc/%1").arg(poolName), defaultValue).toHash();
    IPCMemPoolInfo info;
    info.chunkSizeMb = pool.value("chunk_size_mb", 6).toUInt();
    info.chunkCount = pool.value("chunk_count", 20).toUInt();

    return info;
}

IPCMemPoolInfo IPCConfig::memPool1Info() const
{
    return getMemPoolInfo(m_s, "mempool1");
}

IPCMemPoolInfo IPCConfig::memPool1InfoDefaults() const
{
    return getMemPoolInfo(m_s, "mempool1", true);
}

void IPCConfig::setMemPool1Info(const IPCMemPoolInfo &memPoolInfo)
{
    m_s->setValue(
        "ipc/mempool1",
        QHash<QString, QVariant>({
            {"chunk_size_mb", memPoolInfo.chunkSizeMb},
            {"chunk_count",   memPoolInfo.chunkCount }
    }));
}

IPCMemPoolInfo IPCConfig::memPool2Info() const
{
    return getMemPoolInfo(m_s, "mempool2");
}

IPCMemPoolInfo IPCConfig::memPool2InfoDefaults() const
{
    return getMemPoolInfo(m_s, "mempool2", true);
}

void IPCConfig::setMemPool2Info(const IPCMemPoolInfo &memPoolInfo)
{
    m_s->setValue(
        "ipc/mempool2",
        QHash<QString, QVariant>({
            {"chunk_size_mb", memPoolInfo.chunkSizeMb},
            {"chunk_count",   memPoolInfo.chunkCount }
    }));
}

bool IPCConfig::checkMemPoolValuesSane(uint maxRamPercentageUsed) const
{
    const auto memTotalKiB = readMemInfo().memTotalKiB;
    const auto memPool1 = memPool1Info();
    const auto memPool2 = memPool2Info();

    const auto memPoolsSizeKiB = (memPool1.chunkSizeMb * memPool1.chunkCount
                                  + memPool2.chunkSizeMb * memPool2.chunkCount)
                                 * 1024;
    const double memPoolSizePercentage = (memPoolsSizeKiB * 100.0) / memTotalKiB;

    return memPoolSizePercentage < maxRamPercentageUsed;
}

void IPCConfig::resetMemPoolDefaults()
{
    setMemPool1Info(memPool1InfoDefaults());
    setMemPool2Info(memPool2InfoDefaults());
}

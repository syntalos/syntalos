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

#pragma once

#include <QLoggingCategory>
#include <QObject>

class QSettings;

namespace Syntalos
{

Q_DECLARE_LOGGING_CATEGORY(logIPCConfig)

struct IPCMemPoolInfo {
    uint32_t chunkSizeMb;
    uint32_t chunkCount;
};

/**
 * @brief IPC (RouDi) configuration for Syntalos
 */
class IPCConfig : public QObject
{
    Q_OBJECT

public:
    explicit IPCConfig(QObject *parent = nullptr);

    void sync();

    bool roudiMonitoringEnabled() const;
    void setRoudiMonitoringEnabled(bool enabled);

    IPCMemPoolInfo memPool1Info() const;
    IPCMemPoolInfo memPool1InfoDefaults() const;
    void setMemPool1Info(const IPCMemPoolInfo &memPoolInfo);

    IPCMemPoolInfo memPool2Info() const;
    IPCMemPoolInfo memPool2InfoDefaults() const;
    void setMemPool2Info(const IPCMemPoolInfo &memPoolInfo);

    bool checkMemPoolValuesSane(uint maxRamPercentageUsed = 80) const;
    void resetMemPoolDefaults();

private:
    QSettings *m_s;
};

} // namespace Syntalos

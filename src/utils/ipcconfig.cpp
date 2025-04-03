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

bool IPCConfig::roudiMonitoringEnabled() const
{
    return m_s->value("ipc/roudi_monitoring", true).toBool();
}

void IPCConfig::setRoudiMonitoringEnabled(bool enabled)
{
    m_s->setValue("ipc/roudi_monitoring", enabled);
}

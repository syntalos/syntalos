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

#pragma once

#include <QJsonArray>
#include <QList>
#include <QString>

#include "sysinfo.h"

namespace SyBench
{

/**
 * @brief One system configuration check relevant for acquisition performance
 */
struct HealthItem {
    QString name;
    QString value;
    Syntalos::SysInfoCheckResult status = Syntalos::SysInfoCheckResult::UNKNOWN;
};

QString healthStatusString(Syntalos::SysInfoCheckResult status);

/**
 * @brief Run the system checks and describe their outcome.
 */
QList<HealthItem> collectHealthItems();

QJsonArray healthToJson(const QList<HealthItem> &items);

} // namespace SyBench

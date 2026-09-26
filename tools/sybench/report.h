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

#include <QJsonObject>
#include <QList>
#include <QString>
#include <expected>

#include "benchsession.h"
#include "health.h"

namespace SyBench
{

/**
 * @brief Build the benchmark report document
 * @param health The system checks as they were when the benchmark finished.
 */
QJsonObject buildReport(
    const QList<LadderRecord> &ladders,
    const SessionConfig &config,
    const QList<HealthItem> &health);

/**
 * @brief Save the report as JSON file.
 */
auto saveReport(
    const QString &fileName,
    const QList<LadderRecord> &ladders,
    const SessionConfig &config,
    const QList<HealthItem> &health) -> std::expected<void, QString>;

} // namespace SyBench

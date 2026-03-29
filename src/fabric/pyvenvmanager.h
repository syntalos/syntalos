/*
 * Copyright (C) 2020-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include <QString>
#include <QLoggingCategory>

namespace Syntalos
{

Q_DECLARE_LOGGING_CATEGORY(logVEnv)

/**
 * Get the absolute directory path of a virtual environment with the given name.
 */
QString pythonVEnvDirForName(const QString &venvName);

bool pythonVirtualEnvExists(const QString &venvName);

bool createPythonVirtualEnv(const QString &venvName, const QString &requirementsFile = QString());

} // namespace Syntalos

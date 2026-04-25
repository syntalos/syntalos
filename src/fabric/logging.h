/*
 * Copyright (C) 2022-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include "logging-codecs.h"

namespace Syntalos
{

using QuillLogger = quill::Logger;

#define logRoot ::Syntalos::getDefaultLogger()

quill::Logger *getLogger(const std::string &name);
quill::Logger *getLogger(const QString &name);
quill::Logger *getLogger(const char *name);

/**
 * Get a temporary logger for an arbitrary module name. This is not
 * for high-volume logging and mostly for compatibility with older
 * code, think twice before using it in any new code.
 *
 * Usually there is a better solution.
 *
 * @param name A custom-defined module name.
 * @return A logger for the module ID.
 */
quill::Logger *logModTmp(const char *name);

inline quill::Logger *getDefaultLogger()
{
    return getLogger("root");
}

/**
 * Remove a logger explicitly. Should usually not be needed.
 */
void removeLogger(quill::Logger *logger);

/**
 * Initialize Syntalos logging. Must only ever be called once, at program startup.
 * Purely internal API.
 */
void initializeSyLogSystem(quill::LogLevel consoleLogLevel = quill::LogLevel::Info);

void shutdownSyLogSystem();

} // namespace Syntalos

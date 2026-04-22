/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include <cstdint>
#include <functional>
#include <string_view>

namespace Syntalos::datactl
{

/**
 * @brief Log severity enum
 *
 * This enum is kept intentionally directly compatible with
 * the quill::LogLevel enum.
 * We must keep both in sync, to directly convert between the two!
 */
enum class LogSeverity : uint8_t {
    TraceL3,
    TraceL2,
    TraceL1,
    Debug,
    Info,
    Notice,
    Warning,
    Error,
    Critical,
};

/**
 * @brief Describes a single log message.
 *
 * The @p message string_view is only valid for the duration of the handler callback.
 */
struct LogMessage {
    LogSeverity severity;
    const char *category; /// Static string literal, e.g. "time.clock"
    const char *file;
    int line;
    const char *function;
    std::string_view message; /// Valid only for the duration of the callback
};

using LogHandlerFn = std::function<void(const LogMessage &)>;

/**
 * @brief Install a log handler for all datactl log messages.
 *
 * Replaces any previously installed handler. Pass an empty/default-constructed
 * @p handler to stop logging. Intended to be called once during host process
 * initialization; not safe to call concurrently with active logging.
 */
void setLogHandler(LogHandlerFn handler);

/**
 * @brief Set the minimum log severity globally across all datactl categories.
 *
 * Messages below @p min are discarded before formatting.
 */
void setLogSeverity(LogSeverity min);

/**
 * @brief Set the minimum log severity for a specific category.
 */
void setLogSeverity(const char *category, LogSeverity min);

} // namespace Syntalos::datactl

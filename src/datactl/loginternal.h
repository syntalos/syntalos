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

// Internal logging helpers for use only within the syntalos-datactl library.
// Not part of the public API.

#pragma once

#include <atomic>
#include <format>
#include <string>

#include "logging.h"

namespace Syntalos::datactl
{

/**
 * @brief A named log category with a per-instance severity threshold.
 */
struct LogCategory {
    const char *name;
    std::atomic<int> threshold; /// Minimum LogSeverity (as int) to dispatch
    LogCategory *next = nullptr;

    explicit LogCategory(const char *name, LogSeverity defaultSeverity = LogSeverity::Info) noexcept;
};

/// Head of the global category linked list
extern std::atomic<LogCategory *> g_categoryListHead;

/// Set to true whenever a non-empty handler is installed
extern std::atomic<bool> g_handlerActive;

inline bool shouldLog(const LogCategory &c, LogSeverity s) noexcept
{
    return g_handlerActive.load(std::memory_order_acquire) && int(s) >= c.threshold.load(std::memory_order_relaxed);
}

void dispatchLog(
    const LogCategory &cat,
    LogSeverity sev,
    const char *file,
    int line,
    const char *function,
    const std::string &message);

} // namespace Syntalos::datactl

/**
 * Define a file-local log category.
 *
 * The category self-registers in the global list at static-init time.
 */
#define SY_DEFINE_LOG_CATEGORY(varname, catname) static ::Syntalos::datactl::LogCategory varname(catname)

/**
 * Forward-declare a log category for use from a header.
 */
#define SY_DECLARE_LOG_CATEGORY(varname) extern ::Syntalos::datactl::LogCategory varname

#define SY_LOG(cat, sev, ...)                                                                                       \
    do {                                                                                                            \
        if (::Syntalos::datactl::shouldLog((cat), (sev)))                                                           \
            ::Syntalos::datactl::dispatchLog((cat), (sev), __FILE__, __LINE__, __func__, std::format(__VA_ARGS__)); \
    } while (0)

#define SY_LOG_DEBUG(cat, ...)    SY_LOG((cat), ::Syntalos::datactl::LogSeverity::Debug, __VA_ARGS__)
#define SY_LOG_INFO(cat, ...)     SY_LOG((cat), ::Syntalos::datactl::LogSeverity::Info, __VA_ARGS__)
#define SY_LOG_WARNING(cat, ...)  SY_LOG((cat), ::Syntalos::datactl::LogSeverity::Warning, __VA_ARGS__)
#define SY_LOG_CRITICAL(cat, ...) SY_LOG((cat), ::Syntalos::datactl::LogSeverity::Critical, __VA_ARGS__)

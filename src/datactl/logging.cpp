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

#include "logging.h"
#include "loginternal.h"

#include <cstdio>
#include <string>
#include <string_view>

namespace Syntalos::datactl
{

// Zero-initialized (constant initialization) before any dynamic init fires,
// so LogCategory constructors in other TUs can safely prepend to this list.
std::atomic<LogCategory *> g_categoryListHead{nullptr};

std::atomic<bool> g_handlerActive{true}; // default handler is always installed

LogCategory::LogCategory(const char *name_, LogSeverity defaultSeverity) noexcept
    : name(name_),
      threshold(static_cast<int>(defaultSeverity)),
      next(nullptr)
{
    // wait-free CAS prepend - safe even when multiple TUs register simultaneously
    auto head = g_categoryListHead.load(std::memory_order_relaxed);
    do {
        next = head;
    } while (
        !g_categoryListHead.compare_exchange_weak(head, this, std::memory_order_release, std::memory_order_relaxed));
}

namespace detail
{
static void defaultHandler(const LogMessage &m)
{
    auto out = (m.severity >= LogSeverity::Warning) ? stderr : stdout;
    std::fprintf(out, "%s: %.*s\n", m.category, static_cast<int>(m.message.size()), m.message.data());
}

static LogHandlerFn g_handler{defaultHandler};
} // namespace detail

void dispatchLog(
    const LogCategory &cat,
    LogSeverity sev,
    const char *file,
    int line,
    const char *function,
    const std::string &message)
{
    const LogMessage lm{sev, cat.name, file, line, function, std::string_view(message)};
    detail::g_handler(lm);
}

// ---- Public API ----

void setLogHandler(LogHandlerFn handler)
{
    detail::g_handler = std::move(handler);
    g_handlerActive.store(bool(detail::g_handler), std::memory_order_release);
}

void setLogSeverity(LogSeverity min)
{
    auto cat = g_categoryListHead.load(std::memory_order_acquire);
    while (cat) {
        cat->threshold.store(static_cast<int>(min), std::memory_order_relaxed);
        cat = cat->next;
    }
}

void setLogSeverity(const char *category, LogSeverity min)
{
    const std::string_view name(category);
    auto cat = g_categoryListHead.load(std::memory_order_acquire);
    while (cat) {
        if (std::string_view(cat->name) == name)
            cat->threshold.store(static_cast<int>(min), std::memory_order_relaxed);
        cat = cat->next;
    }
}

} // namespace Syntalos::datactl

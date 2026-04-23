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

// Backend headers must come before logging.h to avoid partial specialization
// conflicts: DeferredFormatCodec.h (pulled in by logging.h) triggers instantiation
// of fmtquill formatters before format.h can partially specialize them.
#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/sinks/ConsoleSink.h>

#include "logging.h"

#include <iostream>
#include <datactl/logging.h>

namespace Syntalos
{

static std::shared_ptr<quill::Sink> g_consoleSink = nullptr;
static quill::LogLevel g_defaultLogLevel = quill::LogLevel::Info;

quill::Logger *getLogger(const std::string &name)
{
    quill::PatternFormatterOptions fmtOpt{"%(time) %(thread_name):%(logger): %(message)", "%H:%M:%S.%Qus"};
    auto logger = quill::Frontend::create_or_get_logger(name, g_consoleSink, fmtOpt);
    logger->set_log_level(g_defaultLogLevel);

    return logger;
}

quill::Logger *getLogger(const QString &name)
{
    return getLogger(name.toStdString());
}

quill::Logger *getLogger(const char *name)
{
    return getLogger(std::string(name));
}
quill::Logger *logModTmp(const char *name)
{
    const auto logName = std::format("mod.{}", name);
    return getLogger(logName);
}

static void datactlLogHandler(const datactl::LogMessage &m)
{
    // cache one Quill logger per datactl category (categories are static strings, map by pointer)
    thread_local std::unordered_map<const char *, quill::Logger *> loggers;
    auto [it, inserted] = loggers.emplace(m.category, nullptr);
    if (inserted)
        it->second = getLogger(m.category);

    QUILL_LOG_RUNTIME_METADATA_CALL(
        quill::MacroMetadata::Event::LogWithRuntimeMetadataShallowCopy,
        it->second,
        static_cast<quill::LogLevel>(m.severity),
        m.file,
        m.line,
        m.function,
        "",
        "{}",
        m.message);
}

void initializeSyLogSystem(quill::LogLevel consoleLogLevel)
{
    // trying to initialize the log system twice is a critical error
    if (g_consoleSink) {
        std::cerr << "Tried to initialize the Syntalos logging system twice. This is not allowed!" << std::endl;
        abort();
        return;
    }

    // start Quill's async logging backend
    quill::Backend::start();

    // register our console sink
    g_consoleSink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sy_console");

    // forward datactl library log messages into Quill
    datactl::setLogHandler(datactlLogHandler);

    // configure defaults
    g_defaultLogLevel = consoleLogLevel;
    g_consoleSink->set_log_level_filter(consoleLogLevel);
}

} // namespace Syntalos

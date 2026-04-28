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
#include <glib.h>
#include <iox2/log.hpp>
#include <datactl/logging.h>

namespace Syntalos
{

static std::shared_ptr<quill::Sink> g_consoleSink = nullptr;
static quill::LogLevel g_defaultLogLevel = quill::LogLevel::Info;

static QtMessageHandler g_prevQtHandler = nullptr;

quill::Logger *getLogger(const std::string &name)
{
    auto logger = quill::Frontend::get_logger(name);
    if (logger != nullptr)
        return logger;

    quill::PatternFormatterOptions fmtOpt{
        "%(time) %(log_level_short_code) %(thread_name):%(logger): %(message)", "%H:%M:%S.%Qus"};
    logger = quill::Frontend::create_or_get_logger(name, g_consoleSink, fmtOpt);
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

void removeLogger(quill::Logger *logger)
{
    quill::Frontend::remove_logger_blocking(logger);
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

static void qtLogHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    const QByteArray utf8 = msg.toUtf8();
    const char *category = ctx.category ? ctx.category : "qt";
    const char *file = ctx.file ? ctx.file : "";
    const char *function = ctx.function ? ctx.function : "";

    auto logger = getLogger(category);
    quill::LogLevel level = quill::LogLevel::Info;

    switch (type) {
    case QtDebugMsg:
        level = quill::LogLevel::Debug;
        break;
    case QtInfoMsg:
        level = quill::LogLevel::Info;
        break;
    case QtWarningMsg:
        level = quill::LogLevel::Warning;
        break;
    case QtCriticalMsg:
        level = quill::LogLevel::Error;
        break;
    case QtFatalMsg:
        level = quill::LogLevel::Critical;
        break;
    }

    QUILL_LOG_RUNTIME_METADATA_CALL(
        quill::MacroMetadata::Event::LogWithRuntimeMetadataShallowCopy,
        logger,
        level,
        file,
        ctx.line,
        function,
        "",
        "{}",
        utf8.constData());

    if (type == QtFatalMsg) {
        // Qt expects us to abort on qFatal, so we do this here
        logger->flush_log();
        std::abort();
    }
}

static GLogWriterOutput glibLogWriter(GLogLevelFlags log_level, const GLogField *fields, gsize n_fields, gpointer)
{
    std::string domainStr;
    std::string msgStr;
    std::string fileStr;
    std::string funcStr;
    int line = 0;

    auto assignFieldStr = [](std::string &dst, const GLogField &f) {
        if (!f.value)
            return;
        const auto s = static_cast<const char *>(f.value);
        if (f.length < 0)
            dst.assign(s);
        else
            dst.assign(s, static_cast<size_t>(f.length));
    };

    for (gsize i = 0; i < n_fields; ++i) {
        const auto &f = fields[i];
        if (!f.key)
            continue;

        if (std::strcmp(f.key, "GLIB_DOMAIN") == 0) {
            assignFieldStr(domainStr, f);
        } else if (std::strcmp(f.key, "MESSAGE") == 0) {
            assignFieldStr(msgStr, f);
        } else if (std::strcmp(f.key, "CODE_FILE") == 0) {
            assignFieldStr(fileStr, f);
        } else if (std::strcmp(f.key, "CODE_FUNC") == 0) {
            assignFieldStr(funcStr, f);
        } else if (std::strcmp(f.key, "CODE_LINE") == 0) {
            std::string lineStr;
            assignFieldStr(lineStr, f);
            if (!lineStr.empty())
                line = static_cast<int>(std::strtol(lineStr.c_str(), nullptr, 10));
        }
    }

    auto logger = getLogger(domainStr.empty() ? "g" : domainStr);

    quill::LogLevel level;
    switch (log_level & G_LOG_LEVEL_MASK) {
    case G_LOG_LEVEL_ERROR:
        level = quill::LogLevel::Critical;
        break;
    case G_LOG_LEVEL_CRITICAL:
        level = quill::LogLevel::Error;
        break;
    case G_LOG_LEVEL_WARNING:
        level = quill::LogLevel::Warning;
        break;
    case G_LOG_LEVEL_MESSAGE:
    case G_LOG_LEVEL_INFO:
        level = quill::LogLevel::Info;
        break;
    case G_LOG_LEVEL_DEBUG:
        level = quill::LogLevel::Debug;
        break;
    default:
        level = quill::LogLevel::Info;
        break;
    }

    if ((log_level & G_LOG_FLAG_FATAL) != 0)
        level = quill::LogLevel::Critical;

    QUILL_LOG_RUNTIME_METADATA_CALL(
        quill::MacroMetadata::Event::LogWithRuntimeMetadataShallowCopy,
        logger,
        level,
        fileStr.empty() ? "" : fileStr.c_str(),
        line,
        funcStr.empty() ? "" : funcStr.c_str(),
        "",
        "{}",
        msgStr.empty() ? "" : msgStr.c_str());

    return G_LOG_WRITER_HANDLED;
}

/**
 * Log forwarder to move iceoryx messages to our Quill loggers.
 */
class IoxLogger : public iox2::Log
{
public:
    IoxLogger()
        : iox2::Log(),
          m_log(getLogger("iox"))
    {
    }

    void log(iox2::LogLevel ioxLogLevel, const char *origin, const char *message) override
    {
        quill::LogLevel level;
        switch (ioxLogLevel) {
        case iox2::LogLevel::Debug:
            level = quill::LogLevel::Debug;
            break;
        case iox2::LogLevel::Info:
            level = quill::LogLevel::Info;
            break;
        case iox2::LogLevel::Warn:
            level = quill::LogLevel::Warning;
            break;
        case iox2::LogLevel::Error:
            level = quill::LogLevel::Error;
            break;
        case iox2::LogLevel::Fatal:
            level = quill::LogLevel::Critical;
            break;
        case iox2::LogLevel::Trace:
            level = quill::LogLevel::TraceL1;
            break;
        default:
            level = quill::LogLevel::Info;
        }

        QUILL_LOG_RUNTIME_METADATA_CALL(
            quill::MacroMetadata::Event::LogWithRuntimeMetadataShallowCopy,
            m_log,
            level,
            "",
            0,
            origin,
            "",
            "{}",
            message);
    }

private:
    QuillLogger *m_log;
};

void initializeSyLogSystem(quill::LogLevel consoleLogLevel)
{
    // trying to initialize the log system twice is a critical error
    if (g_consoleSink) {
        std::cerr << "Tried to initialize the Syntalos logging system twice. This is not allowed!" << std::endl;
        abort();
        return;
    }

    quill::BackendOptions backendOptn;

    // we want to log UTF-8 characters, so disable sanitization
    backendOptn.check_printable_char = {};

    // start Quill's async logging backend
    quill::Backend::start(backendOptn);

    // register our console sink
    g_consoleSink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sy-console");

    // forward datactl library log messages into Quill
    datactl::setLogHandler(datactlLogHandler);

    // forward any Qt log messages into Quill as well
    g_prevQtHandler = qInstallMessageHandler(qtLogHandler);

    // forward any GLib log messages
    g_log_set_writer_func(glibLogWriter, nullptr, nullptr);

    // forward iceoryx2 messages
    static IoxLogger ioxLogForwarder = IoxLogger();
    iox2::set_logger(ioxLogForwarder);

    // configure defaults
    g_defaultLogLevel = consoleLogLevel;
    g_consoleSink->set_log_level_filter(consoleLogLevel);
}

void shutdownSyLogSystem()
{
    // reset all logging handlers back to defaults, if we can
    if (g_prevQtHandler != nullptr)
        qInstallMessageHandler(g_prevQtHandler);
    g_prevQtHandler = nullptr;
    datactl::setLogHandler(nullptr);

    // NOTE: It is forbidden to reset the GLib log handler, and we cannot
    // reset the IOX handler either. The shutdown function must be called
    // last in a program, after no more logging is possible (or never).

    quill::Backend::stop();
}

} // namespace Syntalos

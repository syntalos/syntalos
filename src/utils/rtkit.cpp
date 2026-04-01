/*
 * Copyright (C) 2019-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "rtkit.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef signals
#define SY_RESTORE_QT_SIGNALS
#undef signals
#endif
#include <gio/gio.h>
#ifdef SY_RESTORE_QT_SIGNALS
#define signals Q_SIGNALS
#undef SY_RESTORE_QT_SIGNALS
#endif

#include <climits>
#include <format>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <QDebug>
#include <pthread.h>
#include <sched.h>

#ifndef RLIMIT_RTTIME
#define RLIMIT_RTTIME 15
#endif

Q_LOGGING_CATEGORY(logRtKit, "rtkit")

static const char RTPORTAL_SERVICE_NAME[] = "org.freedesktop.portal.Desktop";
static const char RTPORTAL_OBJECT_PATH[] = "/org/freedesktop/portal/desktop";
static const char RTPORTAL_INTERFACE_NAME[] = "org.freedesktop.portal.Realtime";

static const char RTKIT_SERVICE_NAME[] = "org.freedesktop.RealtimeKit1";
static const char RTKIT_OBJECT_PATH[] = "/org/freedesktop/RealtimeKit1";
static const char RTKIT_INTERFACE_NAME[] = "org.freedesktop.RealtimeKit1";

static bool variantToInt64(GVariant *variant, int64_t *value)
{
    if (variant == nullptr || value == nullptr)
        return false;

    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_INT64)) {
        *value = g_variant_get_int64(variant);
        return true;
    }
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_UINT64)) {
        *value = static_cast<long long>(g_variant_get_uint64(variant));
        return true;
    }
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_INT32)) {
        *value = g_variant_get_int32(variant);
        return true;
    }
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_UINT32)) {
        *value = g_variant_get_uint32(variant);
        return true;
    }

    return false;
}

RtKit::RtKit()
    : m_rtkitProxy(nullptr),
      m_rtPortalProxy(nullptr)
{
    g_autoptr(GError) error = nullptr;

    // Create proxy for realtime portal (session bus)
    m_rtPortalProxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SESSION,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        RTPORTAL_SERVICE_NAME,
        RTPORTAL_OBJECT_PATH,
        RTPORTAL_INTERFACE_NAME,
        nullptr,
        &error);

    if (error != nullptr) {
        m_lastError = std::format("Failed to create Realtime Portal proxy: {}", error->message);
        g_clear_error(&error);
    }

    // Create proxy for RtKit (system bus)
    m_rtkitProxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SYSTEM,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        RTKIT_SERVICE_NAME,
        RTKIT_OBJECT_PATH,
        RTKIT_INTERFACE_NAME,
        nullptr,
        &error);

    if (error != nullptr) {
        m_lastError = std::format("Failed to create RtKit proxy: {}", error->message);
        g_clear_error(&error);
    }
}

RtKit::~RtKit()
{
    if (m_rtkitProxy != nullptr)
        g_object_unref(m_rtkitProxy);
    if (m_rtPortalProxy != nullptr)
        g_object_unref(m_rtPortalProxy);
}

std::string RtKit::lastError() const
{
    return m_lastError;
}

int RtKit::queryMaxRealtimePriority(bool *ok)
{
    return getIntProperty("MaxRealtimePriority", ok);
}

int RtKit::queryMinNiceLevel(bool *ok)
{
    return getIntProperty("MinNiceLevel", ok);
}

long long RtKit::queryRTTimeUSecMax(bool *ok)
{
    return getIntProperty("RTTimeUSecMax", ok);
}

bool RtKit::makeHighPriority(pid_t thread, int niceLevel)
{
    if (thread == 0)
        thread = gettid();

    // Try using the realtime portal first
    if (m_rtPortalProxy != nullptr) {
        g_autoptr(GError) error = nullptr;
        g_autoptr(GVariant) result = g_dbus_proxy_call_sync(
            m_rtPortalProxy,
            "MakeThreadHighPriorityWithPID",
            g_variant_new("(tti)", (guint64)getpid(), (guint64)thread, (gint32)niceLevel),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            &error);

        if (result != nullptr)
            return true;
    }

    // Fallback to using RtKit directly
    if (m_rtkitProxy != nullptr) {
        g_autoptr(GError) error = nullptr;
        g_autoptr(GVariant) result = g_dbus_proxy_call_sync(
            m_rtkitProxy,
            "MakeThreadHighPriority",
            g_variant_new("(ti)", (guint64)thread, (gint32)niceLevel),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            &error);

        if (result != nullptr)
            return true;

        if (error != nullptr) {
            m_lastError = std::format(
                "Unable to change thread priority to high: {}: {}",
                error->domain == G_DBUS_ERROR ? error->code : 0,
                error->message);

            // rtkit maps EBUSY to org.freedesktop.DBus.Error.AccessDenied when the per-user
            // MaxConcurrentThreads limit is exceeded. The error name looks like a plain
            // permission problem but is actually resource exhaustion, so disambiguate.
            if (error->domain == G_DBUS_ERROR && error->code == G_DBUS_ERROR_ACCESS_DENIED
                && g_strrstr(error->message, "busy") != nullptr) {
                m_lastError =
                    "Unable to change thread priority to high: RtKit's per-user "
                    "concurrent-thread limit has been reached - too many worker threads "
                    "are already elevated simultaneously.";
            }
        }
    }

    return false;
}

bool RtKit::makeRealtime(pid_t thread, uint priority)
{
    if (thread == 0) {
        struct sched_param sp = {};
        sp.sched_priority = priority;

        if (pthread_setschedparam(pthread_self(), SCHED_RR | SCHED_RESET_ON_FORK, &sp) == 0) {
            qCDebug(logRtKit).noquote() << "Realtime priority obtained via SCHED_RR | SCHED_RESET_ON_FORK directly";
            return true;
        }
        thread = gettid();
    }

    // Try using the realtime portal first
    if (m_rtPortalProxy != nullptr) {
        g_autoptr(GError) error = nullptr;
        g_autoptr(GVariant) result = g_dbus_proxy_call_sync(
            m_rtPortalProxy,
            "MakeThreadRealtimeWithPID",
            g_variant_new("(ttu)", (guint64)getpid(), (guint64)thread, (guint32)priority),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            &error);

        if (result != nullptr)
            return true;
    }

    // Fallback to RtKit directly
    if (m_rtkitProxy != nullptr) {
        g_autoptr(GError) error = nullptr;
        g_autoptr(GVariant) result = g_dbus_proxy_call_sync(
            m_rtkitProxy,
            "MakeThreadRealtime",
            g_variant_new("(tu)", (guint64)thread, (guint32)priority),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            &error);

        if (result != nullptr)
            return true;

        if (error != nullptr) {
            m_lastError = std::format(
                "Unable to change thread priority to realtime: {}: {}",
                error->domain == G_DBUS_ERROR ? error->code : 0,
                error->message);
        }
    }

    return false;
}

int64_t RtKit::getIntProperty(const std::string &propName, bool *ok)
{
    // Try to get the property from the realtime portal first
    if (m_rtPortalProxy != nullptr) {
        g_autoptr(GError) error = nullptr;
        g_autoptr(GVariant) result = g_dbus_proxy_call_sync(
            m_rtPortalProxy,
            "org.freedesktop.DBus.Properties.Get",
            g_variant_new("(ss)", RTPORTAL_INTERFACE_NAME, propName.c_str()),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            &error);

        if (result != nullptr) {
            g_autoptr(GVariant) variant = nullptr;
            g_variant_get(result, "(v)", &variant);
            if (variant != nullptr) {
                int64_t value = 0;
                if (variantToInt64(variant, &value)) {
                    if (ok != nullptr)
                        (*ok) = true;
                    return value;
                }

                m_lastError = std::format(
                    "Realtime Portal property '{}' has unsupported type '{}'.",
                    propName,
                    g_variant_get_type_string(variant));
                if (ok != nullptr)
                    (*ok) = false;
            }
        } else if (error != nullptr) {
            m_lastError = std::format(
                "Realtime Portal property DBus request for '{}' failed: {}", propName, error->message);
        }
    }

    // Fallback to using RtKit directly
    if (m_rtkitProxy != nullptr) {
        g_autoptr(GError) error = nullptr;
        g_autoptr(GVariant) result = g_dbus_proxy_call_sync(
            m_rtkitProxy,
            "org.freedesktop.DBus.Properties.Get",
            g_variant_new("(ss)", RTKIT_INTERFACE_NAME, propName.c_str()),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            &error);

        if (result != nullptr) {
            g_autoptr(GVariant) variant = nullptr;
            g_variant_get(result, "(v)", &variant);
            if (variant != nullptr) {
                int64_t value = 0;
                if (variantToInt64(variant, &value)) {
                    if (ok != nullptr)
                        (*ok) = true;
                    return value;
                }

                m_lastError = std::format(
                    "RtKit property '{}' has unsupported type '{}'.", propName, g_variant_get_type_string(variant));
                if (ok != nullptr)
                    (*ok) = false;
            } else {
                m_lastError = std::format("Reply to RtKit property request for '{}' was empty.", propName);
            }
        } else if (error != nullptr) {
            m_lastError = std::format("RtKit property DBus request for '{}' failed: {}", propName, error->message);
        }
    }

    if (ok == nullptr)
        qCWarning(logRtKit).noquote() << m_lastError;
    else
        (*ok) = false;

    return LLONG_MAX;
}

bool setCurrentThreadNiceness(int nice)
{
    RtKit rtkit;
    const auto minNice = rtkit.queryMinNiceLevel();
    if (minNice < 0) {
        if (nice < minNice) {
            qCDebug(logRtKit).noquote().nospace()
                << "Unable to set thread niceness to " << nice << ", clamped to min value " << minNice;
            nice = minNice;
        }
    }

    if (!rtkit.makeHighPriority(0, nice)) {
        qCWarning(logRtKit).noquote().nospace() << rtkit.lastError();
        return false;
    }

    return true;
}

bool setCurrentThreadRealtime(int priority)
{
    struct rlimit rlim = {};

    RtKit rtkit;
    const auto maxRTTimeUsec = rtkit.queryRTTimeUSecMax();
    if (maxRTTimeUsec < (100 * 1000)) {
        qCWarning(logRtKit).noquote()
            << "Unable to set realtime priority: Permitted RLIMIT_RTTIME is too low (< 100ms)";
        return false;
    }

    rlim.rlim_cur = rlim.rlim_max = maxRTTimeUsec;
    if (setrlimit(RLIMIT_RTTIME, &rlim) < 0) {
        qCWarning(logRtKit).noquote() << "Failed to set RLIMIT_RTTIME:" << strerror(errno);
        return false;
    }

    const auto maxRTPrio = rtkit.queryMaxRealtimePriority();
    if (priority > maxRTPrio) {
        qCDebug(logRtKit).noquote().nospace()
            << "Unable to set thread realtime priority to " << priority << ", clamped to max value " << maxRTPrio;
        priority = maxRTPrio;
    }

    return rtkit.makeRealtime(0, priority);
}

/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include <string.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <QDBusArgument>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>

#ifndef RLIMIT_RTTIME
#define RLIMIT_RTTIME 15
#endif

static const auto RTPORTAL_SERVICE_NAME = QStringLiteral("org.freedesktop.portal.Desktop");
static const auto RTPORTAL_OBJECT_PATH = QStringLiteral("/org/freedesktop/portal/desktop");
static const auto RTPORTAL_INTERFACE_NAME = QStringLiteral("org.freedesktop.portal.Realtime");

static const auto RTKIT_SERVICE_NAME = QStringLiteral("org.freedesktop.RealtimeKit1");
static const auto RTKIT_OBJECT_PATH = QStringLiteral("/org/freedesktop/RealtimeKit1");
static const auto RTKIT_INTERFACE_NAME = QStringLiteral("org.freedesktop.RealtimeKit1");

Q_LOGGING_CATEGORY(logRtKit, "rtkit")

RtKit::RtKit(QObject *parent)
    : QObject(parent)
{
    m_rtPortalIntf = new QDBusInterface(
        RTPORTAL_SERVICE_NAME, RTPORTAL_OBJECT_PATH, RTPORTAL_INTERFACE_NAME, QDBusConnection::sessionBus(), this);

    m_rtkitIntf = new QDBusInterface(
        RTKIT_SERVICE_NAME, RTKIT_OBJECT_PATH, RTKIT_INTERFACE_NAME, QDBusConnection::systemBus(), this);
}

QString RtKit::lastError() const
{
    return m_lastError;
}

int RtKit::queryMaxRealtimePriority(bool *ok)
{
    return getIntProperty(QStringLiteral("MaxRealtimePriority"), ok);
}

int RtKit::queryMinNiceLevel(bool *ok)
{
    return getIntProperty(QStringLiteral("MinNiceLevel"), ok);
}

long long RtKit::queryRTTimeUSecMax(bool *ok)
{
    return getIntProperty(QStringLiteral("RTTimeUSecMax"), ok);
}

bool RtKit::makeHighPriority(pid_t thread, int niceLevel)
{
    if (thread == 0)
        thread = gettid();

    // try using the realtime portal first
    QDBusReply<void> reply = m_rtPortalIntf->call(
        QStringLiteral("MakeThreadHighPriorityWithPID"),
        QVariant::fromValue((qulonglong)getpid()),
        QVariant::fromValue((qulonglong)thread),
        QVariant::fromValue((int32_t)niceLevel));
    if (reply.isValid())
        return true;

    // fallback to using RtKit directly
    reply = m_rtkitIntf->call(
        QStringLiteral("MakeThreadHighPriority"),
        QVariant::fromValue((qulonglong)thread),
        QVariant::fromValue((int32_t)niceLevel));
    if (reply.isValid())
        return true;

    m_lastError = QStringLiteral("Unable to change thread priority to high: %1: %2")
                      .arg(reply.error().name(), reply.error().message());
    return false;
}

bool RtKit::makeRealtime(pid_t thread, uint priority)
{
    if (thread == 0) {
        struct sched_param sp = {};
        sp.sched_priority = priority;

        if (pthread_setschedparam(pthread_self(), SCHED_OTHER | SCHED_RESET_ON_FORK, &sp) == 0) {
            qCDebug(logRtKit).noquote() << "Realtime priority obtained via SCHED_OTHER | SCHED_RESET_ON_FORK directly";
            return true;
        }
        thread = gettid();
    }

    // try using the realtime portal first
    QDBusReply<void> reply = m_rtPortalIntf->call(
        QStringLiteral("MakeThreadRealtimeWithPID"),
        QVariant::fromValue((qulonglong)getpid()),
        QVariant::fromValue((qulonglong)thread),
        QVariant::fromValue((uint32_t)priority));
    if (reply.isValid())
        return true;

    // fallback to RtKit directly
    reply = m_rtkitIntf->call(
        QStringLiteral("MakeThreadRealtime"),
        QVariant::fromValue((qulonglong)thread),
        QVariant::fromValue((uint32_t)priority));
    if (reply.isValid())
        return true;

    m_lastError = QStringLiteral("Unable to change thread priority to realtime: %1: %2")
                      .arg(reply.error().name(), reply.error().message());
    return false;
}

long long RtKit::getIntProperty(const QString &propName, bool *ok)
{
    // try to get the property from the realtime portal first
    auto m = QDBusMessage::createMethodCall(
        RTPORTAL_SERVICE_NAME,
        RTPORTAL_OBJECT_PATH,
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Get"));
    m << RTPORTAL_INTERFACE_NAME << propName;

    QDBusReply<QVariant> reply = QDBusConnection::sessionBus().call(m);
    if (reply.isValid()) {
        const auto value = reply.value();
        if (value.isValid()) {
            if (ok != nullptr)
                (*ok) = true;
            return value.toLongLong();
        } else {
            m_lastError = QStringLiteral("Reply to Realtime Portal property request for '%1' was empty.").arg(propName);
        }
    } else {
        m_lastError = QStringLiteral("Realtime Portal property DBus request for '%1' failed: %2: %3")
                          .arg(propName, reply.error().name(), reply.error().message());
    }

    // fallback to using RtKit directly
    m = QDBusMessage::createMethodCall(
        RTKIT_SERVICE_NAME,
        RTKIT_OBJECT_PATH,
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Get"));
    m << RTKIT_INTERFACE_NAME << propName;

    reply = QDBusConnection::systemBus().call(m);
    if (reply.isValid()) {
        const auto value = reply.value();
        if (value.isValid()) {
            if (ok != nullptr)
                (*ok) = true;
            return value.toLongLong();
        } else {
            m_lastError = QStringLiteral("Reply to RtKit property request for '%1' was empty.").arg(propName);
        }
    } else {
        m_lastError = QStringLiteral("RtKit property DBus request for '%1' failed: %2: %3")
                          .arg(propName, reply.error().name(), reply.error().message());
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
        qCDebug(logRtKit).noquote().nospace() << rtkit.lastError();
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
        qCWarning(logRtKit).noquote() << "Unable to set realtime priority: Permitted RLIMIT_RTTIME is too low (<100µs)";
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

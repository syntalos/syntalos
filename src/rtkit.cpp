/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include <QDebug>
#include <QDBusInterface>
#include <QDBusArgument>
#include <QDBusReply>

#ifndef RLIMIT_RTTIME
#define RLIMIT_RTTIME 15
#endif
static const auto RTKIT_SERVICE_NAME = QStringLiteral("org.freedesktop.RealtimeKit1");
static const auto RTKIT_OBJECT_PATH = QStringLiteral("/org/freedesktop/RealtimeKit1");
static const auto RTKIT_INTERFACE_NAME = QStringLiteral("org.freedesktop.RealtimeKit1");

Q_LOGGING_CATEGORY(logRtKit, "rtkit")

RtKit::RtKit(QObject *parent)
    : QObject(parent)
{
    m_rtkitIntf = new QDBusInterface(RTKIT_SERVICE_NAME,
                                     RTKIT_OBJECT_PATH,
                                     RTKIT_INTERFACE_NAME,
                                     QDBusConnection::systemBus(),
                                     this);
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

    QDBusReply<void> reply = m_rtkitIntf->call(QStringLiteral("MakeThreadHighPriority"),
                                              QVariant::fromValue((qulonglong) thread),
                                              QVariant::fromValue((int32_t) niceLevel));
    if (reply.isValid())
        return true;

    m_lastError = QStringLiteral("Unable to change thread priority via RtKit: %1: %2").arg(reply.error().name(),
                                                                                           reply.error().message());
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

    QDBusReply<void> reply = m_rtkitIntf->call(QStringLiteral("MakeThreadRealtime"),
                                              QVariant::fromValue((qulonglong) thread),
                                              QVariant::fromValue((uint32_t) priority));
    if (reply.isValid())
        return true;

    m_lastError = QStringLiteral("Unable to change thread priority via RtKit: %1: %2").arg(reply.error().name(),
                                                                                           reply.error().message());
    return false;
}

long long RtKit::getIntProperty(const QString &propName, bool *ok)
{
    auto m = QDBusMessage::createMethodCall(RTKIT_SERVICE_NAME,
                                            RTKIT_OBJECT_PATH,
                                            QStringLiteral("org.freedesktop.DBus.Properties"),
                                            QStringLiteral("Get"));
    m << RTKIT_INTERFACE_NAME << propName;

    QDBusReply<QVariant> reply = QDBusConnection::systemBus().call(m);
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
        m_lastError = QStringLiteral("RtKit property DBus request for '%1' failed: %2: %3").arg(propName,
                                                                                                reply.error().name(), reply.error().message());
    }

    if (ok == nullptr)
        qCWarning(logRtKit).noquote() << m_lastError;
    else
        (*ok) = false;

    return LLONG_MAX;
}

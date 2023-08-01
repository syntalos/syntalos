/*
 * Copyright (C) 2019-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include <QLoggingCategory>
#include <QObject>

Q_DECLARE_LOGGING_CATEGORY(logRtKit)

class QDBusInterface;

class RtKit : public QObject
{
    Q_OBJECT
public:
    explicit RtKit(QObject *parent = nullptr);

    QString lastError() const;

    int queryMaxRealtimePriority(bool *ok = nullptr);
    int queryMinNiceLevel(bool *ok = nullptr);
    long long queryRTTimeUSecMax(bool *ok = nullptr);

    bool makeHighPriority(pid_t thread, int niceLevel);
    bool makeRealtime(pid_t thread, uint priority);

private:
    QDBusInterface *m_rtkitIntf;
    QString m_lastError;

    long long getIntProperty(const QString &propName, bool *ok = nullptr);
};

bool setCurrentThreadNiceness(int nice);
bool setCurrentThreadRealtime(int priority);

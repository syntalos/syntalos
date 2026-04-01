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

#pragma once

#include <string>

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logRtKit)

typedef struct _GDBusProxy GDBusProxy;

class RtKit
{
public:
    explicit RtKit();
    ~RtKit();

    [[nodiscard]] std::string lastError() const;

    int queryMaxRealtimePriority(bool *ok = nullptr);
    int queryMinNiceLevel(bool *ok = nullptr);
    long long queryRTTimeUSecMax(bool *ok = nullptr);

    bool makeHighPriority(pid_t thread, int niceLevel);
    bool makeRealtime(pid_t thread, uint priority);

private:
    GDBusProxy *m_rtkitProxy;
    GDBusProxy *m_rtPortalProxy;
    std::string m_lastError;

    int64_t getIntProperty(const std::string &propName, bool *ok = nullptr);
};

bool setCurrentThreadNiceness(int nice);
bool setCurrentThreadRealtime(int priority);

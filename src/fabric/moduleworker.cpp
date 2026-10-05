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

#include "moduleworker.h"

#include <algorithm>

#include "moduleapi.h"

using namespace Syntalos;

detail::WorkerHolderBase::~WorkerHolderBase() = default;

void WorkerTimer::start(const microseconds_t &delay) const
{
    if (!d)
        return;
    const std::lock_guard<std::mutex> lock(d->mutex);
    if (d->arm)
        d->arm(std::max<std::int64_t>(delay.count(), 0));
}

void WorkerTimer::start(const milliseconds_t &delay) const
{
    start(std::chrono::duration_cast<microseconds_t>(delay));
}

void WorkerTimer::stop() const
{
    if (!d)
        return;
    const std::lock_guard<std::mutex> lock(d->mutex);
    if (d->arm)
        d->arm(-1);
}

void WorkerContext::waitForStart() const
{
    if (m_mod != nullptr)
        m_mod->workerWaitForStart();
}

void WorkerContext::raiseError(const QString &message) const
{
    if (m_mod != nullptr)
        m_mod->raiseError(message);
}

void WorkerContext::raiseError(const std::string &message) const
{
    raiseError(QString::fromStdString(message));
}

void WorkerContext::raiseError(const char *message) const
{
    raiseError(QString::fromUtf8(message));
}

void WorkerContext::setStatusMessage(const QString &message) const
{
    if (m_mod != nullptr)
        m_mod->setStatusMessage(message);
}

void WorkerContext::setStatusMessage(const std::string &message) const
{
    setStatusMessage(QString::fromStdString(message));
}

void WorkerContext::setStatusMessage(const char *message) const
{
    setStatusMessage(QString::fromUtf8(message));
}

void WorkerContext::setStateDormant() const
{
    if (m_mod != nullptr)
        m_mod->setStateDormant();
}

void WorkerContext::setStateReady() const
{
    if (m_mod != nullptr)
        m_mod->setStateReady();
}

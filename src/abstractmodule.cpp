/*
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <opencv2/core.hpp>
#include "abstractmodule.h"

AbstractModule::AbstractModule(QObject *parent) :
    QObject(parent),
    m_name(id()),
    m_started(false),
    m_state(ModuleState::PREPARING),
    m_initialized(false)
{
}

ModuleState AbstractModule::state() const
{
    return m_state;
}

QString AbstractModule::id() const
{
    return QStringLiteral("unknown");
}

QString AbstractModule::name() const
{
    return m_name;
}

void AbstractModule::setName(const QString &name)
{
    m_name = name;
}

QString AbstractModule::description() const
{
    return QStringLiteral("An unknown description.");
}

QPixmap AbstractModule::pixmap() const
{
    return QPixmap(":/module/generic");
}

ModuleFeatures AbstractModule::features() const
{
    return ModuleFeature::DISPLAY |
           ModuleFeature::SETTINGS |
           ModuleFeature::ACTIONS;
}

void AbstractModule::start()
{
    m_started = true;
}

bool AbstractModule::runCycle()
{
    return true;
}

bool AbstractModule::prepareThreads()
{
    m_started = false;
    return true;
}

void AbstractModule::finalize()
{
    // Do nothing.
}

void AbstractModule::showDisplayUi()
{
    // Do nothing.
}

void AbstractModule::showSettingsUi()
{
    // Do nothing
}

void AbstractModule::hideDisplayUi()
{
    // Do nothing
}

void AbstractModule::hideSettingsUi()
{
    // Do nothing.
}

QList<QAction *> AbstractModule::actions()
{
    QList<QAction*> res;
    return res;
}

QByteArray AbstractModule::serializeSettings()
{
    QByteArray zero;
    return zero;
}

bool AbstractModule::loadSettings(const QByteArray &data)
{
    Q_UNUSED(data);
    return true;
}

QString AbstractModule::lastError() const
{
    return m_lastError;
}

bool AbstractModule::singleton() const
{
    return false;
}

bool AbstractModule::canRemove(AbstractModule *mod)
{
    Q_UNUSED(mod);
    return true;
}

void AbstractModule::setInitialized()
{
    m_initialized = true;
}

bool AbstractModule::initialized() const
{
    return m_initialized;
}

void AbstractModule::setState(ModuleState state)
{
    m_state = state;
    emit stateChanged(state);
}

void AbstractModule::raiseError(const QString &message)
{
    m_lastError = message;
    emit error(message);
    setState(ModuleState::ERROR);
}

void AbstractModule::setStatusMessage(const QString &message)
{
    emit statusMessage(message);
}

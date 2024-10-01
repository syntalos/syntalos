/*
 * Copyright (C) 2020-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "syntaloslinkmodule.h"

#include <QDebug>
#include <QCoreApplication>

using namespace Syntalos;

namespace Syntalos
{

class SyntalosLinkModule::Private
{
public:
    Private() {}

    ~Private() {}
};

SyntalosLinkModule::SyntalosLinkModule(SyntalosLink *slink)
    : QObject(slink),
      m_running(false),
      d(new SyntalosLinkModule::Private),
      m_slink(slink)
{

    // set callbacks
    m_slink->setShutdownCallback([this]() {
        m_running = false;
        qDebug().noquote() << "Shutting down.";
        QCoreApplication::processEvents();
        awaitData(1000);
        exit(0);
    });

    m_slink->setPrepareStartCallback([this](const QByteArray &settings) {
        prepare(settings);
    });

    m_slink->setStartCallback([this]() {
        m_running = true;
        start();
    });

    m_slink->setStopCallback([this]() {
        m_running = false;
        stop();
    });
}

SyntalosLinkModule::~SyntalosLinkModule() {}

void SyntalosLinkModule::raiseError(const QString &title, const QString &message)
{
    m_slink->raiseError(title, message);
}

void SyntalosLinkModule::raiseError(const QString &message)
{
    m_slink->raiseError(message);
}

SyncTimer *SyntalosLinkModule::timer() const
{
    return m_slink->timer();
}

void SyntalosLinkModule::awaitData(int timeoutUsec)
{
    m_slink->awaitData(timeoutUsec);
}

ModuleState SyntalosLinkModule::state() const
{
    return m_slink->state();
}

void SyntalosLinkModule::setState(ModuleState state)
{
    m_slink->setState(state);
}

void SyntalosLinkModule::setStatusMessage(const QString &message)
{
    m_slink->setStatusMessage(message);
}

bool SyntalosLinkModule::prepare(const QByteArray &settings)
{
    setState(ModuleState::PREPARING);
    return true;
}

void SyntalosLinkModule::start()
{
    // Implemented by derived classes
    setState(ModuleState::RUNNING);
}

void SyntalosLinkModule::stop()
{
    // Implemented by derived classes
    setState(ModuleState::IDLE);
}

} // namespace Syntalos

/*
 * Copyright (C) 2020-2026 Matthias Klumpp <matthias@tenstral.net>
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
#include <QTimer>
#include <QCoreApplication>

using namespace Syntalos;

namespace Syntalos
{

class SyntalosLinkModule::Private
{
public:
    Private() = default;
    ~Private() = default;
};

SyntalosLinkModule::SyntalosLinkModule(SyntalosLink *slink)
    : m_running(false),
      d(new SyntalosLinkModule::Private),
      m_slink(slink)
{
    // set callbacks
    m_slink->setShutdownCallback([this]() {
        m_running = false;
        shutdown();
    });

    m_slink->setPrepareRunCallback([this]() {
        if (!prepare()) {
            if (m_slink->state() != ModuleState::ERROR)
                raiseError("Module preparation failed.");
            return false;
        }
        return true;
    });

    m_slink->setStartCallback([this]() {
        m_running = true;
        start();
    });

    m_slink->setStopCallback([this]() {
        m_running = false;
        stop();
    });

    m_slink->setLoadSettingsCallback([this](const ByteVector &settings, const fs::path &baseDir) {
        if (!loadSettings(settings, baseDir)) {
            if (m_slink->state() != ModuleState::ERROR)
                raiseError("Loading settings failed.");
            return false;
        }

        return true;
    });

    m_slink->setSaveSettingsCallback([this](ByteVector &settings, const fs::path &baseDir) {
        saveSettings(settings, baseDir);
        return true;
    });

    // signal that we are ready and done with initialization
    m_slink->setState(ModuleState::IDLE);
}

SyntalosLinkModule::~SyntalosLinkModule() = default;

void SyntalosLinkModule::raiseError(const std::string &title, const std::string &message)
{
    m_slink->raiseError(title, message);
}

void SyntalosLinkModule::raiseError(const std::string &message)
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

void SyntalosLinkModule::setStatusMessage(const std::string &message)
{
    m_slink->setStatusMessage(message);
}

bool SyntalosLinkModule::prepare()
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

void SyntalosLinkModule::shutdown()
{
    qDebug().noquote() << "Shutting down.";
    QCoreApplication::processEvents();
    awaitData(1000);
    // Use quit() instead of exit() so the Qt event loop returns from any a.exec() in main(),
    // allowing the C++ stack to unwind and all destructors to run properly.
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
}

const TestSubjectInfo &SyntalosLinkModule::testSubject() const
{
    return m_slink->testSubject();
}

void SyntalosLinkModule::saveSettings(ByteVector &settings, const fs::path &baseDir)
{
    // to be implemented by child classes
}

bool SyntalosLinkModule::loadSettings(const ByteVector &settings, const fs::path &baseDir)
{
    return true;
}

} // namespace Syntalos

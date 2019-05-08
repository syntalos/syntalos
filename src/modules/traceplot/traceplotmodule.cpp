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

#include "traceplotmodule.h"

#include <QMessageBox>

#include "traceplotproxy.h"
#include "tracedisplay.h"
#include "modules/rhd2000/rhd2000module.h"

TracePlotModule::TracePlotModule(QObject *parent)
    : AbstractModule(parent),
      m_traceProxy(nullptr),
      m_displayWindow(nullptr),
      m_intanModule(nullptr)
{
    m_name = QStringLiteral("TracePlot");
}

TracePlotModule::~TracePlotModule()
{
    // unregister with the Intan module
    if (m_intanModule)
        m_intanModule->setPlotProxy(nullptr);

    // cleanup
    if (m_displayWindow)
        delete m_displayWindow;
    if (m_traceProxy)
        delete m_traceProxy;
}

QString TracePlotModule::id() const
{
    return QStringLiteral("traceplot");
}

QString TracePlotModule::description() const
{
    return QStringLiteral("Allow real-time display and modification of user selected data traces recorded with a Intan RHD2000 module.");
}

QPixmap TracePlotModule::pixmap() const
{
    return QPixmap(":/module/traceplot");
}

bool TracePlotModule::singleton() const
{
    return true;
}

bool TracePlotModule::canRemove(AbstractModule *mod)
{
    return mod != m_intanModule;
}

bool TracePlotModule::initialize(ModuleManager *manager)
{
    assert(!initialized());
    setState(ModuleState::INITIALIZING);

    // create trace parameters and data proxy for the trace display
    m_traceProxy = new TracePlotProxy(this);
    m_displayWindow = new TraceDisplay(m_traceProxy);

    m_intanModule = nullptr;
    Q_FOREACH(auto mod, manager->activeModules()) {
        auto rhdmod = dynamic_cast<Rhd2000Module*>(mod);
        if (rhdmod) {
            m_intanModule = rhdmod;
        }
    }
    if (m_intanModule == nullptr) {
        raiseError("Unable to find an RHD2000 module to connect to Please add a module of this type first.");
        return false;
    }

    m_intanModule->setPlotProxy(m_traceProxy);
    setState(ModuleState::READY);
    setInitialized();
    return true;
}

bool TracePlotModule::prepare(const QString &storageRootDir, const TestSubject &testSubject, HRTimer *timer)
{
    Q_UNUSED(storageRootDir);
    Q_UNUSED(testSubject);
    Q_UNUSED(timer);
    setState(ModuleState::PREPARING);

    // reset trace plot data
    m_traceProxy->reset();

    setState(ModuleState::READY);
    return true;
}

void TracePlotModule::stop()
{

}

void TracePlotModule::showDisplayUi()
{
    m_displayWindow->show();
}

void TracePlotModule::hideDisplayUi()
{
    m_displayWindow->hide();
}

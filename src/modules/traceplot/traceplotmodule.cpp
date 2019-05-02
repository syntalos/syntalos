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

#include "traceplotproxy.h"
#include "tracedisplay.h"

TracePlotModule::TracePlotModule(QObject *parent)
    : AbstractModule(parent)
{
    m_traceProxy = new TracePlotProxy;
    m_displayWindow = new TraceDisplay;

    // set the combined trace parameters and data proxy for the "Trace" tab
    m_traceProxy = new TracePlotProxy(this);
    m_displayWindow->setPlotProxy(m_traceProxy);

    //! FIXME
   // m_intanUI->getWavePlot()->setPlotProxy(m_traceProxy);
}

TracePlotModule::~TracePlotModule()
{
    delete m_displayWindow;
    delete m_traceProxy;
}

QString TracePlotModule::id()
{
    return QStringLiteral("traceplot");
}

QString TracePlotModule::displayName() const
{
    return QStringLiteral("TracePlot");
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

bool TracePlotModule::initialize()
{
    setState(ModuleState::READY);
    return true;
}

bool TracePlotModule::prepare(const QString &storageRootDir, const QString &subjectId)
{

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

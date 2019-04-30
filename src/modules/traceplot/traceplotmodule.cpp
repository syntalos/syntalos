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
}

TracePlotModule::~TracePlotModule()
{
    delete m_displayWindow;
    delete m_traceProxy;
}

QString TracePlotModule::name() const
{
    return QStringLiteral("traceplot");
}

QString TracePlotModule::displayName() const
{
    return QStringLiteral("TracePlot");
}

QPixmap TracePlotModule::pixmap() const
{
    return QPixmap(":/module/traceplot");
}

bool TracePlotModule::initialize()
{
    setState(ModuleState::READY);
    return true;
}

void TracePlotModule::showDisplayUi()
{
    m_displayWindow->show();
}

void TracePlotModule::hideDisplayUi()
{
    m_displayWindow->hide();
}

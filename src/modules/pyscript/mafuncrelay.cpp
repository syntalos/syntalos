/*
 * Copyright (C) 2019 Matthias Klumpp <matthias@tenstral.net>
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

#include "mafuncrelay.h"

#include <QDebug>

QHash<QString, QRect> MaFuncRelay::ms_eventTableGeometries = QHash<QString, QRect>();

MaFuncRelay::MaFuncRelay(const QString &eventTablesDir, QObject *parent)
    : QObject(parent)
{
    m_canStartScript = false;
    m_eventTablesDir = eventTablesDir;
}

MaFuncRelay::~MaFuncRelay()
{
    // safeguard to not have a massive table in case some Python script goes
    // crazy and many tables with different names were created in the past
    if (ms_eventTableGeometries.count() > 20)
       ms_eventTableGeometries.clear();

    // delete tables and store their geometries in a static hash map
    for (auto &tab : m_eventTables) {
        ms_eventTableGeometries.insert(tab->name(), tab->geometry());
        delete tab;
    }
}

void MaFuncRelay::setPyScript(const QString &script)
{
    m_pyScript = script;
}

QString MaFuncRelay::pyScript() const
{
    return m_pyScript;
}

void MaFuncRelay::setCanStartScript(bool startable)
{
    m_canStartScript = startable;
}

bool MaFuncRelay::canStartScript() const
{
    return m_canStartScript;
}

int MaFuncRelay::registerNewFirmataModule(const QString &)
{
#if 0
    for (auto &mod : m_modManager->activeModules()) {
        auto fmod = qobject_cast<FirmataIOModule*>(mod);
        if (fmod == nullptr)
            continue;
        if (fmod->name() == name) {
            m_firmataModRegistry.append(fmod);
            return m_firmataModRegistry.size() - 1;
        }
    }
#endif

    return -1;
}

FirmataIOModule *MaFuncRelay::firmataModule(int id)
{
    if (id > m_firmataModRegistry.size() - 1)
        return nullptr;
    return m_firmataModRegistry[id];
}

int MaFuncRelay::newEventTable(const QString &name)
{
    auto tab = new EventTable(m_eventTablesDir, name, this);
    tab->open();

    // restore previous geoemtry, if we have one recorded for this table
    const auto geometry = ms_eventTableGeometries.value(tab->name());
    if (!geometry.isEmpty())
        tab->setGeometry(geometry);

    tab->show();
    m_eventTables.append(tab);
    return m_eventTables.size() - 1;
}

bool MaFuncRelay::eventTableSetHeader(int tableId, const QStringList &headers)
{
    if (tableId > m_eventTables.size() - 1)
        return false;

    auto tab = m_eventTables[tableId];
    auto data = QStringList(headers);
    data.prepend("Time");
    tab->setHeader(data);
    return true;
}

bool MaFuncRelay::eventTableAddEvent(long long timestamp, int tableId, const QStringList &event)
{
    if (tableId > m_eventTables.size() - 1)
        return false;

    auto tab = m_eventTables[tableId];
    auto data = QStringList(event);
    data.prepend(QString::number(timestamp));

    tab->addEvent(data);
    return true;
}

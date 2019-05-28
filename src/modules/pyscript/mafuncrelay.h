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

#ifndef MAFUNCRELAY_H
#define MAFUNCRELAY_H

#include <QObject>
#include "modulemanager.h"

#include "hrclock.h"
#include "eventtable.h"
#include "modules/firmata-io/firmataiomodule.h"

class MaFuncRelay : public QObject
{
    Q_OBJECT
public:
    explicit MaFuncRelay(ModuleManager *modManager, const QString& eventTablesDir, QObject *parent = nullptr);
    ~MaFuncRelay();

    void setPyScript(const QString& script);
    QString pyScript() const;

    void setCanStartScript(bool startable);
    bool canStartScript() const;

    int registerNewFirmataModule(const QString& name);
    FirmataIOModule *firmataModule(int id);

    int newEventTable(const QString& name);
    bool eventTableSetHeader(int tableId, const QStringList& headers);
    bool eventTableAddEvent(long long timestamp, int tableId, const QStringList& event);

private:
    QString m_pyScript;
    bool m_canStartScript;
    QString m_eventTablesDir;

    ModuleManager *m_modManager;
    QList<FirmataIOModule*> m_firmataModRegistry;

    QList<EventTable*> m_eventTables;
};

#endif // MAFUNCRELAY_H

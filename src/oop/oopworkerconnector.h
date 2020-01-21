/*
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#include <QObject>
#include <QSharedPointer>
#include <memory>

#include "sharedmemory.h"
#include "rep_interface_replica.h"

class OOPWorkerConnector : public QObject
{
    Q_OBJECT
public:
    OOPWorkerConnector(QSharedPointer<OOPWorkerReplica> ptr);
    ~OOPWorkerConnector() override;

    bool connectAndRun();

private:
    QSharedPointer<OOPWorkerReplica> m_reptr;
    QProcess *m_proc;
    std::unique_ptr<SharedMemory> m_shmSend;
    std::unique_ptr<SharedMemory> m_shmRecv;
};

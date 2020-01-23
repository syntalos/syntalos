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
#include <QProcess>
#include <memory>

#include "moduleapi.h"
#include "rep_interface_replica.h"

class SharedMemory;

class OOPWorkerConnector : public QObject
{
    Q_OBJECT
public:
    OOPWorkerConnector(QSharedPointer<OOPWorkerReplica> ptr);
    ~OOPWorkerConnector() override;

    void terminate(QEventLoop *loop = nullptr);

    bool connectAndRun();

    void setInputPorts(QList<std::shared_ptr<StreamInputPort>> inPorts);
    void setOutputPorts(QList<std::shared_ptr<StreamOutputPort>> outPorts);

    void initWithPythonScript(const QString &script, const QString &env = QString());

    void start(steady_hr_timepoint timePoint);

private:
    QSharedPointer<OOPWorkerReplica> m_reptr;
    QProcess *m_proc;

    std::vector<std::unique_ptr<SharedMemory>> m_shmSend;
    std::vector<std::unique_ptr<SharedMemory>> m_shmRecv;
    QList<std::shared_ptr<StreamOutputPort>> m_outPorts;
};

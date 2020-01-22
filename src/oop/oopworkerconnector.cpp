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

#include "oopworkerconnector.h"

#include <thread>
#include <QUuid>

#include <opencv2/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "utils.h"
#include "cvmatshm.h"

OOPWorkerConnector::OOPWorkerConnector(QSharedPointer<OOPWorkerReplica> ptr)
    : QObject(nullptr),
      m_reptr(ptr),
      m_proc(new QProcess(this)),
      m_shmSend(new SharedMemory),
      m_shmRecv(new SharedMemory)
{
    m_proc->setProcessChannelMode(QProcess::ForwardedChannels);
}

OOPWorkerConnector::~OOPWorkerConnector()
{
    terminate();
}

void OOPWorkerConnector::terminate()
{
    if (m_proc->state() != QProcess::Running)
        return;

    // ask the worker to shut down
    m_reptr->shutdown();

    // give our worker 10sec to react
    m_proc->waitForFinished(10000);
    m_proc->terminate();

    // give the process 5sec to terminate
    m_proc->waitForFinished(5000);

    // finally kill the unresponsive worker
    m_proc->kill();
}

bool OOPWorkerConnector::connectAndRun()
{
    const auto address = QStringLiteral("local:maw-%1").arg(createRandomString(16));
    m_reptr->node()->connectToNode(QUrl(address));

    const auto workerExe = QStringLiteral("%1/pyworker/pyworker").arg(QCoreApplication::applicationDirPath());
    m_proc->start(workerExe, QStringList() << address);
    if (!m_proc->waitForStarted())
        return false;
    return m_reptr->waitForSource(10000);
}

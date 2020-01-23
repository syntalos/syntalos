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
      m_proc(new QProcess(this))
{
    m_proc->setProcessChannelMode(QProcess::ForwardedChannels);
}

OOPWorkerConnector::~OOPWorkerConnector()
{
    terminate();
}

void OOPWorkerConnector::terminate(QEventLoop *loop)
{
    if (m_proc->state() != QProcess::Running)
        return;

    // ask the worker to shut down
    m_reptr->shutdown();
    if (loop)
        loop->processEvents();
    else
        QCoreApplication::processEvents();

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

void OOPWorkerConnector::setInputPorts(QList<std::shared_ptr<StreamInputPort> > inPorts)
{
    m_shmSend.clear();

    QList<InputPortInfo> iPortInfo;
    for (int i = 0; i < inPorts.size(); i++) {
        const auto &iport = inPorts[i];
        std::unique_ptr<SharedMemory> shm(new SharedMemory);
        auto shmPtr = shm.get();
        m_shmSend.push_back(std::move(shm));

        InputPortInfo pi;
        pi.setId(i);
        pi.setTitle(iport->title());

        pi.setConnected(false);
        if (iport->hasSubscription()) {
            pi.setConnected(true);
            pi.setMetadata(iport->subscriptionVar()->metadata());
        }
        pi.setDataTypeName(iport->acceptedTypeName());

        shmPtr->createShmKey();
        pi.setShmKeyRecv(shmPtr->shmKey());

        iPortInfo.append(pi);
    }

    m_reptr->setInputPortInfo(iPortInfo);
}

void OOPWorkerConnector::setOutputPorts(QList<std::shared_ptr<StreamOutputPort> > outPorts)
{
    m_shmSend.clear();
    m_outPorts.clear();

    QList<OutputPortInfo> oPortInfo;
    for (int i = 0; i < outPorts.size(); i++) {
        const auto &oport = outPorts[i];
        std::unique_ptr<SharedMemory> shm(new SharedMemory);
        auto shmPtr = shm.get();
        m_shmRecv.push_back(std::move(shm));
        m_outPorts.append(oport);

        OutputPortInfo pi;
        pi.setId(i);
        pi.setTitle(oport->title());

        pi.setConnected(true); // TODO: Make this dependent on whether something is actually subscribed to the port
        pi.setMetadata(oport->streamVar()->metadata());
        pi.setDataTypeName(oport->streamVar()->dataTypeName());

        shmPtr->createShmKey();
        pi.setShmKeySend(shmPtr->shmKey());

        oPortInfo.append(pi);
    }

    m_reptr->setOutputPortInfo(oPortInfo);
}

void OOPWorkerConnector::initWithPythonScript(const QString &script, const QString &env)
{
    m_reptr->initializeFromData(script, env).waitForFinished(10000);
}

void OOPWorkerConnector::start(steady_hr_timepoint timePoint)
{
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count();
    m_reptr->start(timestamp);
}

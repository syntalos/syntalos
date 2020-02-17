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

#include "streams/frametype.h"
#include "utils.h"
#include "ipcmarshal.h"

OOPWorkerConnector::OOPWorkerConnector(QSharedPointer<OOPWorkerReplica> ptr, const QString &workerBin)
    : QObject(nullptr),
      m_reptr(ptr),
      m_proc(new QProcess(this)),
      m_workerBinary(workerBin)
{
    connect(m_reptr.data(), &OOPWorkerReplica::sendOutput, this, &OOPWorkerConnector::receiveOutput);
    connect(m_reptr.data(), &OOPWorkerReplica::updateOutPortMetadata, this, &OOPWorkerConnector::receiveOutputPortMetadataUpdate);

    // merge stdout of worker with ours by default
    setCaptureStdout(false);
}

OOPWorkerConnector::~OOPWorkerConnector()
{
    terminate();
}

void OOPWorkerConnector::setWorkerBinary(const QString &binPath)
{
    m_workerBinary = binPath;
}

void OOPWorkerConnector::terminate(QEventLoop *loop)
{
    if (loop)
        loop->processEvents();

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
    m_failed = false;
    const auto address = QStringLiteral("local:maw-%1").arg(createRandomString(16));
    m_reptr->node()->connectToNode(QUrl(address));

    if (m_workerBinary.isEmpty()) {
        qWarning().noquote() << "OOP module has not set a worker binary";
        m_failed = true;
        return false;
    }

    m_proc->start(m_workerBinary, QStringList() << address);
    if (!m_proc->waitForStarted()) {
        m_failed = true;
        return false;
    }

    if (!m_reptr->waitForSource(10000)) {
        m_failed = true;
        return false;
    }

    return true;
}

void OOPWorkerConnector::setInputPorts(QList<std::shared_ptr<VarStreamInputPort> > inPorts)
{
    m_shmSend.clear();
    m_subs.clear();

    QList<InputPortInfo> iPortInfo;
    for (int i = 0; i < inPorts.size(); i++) {
        const auto &iport = inPorts[i];
        std::unique_ptr<SharedMemory> shm(new SharedMemory);
        auto shmPtr = shm.get();
        m_shmSend.push_back(std::move(shm));

        InputPortInfo pi;
        pi.setId(i);
        pi.setIdstr(iport->id());

        pi.setConnected(false);
        if (iport->hasSubscription()) {
            pi.setConnected(true);
            pi.setMetadata(iport->subscriptionVar()->metadata());

            m_subs.push_back(std::make_pair(i, iport->subscriptionVar()));
        }
        pi.setDataTypeName(iport->dataTypeName());

        shmPtr->createShmKey();
        pi.setShmKeyRecv(shmPtr->shmKey());

        iPortInfo.append(pi);
    }

    m_reptr->setInputPortInfo(iPortInfo);
}

void OOPWorkerConnector::setOutputPorts(QList<std::shared_ptr<StreamOutputPort> > outPorts)
{
    m_shmRecv.clear();
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
        pi.setIdstr(oport->id());

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

void OOPWorkerConnector::forwardInputData(QEventLoop *loop)
{
    for(auto &sip : m_subs) {
        if (m_failed)
            break;

        // retrieve next variant, don't wait
        auto res = sip.second->peekNextVar();
        if (res.isValid())
            sendInputData(sip.second->dataTypeId(), sip.first, res, loop);
    }
}

bool OOPWorkerConnector::failed() const
{
    return m_failed;
}

bool OOPWorkerConnector::captureStdout() const
{
    return m_captureStdout;
}

void OOPWorkerConnector::setCaptureStdout(bool capture)
{
    m_captureStdout = capture;
    if (m_captureStdout)
        m_proc->setProcessChannelMode(QProcess::MergedChannels);
    else
        m_proc->setProcessChannelMode(QProcess::ForwardedChannels);
}

QString OOPWorkerConnector::readProcessStdout()
{
    if (!m_captureStdout)
        return QString();

    return m_proc->readAllStandardOutput();
}

static bool unmarshalDataAndOutput(int typeId, const QVariantList &params, std::unique_ptr<SharedMemory> &shm, StreamOutputPort *port)
{
    if (typeId == qMetaTypeId<Frame>()) {
        milliseconds_t msec(params[0].toLongLong());
        Frame frame(cvMatFromShm(shm), msec);

        port->stream<Frame>()->push(frame);
        return true;
    }

    if (typeId == qMetaTypeId<ControlCommand>()) {
        ControlCommand ctl;
        ctl.kind = static_cast<ControlCommandKind>(params[0].toInt());
        ctl.command = params[0].toString();

        port->stream<ControlCommand>()->push(ctl);
        return true;
    }

    if (typeId == qMetaTypeId<TableRow>()) {
        auto rows = params[0].toStringList();
        port->stream<TableRow>()->push(rows);
        return true;
    }

    return false;
}

void OOPWorkerConnector::receiveOutput(int outPortId, QVariantList params)
{
    auto outPort = m_outPorts[outPortId];
    const auto typeId = outPort->dataTypeId();

    if (!unmarshalDataAndOutput(typeId, params, m_shmRecv[outPortId], outPort.get()))
            qWarning().noquote() << "Could not interpret data received from worker on port" << outPort->id();
}

void OOPWorkerConnector::receiveOutputPortMetadataUpdate(int outPortId, QVariantHash metadata)
{
    auto outPort = m_outPorts[outPortId];
    outPort->streamVar()->setMetadata(metadata);
}

void OOPWorkerConnector::sendInputData(int typeId, int portId, const QVariant &data, QEventLoop *loop)
{
    QVariantList params;

    auto ret = marshalDataElement(typeId, data,
                                  params, m_shmSend[portId]);
    if (!ret) {
        const auto dataTypeName = QMetaType::typeName(typeId);
        if (!m_shmSend[portId]->lastError().isEmpty())
            emit m_reptr->error(QStringLiteral("Unable to write %1 element into shared memory: %2").arg(dataTypeName).arg(m_shmSend[portId]->lastError()));
        else
            emit m_reptr->error(QStringLiteral("Marshalling of %1 element for subprocess submission failed. This is a bug.").arg(dataTypeName));
        return;
    }

    if (!m_reptr->receiveInput(portId, params).waitForFinished(100)) {
        // ensure we handle potential error events before emitting our own
        if (loop != nullptr)
            loop->processEvents();

        // if we are in a failed state, we already emitted and error - don't send a second one
        if (m_failed)
            return;

        // if we weren't failed already, the worker died unexpectedly
        m_failed = true;
        emit m_reptr->error(QStringLiteral("Worker failed to react to new input data submission! It probably died."));
    }
}

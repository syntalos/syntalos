/*
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QObject>
#include <QProcess>
#include <QSharedPointer>
#include <memory>

#include "moduleapi.h"
#include "rep_interface_replica.h"

using namespace Syntalos;

class SharedMemory;

class OOPWorkerConnector : public QObject
{
    Q_OBJECT
public:
    OOPWorkerConnector(QSharedPointer<OOPWorkerReplica> ptr, const QString &workerBin);
    ~OOPWorkerConnector();

    void setWorkerBinary(const QString &binPath);

    bool isRunning();
    void terminate(QEventLoop *loop = nullptr);

    bool connectAndRun(const QVector<uint> &cpuAffinity);

    void setPorts(
        QList<std::shared_ptr<VarStreamInputPort>> inPorts,
        QList<std::shared_ptr<StreamOutputPort>> outPorts);

    void initWithPythonScript(const QString &script, const QString &wdir = QString());
    void setPythonVirtualEnv(const QString &venvDir);

    void prepareStart(const QByteArray &settings = QByteArray());
    void start(const symaster_timepoint &timePoint);

    void forwardInputData(QEventLoop *loop = nullptr);

    bool failed() const;

    QRemoteObjectPendingReply<QByteArray> changeSettings(const QByteArray &oldSettings);

    bool captureStdout() const;
    void setCaptureStdout(bool capture);

    QString readProcessStdout();

private slots:
    void receiveReadyChange(bool ready);
    void receiveOutput(int outPortId, QVariant argData);
    void receiveOutputPortMetadataUpdate(int outPortId, const QVariantHash &metadata);
    void receiveInputThrottleRequest(int inPortId, uint itemsPerSec, bool allowMore);

private:
    QSharedPointer<OOPWorkerReplica> m_reptr;
    QProcess *m_proc;
    QString m_workerBinary;
    QString m_pyVenvDir;
    bool m_captureStdout;
    bool m_workerReady;
    bool m_failed;

    std::vector<std::unique_ptr<SharedMemory>> m_shmSend;
    std::vector<std::unique_ptr<SharedMemory>> m_shmRecv;
    std::vector<std::pair<int, std::shared_ptr<VariantStreamSubscription>>> m_subs;
    QList<std::shared_ptr<StreamOutputPort>> m_outPorts;
    int m_inPortsAvailable;
    int m_outPortsAvailable;

    void sendInputData(int typeId, int portId, const QVariant &data, QEventLoop *loop = nullptr);
};

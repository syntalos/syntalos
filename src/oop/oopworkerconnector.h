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

using namespace Syntalos;

class SharedMemory;

class OOPWorkerConnector : public QObject
{
    Q_OBJECT
public:
    OOPWorkerConnector(QSharedPointer<OOPWorkerReplica> ptr, const QString &workerBin);
    ~OOPWorkerConnector();

    void setWorkerBinary(const QString &binPath);

    void terminate(QEventLoop *loop = nullptr);

    bool connectAndRun(const QVector<uint> &cpuAffinity);

    void setPorts(QList<std::shared_ptr<VarStreamInputPort>> inPorts,
                  QList<std::shared_ptr<StreamOutputPort>> outPorts);

    void initWithPythonScript(const QString &script, const QString &env = QString());

    void start(const symaster_timepoint &timePoint);

    void forwardInputData(QEventLoop *loop = nullptr);

    bool failed() const;

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

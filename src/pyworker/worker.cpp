/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include "worker.h"

#include <unistd.h>
#include <sys/mman.h>

#include <opencv2/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "cvmatshm.h"

OOPWorker::OOPWorker(QObject *parent)
    : OOPWorkerSource(parent),
      m_running(false)
{
    qDebug() << "Worker created!";

    m_running = true;
}

OOPWorker::~OOPWorker()
{
}

bool OOPWorker::ready() const
{
    return true;
}

bool OOPWorker::initializeFromData(const QString &script, const QString &env)
{
    Q_UNUSED(env)
    Q_UNUSED(script)
    return false;
}

bool OOPWorker::initializeFromFile(const QString &fname, const QString &env)
{
    Q_UNUSED(env)
    Q_UNUSED(fname)
    return false;
}

void OOPWorker::setInputPortInfo(QList<InputPortInfo> ports)
{
    m_inPortInfo = ports;
    m_shmRecv.clear();

    // set up our incoming shared memory links
    for (int i = 0; i < m_inPortInfo.size(); i++)
        m_shmRecv.append(std::unique_ptr<SharedMemory>(new SharedMemory));

    for (int i = 0; i < m_inPortInfo.size(); i++) {
        if (i >= m_inPortInfo.size()) {
            raiseError("Invalid data sent for input port information!");
            return;
        }
        auto port = m_inPortInfo[i];
        auto shmem = m_shmRecv[port.id()];

        shmem->setShmKey(port.shmKeyRecv());
        shmem->attach();
    }
}

void OOPWorker::setOutputPortInfo(QList<OutputPortInfo> ports)
{
    m_outPortInfo = ports;

    // set up our outgoing shared memory links
    for (int i = 0; i < m_outPortInfo.size(); i++)
        m_shmSend.append(std::unique_ptr<SharedMemory>(new SharedMemory));

    for (int i = 0; i < m_outPortInfo.size(); i++) {
        if (i >= m_outPortInfo.size()) {
            raiseError("Invalid data sent for output port information!");
            return;
        }
        auto port = m_outPortInfo[i];
        auto shmem = m_shmSend[port.id()];

        shmem->setShmKey(port.shmKeySend());

        // FIXME: Create shared memory with the right size for the datatype
        shmem->create(10);
    }
}

void OOPWorker::shutdown()
{
    QCoreApplication::exit(0);
}

void OOPWorker::run()
{
    if (!m_running) {
        // we are not running - check again later
        QTimer::singleShot(0, this, &OOPWorker::run);
        return;
    }
}

bool OOPWorker::submitInput(int inPortId)
{
    Q_UNUSED(inPortId)
    // TODO

    return true;
}

void OOPWorker::raiseError(const QString &message)
{
    emit error(message);
    shutdown();
}

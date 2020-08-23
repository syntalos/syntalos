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

#pragma once

#include <QObject>
#include <QQueue>
#include <QTimer>
#include <pybind11/pybind11.h>

#include "rep_interface_source.h"
#include "sharedmemory.h"
#include "ipcmarshal.h"

namespace py = pybind11;
using namespace Syntalos;
class PyBridge;

class OOPWorker : public OOPWorkerSource
{
    Q_OBJECT
public:
    OOPWorker(QObject *parent = nullptr);
    ~OOPWorker() override;

    Stage stage() const override;

    std::optional<InputPortInfo> inputPortInfoByIdString(const QString &idstr);
    std::optional<OutputPortInfo> outputPortInfoByIdString(const QString &idstr);

    bool submitOutput(int outPortId, py::object pyObj);
    void setOutPortMetadataValue(int outPortId, const QString &key, const QVariant &value);
    void setInputThrottleItemsPerSec(int inPortId, uint itemsPerSec, bool allowMore = true);

    void raiseError(const QString &message);

    void makeDocFileAndQuit(const QString &fname);

public Q_SLOTS:
    bool setNiceness(int nice) override;
    void setMaxRealtimePriority(int priority) override;
    void setCPUAffinity(QVector<uint> cores) override;

    bool initializeFromData(const QString & script, const QString & env) override;
    bool initializeFromFile(const QString & fname, const QString & env) override;

    void setInputPortInfo(const QList<InputPortInfo> &ports) override;
    void setOutputPortInfo(const QList<OutputPortInfo> &ports) override;

    void start(long startTimestampUsec) override;
    void shutdown() override;

    void runScript();

    std::optional<bool> waitForInput();
    bool receiveInput(int inPortId, const QVariant &argData = QVariant()) override;

protected:
    void setStage(Stage stage);

private:
    Stage m_stage;
    bool m_running;
    QString m_script;
    std::vector<std::unique_ptr<SharedMemory>> m_shmSend;
    std::vector<std::unique_ptr<SharedMemory>> m_shmRecv;

    QList<InputPortInfo> m_inPortInfo;
    QList<OutputPortInfo> m_outPortInfo;

    int m_maxRTPriority;

    PyBridge *m_pyb;

    void emitPyError();
};

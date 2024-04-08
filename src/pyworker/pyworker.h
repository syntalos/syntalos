/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include <pybind11/pybind11.h>
#include <QObject>
#include <QHash>
#include <QTimer>
#include <syntaloslink.h>

namespace py = pybind11;
using namespace Syntalos;
class PyBridge;

namespace Syntalos
{
Q_DECLARE_LOGGING_CATEGORY(logPyWorker)
}

using PyNewDataFn = std::function<void(const py::object &obj)>;

class Q_DECL_HIDDEN PyWorker : public QObject
{
    Q_OBJECT
public:
    PyWorker(SyntalosLink *slink, QObject *parent = nullptr);
    ~PyWorker() override;

    ModuleState state() const;
    SyncTimer *timer() const;
    bool isRunning() const;

    std::shared_ptr<InputPortInfo> inputPortById(const QString &idstr);
    std::shared_ptr<OutputPortInfo> outputPortById(const QString &idstr);

    void awaitData(int timeoutUsec = -1);

    bool submitOutput(const std::shared_ptr<OutputPortInfo> &oport, const py::object &pyObj);

    void setOutPortMetadataValue(
        const std::shared_ptr<OutputPortInfo> &oport,
        const QString &key,
        const QVariant &value);
    void setInputThrottleItemsPerSec(const std::shared_ptr<InputPortInfo> &iport, uint itemsPerSec);

    void raiseError(const QString &message);
    bool loadPythonScript(const QString &script, const QString &wdir);

    QByteArray changeSettings(const QByteArray &oldSettings);

    bool prepareStart(const QByteArray &settings);
    void start();
    bool stop();
    void shutdown();
    void prepareAndRun();

    static void makeDocFileAndQuit(const QString &fname);

protected:
    void setState(ModuleState state);

private:
    SyntalosLink *m_link;
    PyBridge *m_pyb;
    QTimer *m_evTimer;
    bool m_pyInitialized;
    PyObject *m_pyMain;

    bool m_running;
    QByteArray m_settings;

    void emitPyError();
};

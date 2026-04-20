/*
 * Copyright (C) 2019-2026 Matthias Klumpp <matthias@tenstral.net>
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
#define SY_DATACTL_NO_FRAMETYPE
#include <syntalos-mlink>
#undef SY_DATACTL_NO_FRAMETYPE

namespace py = pybind11;
using namespace Syntalos;

namespace Syntalos
{
Q_DECLARE_LOGGING_CATEGORY(logPyWorker)
}

class Q_DECL_HIDDEN PyWorker : public QObject
{
    Q_OBJECT
public:
    PyWorker(SyntalosLink *slink, QObject *parent = nullptr);
    ~PyWorker() override;

    ModuleState state() const;
    SyncTimer *timer() const;
    bool isRunning() const;

    void awaitData(int timeoutUsec = -1);

    void raiseError(const std::string &message);
    bool loadPythonScript(const std::string &script, const std::string &wdir);

    bool prepareStart(const ByteVector &settings);
    void start();
    bool stop();
    void shutdown();
    void executePythonRunFn();

protected:
    void setState(ModuleState state);

private:
    SyntalosLink *m_link;
    QTimer *m_evTimer;
    bool m_scriptLoaded;

    bool m_running;
    ByteVector m_settings;

    py::module_ m_mlinkMod; // keeps syntalos_mlink alive for the interpreter lifetime
    py::object m_mlinkObj;  // keeps the PySyLinkManager wrapper alive

    void resetPyCallbacks();
    bool initPythonInterpreter();
    void emitPyError();
};

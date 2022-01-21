/*
 * Copyright (C) 2016-2018 Matthias Klumpp <matthias@tenstral.net>
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

#define QT_NO_KEYWORDS
#pragma once

#include <QObject>
#include <mutex>
#include <memory>
#include <pybind11/pybind11.h>

#include "../syclock.h"

namespace py = pybind11;
using namespace Syntalos;

class OOPWorker;
class Q_DECL_HIDDEN PyBridge : public QObject
{
    Q_OBJECT
public:
    static PyBridge *instance(OOPWorker *worker = nullptr) {
        static std::mutex _mutex;
        std::lock_guard<std::mutex> lock(_mutex);
        static PyBridge *_instance = nullptr;
        if (_instance == nullptr) {
            assert(worker != nullptr);
            _instance = new PyBridge(worker);
        }
        return _instance;
    }

    explicit PyBridge(OOPWorker *worker = nullptr);
    ~PyBridge();

    SyncTimer *timer() const;
    OOPWorker *worker();

    QList<QQueue<py::object>> incomingData;

private:
    Q_DISABLE_COPY(PyBridge)

    SyncTimer *m_syTimer;
    OOPWorker *m_worker;
};

void pythonRegisterSyioModule();

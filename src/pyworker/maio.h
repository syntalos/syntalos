/*
 * Copyright (C) 2016-2018 Matthias Klumpp <matthias@tenstral.net>
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
#include <mutex>
#include <memory>
#include "../hrclock.h"

class PyBridge : public QObject
{
    Q_OBJECT
public:
    static PyBridge *instance(QObject *parent = nullptr) {
        static std::mutex _mutex;
        std::lock_guard<std::mutex> lock(_mutex);
        static PyBridge *_instance = nullptr;
        if (_instance == nullptr) {
            _instance = new PyBridge(parent);
        }
        return _instance;
    }

    explicit PyBridge(QObject *parent = nullptr);
    ~PyBridge();

    HRTimer *timer() const;

private:
    Q_DISABLE_COPY(PyBridge)

    HRTimer *m_timer;
};

void pythonRegisterMaioModule();

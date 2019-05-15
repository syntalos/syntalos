/*
 * Copyright (C) 2019 Matthias Klumpp <matthias@tenstral.net>
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

#include "pycontroller.h"

#include <QCoreApplication>
#include <QDebug>

#include "zmqclient.h"

PyController::PyController(QObject *parent)
    : QObject(parent)
{
    m_conn = new ZmqClient(this);
}

void PyController::run()
{
    qDebug() << "Started new Python session";

    const auto args = QCoreApplication::arguments();
    if (args.length() <= 1) {
        qCritical() << "No socket passed as parameter.";
        emit finished(4);
        return;
    }
    const auto socketName = args[1];

    m_conn->connect(socketName);
    qDebug() << m_conn->runRpc("timeSinceStartMsec");
}

/*
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef ZMQSERVER_H
#define ZMQSERVER_H

#include <memory>
#include <QObject>
#include <QJsonValue>
#include <QJsonArray>

#include "hrclock.h"
#include "rpc-shared-info.h"
#include "mafuncrelay.h"

class ZmqServer : public QObject
{
    Q_OBJECT
public:
    explicit ZmqServer(MaFuncRelay *funcRelay, QObject *parent = nullptr);
    ~ZmqServer();

    bool start(HRTimer *timer = nullptr);
    void stop();

    QString socketName() const;

signals:

private:
    class ZSData;
    std::unique_ptr<ZSData> d;

    QJsonValue handleRpcRequest(const MaPyFunction funcId, const QJsonArray& params);
    static void rpcThread(void *srvPtr);
    bool startRpcThread();
    void finishRpcThread();
};

#endif // ZMQSERVER_H

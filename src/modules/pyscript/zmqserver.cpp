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

#include "zmqserver.h"

#include <czmq.h>
#include <QDebug>
#include <thread>
#include <QJsonDocument>
#include <atomic>

#include "utils.h"
#include "hrclock.h"
#include "rpc-shared-info.h"

#pragma GCC diagnostic ignored "-Wpadded"
class ZmqServer::ZSData
{
public:
    ZSData() {
        running = false;
        timer = nullptr;
        thread = nullptr;
    }
    ~ZSData() { }

    zsock_t *server;
    QString socketPath;
    HRTimer *timer;
    MaFuncRelay *funcRelay;

    std::thread *thread;
    std::atomic_bool running;
};
#pragma GCC diagnostic pop

ZmqServer::ZmqServer(MaFuncRelay *funcRelay, QObject *parent)
    : QObject(parent),
      d(new ZSData)
{
    d->server = zsock_new(ZMQ_REP);
    d->funcRelay = funcRelay;

    const auto socketDir = qEnvironmentVariable("XDG_RUNTIME_DIR", "/tmp/");
    d->socketPath = QStringLiteral("%1/mapy-%2.sock").arg(socketDir).arg(createRandomString(8));
}

ZmqServer::~ZmqServer()
{
    stop();
    zsock_destroy(&d->server);
}

bool ZmqServer::start(HRTimer *timer)
{
    d->timer = timer;
    zsock_bind(d->server, "ipc://%s", d->socketPath.toUtf8().constData());
    return startRpcThread();
}

void ZmqServer::stop()
{
    finishRpcThread();
    d->timer = nullptr;
}

QString ZmqServer::socketName() const
{
    return d->socketPath;
}

QJsonValue ZmqServer::handleRpcRequest(const MaPyFunction funcId, const QJsonArray &params)
{
    switch (funcId) {
    case MaPyFunction::G_getPythonScript: {
        return QJsonValue(d->funcRelay->pyScript());
    }

    case MaPyFunction::G_canStart: {
        return QJsonValue(d->funcRelay->canStartScript());
    }

    case MaPyFunction::G_timeSinceStartMsec: {
        if (d->timer == nullptr)
            return QJsonValue(static_cast<long long>(0));
        return QJsonValue(static_cast<long long>(d->timer->timeSinceStartMsec().count()));
    }

    case MaPyFunction::F_getFirmataModuleId: {
        if (params.count() != 1)
            return QJsonValue(-1);
        return QJsonValue(d->funcRelay->registerNewFirmataModule(params[0].toString()));
    }

    case MaPyFunction::F_newDigitalPin: {
        if (params.count() != 4)
            return QJsonValue(false);
        auto fmod = d->funcRelay->firmataModule(params[0].toInt());
        if (fmod == nullptr)
            return QJsonValue(false);
        auto t = params[3].toInt();
        fmod->newDigitalPin(params[1].toInt(), params[2].toString(), t != 0, t == 2);
        return QJsonValue(true);
    }

    case MaPyFunction::T_newEventTable: {
        if (params.count() != 1)
            return QJsonValue(-1);
        return QJsonValue(d->funcRelay->newEventTable(params[0].toString()));
    }

    case MaPyFunction::T_setHeader: {
        if (params.count() != 2)
            return QJsonValue(false);
        QStringList slist;
        Q_FOREACH(auto v, params[1].toArray())
            slist.append(v.toString());
        return QJsonValue(d->funcRelay->eventTableSetHeader(params[0].toInt(-1), slist));
    }

    case MaPyFunction::T_addEvent: {
        if (params.count() != 2)
            return QJsonValue(false);
        QStringList slist;
        Q_FOREACH(auto v, params[1].toArray())
            slist.append(v.toString());
        return QJsonValue(d->funcRelay->eventTableAddEvent(params[0].toInt(-1), slist));
    }


    default:
        return QJsonValue(QJsonValue::Null);
    }
}

void ZmqServer::rpcThread(void *srvPtr)
{
    auto self = static_cast<ZmqServer*> (srvPtr);

    while (self->d->running) {
        auto reqStr = zstr_recv(self->d->server);
        if (reqStr == nullptr)
            continue;

        auto req = QJsonDocument::fromJson(QByteArray(reqStr, static_cast<int>(strlen(reqStr))));
        if (!req.isObject()) {
            qWarning() << "Received invalid request from worker:" << reqStr;
            zstr_free(&reqStr);
            zstr_send(self->d->server, "{\"failed\": true}");
            continue;
        }
        const auto funcId = static_cast<MaPyFunction>(req.object().value(QStringLiteral("callId")).toInt());
        const auto params = req.object().value(QStringLiteral("params")).toArray();
        zstr_free(&reqStr);

        const auto res = self->handleRpcRequest(funcId, params);
        QJsonObject resObj;
        resObj.insert(QStringLiteral("result"), res);

        QJsonDocument rep(resObj);
        zstr_send(self->d->server, rep.toJson(QJsonDocument::JsonFormat::Compact).toStdString().c_str());
    }
}

bool ZmqServer::startRpcThread()
{
    finishRpcThread();

    d->running = true;
    d->thread = new std::thread(rpcThread, this);
    return true;
}

void ZmqServer::finishRpcThread()
{
    if (d->thread != nullptr) {
        d->running = false;
        d->thread->join();
        delete d->thread;
        d->thread = nullptr;
    }
}

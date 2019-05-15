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

    std::thread *thread;
    std::atomic_bool running;
};
#pragma GCC diagnostic pop

ZmqServer::ZmqServer(QObject *parent)
    : QObject(parent),
      d(new ZSData)
{
    d->server = zsock_new(ZMQ_REP);

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

QJsonValue ZmqServer::handleRpcRequest(const QString &funcName, const QJsonArray &params)
{
    if (funcName == QStringLiteral("timeSinceStartMsec")) {
        if (d->timer == nullptr)
            return QJsonValue(static_cast<qint64>(0));
        return QJsonValue(static_cast<qint64>(d->timer->timeSinceStartMsec().count()));
    }

    return QJsonValue();
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
        const auto funcName = req.object().value(QStringLiteral("call")).toString();
        const auto params = req.object().value(QStringLiteral("params")).toArray();
        zstr_free(&reqStr);

        const auto res = self->handleRpcRequest(funcName, params);
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

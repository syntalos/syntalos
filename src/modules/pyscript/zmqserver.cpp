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
#include <cerrno>
#include <cstring>

#include "utils.h"
#include "hrclock.h"
#include "rpc-shared-info.h"

#pragma GCC diagnostic ignored "-Wpadded"
class MainThreadRpcRequest {
public:
    MainThreadRpcRequest()
        : requestReady(false),
          resultReady(false)
    {}

    void setResult(const QJsonValue& value)
    {
        result = value;
        requestReady = false;
        resultReady = true;
    }

    void setRequest(const MaPyFunction& func, const long long time, const QJsonArray& funcParams)
    {
        funcId = func;
        timestamp = time;
        params = funcParams;
        result = QJsonValue::Null;

        resultReady = false;
        requestReady = true;
    }

    QJsonValue requestAndWait(const MaPyFunction& func, const long long time, const QJsonArray& funcParams)
    {
        setRequest(func, time, funcParams);
        while (!resultReady) { }
        return result;
    }

    MaPyFunction funcId;
    QJsonArray params;
    long long timestamp;
    std::atomic_bool requestReady;

    QJsonValue result;
    std::atomic_bool resultReady;
};

class ZmqServer::ZSData
{
public:
    ZSData()
    {
        running = false;
        timer = nullptr;
        thread = nullptr;
        mtRequest = new MainThreadRpcRequest;
    }

    ~ZSData()
    {
        delete mtRequest;;
    }

    zsock_t *server;
    QString socketPath;
    HRTimer *timer;
    MaFuncRelay *funcRelay;

    std::thread *thread;
    std::atomic_bool running;

    MainThreadRpcRequest *mtRequest;
};
#pragma GCC diagnostic pop

ZmqServer::ZmqServer(MaFuncRelay *funcRelay, QObject *parent)
    : QObject(parent),
      d(new ZSData)
{
    d->server = zsock_new(ZMQ_REP);
    d->funcRelay = funcRelay;
    zsock_set_sndtimeo(d->server, 10000);  // set timeout of 10sec on message sending

    const auto socketDir = qEnvironmentVariable("XDG_RUNTIME_DIR", "/tmp/");
    d->socketPath = QStringLiteral("%1/mapy-%2.sock").arg(socketDir).arg(createRandomString(8));
}

ZmqServer::~ZmqServer()
{
    stop();
    zsock_destroy(&d->server);
    QFile socket(d->socketPath);
    socket.remove();
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

void ZmqServer::processMainThreadRpc()
{
    // unfortunately, we can't run all the code in a dedicated thread, as a few functions need
    // access to GUI elements.
    // those are called here instead.
    if (!d->mtRequest->requestReady)
        return;

    switch (d->mtRequest->funcId) {
    case MaPyFunction::T_newEventTable: {
        if (d->mtRequest->params.count() != 1)
            d->mtRequest->setResult(-1);
        else
            d->mtRequest->setResult(QJsonValue(d->funcRelay->newEventTable(d->mtRequest->params[0].toString())));
        break;
    }

    case MaPyFunction::T_setHeader: {
        if (d->mtRequest->params.count() != 2) {
            d->mtRequest->setResult(false);
        } else {
            QStringList slist;
            for (const auto v : d->mtRequest->params[1].toArray())
                slist.append(v.toString());
            d->mtRequest->setResult(QJsonValue(d->funcRelay->eventTableSetHeader(d->mtRequest->params[0].toInt(-1), slist)));
        }
        break;
    }

    case MaPyFunction::T_addEvent: {
        if (d->mtRequest->params.count() != 2) {
            d->mtRequest->setResult(false);
        } else {
            QStringList slist;
            for (const auto v : d->mtRequest->params[1].toArray())
                slist.append(v.toString());
            d->mtRequest->setResult(QJsonValue(d->funcRelay->eventTableAddEvent(d->mtRequest->timestamp, d->mtRequest->params[0].toInt(-1), slist)));
        }
        break;
    }

    default:
        d->mtRequest->result = QJsonValue(QJsonValue::Null);
        d->mtRequest->resultReady = true;
        d->mtRequest->requestReady = false;
    }
}

QJsonValue ZmqServer::handleRpcRequest(const MaPyFunction funcId, const QJsonArray &params)
{
    auto timestamp = static_cast<long long>(d->timer->timeSinceStartMsec().count());

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
        return QJsonValue(timestamp);
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

    case MaPyFunction::F_fetchDigitalInput: {
        if (params.count() != 1)
            return QJsonValue(false);
        auto fmod = d->funcRelay->firmataModule(params[0].toInt());
        if (fmod == nullptr)
            return QJsonValue(false);
        QJsonArray res;
        QPair<QString, bool> pair;
        if (fmod->fetchDigitalInput(&pair)) {
            res.append(true);
            res.append(pair.first);
            res.append(pair.second);
            return res;
        } else {
            res.append(false);
            res.append(QString());
            res.append(false);
            return res;
        }
    }

    case MaPyFunction::F_pinSetValue: {
        if (params.count() != 3)
            return QJsonValue(false);
        auto fmod = d->funcRelay->firmataModule(params[0].toInt());
        if (fmod == nullptr)
            return QJsonValue(false);

        fmod->pinSetValue(params[1].toString(), params[2].toBool());
        return QJsonValue(true);
    }

    case MaPyFunction::F_pinSignalPulse: {
        if (params.count() != 2)
            return QJsonValue(false);
        auto fmod = d->funcRelay->firmataModule(params[0].toInt());
        if (fmod == nullptr)
            return QJsonValue(false);

        fmod->pinSignalPulse(params[1].toString());
        return QJsonValue(true);
    }

    // these functiona may touch UI elements and must be run in the main thread
    case MaPyFunction::T_newEventTable:
    case MaPyFunction::T_setHeader:
    case MaPyFunction::T_addEvent: {
        return d->mtRequest->requestAndWait(funcId, timestamp, params);
    }

    default:
        return QJsonValue(QJsonValue::Null);
    }
}

void ZmqServer::rpcThread(void *srvPtr)
{
    auto self = static_cast<ZmqServer*> (srvPtr);

    auto poller = zpoller_new(self->d->server, NULL);

    while (self->d->running) {
        // poll for new data, wait for 4sec
        if (zpoller_wait(poller, 4 * 1000) == nullptr)
            continue; // we had nothing to receive

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

    zpoller_destroy(&poller);
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

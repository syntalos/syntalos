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

#include "zmqclient.h"

#include <czmq.h>
#include <QDebug>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>

#pragma GCC diagnostic ignored "-Wpadded"
class ZmqClient::ZCData : public QSharedData
{
public:
    ZCData() { }
    ~ZCData() { }

    zsock_t *client;
    QString socketPath;
};
#pragma GCC diagnostic pop

ZmqClient::ZmqClient(QObject *parent)
    : QObject(parent),
      d(new ZCData)
{
    d->client = zsock_new (ZMQ_REQ);
    zsock_set_rcvtimeo(d->client, 20000);  // set timeout of 20sec on message receiving
}

ZmqClient::~ZmqClient()
{
    zsock_destroy(&d->client);
}

bool ZmqClient::connect(const QString &ipcSocketPath)
{
    d->socketPath = ipcSocketPath;
    const auto ret = zsock_connect (d->client, "ipc://%s", d->socketPath.toUtf8().constData());
    return ret == 0;
}

QVariant ZmqClient::runRpc(const QString &funcName, const QJsonArray &values)
{
    QJsonObject req;
    req.insert(QStringLiteral("call"), funcName);
    req.insert(QStringLiteral("params"), values);

    QJsonDocument qdoc(req);
    zstr_send(d->client, qdoc.toJson(QJsonDocument::JsonFormat::Compact).toStdString().c_str());

    auto reply = zstr_recv(d->client);
    if (reply == nullptr) {
        qCritical() << "Did not receive a reply from MazeAmaze in time, shutting down.";
        QCoreApplication::exit(4);
        zstr_free(&reply);
        return QVariant();
    }

    auto adoc = QJsonDocument::fromJson(QByteArray(reply, static_cast<int>(strlen(reply))));
    zstr_free(&reply);
    if (!adoc.isObject()) {
        qCritical() << "Received invalid reply from MazeAmaze, can not continue.";
        QCoreApplication::exit(4);
        return QVariant();
    }
    const auto aobj = adoc.object();
    if (aobj.value("failed").toBool(false)) {
        qCritical() << "The request we sent failed. Can not continue.";
        QCoreApplication::exit(4);
        return QVariant();
    }

    return aobj.value("result").toVariant();
}

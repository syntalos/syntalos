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

#ifndef ZMQCLIENT_H
#define ZMQCLIENT_H

#include <QObject>
#include <QSharedDataPointer>
#include <QVariant>
#include <QJsonArray>
#include <mutex>

#include "../rpc-shared-info.h"

class ZmqClient : public QObject
{
    Q_OBJECT
public:
    static ZmqClient *instance(QObject *parent = nullptr) {
        static std::mutex _mutex;
        std::lock_guard<std::mutex> lock(_mutex);
        static ZmqClient *_instance = nullptr;
        if (_instance == nullptr) {
            _instance = new ZmqClient(parent);
        }
        return _instance;
    }

    explicit ZmqClient(QObject *parent = nullptr);
    ~ZmqClient();

    bool connect(const QString& ipcSocketPath);

    QVariant runRpc(MaPyFunction funcId, const QJsonArray& values = QJsonArray());

private:
    Q_DISABLE_COPY(ZmqClient)

    class ZCData;
    QSharedDataPointer<ZCData> d;
};

#endif // ZMQCLIENT_H

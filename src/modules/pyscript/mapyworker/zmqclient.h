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

class ZmqClient : public QObject
{
    Q_OBJECT
public:
    explicit ZmqClient(QObject *parent = nullptr);
    ~ZmqClient();

    bool connect(const QString& ipcSocketPath);

    QVariant runRpc(const QString& funcName, const QJsonArray& values = QJsonArray());

signals:

public slots:

private:
    class ZCData;
    QSharedDataPointer<ZCData> d;
};

#endif // ZMQCLIENT_H

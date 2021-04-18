//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.0.3
//
//  Copyright (c) 2020-2021 Intan Technologies
//
//  This file is part of the Intan Technologies RHX Data Acquisition Software.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <http://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

#ifndef TCPCOMMUNICATOR_H
#define TCPCOMMUNICATOR_H

#include <QObject>
#include <QtNetwork>

class TCPCommunicator : public QObject
{
    Q_OBJECT
public:

    enum ConnectionStatus {
        Disconnected,
        Pending,
        Connected
    };

    explicit TCPCommunicator(QString address_ = "127.0.0.1", int port_ = 5000, QObject *parent = nullptr);
    bool connectionAvailable();
    bool serverListening();
    bool listen(QString host, int port);
    QString read();
    void writeQString(QString message);
    void writeData(char* data, qint64 len);
    qint64 bytesUnwritten();

    bool passwordCleared;
    ConnectionStatus status;
    QString address;
    int port;

signals:
    void newConnection();
    void readyRead();
    void statusChanged();

public slots:
    void attemptNewConnection();
    void returnToDisconnected();
    void establishConnection();
    void TCPDataOutput(QByteArray* array, qint64 len);

private slots:
    void emitNewConnection();
    void emitReadyRead();

private:
    QTcpServer *server;
    QTcpSocket *socket;
};

#endif // TCPCOMMUNICATOR_H

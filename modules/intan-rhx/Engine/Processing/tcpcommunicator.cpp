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

#include "tcpcommunicator.h"

TCPCommunicator::TCPCommunicator(QString address_, int port_, QObject *parent) :
    QObject(parent),
    passwordCleared(false),
    status(Disconnected),
    address(address_),
    port(port_),
    server(nullptr),
    socket(nullptr)
{
    server = new QTcpServer(this);
    connect(server, SIGNAL(newConnection()), this, SLOT(emitNewConnection()));
}

bool TCPCommunicator::connectionAvailable()
{
    return server->hasPendingConnections();
}

void TCPCommunicator::establishConnection()
{
    if (connectionAvailable()) {
        socket = server->nextPendingConnection();
        connect(socket, SIGNAL(readyRead()), this, SLOT(emitReadyRead()));
        connect(socket, SIGNAL(disconnected()), this, SLOT(returnToDisconnected()));
        server->close();
        status = Connected;
        emit statusChanged();
    }
}

bool TCPCommunicator::serverListening()
{
    return server->isListening();
}

bool TCPCommunicator::listen(QString host, int port)
{
    status = Pending;
    emit statusChanged();
    return server->listen(QHostAddress(host), port);
}

QString TCPCommunicator::read()
{
    return socket->readAll();
}

void TCPCommunicator::writeQString(QString message)
{
    socket->write(message.toLatin1());
    socket->waitForBytesWritten();
}

void TCPCommunicator::writeData(char *data, qint64 len)
{
    socket->write(data, len);
    socket->waitForBytesWritten();
}

void TCPCommunicator::attemptNewConnection()
{
    if (!serverListening()) {
        listen(address, port);
    }
}

void TCPCommunicator::returnToDisconnected()
{
    if (socket) {
        socket->disconnectFromHost();
        socket = nullptr;
    }
    server->close();
    status = Disconnected;
    emit statusChanged();
}

qint64 TCPCommunicator::bytesUnwritten()
{
    return socket->bytesToWrite();
}

void TCPCommunicator::emitNewConnection()
{
    emit newConnection();
}

void TCPCommunicator::emitReadyRead()
{
    emit readyRead();
}

void TCPCommunicator::TCPDataOutput(QByteArray *array, qint64 len)
{
    socket->write(array->data(), len);
}

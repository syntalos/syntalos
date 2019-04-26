//  ------------------------------------------------------------------------
//
//  This file is part of the Intan Technologies RHD2000 Interface
//  Version 1.3
//  Copyright (C) 2013 Intan Technologies
//
//  ------------------------------------------------------------------------
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <QVector>
#include <QString>
#include <QDataStream>

#include "qtincludes.h"

#include "signalgroup.h"
#include "signalchannel.h"

// Data structure containing description of a particular signal channel
// (e.g., an amplifier channel on a particular RHD2000 chip, a digital
// input from the USB interface board, etc.).

// Default constructor.
SignalChannel::SignalChannel()
{
    signalGroup = 0;

    enabled = true;
    alphaOrder = -1;
    userOrder = -1;

    voltageTriggerMode = true;
    voltageThreshold = 0;
    digitalTriggerChannel = 0;
    digitalEdgePolarity = true;

    electrodeImpedanceMagnitude = 0.0;
    electrodeImpedancePhase = 0.0;
}

// Contructor linking new signal channel to a particular signal group.
SignalChannel::SignalChannel(SignalGroup *initSignalGroup)
{
    signalGroup = initSignalGroup;
    alphaOrder = -1;
}

// Detailed constructor with all signal channel information supplied.
SignalChannel::SignalChannel(const QString &initCustomChannelName, const QString &initNativeChannelName,
                             int initNativeChannelNumber, SignalType initSignalType,
                             int initBoardChannel, int initBoardStream, SignalGroup *initSignalGroup)
{   
    signalGroup = initSignalGroup;

    customChannelName = initCustomChannelName;
    nativeChannelName = initNativeChannelName;
    nativeChannelNumber = initNativeChannelNumber;
    signalType = initSignalType;
    boardStream = initBoardStream;
    chipChannel = initBoardChannel;

    enabled = true;
    alphaOrder = -1;
    userOrder = initNativeChannelNumber;

    voltageTriggerMode = true;
    voltageThreshold = 0;
    digitalTriggerChannel = 0;
    digitalEdgePolarity = true;

    electrodeImpedanceMagnitude = 0.0;
    electrodeImpedancePhase = 0.0;
}

// Streams this signal channel out to binary data stream.
QDataStream& operator<<(QDataStream &outStream, const SignalChannel &a)
{
    outStream << a.nativeChannelName;
    outStream << a.customChannelName;
    outStream << (qint16) a.nativeChannelNumber;
    outStream << (qint16) a.userOrder;
    outStream << (qint16) a.signalType;
    outStream << (qint16) a.enabled;
    outStream << (qint16) a.chipChannel;
    outStream << (qint16) a.boardStream;
    outStream << (qint16) a.voltageTriggerMode;
    outStream << (qint16) a.voltageThreshold;
    outStream << (qint16) a.digitalTriggerChannel;
    outStream << (qint16) a.digitalEdgePolarity;
    outStream << a.electrodeImpedanceMagnitude;
    outStream << a.electrodeImpedancePhase;
    return outStream;
}

// Streams this signal channel in from binary data stream.
QDataStream& operator>>(QDataStream &inStream, SignalChannel &a)
{
    qint16 tempQint16;

    inStream >> a.nativeChannelName;
    inStream >> a.customChannelName;
    inStream >> tempQint16;
    a.nativeChannelNumber = (int) tempQint16;
    inStream >> tempQint16;
    a.userOrder = (int) tempQint16;
    inStream >> tempQint16;
    a.signalType = (SignalType) tempQint16;
    inStream >> tempQint16;
    a.enabled  = (bool) tempQint16;
    inStream >> tempQint16;
    a.chipChannel  = (int) tempQint16;
    inStream >> tempQint16;
    a.boardStream  = (int) tempQint16;
    inStream >> tempQint16;
    a.voltageTriggerMode = (bool) tempQint16;
    inStream >> tempQint16;
    a.voltageThreshold = (int) tempQint16;
    inStream >> tempQint16;
    a.digitalTriggerChannel = (int) tempQint16;
    inStream >> tempQint16;
    a.digitalEdgePolarity = (bool) tempQint16;
    inStream >> a.electrodeImpedanceMagnitude;
    inStream >> a.electrodeImpedancePhase;
    return inStream;
}

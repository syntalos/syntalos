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

#ifndef SIGNALCHANNEL_H
#define SIGNALCHANNEL_H

#include "qtincludes.h"

class SignalGroup;
class QDataStream;
class QFile;
class QString;

using namespace std;

enum SignalType {
    AmplifierSignal,
    AuxInputSignal,
    SupplyVoltageSignal,
    BoardAdcSignal,
    BoardDigInSignal,
    BoardDigOutSignal
};

class SignalChannel
{
    friend QDataStream &operator<<(QDataStream &outStream, const SignalChannel &a);
    friend QDataStream &operator>>(QDataStream &inStream, SignalChannel &a);

public:
    SignalChannel();
    SignalChannel(SignalGroup *initSignalGroup);
    SignalChannel(const QString &initCustomChannelName, const QString &initNativeChannelName,
                  int initNativeChannelNumber, SignalType initSignalType, int initBoardChannel,
                  int initBoardStream, SignalGroup *initSignalGroup);

    SignalGroup *signalGroup;

    QString nativeChannelName;
    QString customChannelName;
    int nativeChannelNumber;
    int alphaOrder;
    int userOrder;

    SignalType signalType;
    bool enabled;

    int chipChannel;
    int boardStream;

    bool voltageTriggerMode;
    int voltageThreshold;
    int digitalTriggerChannel;
    bool digitalEdgePolarity;

    double electrodeImpedanceMagnitude;
    double electrodeImpedancePhase;

    QString saveFileName;
    QFile *saveFile;
    QDataStream *saveStream;

private:

};

#endif // SIGNALCHANNEL_H

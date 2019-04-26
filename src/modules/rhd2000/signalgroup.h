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

#ifndef SIGNALGROUP_H
#define SIGNALGROUP_H

#include "signalchannel.h"
#include "qtincludes.h"

class SignalGroup
{
    friend QDataStream &operator<<(QDataStream &outStream, const SignalGroup &a);
    friend QDataStream &operator>>(QDataStream &inStream, SignalGroup &a);

public:
    SignalGroup();
    SignalGroup(const QString &initialName, const QString &initialPrefix);
    QVector<SignalChannel> channel;
    QString name;
    QString prefix;
    bool enabled;

    void addAmplifierChannel();
    void addAmplifierChannel(int nativeChannelNumber, int chipChannel, int boardStream);
    void addAuxInputChannel(int nativeChannelNumber, int chipChannel, int nameNumber, int boardStream);
    void addSupplyVoltageChannel(int nativeChannelNumber, int chipChannel, int nameNumber, int boardStream);
    void addBoardAdcChannel(int nativeChannelNumber);
    void addBoardDigInChannel(int nativeChannelNumber);
    void addBoardDigOutChannel(int nativeChannelNumber);

    void addChannel(SignalChannel *newChannel);

    SignalChannel* channelByNativeOrder(int index);
    SignalChannel* channelByAlphaOrder(int index);
    SignalChannel* channelByIndex(int index);
    int numChannels() const;
    int numAmplifierChannels() const;
    void updateAlphabeticalOrder();
    void setOriginalChannelOrder();
    void setAlphabeticalChannelOrder();
    void print() const;

private:
};

#endif // SIGNALGROUP_H

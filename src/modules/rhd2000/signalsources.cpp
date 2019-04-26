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
#include <iostream>

#include "qtincludes.h"

#include "signalchannel.h"
#include "signalgroup.h"
#include "signalsources.h"

using namespace std;

// Data structure containing descriptions of all signal sources acquired
// from the USB interface board.

SignalSources::SignalSources()
{
    // We have six signal ports: four SPI ports (A-D), the interface board
    // ADC inputs, and the interface board digital inputs.
    signalPort.resize(7);

    signalPort[0].name = "Port A";
    signalPort[1].name = "Port B";
    signalPort[2].name = "Port C";
    signalPort[3].name = "Port D";
    signalPort[4].name = "Board ADC Inputs";
    signalPort[5].name = "Board Digital Inputs";
    signalPort[6].name = "Board Digital Outputs";

    signalPort[0].prefix = "A";
    signalPort[1].prefix = "B";
    signalPort[2].prefix = "C";
    signalPort[3].prefix = "D";
    signalPort[4].prefix = "ADC";
    signalPort[5].prefix = "DIN";
    signalPort[6].prefix = "DOUT";

    signalPort[0].enabled = false;
    signalPort[1].enabled = false;
    signalPort[2].enabled = false;
    signalPort[3].enabled = false;
    signalPort[4].enabled = true;   // Board analog inputs are always enabled
    signalPort[5].enabled = true;   // Board digital inputs are always enabled
    signalPort[6].enabled = true;

    // Add 8 board analog input signals
    for (int channel = 0; channel < 8; ++channel) {
        signalPort[4].addBoardAdcChannel(channel);
        signalPort[4].channel[channel].enabled = false;
    }
    // Add 16 board digital input signals
    for (int channel = 0; channel < 16; ++channel) {
        signalPort[5].addBoardDigInChannel(channel);
        signalPort[5].channel[channel].enabled = false;
    }
    // Add 16 board digital output signals
    for (int channel = 0; channel < 16; ++channel) {
        signalPort[6].addBoardDigOutChannel(channel);
        signalPort[6].channel[channel].enabled = true;
    }
    // Amplifier channels on SPI ports A-D are added later, if amplifier
    // boards are found to be connected to these ports.
}

// Return a pointer to a SignalChannel with a particular nativeName (e.g., "A-02").
SignalChannel* SignalSources::findChannelFromName(const QString &nativeName)
{
    SignalChannel *channel;

    for (int i = 0; i < signalPort.size(); ++i) {
        for (int j = 0; j < signalPort[i].numChannels(); ++j) {
            if (signalPort[i].channel[j].nativeChannelName == nativeName) {
                channel = &signalPort[i].channel[j];
                return channel;
            }
        }
    }

    return 0;
}

// Return a pointer to a SignalChannel corresponding to a particular USB interface
// data stream and chip channel number.
SignalChannel* SignalSources::findAmplifierChannel(int boardStream, int chipChannel)
{
    SignalChannel *channel;

    for (int i = 0; i < signalPort.size(); ++i) {
        for (int j = 0; j < signalPort[i].numChannels(); ++j) {
            if (signalPort[i].channel[j].signalType == AmplifierSignal &&
                    signalPort[i].channel[j].boardStream == boardStream &&
                    signalPort[i].channel[j].chipChannel == chipChannel) {
                channel = &signalPort[i].channel[j];
                return channel;
            }
        }
    }

    return 0;
}

// Stream all signal sources out to binary data stream.
QDataStream& operator<<(QDataStream &outStream, const SignalSources &a)
{
    outStream << (qint16) a.signalPort.size();
    for (int i = 0; i < a.signalPort.size(); ++i) {
        outStream << a.signalPort[i];
    }
    return outStream;
}

// Stream all signal sources in from binary data stream.
QDataStream& operator>>(QDataStream &inStream, SignalSources &a)
{
    qint16 tempQint16;
    int nGroups;

    inStream >> tempQint16;
    nGroups = (int) tempQint16;
    a.signalPort.resize(nGroups);

    for (int i = 0; i < nGroups; ++i) {
        inStream >> a.signalPort[i];
    }

    return inStream;
}

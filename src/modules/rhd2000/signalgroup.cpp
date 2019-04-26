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

using namespace std;

// Data structure containing a description of all signal channels on a particular
// signal port (e.g., SPI Port A, or Interface Board Digital Inputs).

// Must have a default constructor to create vectors of this object.
SignalGroup::SignalGroup()
{
}

// Constructor.
SignalGroup::SignalGroup(const QString &initialName, const QString &initialPrefix)
{
    name = initialName;
    prefix = initialPrefix;
    enabled = true;
}

// Add new amplifier channel to this signal group.
void SignalGroup::addAmplifierChannel()
{
    SignalChannel *newChannel = new SignalChannel(this);
    channel.append(*newChannel);
}

// Add new amplifier channel (with specified properties) to this signal group.
void SignalGroup::addAmplifierChannel(int nativeChannelNumber, int chipChannel,
                                      int boardStream)
{
    QString nativeChannelName;
    QString customChannelName;

    nativeChannelName = prefix + "-" +
            QString("%1").arg(nativeChannelNumber, 3, 10, QChar('0'));
    customChannelName = nativeChannelName;

    SignalChannel *newChannel = new SignalChannel(customChannelName, nativeChannelName,
                                                  nativeChannelNumber, AmplifierSignal,
                                                  chipChannel, boardStream, this);
    channel.append(*newChannel);
    updateAlphabeticalOrder();
}

// Add new auxiliary input channel to this signal group.
void SignalGroup::addAuxInputChannel(int nativeChannelNumber, int chipChannel, int nameNumber, int boardStream)
{
    QString nativeChannelName;
    QString customChannelName;

    nativeChannelName = prefix + "-" + "AUX" + QString::number(nameNumber);
    customChannelName = nativeChannelName;

    SignalChannel *newChannel = new SignalChannel(customChannelName, nativeChannelName,
                                                  nativeChannelNumber, AuxInputSignal,
                                                  chipChannel, boardStream, this);
    channel.append(*newChannel);
    updateAlphabeticalOrder();
}

// Add new supply voltage channel to this signal group.
void SignalGroup::addSupplyVoltageChannel(int nativeChannelNumber, int chipChannel, int nameNumber, int boardStream)
{
    QString nativeChannelName;
    QString customChannelName;

    nativeChannelName = prefix + "-" + "VDD" + QString::number(nameNumber);
    customChannelName = nativeChannelName;

    SignalChannel *newChannel = new SignalChannel(customChannelName, nativeChannelName,
                                                  nativeChannelNumber, SupplyVoltageSignal,
                                                  chipChannel, boardStream, this);
    channel.append(*newChannel);
    updateAlphabeticalOrder();
}

// Add new USB interface board ADC channel to this signal group.
void SignalGroup::addBoardAdcChannel(int nativeChannelNumber)
{
    QString nativeChannelName;
    QString customChannelName;

    nativeChannelName = prefix + "-" + QString("%1").arg(nativeChannelNumber, 2, 10, QChar('0'));
    customChannelName = nativeChannelName;

    SignalChannel *newChannel = new SignalChannel(customChannelName, nativeChannelName,
                                                  nativeChannelNumber, BoardAdcSignal,
                                                  nativeChannelNumber, 0, this);
    channel.append(*newChannel);
    updateAlphabeticalOrder();
}

// Add new USB interface board digital input channel to this signal group.
void SignalGroup::addBoardDigInChannel(int nativeChannelNumber)
{
    QString nativeChannelName;
    QString customChannelName;

    nativeChannelName = prefix + "-" + QString("%1").arg(nativeChannelNumber, 2, 10, QChar('0'));
    customChannelName = nativeChannelName;

    SignalChannel *newChannel = new SignalChannel(customChannelName, nativeChannelName,
                                                  nativeChannelNumber, BoardDigInSignal,
                                                  nativeChannelNumber, 0, this);
    channel.append(*newChannel);
    updateAlphabeticalOrder();
}

// Add new USB interface board digital output channel to this signal group.
void SignalGroup::addBoardDigOutChannel(int nativeChannelNumber)
{
    QString nativeChannelName;
    QString customChannelName;

    nativeChannelName = prefix + "-" + QString("%1").arg(nativeChannelNumber, 2, 10, QChar('0'));
    customChannelName = nativeChannelName;

    SignalChannel *newChannel = new SignalChannel(customChannelName, nativeChannelName,
                                                  nativeChannelNumber, BoardDigOutSignal,
                                                  nativeChannelNumber, 0, this);
    channel.append(*newChannel);
    updateAlphabeticalOrder();
}

// Add a previously-created signal channel to this signal group.
void SignalGroup::addChannel(SignalChannel *newChannel)
{
    channel.append(*newChannel);
    updateAlphabeticalOrder();
}

// Returns a pointer to a signal channel with a particular native order index.
SignalChannel* SignalGroup::channelByNativeOrder(int index)
{
    for (int i = 0; i < channel.size(); ++i) {
        if (channel[i].nativeChannelNumber == index) {
            return &channel[i];
        }
    }
    return 0;
}

// Returns a pointer to a signal channel with a particular alphabetical order index.
SignalChannel* SignalGroup::channelByAlphaOrder(int index)
{
    for (int i = 0; i < channel.size(); ++i) {
        if (channel[i].alphaOrder == index) {
            return &channel[i];
        }
    }
    return 0;
}

// Returns a pointer to a signal channel with a particular user-selected order index
SignalChannel* SignalGroup::channelByIndex(int index)
{
    for (int i = 0; i < channel.size(); ++i) {
        if (channel[i].userOrder == index) {
            return &channel[i];
        }
    }
    cerr << "SignalGroup::channelByUserOrder: index " << index << " not found." << endl;
    return 0;
}

// Returns the total number of channels in this signal group.
int SignalGroup::numChannels() const
{
    return channel.size();
}

// Returns the total number of AMPLIFIER channels in this signal group.
int SignalGroup::numAmplifierChannels() const
{
    int count = 0;
    for (int i = 0; i < channel.size(); ++i) {
        if (channel[i].signalType == AmplifierSignal) {
            ++count;
        }
    }
    return count;
}

// Updates the alphabetical order indices of all signal channels in this signal group.
void SignalGroup::updateAlphabeticalOrder()
{
    QString currentFirstName;
    int currentFirstIndex;
    int i, j;
    int size = channel.size();

    // Mark all alphaetical order indices with -1 to indicate they have not yet
    // been assigned an order.
    for (i = 0; i < size; ++i) {
        channel[i].alphaOrder = -1;
    }

    for (i = 0; i < size; ++i) {
        // Find the first remaining non-alphabetized item.
        j = 0;
        while (channel[j].alphaOrder != -1) {
            ++j;
        }
        currentFirstName = channel[j].customChannelName;
        currentFirstIndex = j;

        // Now compare all other remaining items.
        while (++j < size) {
            if (channel[j].alphaOrder == -1) {
                if (channel[j].customChannelName.toLower() < currentFirstName.toLower()) {
                    currentFirstName = channel[j].customChannelName;
                    currentFirstIndex = j;
                }
            }
        }

        // Now assign correct alphabetical order to this item.
        channel[currentFirstIndex].alphaOrder = i;
    }
}

// Restores channels to their original order.
void SignalGroup::setOriginalChannelOrder()
{
    for (int i = 0; i < channel.size(); ++i) {
        channel[i].userOrder = channel[i].nativeChannelNumber;
    }
}

// Orders signal channels alphabetically.
void SignalGroup::setAlphabeticalChannelOrder()
{
    updateAlphabeticalOrder();
    for (int i = 0; i < channel.size(); ++i) {
        channel[i].userOrder = channel[i].alphaOrder;
    }
}

// Diagnostic routine to display all channels in this group (to cout).
void SignalGroup::print() const
{
    cout << "SignalGroup " << name.toStdString() << " (" << prefix.toStdString() <<
            ") enabled:" << enabled << endl;
    for (int i = 0; i < channel.size(); ++i) {
        cout << "  SignalChannel " << channel[i].nativeChannelNumber << " " << channel[i].customChannelName.toStdString() << " (" <<
                channel[i].nativeChannelName.toStdString() << ") stream:" << channel[i].boardStream <<
                " channel:" << channel[i].chipChannel << endl;
    }
    cout << endl;
}

// Streams all signal channels in this group out to binary data stream.
QDataStream& operator<<(QDataStream &outStream, const SignalGroup &a)
{
    outStream << a.name;
    outStream << a.prefix;
    outStream << (qint16) a.enabled;
    outStream << (qint16) a.numChannels();
    outStream << (qint16) a.numAmplifierChannels();
    for (int i = 0; i < a.numChannels(); ++i) {
        outStream << a.channel[i];
    }
    return outStream;
}

// Streams all signal channels in this group in from binary data stream.
QDataStream& operator>>(QDataStream &inStream, SignalGroup &a)
{
    qint16 tempQint16;
    int nTotal, nAmps;

    inStream >> a.name;
    inStream >> a.prefix;
    inStream >> tempQint16;
    a.enabled = (bool) tempQint16;
    inStream >> tempQint16;
    nTotal = (int) tempQint16;
    inStream >> tempQint16;
    nAmps = (int) tempQint16;

    // Delete all existing SignalChannel objects in this SignalGroup
    a.channel.clear();

    for (int i = 0; i < nTotal; ++i) {
        a.addAmplifierChannel();
        inStream >> a.channel[i];
    }
    a.updateAlphabeticalOrder();

    return inStream;
}

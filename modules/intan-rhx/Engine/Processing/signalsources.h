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
#ifndef SIGNALSOURCES_H
#define SIGNALSOURCES_H

#include <string>
#include <vector>
#include <map>
#include <QColor>
#include <QString>
#include "channel.h"
#include "displayundomanager.h"
#include "stimparameters.h"
#include "rhxglobals.h"
#include "systemstate.h"

using namespace std;

class MultiColumnDisplay;

// Data structure containing a description of all signal channels on a particular signal port (e.g., SPI Port A, or
// Digital Inputs).
class SignalGroup
{
public:
    SignalGroup(const QString &name_, const QString &prefix_, SystemState* state_, bool enabled_ = true);
    ~SignalGroup();

    void addAmplifierChannel(int nativeChannelNumber, int boardStream, int commandStream, int chipChannel);
    void addAuxInputChannel(int nativeChannelNumber, int boardStream, int auxNumber, int nameNumber);
    void addSupplyVoltageChannel(int nativeChannelNumber, int boardStream, int nameNumber);
    void addAnalogDigitalIOChannel(SignalType signalType, int nativeChannelNumber);

    void removeAllChannels();

    QString getName() const { return name; }
    QString getPrefix() const { return prefix->getValueString(); }
    bool isEnabled() const { return enabled; }
    void setEnabled(bool enabled_) { enabled = enabled_; }
    ControllerType getControllerType() const { return state->getControllerTypeEnum(); }

    Channel* channelByCustomName(const QString& customName) const;
    Channel* channelByUserOrder(SignalType signalType, int index) const;
    Channel* channelByIndex(int index) const { return signalChannels[index]; }
    int numChannels() const { return (int) signalChannels.size(); }
    int numChannels(SignalType type) const;
    int minUserOrder(SignalType type) const;
    void setOriginalChannelOrder();
    void setAlphabeticalChannelOrder();

    QStringList getAttributes(XMLGroup xmlGroup) const;

    SingleItemList portItems;
    BooleanItem *manualDelayEnabled;
    IntRangeItem *manualDelay;
    BooleanItem *auxDigOutEnabled;
    IntRangeItem *auxDigOutChannel;
    StringItem *prefix;

    IntRangeItem *numAmpChannels;
    IntRangeItem *numAuxChannels;
    IntRangeItem *numVddChannels;

private:
    SystemState* state;
    QString name;
    bool enabled;
    vector<Channel*> signalChannels;
};

// Data structure containing descriptions of all signal sources acquired from the controller.
class SignalSources
{
public:
    SignalSources(SystemState* state_);
    ~SignalSources();

    DisplayUndoManager* undoManager;
    void setDisplayForUndo(MultiColumnDisplay* display_) { display = display_; }
    DisplayState saveState() const;
    void restoreState(const DisplayState& savedState);

    int numPortGroups() const { return (int) portGroups.size(); }
    int numBaseGroups() const { return (int) baseGroups.size(); }
    int numGroups() const { return (int) (portGroups.size() + baseGroups.size()); }
    SignalGroup* portGroupByIndex(int index) const { return portGroups[index]; }
    SignalGroup* baseGroupByIndex(int index) const { return baseGroups[index]; }
    SignalGroup* groupByIndex(int index) const;
    SignalGroup* groupByName(const QString& groupName) const;
    Channel* channelByName(const QString& nativeName) const;
    Channel* channelByName(const string& nativeName) const;
    Channel* getAmplifierChannel(int boardStream, int chipChannel) const;
    QString getNativeAndCustomNames(const QString& nativeName) const;
    QString getNativeAndCustomNames(const string& nativeName) const;

    void setSelected(const QString& nativeName, bool selected);
    void setSelectedEntireGroupID(const QString& nativeName, bool selected);

    int numChannelsSelected() const;
    bool onlyAmplifierChannelsSelected() const;
    int enableStateOfSelectedChannels() const;

    int getNextAvailableGroupID() const;
    bool isCustomNamePresent(const QString& customName) const;
    int numChannels(SignalType type) const;
    int numAmplifierChannels() const { return numChannels(AmplifierSignal); }
    ControllerType getControllerType() const { return state->getControllerTypeEnum(); }

    vector<string> amplifierChannelsNameList() const;
    vector<string> completeChannelsNameList() const;
    QStringList amplifierNameListUserOrder(const QString& port) const;
    QStringList headstageSignalNameList(const QString& port) const;
    QStringList controllerIOSignalNameList() const;
    QStringList populatedGroupList() const;
    QStringList populatedGroupListWithChannelCounts() const;

    SignalList getSaveSignalList() const;
    QString singleSelectedAmplifierChannelName() const;
    QString firstChannelName() const;
    Channel* selectedChannel() const;
    void getSelectedSignals(QList<Channel*>& selectedSignals) const;

    QColor channelColor(int colorIndex, int numColors) const;

    void setOriginalChannelOrder();
    void setAlphabeticalChannelOrder();
    void autoColorAmplifierChannels(int numColors, int numChannelsSameColor = 1);
    void autoGroupAmplifierChannels(int groupSize);
    bool anyChannelsGrouped() const;
    void ungroupAllChannels();
    void setSelectedChannelReferences(const QString& refString);

    QStringList getDisplayListAmplifiers(const QString& groupName) const;
    QStringList getDisplayListAmplifiersNoFilters(const QString& groupName) const;
    QStringList getDisplayListAuxInputs(const QString& groupName) const;
    QStringList getDisplayListSupplyVoltages(const QString& groupName) const;
    QStringList getDisplayListBaseGroup(const QString& groupName) const;

    void updateChannelMap();    // call after all amplifier signals are added (e.g., ports rescanned)

    void clearTCPDataOutput();

    QStringList getTcpFilterBands();

private:
    SystemState* state;

    vector<SignalGroup*> portGroups;   // signals from SPI ports (amplifiers, aux inputs, supply voltages)
    vector<SignalGroup*> baseGroups;   // signals from main controller unit (digital and analog I/O)

    map<string, Channel*> channelMap;   // map from native name to channel pointer, for quick access

    MultiColumnDisplay* display;  // needed for undo/redo operations with pinned waveforms and scroll bar state

    bool groupIDisPresent(int groupID) const;
    void saveChannelState(const Channel* channel, ChannelState* saveChannel) const;
    bool restoreChannelState(const ChannelState* saveChannel, Channel* channel);

    bool moveLastDisabledAmplifierChannelToEnd(const SignalGroup* group);
};


#endif // SIGNALSOURCES_H

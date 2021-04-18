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

#include <iostream>
#include <limits>
#include "displaylistmanager.h"
#include "abstractrhxcontroller.h"
#include "multicolumndisplay.h"
#include "signalsources.h"

SignalGroup::SignalGroup(const QString &name_, const QString &prefix_, SystemState* state_, bool enabled_) :
    manualDelayEnabled(nullptr),
    manualDelay(nullptr),
    prefix(nullptr),
    state(state_),
    name(name_),
    enabled(enabled_)
{
    prefix = new StringItem("Prefix", portItems, state, prefix_, XMLGroupReadOnly);

    if (prefix_ == "A" || prefix_ == "B" || prefix_ == "C" || prefix_ == "D" ||
            prefix_ == "E" || prefix_ == "F" || prefix_ == "G" || prefix_ == "H") {
        manualDelayEnabled = new BooleanItem("ManualDelayEnabled", portItems, state, false, XMLGroupGeneral);
        manualDelayEnabled->setRestricted(RestrictIfRunning, RunningErrorMessage);
        manualDelay = new IntRangeItem("ManualDelay", portItems, state, 0, 15, 1, XMLGroupGeneral);
        manualDelay->setRestricted(RestrictIfRunning, RunningErrorMessage);
        auxDigOutEnabled = new BooleanItem("AuxDigOutEnabled", portItems, state, false, XMLGroupGeneral, TypeDependencyNonStim);
        auxDigOutEnabled->setRestricted(RestrictIfRunning, RunningErrorMessage);
        auxDigOutChannel = new IntRangeItem("AuxDigOutChannel", portItems, state, 0, 15, 0, XMLGroupGeneral, TypeDependencyNonStim);
        auxDigOutChannel->setRestricted(RestrictIfRunning, RunningErrorMessage);
        numAmpChannels = new IntRangeItem("NumberAmplifierChannels", portItems, state, 0, 128, 0, XMLGroupNone, TypeDependencyNone);
        numAuxChannels = new IntRangeItem("NumberAuxiliaryChannels", portItems, state, 0, 6, 0, XMLGroupNone, TypeDependencyNonStim);
        numVddChannels = new IntRangeItem("NumberSupplyVoltageChannels", portItems, state, 0, 2, 0, XMLGroupNone, TypeDependencyNonStim);
    }
}

SignalGroup::~SignalGroup()
{
    removeAllChannels();

    for (SingleItemList::const_iterator p = portItems.begin(); p != portItems.end(); ++p) {
        delete p->second;
    }
}

void SignalGroup::addAmplifierChannel(int nativeChannelNumber, int boardStream, int commandStream, int chipChannel)
{
    QString nativeChannelName = prefix->getValueString() + "-" + QString("%1").arg(nativeChannelNumber, 3, 10, QChar('0'));
    QString customChannelName = nativeChannelName;
    Channel* newChannel = new Channel(AmplifierSignal, customChannelName, nativeChannelName, nativeChannelNumber, state,
                                      this, boardStream, chipChannel, commandStream);
    signalChannels.push_back(newChannel);
    numAmpChannels->setValue(numAmpChannels->getValue() + 1);
}

void SignalGroup::addAuxInputChannel(int nativeChannelNumber, int boardStream, int auxNumber, int nameNumber)
{
    QString nativeChannelName = prefix->getValueString() + "-" + "AUX" + QString::number(nameNumber);
    QString customChannelName = nativeChannelName;
    Channel* newChannel = new Channel(AuxInputSignal, customChannelName, nativeChannelName, nativeChannelNumber, state,
                                      this, boardStream, auxNumber);
    signalChannels.push_back(newChannel);
    numAuxChannels->setValue(numAuxChannels->getValue() + 1);
}

void SignalGroup::addSupplyVoltageChannel(int nativeChannelNumber, int boardStream, int nameNumber)
{
    QString nativeChannelName = prefix->getValueString() + "-" + "VDD" + QString::number(nameNumber);
    QString customChannelName = nativeChannelName;
    Channel* newChannel = new Channel(SupplyVoltageSignal, customChannelName, nativeChannelName, nativeChannelNumber, state,
                                      this, boardStream);
    newChannel->setEnabled(false);  // Disable supply voltage channels by default.
    signalChannels.push_back(newChannel);
    numVddChannels->setValue(numVddChannels->getValue() + 1);
}

void SignalGroup::addAnalogDigitalIOChannel(SignalType signalType, int nativeChannelNumber)
{
    QString nativeChannelName = prefix->getValueString() + "-";
    if (signalType == BoardAdcSignal || signalType == BoardDacSignal) {
        nativeChannelName +=
                QString::fromStdString(AbstractRHXController::getAnalogIOChannelNumber(state->getControllerTypeEnum(), nativeChannelNumber));
    } else if (signalType == BoardDigitalInSignal || signalType == BoardDigitalOutSignal) {
        nativeChannelName +=
                QString::fromStdString(AbstractRHXController::getDigitalIOChannelNumber(state->getControllerTypeEnum(), nativeChannelNumber));
    }
    QString customChannelName = nativeChannelName;
    Channel* newChannel = new Channel(signalType, customChannelName, nativeChannelName, nativeChannelNumber, state, this);
    signalChannels.push_back(newChannel);
}

void SignalGroup::removeAllChannels()
{
    for (int i = 0; i < (int) signalChannels.size(); i++) {
        if (signalChannels[i]) delete signalChannels[i];
    }
    signalChannels.clear();
    numAmpChannels->setValue(0);
    numAuxChannels->setValue(0);
    numVddChannels->setValue(0);
}

Channel* SignalGroup::channelByCustomName(const QString& customName) const
{
    for (int i = 0; i < (int) signalChannels.size(); i++) {
        if (signalChannels[i]->getCustomName() == customName) {
            return signalChannels[i];
        }
    }
    return nullptr;
}

Channel* SignalGroup::channelByUserOrder(SignalType signalType, int index) const
{
    Channel* channel = nullptr;
    for (int i = 0; i < numChannels(); ++i) {
        channel = channelByIndex(i);
        if (channel->getSignalType() == signalType) {
            if (channel->getUserOrder() == index) return channel;
        }
    }
    return nullptr;
}

int SignalGroup::numChannels(SignalType type) const
{
    int count = 0;
    for (int i = 0; i < (int) signalChannels.size(); ++i) {
        if (signalChannels[i]->getSignalType() == type) {
            count++;
        }
    }
    return count;
}

int SignalGroup::minUserOrder(SignalType type) const
{
    int min = numeric_limits<int>::max();
    for (int i = 0; i < (int) signalChannels.size(); ++i) {
        if (signalChannels[i]->getSignalType() == type) {
            if (signalChannels[i]->getUserOrder() < min) {
                min = signalChannels[i]->getUserOrder();
            }
        }
    }
    return min;
}

void SignalGroup::setOriginalChannelOrder()
{
    for (int i = 0; i < (int) signalChannels.size(); ++i) {
        signalChannels[i]->setUserOrder(signalChannels[i]->getNativeChannelNumber());
    }
}

void SignalGroup::setAlphabeticalChannelOrder()
{
    // Create lists of custom names.
    QStringList amplifierNames;
    QStringList auxInputNames;
    QStringList supplyVoltageNames;
    QStringList otherNames;
    for (int i = 0; i < (int) signalChannels.size(); ++i) {
        SignalType signalType = signalChannels[i]->getSignalType();
        QString customName = signalChannels[i]->getCustomName();
        if (signalType == AmplifierSignal) {
            amplifierNames.append(customName);
        } else if (signalType == AuxInputSignal) {
            auxInputNames.append(customName);
        } else if (signalType == SupplyVoltageSignal) {
            supplyVoltageNames.append(customName);
        } else {
            otherNames.append(customName);
        }
    }

    // Alphabetize custom name lists by type.
    amplifierNames.sort();
    auxInputNames.sort();
    supplyVoltageNames.sort();
    otherNames.sort();

    // Assign new orders, keeping types together.
    int userOrder = 0;
    for (int i = 0; i < amplifierNames.size(); ++i) {
        Channel* channel = channelByCustomName(amplifierNames.at(i));
        if (channel) channel->setUserOrder(userOrder++);
    }
    for (int i = 0; i < auxInputNames.size(); ++i) {
        Channel* channel = channelByCustomName(auxInputNames.at(i));
        if (channel) channel->setUserOrder(userOrder++);
    }
    for (int i = 0; i < supplyVoltageNames.size(); ++i) {
        Channel* channel = channelByCustomName(supplyVoltageNames.at(i));
        if (channel) channel->setUserOrder(userOrder++);
    }
    for (int i = 0; i < otherNames.size(); ++i) {
        Channel* channel = channelByCustomName(otherNames.at(i));
        if (channel) channel->setUserOrder(userOrder++);
    }
}

QStringList SignalGroup::getAttributes(XMLGroup xmlGroup) const
{
    // Create a QStringList from portItems in the given XMLGroup
    QStringList attributeList;

    QString thisAttribute;
    // Add portItems to attributeList;
    for (SingleItemList::const_iterator p = portItems.begin(); p != portItems.end(); ++p) {
        if (p->second->getXMLGroup() == xmlGroup) {
            bool addAttribute = false;
            switch (p->second->getTypeDependency()) {
            case TypeDependencyNone:
                addAttribute = true;
                break;
            case TypeDependencyNonStim:
                if (state->getControllerTypeEnum() != ControllerStimRecordUSB2) {
                    addAttribute = true;
                }
                break;
            case TypeDependencyStim:
                if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
                    addAttribute = true;
                }
                break;
            }

            if (addAttribute) {
                thisAttribute = p->second->getParameterName() + ":_:" + p->second->getValueString();
                attributeList.append(thisAttribute);
            }
        }
    }
    return attributeList;
}


SignalSources::SignalSources(SystemState* state_) :
    state(state_)
{
    undoManager = new DisplayUndoManager(this);
    display = nullptr;

    for (int i = 0; i < state->numSPIPorts; i++) {
        portGroups.push_back(new SignalGroup("Port " + QString(QChar(65 + i)), QString(QChar(65 + i)), state, true));
    }
    // Amplifier channels on SPI ports are added later, if amplifier boards are found to be connected to these ports.

    int numAnalogIO = AbstractRHXController::numAnalogIO(state->getControllerTypeEnum(), state->expanderConnected->getValue());
    int numDigitalIO = AbstractRHXController::numDigitalIO(state->getControllerTypeEnum(), state->expanderConnected->getValue());

    // Add board analog input signals.
    baseGroups.push_back(new SignalGroup("Analog In Ports", "ANALOG-IN", state));
    for (int channel = 0; channel < numAnalogIO; ++channel) {
        baseGroups.back()->addAnalogDigitalIOChannel(BoardAdcSignal, channel);
    }

    // Add board analog output signals.
    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        baseGroups.push_back(new SignalGroup("Analog Out Ports", "ANALOG-OUT", state));
        for (int channel = 0; channel < numAnalogIO; ++channel) {
            baseGroups.back()->addAnalogDigitalIOChannel(BoardDacSignal, channel);
        }
    }

    // Add board digital input signals.
    baseGroups.push_back(new SignalGroup("Digital In Ports", "DIGITAL-IN", state));
    for (int channel = 0; channel < numDigitalIO; ++channel) {
        baseGroups.back()->addAnalogDigitalIOChannel(BoardDigitalInSignal, channel);
    }

    // Add board digital output signals.
    baseGroups.push_back(new SignalGroup("Digital Out Ports", "DIGITAL-OUT", state));
    for (int channel = 0; channel < numDigitalIO; ++channel) {
        baseGroups.back()->addAnalogDigitalIOChannel(BoardDigitalOutSignal, channel);
    }
}

SignalSources::~SignalSources()
{
    for (int i = 0; i < (int) portGroups.size(); i++) {
        if (portGroups[i]) delete portGroups[i];
    }
    for (int i = 0; i < (int) baseGroups.size(); i++) {
        if (baseGroups[i]) delete baseGroups[i];
    }
    delete undoManager;
}

void SignalSources::updateChannelMap()
{
    // Create map of native names to channel pointers for quick access.
    channelMap.clear();
    for (int i = 0; i < numPortGroups(); i++) {
        const SignalGroup* group = portGroupByIndex(i);
        for (int j = 0; j < group->numChannels(); ++j) {
            Channel* channel = group->channelByIndex(j);
            channelMap[channel->getNativeNameString()] = channel;
        }
    }
    for (int i = 0; i < numBaseGroups(); i++) {
        const SignalGroup* group = baseGroupByIndex(i);
        for (int j = 0; j < group->numChannels(); ++j) {
            Channel* channel = group->channelByIndex(j);
            channelMap[channel->getNativeNameString()] = channel;
        }
    }
}

void SignalSources::clearTCPDataOutput()
{
    // Go through all signal groups.
    for (int group = 0; group < numGroups(); ++group) {
        SignalGroup *thisGroup = groupByIndex(group);
        // Go through all channels.
        for (int channel = 0; channel < thisGroup->numChannels(); ++channel) {
            Channel *thisChannel = thisGroup->channelByIndex(channel);
            thisChannel->clearTCPDataOutput();
        }
    }
}

QStringList SignalSources::getTcpFilterBands()
{
    QStringList filterBandList;
    // Go through all signal groups.
    for (int group = 0; group < numGroups(); ++group) {
        SignalGroup *thisGroup = groupByIndex(group);
        // Go through all channels.
        for (int channel = 0; channel < thisGroup->numChannels(); ++channel) {
            Channel *thisChannel = thisGroup->channelByIndex(channel);
            QStringList thisChannelList = thisChannel->getTcpBandNames();
            if (thisChannelList.size() > 0) {
                filterBandList.append(thisChannelList);
            }
        }
    }
    return filterBandList;
}

SignalGroup* SignalSources::groupByIndex(int index) const
{
    if (index < (int) portGroups.size()) {
        return portGroups[index];
    } else {
        index -= (int) portGroups.size();
    }
    if (index < (int) baseGroups.size()) {
        return baseGroups[index];
    } else {
        return nullptr;
    }
}

// Return a pointer to a SignalGroup with a particular groupName (e.g., "Port A").
SignalGroup* SignalSources::groupByName(const QString& groupName) const
{
    QString groupNameLower = groupName.toLower();
    for (int i = 0; i < numPortGroups(); ++i) {
        SignalGroup* group = portGroupByIndex(i);
        if (group->getName().toLower() == groupNameLower) return group;
    }
    for (int i = 0; i < numBaseGroups(); ++i) {
        SignalGroup* group = baseGroupByIndex(i);
        if (group->getName().toLower() == groupNameLower) return group;
    }
    return nullptr;
}

// Return a pointer to a SignalChannel with a particular nativeName (e.g., "A-002").
Channel* SignalSources::channelByName(const QString& nativeName) const
{
    string nativeNameStr = nativeName.section('|', 0, 0).toStdString();  // strip off filter name, if present
    map<string, Channel*>::const_iterator p = channelMap.find(nativeNameStr);
    if (p != channelMap.end()) {
        return p->second;
    }
//    cerr << "SignalSources::channelByName: name not found: " << nativeNameStr << EndOfLine;
    return nullptr;
}

Channel* SignalSources::channelByName(const string& nativeName) const
{
    return channelByName(QString::fromStdString(nativeName));
}

// Return a pointer to a SignalChannel corresponding to a particular USB interface data stream and chip channel number.
Channel* SignalSources::getAmplifierChannel(int boardStream, int chipChannel) const
{
    for (int i = 0; i < (int) portGroups.size(); ++i) {
        for (int j = 0; j < portGroups[i]->numChannels(); ++j) {
            Channel* channel = portGroups[i]->channelByIndex(j);
            if (channel->getSignalType() == AmplifierSignal) {
                if (channel->getBoardStream() == boardStream && channel->getChipChannel() == chipChannel) {
                    return channel;
                }
            }
        }
    }
    return nullptr;
}

QString SignalSources::getNativeAndCustomNames(const QString& nativeName) const
{
    QString fullName;
    Channel* channel = channelByName(nativeName);
    if (channel) {
        fullName = channel->getNativeAndCustomNames();
    }
    return fullName;
}

QString SignalSources::getNativeAndCustomNames(const string& nativeName) const
{
    return getNativeAndCustomNames(QString::fromStdString(nativeName));
}

void SignalSources::setSelected(const QString& nativeName, bool selected)
{
    Channel* channel = channelByName(nativeName);
    if (!channel) {
        cerr << "SignalSources::setSelected: channel not found from name " << nativeName.toStdString() << '\n';
        return;
    }
    channel->setSelected(selected);
}

void SignalSources::setSelectedEntireGroupID(const QString& nativeName, bool selected)
{
    Channel* channel = channelByName(nativeName);
    if (!channel) {
        cerr << "SignalSources::setSelectedEntireGroupID: channel not found from name " << nativeName.toStdString() << '\n';
        return;
    }
    int groupID = channel->getGroupID();
    if (groupID == 0) {
        channel->setSelected(selected);
    } else {
        for (int i = 0; i < (int) portGroups.size(); ++i) {
            for (int j = 0; j < portGroups[i]->numChannels(); ++j) {
                Channel* channel2 = portGroups[i]->channelByIndex(j);
                if (channel2->getGroupID() == groupID) {
                    channel2->setSelected(selected);
                }
            }
        }
    }
}

int SignalSources::numChannelsSelected() const
{
    int count = 0;
    for (int i = 0; i < numGroups(); i++) {
        const SignalGroup* group = groupByIndex(i);
        for (int j = 0; j < group->numChannels(); ++j) {
            const Channel* channel = group->channelByIndex(j);
            if (channel->isSelected()) ++count;
        }
    }
    return count;
}

bool SignalSources::onlyAmplifierChannelsSelected() const
{
    bool anythingSelected = false;
    for (int i = 0; i < numGroups(); i++) {
        const SignalGroup* group = groupByIndex(i);
        for (int j = 0; j < group->numChannels(); ++j) {
            const Channel* channel = group->channelByIndex(j);
            if (channel->isSelected()) {
                if (channel->getSignalType() != AmplifierSignal) return false;
                anythingSelected = true;
            }
        }
    }
    return anythingSelected;
}

// Returns -1 if no channel are selected; 0 if all selected channels are disabled; 2 if all selected channels are enabled;
// 1 if mixed.  (Values 0-2 correspond to Qt::CheckState values for a QCheckBox.)
int SignalSources::enableStateOfSelectedChannels() const
{
    bool enabledFound = false;
    bool disabledFound = false;
    for (int i = 0; i < numGroups(); i++) {
        const SignalGroup* group = groupByIndex(i);
        for (int j = 0; j < group->numChannels(); ++j) {
            const Channel* channel = group->channelByIndex(j);
            if (channel->isSelected()) {
                if (channel->isEnabled()) enabledFound = true;
                else disabledFound = true;
            }
        }
    }
    if (!enabledFound && !disabledFound) return -1;
    if (!enabledFound && disabledFound) return 0;
    if (enabledFound && disabledFound) return 1;
    return 2;
}


int SignalSources::getNextAvailableGroupID() const
{
    int groupID = 1;
    while (groupIDisPresent(groupID)) {
        ++groupID;
    }
    return groupID;
}

bool SignalSources::groupIDisPresent(int groupID) const
{
    for (int i = 0; i < (int) portGroups.size(); ++i) {
        for (int j = 0; j < portGroups[i]->numChannels(); ++j) {
            const Channel* channel = portGroups[i]->channelByIndex(j);
            if (channel->getGroupID() == groupID) {
                return true;
            }
        }
    }
    return false;
}

bool SignalSources::isCustomNamePresent(const QString& customName) const
{
    for (int i = 0; i < numGroups(); i++) {
        const SignalGroup* group = groupByIndex(i);
        for (int j = 0; j < group->numChannels(); ++j) {
            const Channel* channel = group->channelByIndex(j);
            if (channel->getCustomName() == customName) return true;
        }
    }
    return false;
}

int SignalSources::numChannels(SignalType type) const
{
    int count = 0;
    for (int i = 0; i < numGroups(); ++i) {
        count += groupByIndex(i)->numChannels(type);
    }
    return count;
}

SignalList SignalSources::getSaveSignalList() const
{
    SignalList signalList;
    for (int i = 0; i < numGroups(); i++) {
        const SignalGroup* group = groupByIndex(i);
        for (int j = 0; j < group->numChannels(); ++j) {
            Channel* channel = group->channelByIndex(j);
            if (channel->isEnabled()) {
                signalList.addChannel(channel);
            }
        }
    }
    return signalList;
}

QString SignalSources::singleSelectedAmplifierChannelName() const
{
    int numSelectedChannels = 0;
    QString singleName;
    vector<string> namelist = amplifierChannelsNameList();
    for (auto &name : namelist) {
        QString qName = QString::fromStdString(name);
        Channel *thisChannel = channelByName(qName);
        if (thisChannel->isSelected()) {
            ++numSelectedChannels;
            singleName = qName;
        }
        if (numSelectedChannels > 1) {
            return QString("");
        }
    }

    if (numSelectedChannels == 0) return QString("");
    else return singleName;
}

Channel* SignalSources::selectedChannel() const
{
    for (int i = 0; i < numGroups(); i++) {
        const SignalGroup* group = groupByIndex(i);
        for (int j = 0; j < group->numChannels(); ++j) {
            Channel* channel = group->channelByIndex(j);
            if (channel->isSelected()) return channel;
        }
    }
    return nullptr;
}

void SignalSources::getSelectedSignals(QList<Channel*>& selectedSignals) const
{
    selectedSignals.clear();
    for (int i = 0; i < numGroups(); ++i) {
        const SignalGroup* group = groupByIndex(i);
        for (int j = 0; j < group->numChannels(); ++j) {
            Channel* channel = group->channelByIndex(j);
            if (channel->isSelected())
                selectedSignals.append(channel);
        }
    }
}

QString SignalSources::firstChannelName() const
{
    vector<string> namelist = amplifierChannelsNameList();
    if (amplifierChannelsNameList().size() > 0) {
        return QString::fromStdString(namelist[0]);
    } else {
        return "";
    }
}

vector<string> SignalSources::amplifierChannelsNameList() const
{
    vector<string> namelist;
    namelist.resize(numAmplifierChannels());
    int index = 0;
    for (int i = 0; i < (int) portGroups.size(); ++i) {
        for (int j = 0; j < portGroups[i]->numChannels(); ++j) {
            Channel* channel = portGroups[i]->channelByIndex(j);
            if (channel->getSignalType() == AmplifierSignal) {
                namelist[index++] = channel->getNativeNameString();
            }
        }
    }
    return namelist;
}

vector<string> SignalSources::completeChannelsNameList() const
{
    vector<string> namelist;
    for (int i = 0; i < numGroups(); i++) {
        const SignalGroup* group = groupByIndex(i);
        for (int j = 0; j < group->numChannels(); ++j) {
            Channel* channel = group->channelByIndex(j);
            namelist.push_back(channel->getNativeNameString());
        }
    }
    return namelist;
}

QStringList SignalSources::amplifierNameListUserOrder(const QString& port) const
{
    QStringList namelist;
    QString portLower = port.toLower();
    for (int i = 0; i < (int) portGroups.size(); i++) {
        if (portGroups[i]->getName().toLower() == portLower) {
            for (int j = 0; j < portGroups[i]->numChannels(AmplifierSignal); ++j) {
                const Channel* channel = portGroups[i]->channelByUserOrder(AmplifierSignal, j);
                QString name = channel->getCustomName();
                if (channel->getCustomName() != channel->getNativeName()) {  // If custom name is different from native name,
                    name += " (" + channel->getNativeName() + ")";           // add native name in parentheses.
                }
                namelist.append(name);
            }
            break;
        }
    }
    return namelist;
}

QStringList SignalSources::headstageSignalNameList(const QString& port) const
{
    QStringList namelist;
    QString portLower = port.toLower();
    for (int i = 0; i < (int) portGroups.size(); i++) {
        if (portGroups[i]->getName().toLower() == portLower) {
            for (int j = 0; j < portGroups[i]->numChannels(); ++j) {
                const Channel* channel = portGroups[i]->channelByIndex(j);
                QString name = channel->getCustomName();
                if (channel->getCustomName() != channel->getNativeName()) {  // If custom name is different from native name,
                    name += " (" + channel->getNativeName() + ")";           // add native name in parentheses.
                }
                namelist.append(name);
            }
            break;
        }
    }
    return namelist;
}

QStringList SignalSources::controllerIOSignalNameList() const
{
    QStringList namelist;
    for (int i = 0; i < (int) baseGroups.size(); i++) {
        for (int j = 0; j < baseGroups[i]->numChannels(); ++j) {
            Channel* channel = baseGroups[i]->channelByIndex(j);
            QString name = channel->getCustomName();
            if (channel->getCustomName() != channel->getNativeName()) {  // If custom name is different from native name,
                name += " (" + channel->getNativeName() + ")";           // add native name in parentheses.
            }
            namelist.append(name);
        }
    }
    return namelist;
}

QStringList SignalSources::populatedGroupList() const
{
    QStringList namelist;
    for (int i = 0; i < numGroups(); i++) {
        const SignalGroup* group = groupByIndex(i);
        if (group->numChannels() > 0) {
            namelist.append(group->getName());
        }
    }
    return namelist;
}

QStringList SignalSources::populatedGroupListWithChannelCounts() const
{
    QStringList namelist;
    for (int i = 0; i < numPortGroups(); i++) {
        const SignalGroup* group = portGroupByIndex(i);
        if (group->numChannels() > 0) {
            namelist.append(group->getName() + " (" + QString::number(group->numChannels(AmplifierSignal)) + " ch)");
        }
    }
    for (int i = 0; i < numBaseGroups(); i++) {
        const SignalGroup* group = baseGroupByIndex(i);
        if (group->numChannels() > 0) {
            namelist.append(group->getName());
        }
    }
    return namelist;
}

QStringList SignalSources::getDisplayListAmplifiers(const QString& groupName) const
{
    QStringList displayList;
    const SignalGroup* group = groupByName(groupName);
    if (!group) return displayList;

    QStringList orderedFilterList;
    if (state->filterDisplay1->getValue() != "None") {
        QString filterName = state->filterDisplay1->getDisplayValueString();
        orderedFilterList.append(filterName);
    }
    if (state->filterDisplay2->getValue() != "None") {
        QString filterName = state->filterDisplay2->getDisplayValueString();
        if (!orderedFilterList.contains(filterName)) orderedFilterList.append(filterName);
    }
    if (state->filterDisplay3->getValue() != "None") {
        QString filterName = state->filterDisplay3->getDisplayValueString();
        if (!orderedFilterList.contains(filterName)) orderedFilterList.append(filterName);
    }
    if (state->filterDisplay4->getValue() != "None") {
        QString filterName = state->filterDisplay4->getDisplayValueString();
        if (!orderedFilterList.contains(filterName)) orderedFilterList.append(filterName);
    }

    QStringList orderedChannelList;
    for (int i = 0; i < group->numChannels(AmplifierSignal); ++i) {
        const Channel* channel = group->channelByUserOrder(AmplifierSignal, i);
        if (channel) {
            orderedChannelList.append(channel->getNativeName());
        }
    }

    if (state->arrangeBy->getValue() == "Channel") {
        // Arrange by channel
        for (int i = 0; i < orderedChannelList.size(); ++i) {
            for (int j = 0; j < orderedFilterList.size(); ++j) {
                displayList.append(orderedChannelList[i] + "|" + orderedFilterList[j]);
            }
        }
    } else {
        // Arrange by filter
        for (int j = 0; j < orderedFilterList.size(); ++j) {
            for (int i = 0; i < orderedChannelList.size(); ++i) {
                displayList.append(orderedChannelList[i] + "|" + orderedFilterList[j]);
            }
            displayList.append("/");    // divider
        }
    }
    return displayList;
}

QStringList SignalSources::getDisplayListAmplifiersNoFilters(const QString& groupName) const
{
    QStringList displayList;
    const SignalGroup* group = groupByName(groupName);
    if (!group) return displayList;

    for (int i = 0; i < group->numChannels(AmplifierSignal); ++i) {
        const Channel* channel = group->channelByUserOrder(AmplifierSignal, i);
        if (channel) displayList.append(channel->getNativeName());
    }
    return displayList;
}

QStringList SignalSources::getDisplayListAuxInputs(const QString& groupName) const
{
    QStringList displayList;
    const SignalGroup* group = groupByName(groupName);
    if (!group) return displayList;

    int index = group->minUserOrder(AuxInputSignal);
    for (int i = 0; i < group->numChannels(AuxInputSignal); ++i) {
        const Channel* channel = group->channelByUserOrder(AuxInputSignal, index);
        if (channel) {
            displayList.append(channel->getNativeName());
        } else {
            i--;
        }
        index++;
    }
    return displayList;
}

QStringList SignalSources::getDisplayListSupplyVoltages(const QString& groupName) const
{
    QStringList displayList;
    const SignalGroup* group = groupByName(groupName);
    if (!group) return displayList;

    int index = group->minUserOrder(SupplyVoltageSignal);
    for (int i = 0; i < group->numChannels(SupplyVoltageSignal); ++i) {
        Channel* channel = group->channelByUserOrder(SupplyVoltageSignal, index);
        if (channel) {
            displayList.append(channel->getNativeName());
        } else {
            i--;
        }
        index++;
    }
    return displayList;
}

QStringList SignalSources::getDisplayListBaseGroup(const QString& groupName) const
{
    QStringList displayList;
    const SignalGroup* group = groupByName(groupName);
    if (!group) return displayList;

    for (int i = 0; i < group->numChannels(BoardAdcSignal); ++i) {
        Channel* channel = group->channelByUserOrder(BoardAdcSignal, i);
        if (channel) {
            displayList.append(channel->getNativeName());
        }
    }
    for (int i = 0; i < group->numChannels(BoardDacSignal); ++i) {
        Channel* channel = group->channelByUserOrder(BoardDacSignal, i);
        if (channel) {
            displayList.append(channel->getNativeName());
        }
    }
    for (int i = 0; i < group->numChannels(BoardDigitalInSignal); ++i) {
        Channel* channel = group->channelByUserOrder(BoardDigitalInSignal, i);
        if (channel) {
            displayList.append(channel->getNativeName());
        }
    }
    for (int i = 0; i < group->numChannels(BoardDigitalOutSignal); ++i) {
        Channel* channel = group->channelByUserOrder(BoardDigitalOutSignal, i);
        if (channel) {
            displayList.append(channel->getNativeName());
        }
    }
    return displayList;
}

DisplayState SignalSources::saveState() const
{
    DisplayState savedState;
    savedState.groups.resize(numPortGroups() + numBaseGroups());

    for (int i = 0; i < numPortGroups(); ++i) {
        savedState.groups[i].name = portGroups[i]->getName();
        const SignalGroup* group = portGroupByIndex(i);
        savedState.groups[i].signalChannels.resize(group->numChannels());
        for (int j = 0; j < group->numChannels(); ++j) {
            ChannelState* saveChannel = &savedState.groups[i].signalChannels[j];
            const Channel* channel = group->channelByIndex(j);
            saveChannelState(channel, saveChannel);
        }
    }
    int offset = numPortGroups();
    for (int i = 0; i < numBaseGroups(); ++i) {
        savedState.groups[i + offset].name = baseGroups[i]->getName();
        const SignalGroup* group = baseGroupByIndex(i);
        savedState.groups[i + offset].signalChannels.resize(group->numChannels());
        for (int j = 0; j < group->numChannels(); ++j) {
            ChannelState* saveChannel = &savedState.groups[i + offset].signalChannels[j];
            const Channel* channel = group->channelByIndex(j);
            saveChannelState(channel, saveChannel);
        }
    }

    if (display) {
        savedState.columns.resize(display->numColumns());
        for (int i = 0; i < display->numColumns(); ++i) {
            savedState.columns[i].pinnedWaveNames = display->getPinnedWaveNames(i);
            savedState.columns[i].showPinned = display->arePinnedShown(i);
            savedState.columns[i].columnVisible = display->isColumnVisible(i);
            savedState.columns[i].visiblePortName = display->getSelectedPort(i);
            savedState.columns[i].scrollBarState = display->getScrollBarState(i);
        }
    }

    return savedState;
}

void SignalSources::saveChannelState(const Channel* channel, ChannelState* saveChannel) const
{
    if (!channel) return;
    saveChannel->nativeChannelName = channel->getNativeName();
    saveChannel->customChannelName = channel->getCustomName();
    saveChannel->color = channel->getColor();
    saveChannel->userOrder = channel->getUserOrder();
    saveChannel->groupID = channel->getGroupID();
    saveChannel->enabled = channel->isEnabled();
    saveChannel->selected = channel->isSelected();
    saveChannel->outputToTcp = channel->getOutputToTcp();
    saveChannel->reference = channel->getReference();
}

bool SignalSources::restoreChannelState(const ChannelState* saveChannel, Channel* channel)
{
    if (!saveChannel) return false;
    if (saveChannel->nativeChannelName != channel->getNativeName()) return false;
    channel->setCustomName(saveChannel->customChannelName);
    channel->setColor(saveChannel->color);
    channel->setUserOrder(saveChannel->userOrder);
    channel->setGroupID(saveChannel->groupID);
    channel->setEnabled(saveChannel->enabled);
    channel->setSelected(saveChannel->selected);
    channel->setOutputToTcp(saveChannel->outputToTcp);
    channel->setReference(saveChannel->reference);
    return true;
}

void SignalSources::restoreState(const DisplayState& savedState)
{
    for (int i = 0; i < (int) savedState.groups.size(); ++i) {
        const SignalGroup* group = groupByName(savedState.groups[i].name);
        if (!group) {
            qDebug() << "SignalSources::restoreState: group " << group->getName() << " not found.";
            continue;
        } else if (group->numChannels() != (int) savedState.groups[i].signalChannels.size()) {
            qDebug() << "SignalSources::restoreState: group " << group->getName() <<
                      " has a different number of channels.";
        }
        for (int j = 0; j < (int) savedState.groups[i].signalChannels.size(); ++j) {
            const ChannelState* restoreChannel = &savedState.groups[i].signalChannels[j];
            Channel* channel = channelByName(restoreChannel->nativeChannelName);
            if (!channel) {
                qDebug() << "SignalSources::restoreState: channel " << restoreChannel->nativeChannelName <<
                            " not found.";
            } else {
                restoreChannelState(restoreChannel, channel);
            }
        }
    }

    if (display && ((int) savedState.columns.size() == display->numColumns())) {
        for (int i = 0; i < (int) savedState.columns.size(); ++i) {
            display->setPinnedWaveforms(i, savedState.columns[i].pinnedWaveNames);
            display->setShowPinned(i, savedState.columns[i].showPinned);
            display->setColumnVisible(i, savedState.columns[i].columnVisible);
            display->setSelectedPort(i, savedState.columns[i].visiblePortName);
            display->restoreScrollBarState(i, savedState.columns[i].scrollBarState);
        }
    }
}

void SignalSources::setOriginalChannelOrder()
{
    for (int i = 0; i < numGroups(); ++i) {
        SignalGroup* group = groupByIndex(i);
        group->setOriginalChannelOrder();
    }
}

void SignalSources::setAlphabeticalChannelOrder()
{
    for (int i = 0; i < numGroups(); ++i) {
        SignalGroup* group = groupByIndex(i);
        group->setAlphabeticalChannelOrder();
    }
}

void SignalSources::autoColorAmplifierChannels(int numColors, int numChannelsSameColor)
{
    for (int port = 0; port < numPortGroups(); ++port) {
        const SignalGroup* group = portGroupByIndex(port);
        if (!group->isEnabled()) continue;

        int numAmpChannels = group->numChannels(AmplifierSignal);
        int colorIndex = 0;
        int channelCount = numChannelsSameColor;
        for (int i = 0; i < numAmpChannels; ++i) {
            // We can count on amplifier signals being at top of signal list.
            Channel* channel = group->channelByUserOrder(AmplifierSignal, i);
            bool channelVisible = (state->showDisabledChannels->getValue() || channel->isEnabled());
            if (channelVisible) {
                channel->setColor(channelColor(colorIndex, numColors));
                int groupID = channel->getGroupID();
                if (groupID != 0) {
                    bool sameGroup = true;
                    while (++i < numAmpChannels && sameGroup) {
                        Channel* channel2 = group->channelByUserOrder(AmplifierSignal, i);
                        if (channel2->getGroupID() == groupID) {
                            channel2->setColor(channelColor(colorIndex, numColors));
                        } else {
                            sameGroup = false;
                            i -= 2;
                        }
                    }
                }
                if (--channelCount == 0) {
                    ++colorIndex;
                    channelCount = numChannelsSameColor;
                }
            }
        }
    }
}

QColor SignalSources::channelColor(int colorIndex, int numColors) const
{
    int index = colorIndex % numColors;
    const int NumCycles = 3;
    double hue = NumCycles * ((double) index / (double) numColors);
    while (hue > 1.0) {
        hue -= 1.0;
    }
    const double Saturation = 0.50;
    const double Value = 0.50;
    return QColor::fromHslF(hue, Saturation, Value);
}

void SignalSources::autoGroupAmplifierChannels(int groupSize)
{
    if (groupSize < 2 || groupSize > MaxNumWaveformsInGroup) {
        cerr << "SignalSources::autoGroupAmplifierChannels: illegal groupSize = " << groupSize << '\n';
        return;
    }
    for (int port = 0; port < numPortGroups(); ++port) {
        const SignalGroup* group = portGroupByIndex(port);
        if (!group->isEnabled()) continue;

        // First, ungroup all grouped channels.
        for (int i = 0; i < group->numChannels(AmplifierSignal); ++i) {
            Channel* channel = group->channelByIndex(i);
            channel->setGroupID(0);
        }

        // Second, move all disabled channels to end of group.
        while (moveLastDisabledAmplifierChannelToEnd(group)) {}

        // Now find index of last enabled channel.
        int lastEnabledChannelIndex = 0;
        for (int i = group->numChannels(AmplifierSignal) - 1; i >= 0; --i) {
            Channel* channel = group->channelByUserOrder(AmplifierSignal, i);
            if (channel->isEnabled()) {
                lastEnabledChannelIndex = i;
                break;
            }
        }

        // Next, group enabled channels.
        for (int i = 0; i <= lastEnabledChannelIndex - groupSize + 1; i += groupSize) {
            int groupID = getNextAvailableGroupID();
            QColor groupColor;
            for (int j = i; j < i + groupSize; ++j) {
                Channel* channel = group->channelByUserOrder(AmplifierSignal, j);
                channel->setGroupID(groupID);
                if (j == i) {
                    groupColor = channel->getColor();
                } else {
                    channel->setColor(groupColor);
                }
            }
        }
    }
}

bool SignalSources::anyChannelsGrouped() const
{
    for (int i = 0; i < numPortGroups(); ++i) {
        const SignalGroup* group = portGroupByIndex(i);
        for (int j = 0; j < group->numChannels(); ++j) {
            const Channel* channel = group->channelByIndex(j);
            if (channel->getGroupID() != 0) return true;
        }
    }
    return false;
}

void SignalSources::ungroupAllChannels()
{
    for (int i = 0; i < numPortGroups(); ++i) {
        const SignalGroup* group = portGroupByIndex(i);
        for (int j = 0; j < group->numChannels(); ++j) {
            Channel* channel = group->channelByIndex(j);
            channel->setGroupID(0);
        }
    }
}

void SignalSources::setSelectedChannelReferences(const QString& refString)
{
    for (int i = 0; i < numPortGroups(); ++i) {
        const SignalGroup* group = portGroupByIndex(i);
        for (int j = 0; j < group->numChannels(AmplifierSignal); ++j) {
            Channel* channel = group->channelByIndex(j);
            if (channel->isSelected()) {
                channel->setReference(refString);
            }
        }
    }
}

bool SignalSources::moveLastDisabledAmplifierChannelToEnd(const SignalGroup* group)
{
    int lastEnabledIndex = -1;
    for (int i = group->numChannels(AmplifierSignal) - 1; i >= 0; --i) {
        const Channel* channel = group->channelByUserOrder(AmplifierSignal, i);
        if (channel->isEnabled()) {
            lastEnabledIndex = i;
            break;
        }
    }
    if (lastEnabledIndex == -1) return false;  // All channels are disabled.

    Channel* lastDisabledAmplifierChannel = nullptr;
    for (int i = lastEnabledIndex - 1; i >= 0; --i) {
        Channel* channel = group->channelByUserOrder(AmplifierSignal, i);
        if (!channel->isEnabled()) {
            lastDisabledAmplifierChannel = channel;
            break;
        }
    }
    if (!lastDisabledAmplifierChannel) return false;  // All disabled channels have been moved to end.

    for (int i = lastDisabledAmplifierChannel->getUserOrder() + 1; i <= lastEnabledIndex; ++i) {
        Channel* channel = group->channelByUserOrder(AmplifierSignal, i);
        channel->setUserOrder(channel->getUserOrder() - 1);
    }
    lastDisabledAmplifierChannel->setUserOrder(lastEnabledIndex);
    return true;
}

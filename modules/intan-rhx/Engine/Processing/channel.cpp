//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.4.0
//
//  Copyright (c) 2020-2025 Intan Technologies
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
#include "rhxglobals.h"
#include "signalsources.h"
#include "channel.h"

Channel::Channel(SignalType signalType_, const QString &customChannelName_, const QString &nativeChannelName_,
                 int nativeChannelNumber_, SystemState* state_, SignalGroup *signalGroup_, int boardStream_,
                 int chipChannel_, int commandStream_) :
    stimParameters(nullptr),
    state(state_),
    signalGroup(signalGroup_),
    signalType(signalType_),
    stimCapable(false),
    nativeChannelNumber(nativeChannelNumber_),
    selected(false),
    boardStream(boardStream_),
    chipChannel(chipChannel_),
    commandStream(commandStream_),
    color(nullptr),
    reference(nullptr),
    userOrder(nullptr),
    groupID(nullptr),
    enabled(nullptr),
    nativeChannelName(nullptr),
    customChannelName(nullptr),
    outputToTcp(nullptr),
    outputToTcpLow(nullptr),
    outputToTcpHigh(nullptr),
    outputToTcpSpike(nullptr),
    outputToTcpDc(nullptr),
    outputToTcpStim(nullptr),
    spikeThreshold(nullptr),
    electrodeImpedance({ false, 0.0, 0.0 })
{
    if ((signalType == AmplifierSignal || signalType == BoardDacSignal || signalType == BoardDigitalOutSignal)) {
        stimCapable = true;
        stimParameters = new StimParameters(channelItems, state, signalType);
    }

    nativeChannelName = new ChannelNameItem("NativeChannelName", channelItems, state, nativeChannelName_, XMLGroupReadOnly);
    customChannelName = new StringItem("CustomChannelName", channelItems, state, customChannelName_);

    color = new StringItem("Color", channelItems, state, "#909020");

    userOrder = new IntRangeItem("UserOrder", channelItems, state, 0, 2000, nativeChannelNumber_);
    groupID = new IntRangeItem("GroupID", channelItems, state, 0, 2000, 0);

    enabled = new BooleanItem("Enabled", channelItems, state, true, XMLGroupGeneral);
    if (signalType == BoardAdcSignal || signalType == BoardDacSignal || signalType == BoardDigitalInSignal ||
            signalType == BoardDigitalOutSignal) {
        enabled->setValue(false);
    }

    outputToTcp = new BooleanItem("TCPDataOutputEnabled", channelItems, state, false, XMLGroupNone);  // Wideband for amplifier channels, unfiltered for all others
    if (signalType == AmplifierSignal) {
        spikeThreshold = new IntRangeItem("SpikeThresholdMicroVolts", channelItems, state, -5000, 5000, -70, XMLGroupSpikeSettings);
        outputToTcpLow = new BooleanItem("TCPDataOutputEnabledLow", channelItems, state, false, XMLGroupNone);
        outputToTcpHigh = new BooleanItem("TCPDataOutputEnabledHigh", channelItems, state, false, XMLGroupNone);
        outputToTcpSpike = new BooleanItem("TCPDataOutputEnabledSpike", channelItems, state, false, XMLGroupNone);
        outputToTcpDc = new BooleanItem("TCPDataOutputEnabledDC", channelItems, state, false, XMLGroupNone, TypeDependencyStim);
        outputToTcpStim = new BooleanItem("TCPDataOutputEnabledStim", channelItems, state, false, XMLGroupNone, TypeDependencyStim);
        reference = new StringItem("Reference", channelItems, state, "Hardware");
    }
}

Channel::~Channel()
{
    state->forceUpdate();
    if (stimParameters) delete stimParameters;

    for (SingleItemList::const_iterator p = channelItems.begin(); p != channelItems.end(); ++p) {
        delete p->second;
    }
}

QStringList Channel::getAttributes(XMLGroup xmlGroup) const
{
    // Create a QStringList from channelItems in the given XMLGroup.
    QStringList attributeList;

    QString thisAttribute;
    // Add channelItems to attributeList.
    for (SingleItemList::const_iterator p = channelItems.begin(); p != channelItems.end(); ++p) {
        if (p->second->getXMLGroup() == xmlGroup) {
            bool addAttribute = false;
            switch (p->second->getTypeDependency()) {
            case TypeDependencyNone:
                addAttribute = true;
                break;
            case TypeDependencyNonStim:
                if (state->getControllerTypeEnum() != ControllerStimRecord) {
                    addAttribute = true;
                }
                break;
            case TypeDependencyStim:
                if (state->getControllerTypeEnum() == ControllerStimRecord) {
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

QStringList Channel::getTcpBandNames() const
{
    QStringList tcpBandNames;
    if (getSignalType() == AmplifierSignal) {
        if (outputToTcp->getValue()) tcpBandNames.append(nativeChannelName->getValue() + "|WIDE");
        if (getSignalType() == AmplifierSignal) {
            if (outputToTcpLow->getValue()) tcpBandNames.append(nativeChannelName->getValue() + "|LOW");
            if (outputToTcpHigh->getValue()) tcpBandNames.append(nativeChannelName->getValue() + "|HIGH");
            if (outputToTcpSpike->getValue()) tcpBandNames.append(nativeChannelName->getValue() + "|SPK");
            // We're treating spike differently through tcp, so don't append the SPK name here
            if (state->getControllerTypeEnum() == ControllerStimRecord) {
                if (outputToTcpDc->getValue()) tcpBandNames.append(nativeChannelName->getValue() + "|DC");
                if (outputToTcpStim->getValue()) tcpBandNames.append(nativeChannelName->getValue() + "|STIM");
            }
        }
    } else {
        if (outputToTcp->getValue()) tcpBandNames.append(nativeChannelName->getValue());
    }
    return tcpBandNames;
}

int Channel::convertToRHDSignalType(SignalType type)  // Adjustment to maintain compatibility with RHD format
{
    if (((int) type) > ((int) BoardAdcSignal)) {
        return ((int) type) - 1;
    } else {
        return ((int) type);
    }
}

SignalType Channel::convertRHDIntToSignalType(int type)
{
    if (type > ((int) BoardAdcSignal)) type++;
    return (SignalType) type;
}

SignalType Channel::convertRHSIntToSignalType(int type)
{
    return (SignalType) type;
}

void Channel::setChipChannel(int chipChannel_)
{
    if (chipChannel != chipChannel_) {
        chipChannel = chipChannel_;
        state->forceUpdate();
    }
}

void Channel::setBoardStream(int boardStream_)
{
    if (boardStream != boardStream_) {
        boardStream = boardStream_;
        state->forceUpdate();
    }
}

void Channel::setCommandStream(int commandStream_)
{
    if (commandStream != commandStream_) {
        commandStream = commandStream_;
        state->forceUpdate();
    }
}

void Channel::setImpedance(double magnitude_, double phase_)
{
    electrodeImpedance.magnitude = magnitude_;
    electrodeImpedance.phase = phase_;
    electrodeImpedance.valid = true;
}

QString Channel::getImpedanceMagnitudeString() const
{
    double zMag = electrodeImpedance.magnitude;
    double scale;
    QString unitPrefix;
    if (zMag >= 1.0e6) {
        scale = 1.0e6;
        unitPrefix = "M";
    } else {
        scale = 1.0e3;
        unitPrefix = "k";
    }
    int precision;
    if (zMag >= 100.0e6) precision = 0;
    else if (zMag >= 10.0e6) precision = 1;
    else if (zMag >= 1.0e6) precision = 2;
    else if (zMag >= 100.0e3) precision = 0;
    else if (zMag >= 10.0e3) precision = 1;
    else precision = 2;

    return QString::number(zMag / scale, 'f', precision) + " " + unitPrefix + OmegaSymbol;
}

QString Channel::getImpedancePhaseString() const
{
    return AngleSymbol + QString::number(electrodeImpedance.phase, 'f', 0) + DegreeSymbol;
}

QString Channel::getNativeAndCustomNames() const
{
    QString nativeName = getNativeName();
    QString customName = getCustomName();
    if (nativeName == customName) {
        return nativeName;
    } else {
        return customName + " (" + nativeName + ")";
    }
}

QString Channel::getGroupName() const
{
    return signalGroup->getName();
}

void Channel::clearTCPDataOutput()
{
    outputToTcp->setValue(false);
    if (signalType == AmplifierSignal) {
        outputToTcpLow->setValue(false);
        outputToTcpHigh->setValue(false);
        outputToTcpSpike->setValue(false);
        if (state->getControllerTypeEnum() == ControllerStimRecord) {
            outputToTcpDc->setValue(false);
            outputToTcpStim->setValue(false);
        }
    }
}

void SignalList::addChannel(const Channel *channel)
{
    bool stimEn = false;

    switch (channel->getSignalType()) {
    case AmplifierSignal:
        amplifierIndices[RHXDataBlock::maxChannelsPerStream() * channel->getBoardStream() + channel->getChipChannel()] =
                (int) amplifier.size();
        amplifier.push_back(channel->getNativeNameString());
        if (channel->isStimCapable()) {
            stimEn = channel->isStimEnabled();
        }
        stimEnabled.push_back(stimEn);
        break;
    case AuxInputSignal:
        auxInput.push_back(channel->getNativeNameString());
        break;
    case SupplyVoltageSignal:
        supplyVoltage.push_back(channel->getNativeNameString());
        break;
    case BoardAdcSignal:
        boardAdc.push_back(channel->getNativeNameString());
        break;
    case BoardDacSignal:
        boardDac.push_back(channel->getNativeNameString());
        break;
    case BoardDigitalInSignal:
        boardDigitalIn.push_back(channel->getNativeNameString());
        break;
    case BoardDigitalOutSignal:
        boardDigitalOut.push_back(channel->getNativeNameString());
        break;
    }
}

int SignalList::getAmplifierIndexFromStreamChannel(int stream, int channel) const
{
    std::map<int, int>::const_iterator p = amplifierIndices.find(RHXDataBlock::maxChannelsPerStream() * stream + channel);
    if (p == amplifierIndices.end()) {
        return -1;
    }
    return p->second;
}

void SignalList::print()
{
    std::cout << "SignalList:" << '\n';

    std::cout << amplifier.size() << " amplifier signals:" << '\n';
    for (int i = 0; i < (int) amplifier.size(); ++i) std::cout << amplifier[i] << '\n';

    std::cout << auxInput.size() << " aux input signals:" << '\n';
    for (int i = 0; i < (int) auxInput.size(); ++i) std::cout << auxInput[i] << '\n';

    std::cout << supplyVoltage.size() << " supply voltage signals:" << '\n';
    for (int i = 0; i < (int) supplyVoltage.size(); ++i) std::cout << supplyVoltage[i] << '\n';

    std::cout << boardAdc.size() << " analog in signals:" << '\n';
    for (int i = 0; i < (int) boardAdc.size(); ++i) std::cout << boardAdc[i] << '\n';

    std::cout << boardDac.size() << " analog out signals:" << '\n';
    for (int i = 0; i < (int) boardDac.size(); ++i) std::cout << boardDac[i] << '\n';

    std::cout << boardDigitalIn.size() << " digital in signals:" << '\n';
    for (int i = 0; i < (int) boardDigitalIn.size(); ++i) std::cout << boardDigitalIn[i] << '\n';

    std::cout << boardDigitalOut.size() << " digital out signals:" << '\n';
    for (int i = 0; i < (int) boardDigitalOut.size(); ++i) std::cout << boardDigitalOut[i] << '\n';

    std::cout << '\n';
}

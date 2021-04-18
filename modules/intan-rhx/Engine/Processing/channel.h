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

#ifndef CHANNEL_H
#define CHANNEL_H

#include <string>
#include <vector>
#include <map>
#include <QString>
#include <QColor>
#include "systemstate.h"

using namespace std;

class SignalGroup;

class Channel
{
public:
    static int convertToRHDSignalType(SignalType type);  // adjustment to maintain compatibiltiy with RHD format
    static SignalType convertRHDIntToSignalType(int type);  // converting signal type ints from .rhd data files
    static SignalType convertRHSIntToSignalType(int type);  // converting signal type ints from .rhs data files

    Channel(SignalType signalType_, const QString &customChannelName_, const QString &nativeChannelName_,
            int nativeChannelNumber_, SystemState* state_, SignalGroup *signalGroup_, int boardStream_ = 0,
            int chipChannel_ = 0, int commandStream_ = 0);
    ~Channel();

    SignalType getSignalType() const { return signalType; }
    int getNativeChannelNumber() const { return nativeChannelNumber; }
    QStringList getAttributes(XMLGroup xmlGroup) const;
    QStringList getTcpBandNames() const;

    bool isEnabled() const { return enabled->getValue(); }
    void setEnabled(bool enabled_) { enabled->setValue(enabled_); }
    bool isSelected() const { return selected; }
    void setSelected(bool selected_) { selected = selected_; }
    int getUserOrder() const { return userOrder->getValue(); }
    void setUserOrder(int userOrder_) { userOrder->setValue(userOrder_); }
    int getGroupID() const { return groupID->getValue(); }
    void setGroupID(int groupID_) { groupID->setValue(groupID_); }
    QColor getColor() const { return QColor(color->getValueString()); }
    void setColor(QColor color_) { color->setValue(color_.name()); }

    int getChipChannel() const { return chipChannel; }
    void setChipChannel(int chipChannel_);
    int getBoardStream() const { return boardStream; }
    void setBoardStream(int boardStream_);
    int getCommandStream() const { return commandStream; }
    void setCommandStream(int commandStream_);

    bool isImpedanceValid() const { return electrodeImpedance.valid; }
    double getImpedanceMagnitude() const { return electrodeImpedance.magnitude; }
    double getImpedancePhase() const { return electrodeImpedance.phase; }
    void setImpedance(double magnitude_, double phase_);
    QString getImpedanceMagnitudeString() const;
    QString getImpedancePhaseString() const;

    QString getReference() const { return signalType == AmplifierSignal ? reference->getValueString() : ""; }
    void setReference(QString reference_) { if (signalType == AmplifierSignal) reference->setValue(reference_); }

    QString getNativeName() const { return nativeChannelName->getValue(); }
    QString getCustomName() const { return customChannelName->getValueString(); }
    QString getNativeAndCustomNames() const;
    void setCustomName(QString customChannelName_) { customChannelName->setValue(customChannelName_); }
    string getNativeNameString() const { return nativeChannelName->getValue().toStdString(); }
    QString getGroupName() const;
    bool isStimCapable() const { return stimCapable; }
    bool isStimEnabled() const { return stimParameters ? stimParameters->enabled->getValue() : false; }
    QString getStimTrigger() const { return stimParameters ? stimParameters->triggerSource->getValue() : QString(""); }

    int getSpikeThreshold() const { return spikeThreshold->getValue(); }
    void setSpikeThreshold(int threshold) { spikeThreshold->setValue(threshold); }
    void setupSpikeThresholdSpinBox(QSpinBox *spinBox) { spikeThreshold->setupSpinBox(spinBox); }

    void clearTCPDataOutput();

    bool getOutputToTcp() const { return outputToTcp->getValue(); }
    void setOutputToTcp(bool output) { outputToTcp->setValue(output); }
    bool getOutputToTcpLow() const { return outputToTcpLow->getValue(); }
    void setOutputToTcpLow(bool output) { outputToTcpLow->setValue(output); }
    bool getOutputToTcpHigh() const { return outputToTcpHigh->getValue(); }
    void setOutputToTcpHigh(bool output) { outputToTcpHigh->setValue(output); }
    bool getOutputToTcpSpike() const { return outputToTcpSpike->getValue(); }
    void setOutputToTcpSpike(bool output) { outputToTcpSpike->setValue(output); }
    bool getOutputToTcpDc() const { return outputToTcpDc->getValue(); }
    void setOutputToTcpDc(bool output) { outputToTcpDc->setValue(output); }
    bool getOutputToTcpStim() const { return outputToTcpStim->getValue(); }
    void setOutputToTcpStim(bool output) { outputToTcpStim->setValue(output); }

    StimParameters* stimParameters;

    SingleItemList channelItems;

private:
    SystemState* state;
    SignalGroup *signalGroup;
    SignalType signalType;
    bool stimCapable;

    int nativeChannelNumber;
    bool selected;

    int boardStream;
    int chipChannel;
    int commandStream;

    StringItem *color;
    StringItem *reference;

    IntRangeItem *userOrder;
    IntRangeItem *groupID;  // 0 = not in a group

    BooleanItem *enabled;

    ChannelNameItem *nativeChannelName;
    StringItem *customChannelName;

    BooleanItem *outputToTcp;  // Wideband for amplifier channels, unfiltered for all other channels
    BooleanItem *outputToTcpLow;  // Only applies to amplifier channels
    BooleanItem *outputToTcpHigh;  // Only applies to amplifier channels
    BooleanItem *outputToTcpSpike;  // Only applies to amplifier channels
    BooleanItem *outputToTcpDc;  // Only applies to Stim amplifier channels
    BooleanItem *outputToTcpStim;  // Only applies to Stim amplifier channels

    IntRangeItem *spikeThreshold;

    struct {
        bool valid;
        double magnitude;
        double phase;
    } electrodeImpedance;
};

// Used by Save Managers.
class SignalList
{
public:
    SignalList() {}
    void addChannel(const Channel* signalChannel);
    int getAmplifierIndexFromStreamChannel(int stream, int channel) const;
    void print();

    vector<string> amplifier;
    vector<bool> stimEnabled;
    vector<string> auxInput;
    vector<string> supplyVoltage;
    vector<string> boardAdc;
    vector<string> boardDac;
    vector<string> boardDigitalIn;
    vector<string> boardDigitalOut;

private:
    map<int, int> amplifierIndices;
};

#endif // CHANNEL_H

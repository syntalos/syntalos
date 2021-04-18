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

#ifndef STIMPARAMETERS_H
#define STIMPARAMETERS_H

#include "rhxglobals.h"
#include "stateitem.h"
#include <QString>
#include <vector>

using namespace std;

enum StimPolarity {
    NegativeFirst = 0,
    PositiveFirst = 1
};

enum TriggerSource {
    DigitalIn1 = 0, DigitalIn2 = 1, DigitalIn3 = 2, DigitalIn4 = 3, DigitalIn5 = 4, DigitalIn6 = 5, DigitalIn7 = 6, DigitalIn8 = 7,
    DigitalIn9 = 8, DigitalIn10 = 9, DigitalIn11 = 10, DigitalIn12 = 11, DigitalIn13 = 12, DigitalIn14 = 13, DigitalIn15 = 14, DigitalIn16 = 15,
    AnalogIn1 = 16, AnalogIn2 = 17, AnalogIn3 = 18, AnalogIn4 = 19, AnalogIn5 = 20, AnalogIn6 = 21, AnalogIn7 = 22, AnalogIn8 = 23,
    KeyPress1 = 24, KeyPress2 = 25, KeyPress3 = 26, KeyPress4 = 27, KeyPress5 = 28, KeyPress6 = 29, KeyPress7 = 30, KeyPress8 = 31
};

enum TriggerEdgeOrLevel {
    TriggerEdge = 0,
    TriggerLevel = 1
};

enum TriggerHighOrLow {
    TriggerHigh = 0,
    TriggerLow = 1
};

enum PulseOrTrain {
    SinglePulse = 0,
    PulseTrain = 1
};


class StimParameters
{
public:
    StimParameters(SingleItemList &hList_, SystemState *state_, SignalType signalType_);

    void populateParametersFrom(StimParameters* originalStimParameters);

    DiscreteItemList *stimShape;
    DiscreteItemList *stimPolarity;
    DiscreteItemList *triggerSource;
    DiscreteItemList *triggerEdgeOrLevel;
    DiscreteItemList *triggerHighOrLow;
    DiscreteItemList *pulseOrTrain;

    BooleanItem *enabled;
    BooleanItem *maintainAmpSettle;
    BooleanItem *enableAmpSettle;
    BooleanItem *enableChargeRecovery;

    DoubleRangeItem *firstPhaseDuration;
    DoubleRangeItem *secondPhaseDuration;
    DoubleRangeItem *interphaseDelay;
    DoubleRangeItem *firstPhaseAmplitude;
    DoubleRangeItem *secondPhaseAmplitude;
    DoubleRangeItem *baselineVoltage;
    DoubleRangeItem *postTriggerDelay;
    DoubleRangeItem *pulseTrainPeriod;
    DoubleRangeItem *refractoryPeriod;
    DoubleRangeItem *preStimAmpSettle;
    DoubleRangeItem *postStimAmpSettle;
    DoubleRangeItem *postStimChargeRecovOn;
    DoubleRangeItem *postStimChargeRecovOff;

    IntRangeItem *numberOfStimPulses;

private:
    SignalType signalType;

};


#endif // STIMPARAMETERS_H

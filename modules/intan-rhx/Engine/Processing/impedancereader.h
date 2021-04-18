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

#ifndef IMPEDANCEREADER_H
#define IMPEDANCEREADER_H

#include <vector>
#include <deque>
#include "systemstate.h"
#include "abstractrhxcontroller.h"
#include "rhxdatablock.h"

using namespace std;

struct ComplexPolar {
    double magnitude;
    double phase;
};

class ImpedanceReader
{
public:
    ImpedanceReader(SystemState* state_,  AbstractRHXController* rhxController_);
    bool measureImpedances();
    bool saveImpedances();

private:
    SystemState* state;
    AbstractRHXController* rhxController;

    static double approximateSaturationVoltage(double actualZFreq, double highCutoff);
    static ComplexPolar factorOutParallelCapacitance(ComplexPolar impedance, double frequency, double parasiticCapacitance);
    ComplexPolar measureComplexAmplitude(const deque<RHXDataBlock*> &dataQueue, int stream, int chipChannel,
                                         double sampleRate, double frequency, int numPeriods) const;
    void applyNotchFilter(vector<double> &waveform, double fNotch, double bandwidth, double sampleRate) const;
    static ComplexPolar amplitudeOfFreqComponent(const vector<double> &waveform, int startIndex, int endIndex,
                                                 double sampleRate, double frequency);
    void runDemoImpedanceMeasurement();
};

#endif // IMPEDANCEREADER_H

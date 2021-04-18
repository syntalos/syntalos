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

#ifndef SOFTWAREREFERENCEPROCESSOR_H
#define SOFTWAREREFERENCEPROCESSOR_H

#include <cstdint>
#include <vector>
#include "signalsources.h"
#include "abstractrhxcontroller.h"
#include "rhxdatablock.h"

struct SignalWithSoftwareReference
{
    StreamChannelPair address;
    int referenceIndex;
};


class SoftwareReferenceProcessor
{
public:
    SoftwareReferenceProcessor(ControllerType type_, int numDataStreams_, int numSamples_);
    ~SoftwareReferenceProcessor();

    void updateReferenceInfo(const SignalSources* signalSources);
    void applySoftwareReferences(uint16_t* start);

private:
    ControllerType type;
    int numDataStreams;
    int numSamples;
    int dataFrameSizeInWords;
    int misoWordSize;

    // Reference signals consisting of a single channel.
    vector<SignalWithSoftwareReference> signalListSingleReference;
    vector<StreamChannelPair> singleReferenceList;
    vector<int*> singleReferenceData;

    // Reference signals consisting of an average of multiple channels.
    vector<SignalWithSoftwareReference> signalListMultiReference;
    vector<vector<StreamChannelPair> > multiReferenceList;
    vector<int*> multiReferenceData;

    int findSingleReference(StreamChannelPair singleRef, const vector<StreamChannelPair>& singleRefList) const;
    int findMultiReference(const vector<StreamChannelPair>& multiRef, const vector<vector<StreamChannelPair> >& multiRefList) const;
    void calculateReferenceSignals(const uint16_t* start);
    void readReferenceSignal(StreamChannelPair address, int* destination, const uint16_t* start);
    void addReferenceSignal(StreamChannelPair address, int* destination, const uint16_t* start);
    void subtractReferenceSignal(StreamChannelPair address, const int* refSignal, uint16_t* start);
    void deleteDataArrays();

};

#endif // SOFTWAREREFERENCEPROCESSOR_H

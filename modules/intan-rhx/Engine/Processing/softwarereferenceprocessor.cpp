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
#include "softwarereferenceprocessor.h"

using namespace std;

SoftwareReferenceProcessor::SoftwareReferenceProcessor(ControllerType type_, int numDataStreams_, int numSamples_) :
    type(type_),
    numDataStreams(numDataStreams_),
    numSamples(numSamples_)
{
    dataFrameSizeInWords = RHXDataBlock::dataBlockSizeInWords(type, numDataStreams) /
            RHXDataBlock::samplesPerDataBlock(type);
    misoWordSize = ((type == ControllerStimRecordUSB2) ? 2 : 1);
}

SoftwareReferenceProcessor::~SoftwareReferenceProcessor()
{
    deleteDataArrays();
}

void SoftwareReferenceProcessor::deleteDataArrays()
{
    for (int i = 0; i < (int) singleReferenceData.size(); ++i) {
        delete [] singleReferenceData[i];
    }
    for (int i = 0; i < (int) multiReferenceData.size(); ++i) {
        delete [] multiReferenceData[i];
    }
}

void SoftwareReferenceProcessor::updateReferenceInfo(const SignalSources* signalSources)
{
    // Clear reference info from last run.
    signalListSingleReference.clear();
    singleReferenceList.clear();

    signalListMultiReference.clear();
    for (int i = 0; i < (int) multiReferenceList.size(); ++i) {
        multiReferenceList[i].clear();
    }
    multiReferenceList.clear();

    deleteDataArrays();
    singleReferenceData.clear();
    multiReferenceData.clear();


    // Once ALL or PORT X reference list has been created, remember it and just use the index quickly the next time
    // it is encountered.  This saves the time of recreating it and comparing it.  A value of -1 indicates the
    // particular list has not yet been created, otherwise the value holds the index.
    int allRefIndexShortcut = -1;
    vector<int> portRefIndexShortcut(AbstractRHXController::maxNumSPIPorts(type), -1);

    // Populate with new reference info.
    for (int port = 0; port < signalSources->numPortGroups(); ++port) {
        SignalGroup* group = signalSources->portGroupByIndex(port);
        for (int i = 0; i < group->numChannels(AmplifierSignal); ++i) {
            Channel* channel = group->channelByIndex(i);
            QString refString = channel->getReference();
            if (refString.toLower() != "hardware") {
                SignalWithSoftwareReference signalWithRef;
                signalWithRef.address.stream = channel->getBoardStream();
                signalWithRef.address.channel = channel->getChipChannel();

                // Now, parse reference string from SignalSources.  Examples of reference strings:
                // "A-001", "A-001,A-002,A-003", "PORT A", "ALL"

                bool allRef = refString == "ALL";
                bool portRef = refString.left(4) == "PORT";
                int portNumber = -1;
                if (portRef) {
                    portNumber = (int) (refString.back().toLatin1() - 'A');
                }
                int numRefs = refString.count(QChar(',')) + 1;

                if ((numRefs > 1) || allRef || portRef) {
                    // Multi-channel average reference
                    vector<StreamChannelPair> refList;
                    bool shortcutFound = false;

                    if (allRef) {
                        // "ALL" (All enabled channels on all SPI ports.)
                        if (allRefIndexShortcut > -1) {
                            signalWithRef.referenceIndex = allRefIndexShortcut;
                            shortcutFound = true;
                        } else {
                            char maxPort = (type == ControllerRecordUSB3) ? 'H' : 'D';
                            for (char port = 'A'; port <= maxPort; ++port) {
                                QString portPrefix = QString(QChar(port)) + "-";
                                int maxChannelsPerPort = (type == ControllerStimRecordUSB2) ? 32 : 128;
                                for (int i = 0; i < maxChannelsPerPort; ++i) {
                                    Channel* refChannel = signalSources->channelByName(portPrefix +
                                                                                             QString("%1").arg(i, 3, 10, QChar('0')));
                                    if (refChannel) {
                                        if (refChannel->isEnabled()) {
                                            StreamChannelPair refAddress;
                                            refAddress.stream = refChannel->getBoardStream();
                                            refAddress.channel = refChannel->getChipChannel();
                                            refList.push_back(refAddress);
                                        }
                                    }
                                }
                            }
                        }
                    } else if (portRef) {
                        // "PORT X" (All enabled channels on SPI port X.)
                        if (portRefIndexShortcut[portNumber] > -1) {
                            signalWithRef.referenceIndex = portRefIndexShortcut[portNumber];
                            shortcutFound = true;
                        } else {
                            QString portPrefix = refString.right(1) + "-";
                            int maxChannelsPerPort = (type == ControllerStimRecordUSB2) ? 32 : 128;
                            for (int i = 0; i < maxChannelsPerPort; ++i) {
                                Channel* refChannel = signalSources->channelByName(portPrefix +
                                                                                         QString("%1").arg(i, 3, 10, QChar('0')));
                                if (refChannel) {
                                    if (refChannel->isEnabled()) {
                                        StreamChannelPair refAddress;
                                        refAddress.stream = refChannel->getBoardStream();
                                        refAddress.channel = refChannel->getChipChannel();
                                        refList.push_back(refAddress);
                                    }
                                }
                            }
                        }
                    } else {
                        // List of channels, e.g. "A-005,A-006,B-031"
                        for (int i = 0; i < numRefs; ++i) {
                            Channel* refChannel = signalSources->channelByName(refString.section(',', i, i));
                            if (!refChannel) {
                                cerr << "SoftwareReferenceProcessor: channel not found: " << refString.toStdString() << '\n';
                                return;
                            }
                            StreamChannelPair refAddress;
                            refAddress.stream = refChannel->getBoardStream();
                            refAddress.channel = refChannel->getChipChannel();
                            refList.push_back(refAddress);
                        }
                    }

                    if (!shortcutFound) {
                        std::sort(refList.begin(), refList.end());  // Sort list to facilitate quick comparison.

                        int foundIndex = findMultiReference(refList, multiReferenceList);
                        if (foundIndex == -1) {  // New multi-channel reference is not present in reference list.
                            signalWithRef.referenceIndex = (int) multiReferenceList.size();
                            multiReferenceList.push_back(refList);
                            multiReferenceData.push_back(new int [numSamples]);
                            if (allRef) {
                                allRefIndexShortcut = signalWithRef.referenceIndex;
                            } else if (portRef) {
                                portRefIndexShortcut[portNumber] = signalWithRef.referenceIndex;
                            }
                        } else {  // New multi-channel reference is already in reference list.
                            signalWithRef.referenceIndex = foundIndex;
                        }
                    }
                    signalListMultiReference.push_back(signalWithRef);
                } else {
                    // Single-channel reference, e.g. "A-031"
                    Channel* refChannel = signalSources->channelByName(refString);
                    if (!refChannel) {
                        cerr << "SoftwareReferenceProcessor: channel not found: " << refString.toStdString() << '\n';
                        return;
                    }
                    StreamChannelPair refAddress;
                    refAddress.stream = refChannel->getBoardStream();
                    refAddress.channel = refChannel->getChipChannel();

                    int foundIndex = findSingleReference(refAddress, singleReferenceList);
                    if (foundIndex == -1) {  // New reference is not present in reference list.
                        signalWithRef.referenceIndex = (int) singleReferenceList.size();
                        singleReferenceList.push_back(refAddress);
                        singleReferenceData.push_back(new int [numSamples]);
                    } else {  // New reference is already in reference list.
                        signalWithRef.referenceIndex = foundIndex;
                    }
                    signalListSingleReference.push_back(signalWithRef);
                }
            }
        }
    }
}

int SoftwareReferenceProcessor::findSingleReference(StreamChannelPair singleRef,
                                                    const vector<StreamChannelPair>& singleRefList) const
{
    for (int i = 0; i < (int) singleRefList.size(); ++i) {
        if (singleRef == singleRefList[i]) return i;
    }
    return -1;  // Reference not found in list.
}

int SoftwareReferenceProcessor::findMultiReference(const vector<StreamChannelPair>& multiRef,
                                                   const vector<vector<StreamChannelPair> >& multiRefList) const
{
    int length = (int) multiRef.size();
    for (int i = 0; i < (int) multiRefList.size(); ++i) {
        if (length == (int) multiRefList[i].size()) {
            bool identical = true;
            for (int j = 0; j < length; ++j) {
                if (multiRef[j] != multiRefList[i][j]) {
                    identical = false;
                    break;
                }
            }
            if (identical) return i;
        }
    }
    return -1;  // Reference not found in list.
}

void SoftwareReferenceProcessor::applySoftwareReferences(uint16_t* start)
{
    calculateReferenceSignals(start);

    for (int i = 0; i < (int) signalListSingleReference.size(); ++i) {
        const int* refSignal = singleReferenceData[signalListSingleReference[i].referenceIndex];
        subtractReferenceSignal(signalListSingleReference[i].address, refSignal, start);
    }

    for (int i = 0; i < (int) signalListMultiReference.size(); ++i) {
        const int* refSignal = multiReferenceData[signalListMultiReference[i].referenceIndex];
        subtractReferenceSignal(signalListMultiReference[i].address, refSignal, start);
    }
}

void SoftwareReferenceProcessor::calculateReferenceSignals(const uint16_t* start)
{
    for (int i = 0; i < (int) singleReferenceList.size(); ++i) {
        readReferenceSignal(singleReferenceList[i], singleReferenceData[i], start);
    }

    for (int i = 0; i < (int) multiReferenceList.size(); ++i) {
        readReferenceSignal(multiReferenceList[i][0], multiReferenceData[i], start);
        for (int j = 1; j < (int) multiReferenceList[i].size(); ++j) {
            addReferenceSignal(multiReferenceList[i][j], multiReferenceData[i], start);
        }
        double oneOverN = 1.0 / (double) multiReferenceList[i].size();
        for (int t = 0; t < numSamples; ++t) {
            multiReferenceData[i][t] = round(((double) multiReferenceData[i][t]) * oneOverN);  // Calculate average.
        }
    }
}

void SoftwareReferenceProcessor::readReferenceSignal(StreamChannelPair address, int* destination, const uint16_t* start)
{
    const uint16_t* pRead = start;
    int* pWrite = destination;

    pRead += 6; // Skip header and timestamp.
    pRead += misoWordSize * (numDataStreams * 3);  // Skip auxiliary channels.
    pRead += misoWordSize * ((numDataStreams * address.channel) + address.stream);   // Align with selected stream and channel.
    if (type == ControllerStimRecordUSB2) pRead++;  // Skip top 16 bits of 32-bit MISO word from RHS system.
    for (int i = 0; i < numSamples; ++i) {
        *pWrite = (((int) *pRead) - 32768);
        pWrite++;
        pRead += dataFrameSizeInWords;
    }
}

void SoftwareReferenceProcessor::addReferenceSignal(StreamChannelPair address, int* destination, const uint16_t* start)
{
    const uint16_t* pRead = start;
    int* pWrite = destination;

    pRead += 6; // Skip header and timestamp.
    pRead += misoWordSize * (numDataStreams * 3);  // Skip auxillary channels.
    pRead += misoWordSize * ((numDataStreams * address.channel) + address.stream);   // Align with selected stream and channel.
    if (type == ControllerStimRecordUSB2) pRead++;  // Skip top 16 bits of 32-bit MISO word from RHS system.
    for (int i = 0; i < numSamples; ++i) {
        *pWrite += (((int) *pRead) - 32768);  // Only difference between this function and readReferenceSignal() is this line.
        pWrite++;
        pRead += dataFrameSizeInWords;
    }
}

void SoftwareReferenceProcessor::subtractReferenceSignal(StreamChannelPair address, const int* refSignal, uint16_t* start)
{
    uint16_t* pSignal = start;

    pSignal += 6; // Skip header and timestamp.
    pSignal += misoWordSize * (numDataStreams * 3);  // Skip auxillary channels.
    pSignal += misoWordSize * ((numDataStreams * address.channel) + address.stream);   // Align with selected stream and channel.
    if (type == ControllerStimRecordUSB2) pSignal++;  // Skip top 16 bits of 32-bit MISO word from RHS system.
    for (int i = 0; i < numSamples; ++i) {
        int newVal = ((int) *pSignal) - *refSignal;
        newVal = max(newVal, 0);
        newVal = min(newVal, 65535);
        *pSignal = (uint16_t) newVal;
        pSignal += dataFrameSizeInWords;
        refSignal++;
    }
}

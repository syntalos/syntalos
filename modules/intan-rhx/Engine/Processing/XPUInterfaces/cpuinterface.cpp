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

#include "cpuinterface.h"

CPUInterface::CPUInterface(SystemState *state_, QObject *parent) :
    AbstractXPUInterface(state_, parent)
{
    updateFromState();
}

CPUInterface::~CPUInterface()
{
    if (allocated) {
        freeMemory();
    }
}

void CPUInterface::processDataBlock(uint16_t * data, uint16_t *lowChunk, uint16_t *wideChunk, uint16_t *highChunk,
                                    uint32_t *spikeChunk, uint8_t *spikeIDChunk)
{
    lock_guard<mutex> lockFilter(filterMutex);

    if (channels == 0)
        return;

    uint16_t* rawBlock = data;

    float samplePeriod = 1.0f / sampleRate;

    float notchB2 = filterParameters.notchParams.b2;
    float notchB1 = filterParameters.notchParams.b1;
    float notchB0 = filterParameters.notchParams.b0;
    float notchA2 = filterParameters.notchParams.a2;
    float notchA1 = filterParameters.notchParams.a1;

    float lowB2[4];
    float lowB1[4];
    float lowB0[4];
    float lowA2[4];
    float lowA1[4];

    for (uint8_t filterIndex = 0; filterIndex < 4; ++filterIndex) {
        lowB2[filterIndex] = filterParameters.lowParams[filterIndex].b2;
        lowB1[filterIndex] = filterParameters.lowParams[filterIndex].b1;
        lowB0[filterIndex] = filterParameters.lowParams[filterIndex].b0;
        lowA2[filterIndex] = filterParameters.lowParams[filterIndex].a2;
        lowA1[filterIndex] = filterParameters.lowParams[filterIndex].a1;
    }

    float highB2[4];
    float highB1[4];
    float highB0[4];
    float highA2[4];
    float highA1[4];

    for (uint8_t filterIndex = 0; filterIndex < 4; ++filterIndex) {
        highB2[filterIndex] = filterParameters.highParams[filterIndex].b2;
        highB1[filterIndex] = filterParameters.highParams[filterIndex].b1;
        highB0[filterIndex] = filterParameters.highParams[filterIndex].b0;
        highA2[filterIndex] = filterParameters.highParams[filterIndex].a2;
        highA1[filterIndex] = filterParameters.highParams[filterIndex].a1;
    }

    const unsigned int snippetsPerBlock = (int) ceil((double) ((double) FramesPerBlock / (double) SnippetSize) + 1.0);

    float inFloat[FramesPerBlock];
    float prevHighFloat[FramesPerBlock];
    float lowFloat[4][FramesPerBlock];
    float wideFloat[FramesPerBlock];
    float highFloat[4][FramesPerBlock];

    for (int a = 0; a < FramesPerBlock; ++a) {
        inFloat[a] = 0.0f;
        prevHighFloat[a] = 0.0f;
        wideFloat[a] = 0.0f;
        for (int b = 0; b < 4; ++b) {
            lowFloat[b][a] = 0.0f;
            highFloat[b][a] = 0.0f;
        }
    }

    uint32_t low2ndToLastIndex[4];
    uint32_t lowLastIndex[4];
    uint32_t high2ndToLastIndex[4];
    uint32_t highLastIndex[4];

    uint32_t outIndex;
    uint32_t s;

    float low2ndToLast[4];
    float lowLast[4];
    float high2ndToLast[4];
    float highLast[4];

    for (int channelIndex = 0; channelIndex < channels; channelIndex++) {
        uint32_t lastDataStart = channelIndex * 20;

        for (int i = 0; i < 4; ++i) {
            low2ndToLastIndex[i] = lastDataStart + i;
            lowLastIndex[i] = lastDataStart + 4 + i;
            high2ndToLastIndex[i] = lastDataStart + 8 + i;
            highLastIndex[i] = lastDataStart + 12 + i;
        }
        uint32_t in2ndToLastIndex = lastDataStart + 16;
        uint32_t inLastIndex = lastDataStart + 17;
        uint32_t wide2ndToLastIndex = lastDataStart + 18;
        uint32_t wideLastIndex = lastDataStart + 19;

        int32_t snippetIndex = 0;

        for (int i = 0; i < 4; ++i) {
            low2ndToLast[i] = prevLast2[low2ndToLastIndex[i]];
            lowLast[i] = prevLast2[lowLastIndex[i]];
            high2ndToLast[i] = prevLast2[high2ndToLastIndex[i]];
            highLast[i] = prevLast2[highLastIndex[i]];
        }

        float in2ndToLast = prevLast2[in2ndToLastIndex];
        float inLast = prevLast2[inLastIndex];
        float wide2ndToLast = prevLast2[wide2ndToLastIndex];
        float wideLast = prevLast2[wideLastIndex];
        float threshold = hoops[channelIndex].threshold;
        bool useHoops = (hoops[channelIndex].useHoops == 1) ? true : false;

        for (s = 0; s < snippetsPerBlock; ++s) {
            spikeChunk[s * channels + channelIndex] = 0;
            spikeIDChunk[s * channels + channelIndex] = 0;
        }

        for (s = 0; s < SnippetSize; ++s) {
            prevHighFloat[s] = (float) (0.195f * (((double)parsedPrevHigh[s * channels + channelIndex]) - 32768));
        }

        int32_t inIndexStream, inIndexChannel;
        if (type == ControllerRecordUSB2 || type == ControllerRecordUSB3) {
            inIndexStream = channelIndex / 32;
            inIndexChannel = channelIndex % 32;
        } else {
            inIndexStream = channelIndex / 16;
            inIndexChannel = channelIndex % 16;
        }

        // (0) Index this channel's input data from the rawBlock and convert it to float.
        for (int frame = 0; frame < FramesPerBlock; ++frame) {
            uint16_t acSample;
            if (type == ControllerStimRecordUSB2) {
                acSample = rawBlock[wordsPerFrame * frame + 6 + (numStreams * 3 * 2) +
                        (inIndexChannel * numStreams * 2) + (2 * inIndexStream + 1)];
            } else {
                acSample = rawBlock[wordsPerFrame * frame + 6 + (numStreams * 3) +
                        inIndexChannel * numStreams + inIndexStream];
            }
            inFloat[frame] = (float)(0.195f * (((double)acSample) - 32768));
        }

        int numLowFilterIterations = floor((float)(filterParameters.lowOrder - 1) / 2.0f) + 1;
        int numHighFilterIterations = floor((float)(filterParameters.highOrder - 1) / 2.0f) + 1;

        // s == 0 condition
        // (1) IIR notch filter into wideFloat

        wideFloat[0] = notchB2 * in2ndToLast + notchB1 * inLast + notchB0 * inFloat[0] - notchA2 * wide2ndToLast -
                notchA1 * wideLast;

        // (2) IIR Nth-order low-pass
        // 1st iteration: use wideFloat as input
        lowFloat[0][0] = lowB2[0] * wide2ndToLast + lowB1[0] * wideLast + lowB0[0] * wideFloat[0] -
                lowA2[0] * low2ndToLast[0] - lowA1[0] * lowLast[0];

        // All other iterations: use lowFloat[filterIndex - 1] as input.
        for (uint8_t filterIndex = 1; filterIndex < numLowFilterIterations; ++filterIndex) {
            lowFloat[filterIndex][0] = lowB2[filterIndex] * low2ndToLast[filterIndex - 1] +
                    lowB1[filterIndex] * lowLast[filterIndex - 1] +
                    lowB0[filterIndex] * lowFloat[filterIndex - 1][0] -
                    lowA2[filterIndex] * low2ndToLast[filterIndex] -
                    lowA1[filterIndex] * lowLast[filterIndex];
        }

        // (3) IIR Nth-order high-pass
        // 1st iteration: use wideFloat as input.

        highFloat[0][0] = highB2[0] * wide2ndToLast + highB1[0] * wideLast + highB0[0] * wideFloat[0] -
                highA2[0] * high2ndToLast[0] - highA1[0] * highLast[0];

        // All other iterations: use highFloat[filterIndex - 1] as input.
        for (uint8_t filterIndex = 1; filterIndex < numHighFilterIterations; ++filterIndex) {
            highFloat[filterIndex][0] = highB2[filterIndex] * high2ndToLast[filterIndex - 1] +
                    highB1[filterIndex] * highLast[filterIndex - 1] +
                    highB0[filterIndex] * highFloat[filterIndex - 1][0] -
                    highA2[filterIndex] * high2ndToLast[filterIndex] -
                    highA1[filterIndex] * highLast[filterIndex];
        }

        // s == 1 condition
        // (1) IIR notch filter into wideFloat
        wideFloat[1] = notchB2 * inLast + notchB1 * inFloat[0] + notchB0 * inFloat[1] - notchA2 * wideLast -
                notchA1 * wideFloat[0];

        // (2) IIR Nth-order low-pass
        // 1st iteration: use wideFloat as input.
        lowFloat[0][1] = lowB2[0] * wideLast + lowB1[0] * wideFloat[0] + lowB0[0] * wideFloat[1] -
                lowA2[0] * lowLast[0] - lowA1[0] * lowFloat[0][0];

        // All other iterations: use lowFloat[filterIndex - 1] as input.
        for (uint8_t filterIndex = 1; filterIndex < numLowFilterIterations; ++filterIndex) {
            lowFloat[filterIndex][1] = lowB2[filterIndex] * lowLast[filterIndex - 1] +
                    lowB1[filterIndex] * lowFloat[filterIndex - 1][0] +
                    lowB0[filterIndex] * lowFloat[filterIndex - 1][1] -
                    lowA2[filterIndex] * lowLast[filterIndex] -
                    lowA1[filterIndex] * lowFloat[filterIndex][0];
        }

        // (3) IIR Nth-order high-pass
        // 1st iteration: use wideFloat as input.
        highFloat[0][1] = highB2[0] * wideLast + highB1[0] * wideFloat[0] + highB0[0] * wideFloat[1] -
                highA2[0] * highLast[0] - highA1[0] * highFloat[0][0];

        // All other iterations: use highFloat[filterIndex - 1] as input.
        for (uint8_t filterIndex = 1; filterIndex < numHighFilterIterations; ++filterIndex) {
            highFloat[filterIndex][1] = highB2[filterIndex] * highLast[filterIndex - 1] +
                    highB1[filterIndex] * highFloat[filterIndex - 1][0] +
                    highB0[filterIndex] * highFloat[filterIndex - 1][1] -
                    highA2[filterIndex] * highLast[filterIndex] -
                    highA1[filterIndex] * highFloat[filterIndex][0];
        }

        for (s = 2; s < FramesPerBlock; ++s) {

            // (1) IIR notch filter into wideFloat
            wideFloat[s] = notchB2 * inFloat[s - 2] + notchB1 * inFloat[s - 1] + notchB0 * inFloat[s] -
                    notchA2 * wideFloat[s - 2] - notchA1 * wideFloat[s - 1];

            // (2) IIR Nth-order low-pass
            // 1st iteration: use wideFloat as input.
            lowFloat[0][s] = lowB2[0] * wideFloat[s - 2] + lowB1[0] * wideFloat[s - 1] + lowB0[0] * wideFloat[s] -
                    lowA2[0] * lowFloat[0][s - 2] - lowA1[0] * lowFloat[0][s - 1];

            // All other iterations: use lowfloat[filterIndex - 1] as input.
            for (uint8_t filterIndex = 1; filterIndex < numLowFilterIterations; ++filterIndex) {
                lowFloat[filterIndex][s] = lowB2[filterIndex] * lowFloat[filterIndex - 1][s - 2] +
                        lowB1[filterIndex] * lowFloat[filterIndex - 1][s - 1] +
                        lowB0[filterIndex] * lowFloat[filterIndex - 1][s] -
                        lowA2[filterIndex] * lowFloat[filterIndex][s - 2] -
                        lowA1[filterIndex] * lowFloat[filterIndex][s - 1];
            }

            // (3) IIR Nth-order high-pass
            // 1st iteration: use wideFloat as input.
            highFloat[0][s] = highB2[0] * wideFloat[s - 2] + highB1[0] * wideFloat[s - 1] + highB0[0] * wideFloat[s] -
                    highA2[0] * highFloat[0][s - 2] - highA1[0] * highFloat[0][s - 1];

            // All other iterations: use highFloat[filterIndex - 1] as input.
            for (uint8_t filterIndex = 1; filterIndex < numHighFilterIterations; ++filterIndex) {
                highFloat[filterIndex][s] = highB2[filterIndex] * highFloat[filterIndex - 1][s - 2] +
                        highB1[filterIndex] * highFloat[filterIndex - 1][s - 1] +
                        highB0[filterIndex] * highFloat[filterIndex - 1][s] -
                        highA2[filterIndex] * highFloat[filterIndex][s - 2] -
                        highA1[filterIndex] * highFloat[filterIndex][s - 1];
            }
        }

        float filteredHigh[FramesPerBlock];
        float filteredLow[FramesPerBlock];

        for (int s = 0; s < FramesPerBlock; ++s) {
            filteredHigh[s] = highFloat[numHighFilterIterations - 1][s];
            filteredLow[s] = lowFloat[numLowFilterIterations - 1][s];
        }

        // Across this block, look for any valid rectangle and look back to this block and the previous block to
        // determine valid t0. Add earliest t0 for each rectangle to 'spike' output.
        snippetIndex = 0;

        // Start with threshS = startSearchPos[channelIndex]. This is 0 unless the previous data block ended with a spike.
        // In that case, threshS is a non-zero offset to avoid double-detecting a snippet.
        for (int threshS = startSearchPos[channelIndex] - SnippetSize; threshS < FramesPerBlock - SnippetSize; ++threshS) {

            startSearchPos[channelIndex] = 0;

            // Look to both this data block and the previous block to determine if the threshold was surpassed.
            bool surpassed = false;

            if (threshold >= 0) {  // If threshold was positive:
                if (threshS >= 0) {
                    if (filteredHigh[threshS] > threshold) surpassed = true;
                } else {
                    if (prevHighFloat[SnippetSize + threshS] > threshold) surpassed = true;
                }
            } else {  // If threshold was negative:
                if (threshS >= 0) {
                    if (filteredHigh[threshS] < threshold) surpassed = true;
                } else {
                    if (prevHighFloat[SnippetSize + threshS] < threshold) surpassed = true;
                }
            }

            // Threshold was surpassed.
            if (surpassed) {
                // For ease of understanding, move the samples from [threshS, threshS + SnippetSize] to [0, snippetSize].
                float thisSnippet[FramesPerBlock];
                for (int i = 0; i < SnippetSize; ++i) {
                    int thisS = threshS + i;
                    if (thisS < 0) {
                        thisSnippet[i] = prevHighFloat[SnippetSize + thisS];
                    } else {
                        thisSnippet[i] = filteredHigh[thisS];
                    }
                }

                // Create a struct to hold this channel's hoop info.
                ChannelDetectionStruct detection;
                for (int unit = 0; unit < 4; ++unit) {
                    for (int hoop = 0; hoop < 4; ++hoop) {
                        detection.units[unit].hoops[hoop] = false;
                    }
                }
                detection.maxSurpassed = false;

                // If spikeMaxEnabled is true, then see if any samples in this snippet surpass spikeMax. If they do,
                // then mark detetion.maxSurpassed as true and save which sample.
                if (globalParameters.spikeMaxEnabled) {
                    for (int i = 0; i < SnippetSize; ++i) {
                        if (globalParameters.spikeMax >= 0 && thisSnippet[i] >= globalParameters.spikeMax) {
                            detection.maxSurpassed = true;
                            break;
                        }
                        if (globalParameters.spikeMax < 0 && thisSnippet[i] <= globalParameters.spikeMax) {
                            detection.maxSurpassed = true;
                            break;
                        }
                    }
                }

                // If useHoops is true, then go through all units populating detection.units[unit].hoops[hoop].
                if (useHoops) {
                    // Go through all units.
                    for (int unit = 0; unit < 4; ++unit) {

                        // If this unit has no valid hoops (all tA values are -1.0f), then this is an inactive unit which
                        // should be treated as having no intersect; just go on to the next unit.
                        if (hoops[channelIndex].unitHoops[unit].hoopInfo[0].tA == -1.0f &&
                                hoops[channelIndex].unitHoops[unit].hoopInfo[1].tA == -1.0f &&
                                hoops[channelIndex].unitHoops[unit].hoopInfo[2].tA == -1.0f &&
                                hoops[channelIndex].unitHoops[unit].hoopInfo[3].tA == -1.0f) {
                            continue;
                        }

                        // Go through all hoops.
                        for (int hoop = 0; hoop < 4; ++hoop) {
                            HoopInfoStruct thisHoop = hoops[channelIndex].unitHoops[unit].hoopInfo[hoop];

                            // If this hoop info is invalid (tA is -1.0f), then this is an inactive hoop, which by default passes.
                            // Set true and continue. If all hoops are inactive, then we would have already passed on to the next
                            // unit without flaggin an intersect.
                            if (thisHoop.tA == -1.0f) {
                                detection.units[unit].hoops[hoop] = true;
                                continue;
                            }

                            float tA = thisHoop.tA;
                            float yA = thisHoop.yA;
                            float tB = thisHoop.tB;
                            float yB = thisHoop.yB;

                            // In range [tA, tB], does line segment from (t1, y1) to (t2, y2) intersect user-defined hoop?
                            // If so, mark hoop as jumped through by setting intersect to true.
                            bool intersect = false;

                            // Round tA down and tB up to the nearest discrete sample.
                            int sA = floor(sampleRate * tA);
                            int sB = ceil(sampleRate * tB);

                            // Special case: vertical hoop
                            if (sA == sB) {
                                float y1Data = thisSnippet[sA];
                                if (yB > yA) {
                                    intersect = (y1Data < yB && y1Data > yA);
                                } else {
                                    intersect = (y1Data > yB && y1Data < yA);
                                }
                            } else {
                                // General case: non-vertical hoop
                                float slope = (yB - yA) / (tB - tA);
                                // Examine every two adjacent samples in the range [sA, sB] and determine if they intersect the hoop.
                                for (int s1 = sA; s1 < sB - 1; ++s1) {
                                    int s2 = s1 + 1;
                                    float y1Data = thisSnippet[s1];
                                    float y2Data = thisSnippet[s2];

                                    // Convert s1 and s2 to the float t1 and t2 domain.
                                    float t1 = ((float) s1) * samplePeriod;
                                    float t2 = ((float) s2) * samplePeriod;

                                    float y1Hoop = yA + slope * (t1 - tA);
                                    float y2Hoop = yA + slope * (t2 - tA);

                                    // If the data transitions from below to above the hoop (or vice versa), then an intersection
                                    // occurred. Break the loop for checking this hoop.
                                    if ((y1Data >= y1Hoop && y2Data <= y2Hoop) ||
                                            (y1Data <= y1Hoop && y2Data >= y2Hoop)) {
                                        intersect = true;
                                        break;
                                    }

                                    // Otherwise, keep looking over the course of this hoop.
                                }
                            }

                            if (intersect) {  // If intersect occurred, mark this hoop as jumped through.
                                detection.units[unit].hoops[hoop] = true;
                            } else {
                                // If not, exit the hoop loop (default value is false, so effectively setting it false)
                                // and move on to the next unit.
                                break;
                            }
                        } // End loop across all hoops.
                    } // End loop across all units.
                } else {  // If useHoops is false, then just populate detection.units[unit].hoops[hoop] with true.
                    for (int unit = 0; unit < 4; ++unit) {
                        for (int hoop = 0; hoop < 4; ++hoop) {
                            detection.units[unit].hoops[hoop] = true;
                        }
                    }
                }

                uchar ID = 0;
                // Determine correct ID


                if (detection.maxSurpassed) {  // If max has been detected, ID is 128 for max surpassing.
                    ID = 128;
                } else if (true) {
                //} else if (!useHoops) {  // If useHoops is false, ID is 1 to signify threshold crossing.
                    ID = 1;
                } else {  // If useHoops is true, ID is either (a) an active unit or (b) just a threshold crossing.
                    // (a) If a unit is active, ID is either 1, 2, 4, or 8 for the unit.
                    for (uint8_t unit = 0; unit < 4; ++unit) {
                        if (detection.units[unit].hoops[0] && detection.units[unit].hoops[1] &&
                                detection.units[unit].hoops[2] && detection.units[unit].hoops[3]) {
//                            ID = (uint8_t) pow(2.0f, (float) unit);
                            ID = 1u << unit;  // faster implementation of 2^unit
                            break;
                        }
                    }

                    // (b) If no unit is active, ID is 64 to signify threshold crossing.
                    if (ID == 0) ID = 64;
                }

                // Populate spike with timestamp
                // Extract the timestamp of the first frame in this data block
                uint32_t timestampLSW = rawBlock[4]; // Timestamp is always the bytes 8-11 of the datablock (16-bit words 4-5).
                uint32_t timestampMSW = rawBlock[5];
                uint32_t timestamp = (timestampMSW << 16) + timestampLSW;

                // Add threshS to this timestamp to index right (for positive threshS) or left (for negative threshS).
                timestamp += threshS;

                // Write spike detection at this timestamp.
                spikeChunk[snippetIndex * channels + channelIndex] = timestamp;

                // Populate spikeID with correct ID.
                spikeIDChunk[snippetIndex * channels + channelIndex] = ID;

                // Advance by SnippetSize samples since activity up until then will already be flagged as a spike.
                threshS += SnippetSize;

                // Continue detection, preparing for another spike in this block to take the next snippetIndex;
                ++snippetIndex;

                // If the end of this spike snippet is encroaching on the territory of the next data block
                // (with SnippetSize of the next block's start), populate startSearchPos[channel] with
                // the end position of this snippet. This allows the next block to start at a later sample,
                // so there's no risk of double-counting a spike.
                if (threshS > FramesPerBlock - SnippetSize) {
                    startSearchPos[channelIndex] = threshS - (FramesPerBlock - SnippetSize);
                }
            }
        }

        for (s = 0; s < FramesPerBlock; ++s) {
            // Boundary check to make sure result will fit in a uint16_t.
            if (wideFloat[s] > 6389.0f) wideFloat[s] = 6389.0f;
            else if (wideFloat[s] < -6389.0f) wideFloat[s] = -6389.0f;

            if (filteredLow[s] > 6389.0f) filteredLow[s] = 6389.0f;
            else if (filteredLow[s] < -6389.0f) filteredLow[s] = -6389.0f;

            if (filteredHigh[s] > 6389.0f) filteredHigh[s] = 6389.0f;
            else if (filteredHigh[s] < -6389.0f) filteredHigh[s] = -6389.0f;

            // (4) Convert outputs to uint16_t.
            outIndex = s * channels + channelIndex;

            lowChunk[outIndex] = (uint16_t) round((filteredLow[s] / 0.195f) + 32768);
            wideChunk[outIndex] = (uint16_t) round((wideFloat[s] / 0.195f) + 32768);
            highChunk[outIndex] = (uint16_t) round((filteredHigh[s] / 0.195f) + 32768);
        }

        // Update 'prevLast2' array with this block's samples.
        for (int filterIndex = 0; filterIndex < 4; ++filterIndex) {
            prevLast2[low2ndToLastIndex[filterIndex]] = lowFloat[filterIndex][FramesPerBlock - 2];
            prevLast2[lowLastIndex[filterIndex]] = lowFloat[filterIndex][FramesPerBlock - 1];
            prevLast2[high2ndToLastIndex[filterIndex]] = highFloat[filterIndex][FramesPerBlock - 2];
            prevLast2[highLastIndex[filterIndex]] = highFloat[filterIndex][FramesPerBlock - 1];
        }

        prevLast2[in2ndToLastIndex] = inFloat[FramesPerBlock - 2];
        prevLast2[inLastIndex] = inFloat[FramesPerBlock - 1];
        prevLast2[wide2ndToLastIndex] = wideFloat[FramesPerBlock - 2];
        prevLast2[wideLastIndex] = wideFloat[FramesPerBlock - 1];
    }

    // Set the last 50 samples of high to parsedPrevHigh so that they can be used in the next data block
//    memcpy(parsedPrevHigh, &highChunk[(FramesPerBlock - SnippetSize) * channels], SnippetSize * sizeof(uint16_t));
    parsedPrevHigh = &highChunk[(FramesPerBlock - SnippetSize) * channels];
}

void CPUInterface::freeMemory()
{
    delete [] spike;
    delete [] spikeIDs;
    delete [] prevLast2;
    delete [] startSearchPos;
    delete [] hoops;
    delete [] parsedPrevHighOriginal;

    allocated = false;
}

void CPUInterface::speedTest()
{
    state->cpuInfo.diagnosticTime = -1.0f;
    state->cpuInfo.name = "CPU";
    state->cpuInfo.rank = -1;
    state->cpuInfo.used = false;

    setupMemory();
    runDiagnostic(0);

    cleanupMemory();
}

bool CPUInterface::setupMemory()
{
    if (!allocated) initializeMemory();

    return true; // With CPU, really no need to check return value
}

bool CPUInterface::cleanupMemory()
{
    if (allocated) freeMemory();

    return true; // With CPU, really no need to check return value
}

void CPUInterface::initializeMemory()
{
    spike = new uint32_t[totalSnippetsPerBlock * DiagnosticBlocks];
    spikeIDs = new uint8_t[totalSnippetsPerBlock * DiagnosticBlocks];
    for (int s = 0; s < totalSnippetsPerBlock * DiagnosticBlocks; ++s) {
        spike[s] = -1;
        spikeIDs[s] = 0;
    }

    prevLast2 = new float[channels * 20];
    resetPrev();

    startSearchPos = new uint16_t[channels];
    for (int c = 0; c < channels; ++c) {
        startSearchPos[c] = 0;
    }

    hoops = new ChannelHoopsStruct[channels];

    // Simple spike detection initialization w/o using hoops
    for (int c = 0; c < channels; ++c) {
        for (int unit = 0; unit < 4; ++unit) {
            for (int hoop = 0; hoop < 4; ++hoop) {
                hoops[c].unitHoops[unit].hoopInfo[hoop].tA = 0.0F;
                hoops[c].unitHoops[unit].hoopInfo[hoop].yA = 0.0F;
                hoops[c].unitHoops[unit].hoopInfo[hoop].tB = 0.0F;
                hoops[c].unitHoops[unit].hoopInfo[hoop].yB = 0.0F;
            }
        }
        hoops[c].threshold = -70.0F;
        //hoops[c].useHoops = 0; // 1 = true, 0 = false
    }

    // Prep before loop.
    parsedPrevHighOriginal = new uint16_t[SnippetSize * channels];
    for (int s = 0; s < SnippetSize * channels; ++s) {
        parsedPrevHighOriginal[s] = 32768;
    }
    parsedPrevHigh = parsedPrevHighOriginal;

    inputIndex = 0;
    outputIndex = 0;
    spikeIndex = 0;

    allocated = true;
}

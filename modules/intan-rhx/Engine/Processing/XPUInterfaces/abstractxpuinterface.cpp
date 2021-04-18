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

#include "xpucontroller.h"

AbstractXPUInterface::AbstractXPUInterface(SystemState* state_, QObject *parent) :
    QObject(parent),
    allocated(false),
    state(state_),
    type(state->getControllerTypeEnum()),
    channels(state->signalSources->numAmplifierChannels())
{
    globalParameters.wordsPerFrame = 0;
    globalParameters.type = ControllerRecordUSB2;
    globalParameters.numStreams = 0;
    globalParameters.snippetSize = 0;
    globalParameters.sampleRate = 0.0f;
    globalParameters.spikeMax = 0.0f;
    globalParameters.spikeMaxEnabled = 0;

    if (type == ControllerRecordUSB3) {
        channelsPerStream = 32;
    } else if (type == ControllerRecordUSB2) {
        channelsPerStream = 32;
    } else {
        channelsPerStream = 16;
    }
    sampleRate = state->sampleRate->getNumericValue();
}

void AbstractXPUInterface::resetPrev()
{
    for (int i = 0; i < channels * 20; i++) {
        prevLast2[i] = 0.0f;
    }
}

void AbstractXPUInterface::updateMemory()
{
    // Important - don't do anything if not allocated. This indicates that the diagnostic test
    // hasn't run yet, meaning that state->usedXPUIndex() will give an incorrect value, so that
    // setupMemory() will cause a crash for any GPU.
    if (allocated) {
        cleanupMemory();
        setupMemory();
    }
}

void AbstractXPUInterface::updateNumStreams(int numStreams_)
{
    // Set channels to the new value.
    channels = state->signalSources->numAmplifierChannels();

    // Set numStreams to the new value.
    numStreams = numStreams_;

    // Recalculate all values related to numStreams from constructor.
    if (type == ControllerRecordUSB3) {
        wordsPerFrame = ((35 * numStreams) + 16 + (numStreams % 4));
    } else if (type == ControllerRecordUSB2) {
        wordsPerFrame = ((36 * numStreams) + 16);
    } else {
        wordsPerFrame = ((44 * numStreams) + 24);
    }

    wordsPerBlock = wordsPerFrame * FramesPerBlock;
    totalSnippetsPerBlock = numStreams * channelsPerStream * SnippetsPerBlock;
    totalSamplesPerBlock = channels * FramesPerBlock;

    // Update global parameters to reflect new number.
    globalParameters.numStreams = numStreams;
    globalParameters.wordsPerFrame = wordsPerFrame;

    updateConstChars();

    updateMemory();
}

void AbstractXPUInterface::runDiagnostic(int XPUIndex)
{ 
    uint16_t* dataOriginal = new uint16_t[DiagnosticBlocks * wordsPerBlock];
    uint16_t* lowOriginal = new uint16_t[DiagnosticBlocks * FramesPerBlock * channels];
    uint16_t* wideOriginal = new uint16_t[DiagnosticBlocks * FramesPerBlock * channels];
    uint16_t* highOriginal = new uint16_t[DiagnosticBlocks * FramesPerBlock * channels];

    for (int i = 0; i < DiagnosticBlocks * wordsPerBlock; ++i) {
        dataOriginal[i] = 0;
    }

    for (int i = 0; i < DiagnosticBlocks * FramesPerBlock * channels; ++i) {
        lowOriginal[i] = 0;
        wideOriginal[i] = 0;
        highOriginal[i] = 0;
    }

    uint16_t* data_ = dataOriginal;
    uint16_t* low_ = lowOriginal;
    uint16_t* wide_ = wideOriginal;
    uint16_t* high_ = highOriginal;
    uint32_t* spike_ = spike;
    uint8_t* spikeIDs_ = spikeIDs;

    auto start = std::chrono::steady_clock::now();
    for (int block = 0; block < DiagnosticBlocks; block++) {
        processDataBlock(data_, low_, wide_, high_, spike_, spikeIDs_);
        data_ += wordsPerBlock;
        low_ += FramesPerBlock * channels;
        wide_ += FramesPerBlock * channels;
        high_ += FramesPerBlock * channels;
        spike_ += SnippetsPerBlock * channels;
        spikeIDs_ += SnippetsPerBlock * channels;
    }
    auto end = std::chrono::steady_clock::now();

    delete [] dataOriginal;
    delete [] lowOriginal;
    delete [] wideOriginal;
    delete [] highOriginal;

    float elapsedMs = (float) std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    if (XPUIndex == 0) state->cpuInfo.diagnosticTime = elapsedMs;
    else state->gpuList[XPUIndex - 1].diagnosticTime = elapsedMs;
    qDebug() << "Elapsed time: " << elapsedMs << " ms\n";
}

void AbstractXPUInterface::updateFilters()
{
    calculateNotchConstants();
    calculateLowConstants();
    calculateHighConstants();
    updateFilterConstArray();
    updateConstChars();
    updateConstFloats();
}

void AbstractXPUInterface::calculateNotchConstants()
{
    double f = state->notchFreq->getNumericValue();
    double bandwidth = NotchBandwidth;
    double sampleRate = state->sampleRate->getNumericValue();
    // Hacky way to disable notch filter
    if (f == -1.0) {
        filterParameters.notchParams.b0 = 1.0;
        filterParameters.notchParams.b1 = 0.0;
        filterParameters.notchParams.b2 = 0.0;
        filterParameters.notchParams.a1 = 0.0;
        filterParameters.notchParams.a2 = 0.0;
    } else {
        SecondOrderNotchFilter notch(f, bandwidth, sampleRate);
        filterParameters.notchParams.b0 = notch.getB0();
        filterParameters.notchParams.b1 = notch.getB1();
        filterParameters.notchParams.b2 = notch.getB2();
        filterParameters.notchParams.a1 = notch.getA1();
        filterParameters.notchParams.a2 = notch.getA2();
    }
}

void AbstractXPUInterface::calculateLowConstants()
{
    int order = state->lowOrder->getValue();
    QString typeString = state->lowType->getValue();
    double f = state->lowSWCutoffFreq->getValue();
    double sampleRate = state->sampleRate->getNumericValue();

    filterParameters.lowOrder = order;
    HighOrderFilter *filter;
    if (typeString.toLower() == "bessel") filter = new BesselLowpassFilter(order, f, sampleRate);
    else filter = new ButterworthLowpassFilter(order, f, sampleRate);

    vector<BiquadFilter> filters = filter->getFilters();

    for (int i = 0; i < (int) filters.size(); ++i) {
        filterParameters.lowParams[i].b0 = filters[i].getB0();
        filterParameters.lowParams[i].b1 = filters[i].getB1();
        filterParameters.lowParams[i].b2 = filters[i].getB2();
        filterParameters.lowParams[i].a1 = filters[i].getA1();
        filterParameters.lowParams[i].a2 = filters[i].getA2();
    }
    for (int i = (int) filters.size(); i < 4; ++i) {
        filterParameters.lowParams[i].b0 = 0.0f;
        filterParameters.lowParams[i].b1 = 0.0f;
        filterParameters.lowParams[i].b2 = 0.0f;
        filterParameters.lowParams[i].a1 = 0.0f;
        filterParameters.lowParams[i].a2 = 0.0f;
    }

    delete filter;
}

void AbstractXPUInterface::calculateHighConstants()
{
    int order = state->highOrder->getValue();
    QString typeString = state->highType->getValue();
    double f = state->highSWCutoffFreq->getValue();
    double sampleRate = state->sampleRate->getNumericValue();

    filterParameters.highOrder = order;
    HighOrderFilter *filter;
    if (typeString.toLower() == "bessel") filter = new BesselHighpassFilter(order, f, sampleRate);
    else filter = new ButterworthHighpassFilter(order, f, sampleRate);

    vector<BiquadFilter> filters = filter->getFilters();

    for (int i = 0; i < (int) filters.size(); ++i) {
        filterParameters.highParams[i].b0 = filters[i].getB0();
        filterParameters.highParams[i].b1 = filters[i].getB1();
        filterParameters.highParams[i].b2 = filters[i].getB2();
        filterParameters.highParams[i].a1 = filters[i].getA1();
        filterParameters.highParams[i].a2 = filters[i].getA2();
    }
    for (int i = (int) filters.size(); i < 4; ++i) {
        filterParameters.highParams[i].b0 = 0.0f;
        filterParameters.highParams[i].b1 = 0.0f;
        filterParameters.highParams[i].b2 = 0.0f;
        filterParameters.highParams[i].a1 = 0.0f;
        filterParameters.highParams[i].a2 = 0.0f;
    }

    delete filter;
}

void AbstractXPUInterface::updateFromState()
{
    lock_guard<mutex> lockFilter(filterMutex);

    // If any filter params have changed, recalculate them for notch, low, and high.
    updateFilters();

    // If any AmplifierSignal spikeThresholds have changed, update value in 'hoops' to inform XPU
    vector<string> ampChannelNames = state->signalSources->amplifierChannelsNameList();
    for (auto name : ampChannelNames) {
        int channelIndex = state->getSerialIndex(QString::fromStdString(name));
        Channel* thisChannel = state->signalSources->channelByName(QString::fromStdString(name));
        hoops[channelIndex].threshold = thisChannel->getSpikeThreshold();
    }

    // If spikeMax or suppressionEnabled have changed, update them.
    if (globalParameters.spikeMax != state->suppressionThreshold->getValue())
        globalParameters.spikeMax = state->suppressionThreshold->getValue();

    if ((globalParameters.spikeMaxEnabled == 1 && !state->suppressionEnabled->getValue()) ||
            (globalParameters.spikeMaxEnabled == 0 && state->suppressionEnabled->getValue()))
        globalParameters.spikeMaxEnabled = (state->suppressionEnabled->getValue() ? 1 : 0);
    // Ideally would be bool, but bool can't be passed to OpenCL. 1 = true, 0 = false.

    // Update GPU arrays (to substitute structs) with new values from hoops
    updateHoopsVariables();
}

// Default implementation - do nothing (should only be reimplemented by GPUInterface)
void AbstractXPUInterface::updateHoopsVariables()
{
}

// Default impementation - do nothing (should only be reimplemented by GPUInterface)
void AbstractXPUInterface::updateFilterConstArray()
{
}

// Default implementation - do nothing (should only be reimplemented by GPUInterface)
void AbstractXPUInterface::updateConstChars()
{
}

// Default implementation - do nothing (should only be reimplemented by GPUInterface)
void AbstractXPUInterface::updateConstFloats()
{
}

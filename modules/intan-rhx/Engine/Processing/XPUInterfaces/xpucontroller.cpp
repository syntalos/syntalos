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

#include "xpucontroller.h"

XPUController::XPUController(SystemState *state_, bool useOpenCL_, QObject *parent) :
    QObject(parent),
    useOpenCL(useOpenCL_),
    state(state_),
    usedXPUIndex(-1)
{
    state->writeToLog("Entered XPUController ctor");
    connect(state, SIGNAL(stateChanged()), this, SLOT(updateFromState()));

    gpuInterface = new GPUInterface(state, this);
    state->writeToLog("Created GPUInterface");
    cpuInterface = new CPUInterface(state, this);
    state->writeToLog("Created CPUInterface");

    activeInterface = cpuInterface;
}

XPUController::~XPUController()
{
    delete gpuInterface;
    delete cpuInterface;
}

void XPUController::resetPrev()
{
    activeInterface->resetPrev();
}

void XPUController::processDataBlock(uint16_t *data, uint16_t *lowChunk, uint16_t *wideChunk,
                                          uint16_t *highChunk, uint32_t *spikeChunk, uint8_t *spikeIDChunk)
{
    activeInterface->processDataBlock(data, lowChunk, wideChunk, highChunk, spikeChunk, spikeIDChunk);
}

void XPUController::updateNumStreams(int numStreams)
{
    cpuInterface->updateNumStreams(numStreams);
    gpuInterface->updateNumStreams(numStreams);
}

void XPUController::runDiagnostic()
{

    state->gpuList.clear();

    cpuInterface->speedTest();
    if (useOpenCL) {
        gpuInterface->speedTest();
    }
    compare();
}

void XPUController::compare()
{
    // Compare times of each CPU and each entry of gpuList, and update their ranking.
    float prevBestTime = -1.0f;
    int rankIndex = 1;

    int bestIndex = -1;

    while (rankIndex <= state->gpuList.size() + 1) {
        float bestTime = std::numeric_limits<float>::max();
        if (state->cpuInfo.diagnosticTime < bestTime && state->cpuInfo.diagnosticTime > prevBestTime) {
            bestTime = state->cpuInfo.diagnosticTime;
            bestIndex = 0;
        }
        for (int j = 0; j < state->gpuList.size(); j++) {
            if (state->gpuList[j].diagnosticTime < bestTime && state->gpuList[j].diagnosticTime > prevBestTime) {
                bestTime = state->gpuList[j].diagnosticTime;
                bestIndex = j + 1;
            }
        }
        if (bestIndex == 0) {
            state->cpuInfo.rank = rankIndex++;
            if (state->cpuInfo.rank == 1) {
                state->cpuInfo.used = true;
            }
            prevBestTime = state->cpuInfo.diagnosticTime;
        } else {
            state->gpuList[bestIndex - 1].rank = rankIndex++;
            if (state->gpuList[bestIndex - 1].rank == 1) {
                state->gpuList[bestIndex - 1].used = true;
            }
            prevBestTime = state->gpuList[bestIndex - 1].diagnosticTime;
        }
    }

    updateFromState();
}

void XPUController::updateFromState()
{
    // If used XPU has changed in state, then reflect that here.
    if (state->usedXPUIndex() != usedXPUIndex) {
        activeInterface->cleanupMemory();
        usedXPUIndex = state->usedXPUIndex();
        if (usedXPUIndex == 0) {
            activeInterface = cpuInterface;
        } else {
            activeInterface = gpuInterface;
        }
        activeInterface->setupMemory();
    }
    activeInterface->updateFromState();
}

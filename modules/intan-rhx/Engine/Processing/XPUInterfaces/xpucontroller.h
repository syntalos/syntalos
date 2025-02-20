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

#ifndef XPUCONTROLLER_H
#define XPUCONTROLLER_H
#include <QObject>

#include "systemstate.h"
#include "signalsources.h"
#include "cpuinterface.h"
#include "gpuinterface.h"

class XPUController  : public QObject
{
    Q_OBJECT
public:
    explicit XPUController(SystemState* state_, bool useOpenCL_, QObject *parent = nullptr);
    ~XPUController();

    void resetPrev();
    void processDataBlock(uint16_t* data, uint16_t* lowChunk, uint16_t* wideChunk,
                          uint16_t* highChunk, uint32_t* spikeChunk, uint8_t* spikeIDChunk);
    void updateNumStreams(int numStreams);
    void runDiagnostic();

private slots:
    void updateFromState();

private:
    void compare();

    bool useOpenCL;

    SystemState* state;
    CPUInterface *cpuInterface;
    GPUInterface *gpuInterface;
    AbstractXPUInterface* activeInterface;
    int usedXPUIndex;
};

#endif // XPUCONTROLLER_H

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

#ifndef GPUINTERFACE_H
#define GPUINTERFACE_H

#include <QMessageBox>

#include "abstractxpuinterface.h"

class GPUInterface : public AbstractXPUInterface
{
    Q_OBJECT
public:
    explicit GPUInterface(SystemState* state_, QObject *parent = nullptr);
    ~GPUInterface();

    // Called within class in runDiagnostic(), and externally in waveformprocessorthread
    void processDataBlock(uint16_t* data, uint16_t* lowChunk, uint16_t* wideChunk,
                          uint16_t* highChunk, uint32_t* spikeChunk, uint8_t* spikeIDChunk) override;
    bool setupMemory() override;
    bool cleanupMemory() override;
    void speedTest() override;

private:
    bool findPlatformDevices();
    void initializeKernelMemory();
    bool createKernel(int devIndex);
    void freeKernelMemory();
    void gpuErrorMessage(const QString& errorMessage);

    cl_platform_id* platformIds;
    cl_device_id* deviceIds;
    cl_int ret;
    cl_uint numPlatforms;
    cl_uint numDevices;
    cl_bool deviceAvailable;
    cl_device_id id;
    cl_context context;
    cl_command_queue commandQueue;
    cl_program program;
    cl_kernel kernel;

    cl_mem globalParametersHandle;
    cl_mem filterParametersHandle;
    cl_mem gpuHoopsHandle;
    cl_mem gpuDatablockBuffHandle;
    cl_mem gpuPrevLast2BuffHandle;
    cl_mem gpuPrevHighHandle;
    cl_mem gpuLowBuffHandle;
    cl_mem gpuWideBuffHandle;
    cl_mem gpuHighBuffHandle;
    cl_mem gpuSpikeBuffHandle;
    cl_mem gpuSpikeIDsHandle;
    cl_mem gpuStartSearchPosHandle;
};

#endif // GPUINTERFACE_H

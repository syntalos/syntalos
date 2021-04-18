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

#include "gpuinterface.h"

GPUInterface::GPUInterface(SystemState *state_, QObject *parent) :
    AbstractXPUInterface(state_, parent)
{
    updateFromState();
}

GPUInterface::~GPUInterface()
{
    if (allocated) {
        freeKernelMemory();
    }
}

void GPUInterface::processDataBlock(uint16_t *data, uint16_t *lowChunk, uint16_t *wideChunk, uint16_t *highChunk, uint32_t *spikeChunk, uint8_t *spikeIDChunk)
{
    lock_guard<mutex> lockFilter(filterMutex);

    if (channels == 0)
        return;

    // Load all inputs
    ret = clEnqueueWriteBuffer(commandQueue, globalParametersHandle, CL_TRUE, 0, sizeof(GlobalParamStruct), &globalParameters, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS) qDebug() << "Error A0";

    ret = clEnqueueWriteBuffer(commandQueue, filterParametersHandle, CL_TRUE, 0, sizeof(FilterParamStruct), &filterParameters, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS) qDebug() << "Error A1";

    ret = clEnqueueWriteBuffer(commandQueue, gpuHoopsHandle, CL_TRUE, 0, channels * sizeof(ChannelHoopsStruct), hoops, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS) qDebug() << "Error A2";

    ret = clEnqueueWriteBuffer(commandQueue, gpuDatablockBuffHandle, CL_TRUE, 0, wordsPerBlock * sizeof(uint16_t), data, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS) qDebug() << "Error A3";

    ret = clEnqueueWriteBuffer(commandQueue, gpuPrevLast2BuffHandle, CL_TRUE, 0, channels * 20 * sizeof(float), prevLast2, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS) qDebug() << "Error A4";

    ret = clEnqueueWriteBuffer(commandQueue, gpuPrevHighHandle, CL_TRUE, 0, SnippetSize * channels * sizeof(uint16_t), parsedPrevHigh, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS) qDebug() << "Error A5";

    ret = clEnqueueWriteBuffer(commandQueue, gpuStartSearchPosHandle, CL_TRUE, 0, channels * sizeof(uint16_t), startSearchPos, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS) qDebug() << "Error A7";

    // Execute kernel
    size_t globalItemSize = channels;
    ret = clEnqueueNDRangeKernel(commandQueue, kernel, 1, nullptr, &globalItemSize, nullptr, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS) qDebug() << "clEnqueueNDRangeKernel() failed. Ret: " << ret;

    // Read all outputs
    ret = clEnqueueReadBuffer(commandQueue, gpuPrevLast2BuffHandle, CL_TRUE, 0, channels * 20 * sizeof(float), prevLast2, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS) qDebug() << "Error C0";

    ret = clEnqueueReadBuffer(commandQueue, gpuStartSearchPosHandle, CL_TRUE, 0, channels * sizeof(uint16_t), startSearchPos, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS) qDebug() << "Error C1";

    ret = clEnqueueReadBuffer(commandQueue, gpuLowBuffHandle, CL_TRUE, 0, FramesPerBlock * channels * sizeof(uint16_t), lowChunk, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS) qDebug() << "Error C2";

    ret = clEnqueueReadBuffer(commandQueue, gpuWideBuffHandle, CL_TRUE, 0, FramesPerBlock * channels * sizeof(uint16_t), wideChunk, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS) qDebug() << "Error C3";

    ret = clEnqueueReadBuffer(commandQueue, gpuHighBuffHandle, CL_TRUE, 0, FramesPerBlock * channels * sizeof(uint16_t), highChunk, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS) qDebug() << "Error C4";

    ret = clEnqueueReadBuffer(commandQueue, gpuSpikeBuffHandle, CL_TRUE, 0, channels * SnippetsPerBlock * sizeof(uint32_t), spikeChunk, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS) qDebug() << "Error C5";

    ret = clEnqueueReadBuffer(commandQueue, gpuSpikeIDsHandle, CL_TRUE, 0, channels * SnippetsPerBlock * sizeof(uint8_t), spikeIDChunk, 0, nullptr, nullptr);
    if (ret != CL_SUCCESS) qDebug() << "Error C6";

    // Set the last 50 samples of high to parsedPrevHigh so that they can be used in the next data block.
    parsedPrevHigh = &highChunk[(FramesPerBlock - SnippetSize) * channels];
}

void GPUInterface::speedTest()
{
    state->writeToLog("Beginning of speedTest()");
    if (!findPlatformDevices()) {
        state->writeToLog("Platforms not found... exiting early");
        return;
    }
    state->writeToLog("About to enter dev for loop");
    for (int dev = 1; dev <= state->gpuList.size(); ++dev) {
        if (!createKernel(dev)) {
            state->writeToLog("Creating kernel failed... continuing");
            continue;
        }
        state->writeToLog("About to initializeKernelMemory()");
        initializeKernelMemory();
        state->writeToLog("About to runDiagnostic()");
        runDiagnostic(dev);
        state->writeToLog("About to cleanupMemory()");
        cleanupMemory();
    }
    state->writeToLog("End of speedTest()");
}

void GPUInterface::initializeKernelMemory()
{
    state->writeToLog("Beginning of initializeKernelMemory");

    hoops = new ChannelHoopsStruct[channels];

    globalParametersHandle = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(GlobalParamStruct), nullptr, &ret);
    if (ret != CL_SUCCESS) qDebug() << "Error I0";

    filterParametersHandle = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(FilterParamStruct), nullptr, &ret);
    if (ret != CL_SUCCESS) qDebug() << "Error I1";

    gpuHoopsHandle = clCreateBuffer(context, CL_MEM_READ_ONLY, channels * sizeof(ChannelHoopsStruct), nullptr, &ret);
    if (ret != CL_SUCCESS) qDebug() << "Error I2";

    gpuDatablockBuffHandle = clCreateBuffer(context, CL_MEM_READ_ONLY, wordsPerBlock * sizeof(uint16_t), nullptr, &ret);
    if (ret != CL_SUCCESS) qDebug() << "Error I3";

    gpuPrevLast2BuffHandle = clCreateBuffer(context, CL_MEM_READ_WRITE, channels * 20 * sizeof(float), nullptr, &ret);
    if (ret != CL_SUCCESS) qDebug() << "Error I4";

    gpuPrevHighHandle = clCreateBuffer(context, CL_MEM_READ_ONLY, SnippetSize * channels * sizeof(uint16_t), nullptr, &ret);
    if (ret != CL_SUCCESS) qDebug() << "Error I5";

    gpuLowBuffHandle = clCreateBuffer(context, CL_MEM_WRITE_ONLY, channels * FramesPerBlock * sizeof(uint16_t), nullptr, &ret);
    if (ret != CL_SUCCESS) qDebug() << "Error I6";

    gpuWideBuffHandle = clCreateBuffer(context, CL_MEM_WRITE_ONLY, channels * FramesPerBlock * sizeof(uint16_t), nullptr, &ret);
    if (ret != CL_SUCCESS) qDebug() << "Error I7";

    gpuHighBuffHandle = clCreateBuffer(context, CL_MEM_WRITE_ONLY, channels * FramesPerBlock * sizeof(uint16_t), nullptr, &ret);
    if (ret != CL_SUCCESS) qDebug() << "Error I8";

    gpuSpikeBuffHandle = clCreateBuffer(context, CL_MEM_WRITE_ONLY, totalSnippetsPerBlock * sizeof(uint32_t), nullptr, &ret);
    if (ret != CL_SUCCESS) qDebug() << "Error I9";

    gpuSpikeIDsHandle = clCreateBuffer(context, CL_MEM_WRITE_ONLY, totalSnippetsPerBlock * sizeof(uint8_t), nullptr, &ret);
    if (ret != CL_SUCCESS) qDebug() << "Error I10";

    gpuStartSearchPosHandle = clCreateBuffer(context, CL_MEM_READ_WRITE, channels * sizeof(uint16_t), nullptr, &ret);
    if (ret != CL_SUCCESS) qDebug() << "Error I11";

    state->writeToLog("Buffers initialized");

    // Initialize sources and sinks.
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

//    // Default no-detection initialization
//    for (int c = 0; c < channels; ++c) {
//        for (int unit = 0; unit < 4; ++unit) {
//            for (int hoop = 0; hoop < 4; ++hoop) {
//                gpuHoops[c].unitHoops[unit].hoopInfo[hoop].tA = -1.0F;
//                gpuHoops[c].unitHoops[unit].hoopInfo[hoop].yA = 10000.0F;
//                gpuHoops[c].unitHoops[unit].hoopInfo[hoop].tB = -1.0F;
//                gpuHoops[c].unitHoops[unit].hoopInfo[hoop].yB = 10000.0F;
//            }
//        }
//        gpuHoops[c].threshold = 10000.0F;
//        gpuHoops[c].useHoops = 0; // 1 = true, 0 = false
//    }


    // Simple spike detection initialization w/o using hoops
    for (int c = 0; c < channels; ++c) {
        for (int unit = 0; unit < 4; ++unit) {
            for (int hoop = 0; hoop < 4; ++hoop) {
                hoops[c].unitHoops[unit].hoopInfo[hoop].tA = 0.0f;
                hoops[c].unitHoops[unit].hoopInfo[hoop].yA = 0.0f;
                hoops[c].unitHoops[unit].hoopInfo[hoop].tB = 0.0f;
                hoops[c].unitHoops[unit].hoopInfo[hoop].yB = 0.0f;
            }
        }
        hoops[c].threshold = -70.0F;
        //hoops[c].useHoops = 0;  // 1 = true, 0 = false
    }

//    // Simple spike detection initialization: 70 uV threshold, single unit with single horizontal 70 uV hoop between 100 us and 1 ms
//    for (int c = 0; c < channels; ++c) {
//        for (int unit = 0; unit < 4; ++unit) {
//            for (int hoop = 0; hoop < 4; ++hoop) {
//                gpuHoops[c].unitHoops[unit].hoopInfo[hoop].tA = 0.0001f;
//                gpuHoops[c].unitHoops[unit].hoopInfo[hoop].yA = 70.0f;
//                gpuHoops[c].unitHoops[unit].hoopInfo[hoop].tB = 0.001f;
//                gpuHoops[c].unitHoops[unit].hoopInfo[hoop].yB = 70.0f;
//            }
//        }
//        gpuHoops[c].threshold = -70.0f;
//        gpuHoops[c].useHoops = 1; // 1 = true, 0 = false
//    }

    state->writeToLog("Sources and sinks initialized");

    // Set kernel args.
    ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&globalParametersHandle);
    if (ret != CL_SUCCESS) qDebug() << "J0";

    ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&filterParametersHandle);
    if (ret != CL_SUCCESS) qDebug() << "J1";

    ret = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void*)&gpuHoopsHandle);
    if (ret != CL_SUCCESS) qDebug() << "J2";

    ret = clSetKernelArg(kernel, 3, sizeof(cl_mem), (void*)&gpuDatablockBuffHandle);
    if (ret != CL_SUCCESS) qDebug() << "J3";

    ret = clSetKernelArg(kernel, 4, sizeof(cl_mem), (void*)&gpuPrevLast2BuffHandle);
    if (ret != CL_SUCCESS) qDebug() << "J4";

    ret = clSetKernelArg(kernel, 5, sizeof(cl_mem), (void*)&gpuPrevHighHandle);
    if (ret != CL_SUCCESS) qDebug() << "J5";

    ret = clSetKernelArg(kernel, 6, sizeof(cl_mem), (void*)&gpuLowBuffHandle);
    if (ret != CL_SUCCESS) qDebug() << "J6";

    ret = clSetKernelArg(kernel, 7, sizeof(cl_mem), (void*)&gpuWideBuffHandle);
    if (ret != CL_SUCCESS) qDebug() << "J7";

    ret = clSetKernelArg(kernel, 8, sizeof(cl_mem), (void*)&gpuHighBuffHandle);
    if (ret != CL_SUCCESS) qDebug( )<< "J8";

    ret = clSetKernelArg(kernel, 9, sizeof(cl_mem), (void*)&gpuSpikeBuffHandle);
    if (ret != CL_SUCCESS) qDebug() << "J9";

    ret = clSetKernelArg(kernel, 10, sizeof(cl_mem), (void*)&gpuSpikeIDsHandle);
    if (ret != CL_SUCCESS) qDebug() << "J10";

    ret = clSetKernelArg(kernel, 11, sizeof(cl_mem), (void*)&gpuStartSearchPosHandle);
    if (ret != CL_SUCCESS) qDebug() << "J11";

    // Populate global parameters.
    globalParameters.wordsPerFrame = wordsPerFrame;
    if (type == ControllerRecordUSB2) globalParameters.type = 0;
    else if (type == ControllerRecordUSB3) globalParameters.type = 1;
    else globalParameters.type = 2;
    globalParameters.numStreams = numStreams;
    globalParameters.snippetSize = 50;
    globalParameters.sampleRate = sampleRate;
    globalParameters.spikeMax = 100.0f;
    globalParameters.spikeMaxEnabled = 1;
    state->writeToLog("Global parameters populated");

    // Prep before loop.
    parsedPrevHighOriginal = new uint16_t[SnippetSize * channels];
    for (int s = 0; s < SnippetSize * channels; ++s) {
        parsedPrevHighOriginal[s] = 32768;
    }
    parsedPrevHigh = parsedPrevHighOriginal;
    inputIndex = 0, outputIndex = 0, spikeIndex = 0;
    allocated = true;

    state->writeToLog("Prep before loop complete. End of initializeKernelMemory()");
}

bool GPUInterface::setupMemory()
{
    state->writeToLog("Beginning of setupMemory()");
    if (!allocated) {
        state->writeToLog("Not allocated... creating kernel and initializing memory");
        if (!createKernel(state->usedXPUIndex())) return false;
        initializeKernelMemory(); // Consider having this give a return value as well...
        return true;
    }
    state->writeToLog("Already allocated... returning false");
    return false;
}

bool GPUInterface::cleanupMemory()
{
    state->writeToLog("Beginning of cleanupMemory()");
    if (allocated) {
        state->writeToLog("Allocated... freeing kernel memory");
        freeKernelMemory(); // Consider having this give a return value as well...
        state->writeToLog("Freeing kernel memory completed");
    }
    return true;
}


void GPUInterface::freeKernelMemory()
{
    state->writeToLog("Beginning of freeKernelMemory()");
    delete [] spike;
    delete [] spikeIDs;
    delete [] prevLast2;
    delete [] startSearchPos;
    delete [] hoops;

    delete [] parsedPrevHighOriginal;
    state->writeToLog("Deleted arrays");

    if (channels == 0) {
        allocated = false;
        return;
    }
    state->writeToLog("Would have returned if 0 channels");

    ret = clFlush(commandQueue);
    if (ret != CL_SUCCESS) state->writeToLog("Error flushing command queue. Ret: " + QString::number(ret));

    ret = clFinish(commandQueue);
    if (ret != CL_SUCCESS) state->writeToLog("Error finishing command queue. Ret: " + QString::number(ret));

    ret = clReleaseKernel(kernel);
    if (ret != CL_SUCCESS) state->writeToLog("Error releasing kernel. Ret: " + QString::number(ret));

    ret = clReleaseProgram(program);
    if (ret != CL_SUCCESS) state->writeToLog("Error releasing program. Ret: " + QString::number(ret));

    clReleaseMemObject(globalParametersHandle);
    clReleaseMemObject(filterParametersHandle);
    clReleaseMemObject(gpuHoopsHandle);
    clReleaseMemObject(gpuDatablockBuffHandle);
    clReleaseMemObject(gpuPrevLast2BuffHandle);
    clReleaseMemObject(gpuPrevHighHandle);
    clReleaseMemObject(gpuLowBuffHandle);
    clReleaseMemObject(gpuWideBuffHandle);
    clReleaseMemObject(gpuHighBuffHandle);
    clReleaseMemObject(gpuSpikeBuffHandle);
    clReleaseMemObject(gpuSpikeIDsHandle);
    clReleaseMemObject(gpuStartSearchPosHandle);

    clReleaseCommandQueue(commandQueue);
    clReleaseContext(context);
    state->writeToLog("Finished CL releases");

    allocated = false;
}


// Populate state->gpuList based on all available platforms and devices.
bool GPUInterface::findPlatformDevices()
{
    state->writeToLog("Beginning of findPlatformDevices()");
    platformIds = new cl_platform_id[10];
    deviceIds = new cl_device_id[10];

    state->writeToLog("Allocated for platform and deviceIds");

    ret = clGetPlatformIDs(10, platformIds, &numPlatforms);
    if (ret != CL_SUCCESS) {
        gpuErrorMessage(tr("Error finding OpenCL platforms."));
        return false;
    }
    state->writeToLog("Finished clGetPlatformIDs(). About to enter platformIndex for loop");

    for (unsigned int platformIndex = 0; platformIndex < numPlatforms; ++platformIndex) {

        // Platform profile
        char* returnedString = nullptr;
        size_t size;
        clGetPlatformInfo(platformIds[platformIndex], CL_PLATFORM_PROFILE, (size_t) nullptr, nullptr, &size); // get size of profile char array
        returnedString = new char[size];
        clGetPlatformInfo(platformIds[platformIndex], CL_PLATFORM_PROFILE, size, returnedString, nullptr); // get profile char array
        //qDebug() << "Profile: " << returnedString;
        state->writeToLog("Profile: " + QString(returnedString));
        delete [] returnedString;

        // Platform version
        clGetPlatformInfo(platformIds[platformIndex], CL_PLATFORM_VERSION, (size_t) nullptr, nullptr, &size); // get size of version char array
        returnedString = new char[size];
        clGetPlatformInfo(platformIds[platformIndex], CL_PLATFORM_VERSION, size, returnedString, nullptr); // get version char array
        //qDebug() << "Version: " << returnedString;
        state->writeToLog("Version: " + QString(returnedString));
        delete [] returnedString;

        // Platform name
        clGetPlatformInfo(platformIds[platformIndex], CL_PLATFORM_NAME, (size_t) nullptr, nullptr, &size); // get size of name char array
        returnedString = new char[size];
        clGetPlatformInfo(platformIds[platformIndex], CL_PLATFORM_NAME, size, returnedString, nullptr); // get name char array
        //qDebug() << "Name: " << returnedString;
        state->writeToLog("Name: " + QString(returnedString));
        delete [] returnedString;

        // Platform vendor
        clGetPlatformInfo(platformIds[platformIndex], CL_PLATFORM_VENDOR, (size_t) nullptr, nullptr, &size); // get size of vendor char array
        returnedString = new char[size];
        clGetPlatformInfo(platformIds[platformIndex], CL_PLATFORM_VENDOR, size, returnedString, nullptr); // get vendor char array
        //qDebug() << "Vendor: " << returnedString;
        state->writeToLog("Vendor: " + QString(returnedString));
        delete [] returnedString;

        // Platform extensions
        clGetPlatformInfo(platformIds[platformIndex], CL_PLATFORM_EXTENSIONS, (size_t) nullptr, nullptr, &size); // get size of extensions char array
        returnedString = new char[size];
        clGetPlatformInfo(platformIds[platformIndex], CL_PLATFORM_EXTENSIONS, size, returnedString, nullptr); // get extensions char array
        //qDebug() << "Extensions: " << returnedString;
        state->writeToLog("Extensions: " + QString(returnedString));
        delete [] returnedString;


        state->writeToLog("About to call clGetDeviceIDs");
        // Number of devices on this platform (maximum 10)
        ret = clGetDeviceIDs(platformIds[platformIndex], CL_DEVICE_TYPE_DEFAULT, 10, deviceIds, &numDevices);
        if (ret != CL_SUCCESS) {
            gpuErrorMessage(tr("Error getting OpenCL devices"));
            return false;
        }
        state->writeToLog("Finished clGetDeviceIDs");

        state->writeToLog("About to enter device for loop");
        for (unsigned int device = 0; device < numDevices; ++device) {
            GPUInfo thisGPU;
            thisGPU.deviceId = deviceIds[device];
            thisGPU.platformId = platformIds[platformIndex];
            thisGPU.diagnosticTime = -1.0F;
            thisGPU.rank = -1;
            thisGPU.used = false;
            state->gpuList.append(thisGPU);
        }
        state->writeToLog("Finished device for loop");
    }

    delete [] platformIds;
    delete [] deviceIds;

    return true;
}

bool GPUInterface::createKernel(int devIndex)
{
    state->writeToLog("Beginning of createKernel()");
    int gpuIndex = devIndex - 1;
    size_t size;

    // Name of each device on this platform
    char* returnedString = nullptr;
    clGetDeviceInfo(state->gpuList[gpuIndex].deviceId, CL_DEVICE_NAME, (size_t) nullptr, nullptr, &size); // Get size of name char array.
    returnedString = new char[size];
    clGetDeviceInfo(state->gpuList[gpuIndex].deviceId, CL_DEVICE_NAME, size, returnedString, nullptr); // Get name char array.
    state->gpuList[gpuIndex].name = QString::fromLocal8Bit(returnedString);
    qDebug() << "Device  name: " << returnedString;
    state->writeToLog("Device name: " + QString(returnedString));
    delete [] returnedString;

    // Extensions available on this device
    clGetDeviceInfo(state->gpuList[gpuIndex].deviceId, CL_DEVICE_EXTENSIONS, (size_t) nullptr, nullptr, &size); // Get size of extensions char array.
    returnedString = new char[size];
    clGetDeviceInfo(state->gpuList[gpuIndex].deviceId, CL_DEVICE_EXTENSIONS, size, returnedString, nullptr); // Get extensions char array.
    //qDebug() << "Device extension: " << returnedString;
    state->writeToLog("Device extensions: " + QString(returnedString));
    delete [] returnedString;

    // Vendor of this device
    clGetDeviceInfo(state->gpuList[gpuIndex].deviceId, CL_DEVICE_VENDOR, (size_t) nullptr, nullptr, &size); // Get size of vendor char array.
    returnedString = new char[size];
    clGetDeviceInfo(state->gpuList[gpuIndex].deviceId, CL_DEVICE_VENDOR, size, returnedString, nullptr); // Get vendor char array.
    //qDebug() << "Device vendor: " << returnedString;
    state->writeToLog("Device vendor: " + QString(returnedString));
    delete [] returnedString;

    // Vendor ID of this device
    cl_uint returnedUint = 0;
    clGetDeviceInfo(state->gpuList[gpuIndex].deviceId, CL_DEVICE_VENDOR_ID, sizeof(returnedUint), &returnedUint, nullptr); // Get vendor id.
    //qDebug() << "Device vendor id: " << returnedUint;
    state->writeToLog("Device vendor id: " + QString::number(returnedUint));

    // Version of this device
    clGetDeviceInfo(state->gpuList[gpuIndex].deviceId, CL_DEVICE_VERSION, (size_t) nullptr, nullptr, &size); // Get size of version char array.
    returnedString = new char[size];
    clGetDeviceInfo(state->gpuList[gpuIndex].deviceId, CL_DEVICE_VERSION, size, returnedString, nullptr); // Get version char array.
    //qDebug() << "Device version: " << returnedString;
    state->writeToLog("Device version: " + QString(returnedString));
    delete [] returnedString;

    // Version of this driver
    clGetDeviceInfo(state->gpuList[gpuIndex].deviceId, CL_DRIVER_VERSION, (size_t) nullptr, nullptr, &size); // Get size of driver version char array.
    returnedString = new char[size];
    clGetDeviceInfo(state->gpuList[gpuIndex].deviceId, CL_DRIVER_VERSION, size, returnedString, nullptr); // Get driver version char array.
    //qDebug() << "Device driver version: " << returnedString;
    state->writeToLog("Device driver version: " + QString(returnedString));
    delete [] returnedString;

    cl_bool deviceCompilerAvailable = CL_FALSE;
    // Get deviceCompilerAvailable.
    clGetDeviceInfo(state->gpuList[gpuIndex].deviceId, CL_DEVICE_COMPILER_AVAILABLE, sizeof(deviceCompilerAvailable), &deviceCompilerAvailable, nullptr);
    QString deviceCompilerAvailableString = deviceCompilerAvailable == CL_TRUE ? "True" : "False";
    //qDebug() << "Device compiler available: " << deviceCompilerAvailableString;
    state->writeToLog("Device compiler available: " + deviceCompilerAvailableString);

    cl_ulong returnedUlong = 0;
    // Get local mem size
    clGetDeviceInfo(state->gpuList[gpuIndex].deviceId, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(returnedUlong), &returnedUlong, nullptr);
    //qDebug() << "Device local mem size: " << returnedUlong;
    state->writeToLog("Device local mem size: " + QString::number(returnedUlong));

    // Get global mem size
    clGetDeviceInfo(state->gpuList[gpuIndex].deviceId, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(returnedUlong), &returnedUlong, nullptr);
    //qDebug() << "Device global mem size: " << returnedUlong;
    state->writeToLog("Device global mem size: " + QString::number(returnedUlong));

    size_t maxParameterSize;
    // Get max parameter size
    clGetDeviceInfo(state->gpuList[gpuIndex].deviceId, CL_DEVICE_MAX_PARAMETER_SIZE, sizeof(maxParameterSize), &maxParameterSize, nullptr);
    //qDebug() << "Device max parameter size: " << maxParameterSize;
    state->writeToLog("Device max parameter size: " + QString::number(maxParameterSize));

    size_t maxWorkGroupSize;
    // Get max work group size
    clGetDeviceInfo(state->gpuList[gpuIndex].deviceId, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(maxWorkGroupSize), &maxWorkGroupSize, nullptr);
    //qDebug() << "Device max work group size: " << maxWorkGroupSize;
    state->writeToLog("Device max work group size: " + QString::number(maxWorkGroupSize));

    // Get max constant buffer size
    clGetDeviceInfo(state->gpuList[gpuIndex].deviceId, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, sizeof(returnedUlong), &returnedUlong, nullptr);
    //qDebug() << "Device max constant buffer size: " << returnedUlong;
    state->writeToLog("Device max constant buffer size: " + QString::number(returnedUlong));

    deviceAvailable = CL_FALSE;
    // Get deviceAvailable.
    clGetDeviceInfo(state->gpuList[gpuIndex].deviceId, CL_DEVICE_AVAILABLE, sizeof(deviceAvailable), &deviceAvailable, nullptr);
    QString deviceAvailableString = deviceAvailable == CL_TRUE ? "True" : "False";
    //qDebug() << "Device available: " << deviceAvailableString;
    state->writeToLog("Device available: " + deviceAvailableString);
    id = state->gpuList[gpuIndex].deviceId;

    state->writeToLog("About to call clCreateContext()");
    // Create an OpenCL context.
    context = clCreateContext(nullptr, 1, &id, nullptr, nullptr, &ret);
    if (ret != CL_SUCCESS) {
        gpuErrorMessage(tr("Error creating OpenCL context."));
        return false;
    }
    state->writeToLog("Completed clCreateContext()");

    state->writeToLog("About to call clCreateCommandQueue");
    // Create command queue.
    commandQueue = clCreateCommandQueue(context, id, 0, &ret);
    if (ret != CL_SUCCESS) {
        state->writeToLog("Failure creating OpenCL commandqueue. Ret: " + QString::number(ret));
        gpuErrorMessage("Error creating OpenCL commandqueue. Returned error code: " + QString::number(ret));
        return false;
    }
    state->writeToLog("Completed clCreateCommandQueue");

    QString filename(qApp->applicationDirPath() + "/kernel.cl");
    QFile *file = new QFile(filename);
    if (!file->open(QIODevice::ReadOnly)) {
        gpuErrorMessage(tr("Cannot load kernel file to read. Is kernel.cl present?"));
        return false;
    }

    char* sourceStr = new char[MAX_SOURCE_SIZE];
    size_t sourceSize = file->read(sourceStr, MAX_SOURCE_SIZE);
    file->close();

    delete file;

    state->writeToLog("About to call clCreateProgramWithSource()");

    // Create a program from the kernel source.
    program = clCreateProgramWithSource(context, 1, (const char**) &sourceStr, (const size_t*) &sourceSize, &ret);
    if (ret != CL_SUCCESS) {
        gpuErrorMessage(tr("Error creating OpenCL program."));
        return false;
    }
    delete [] sourceStr;
    state->writeToLog("Called clCreateProgramWithSource()");

    state->writeToLog("About to call clBuildProgram()");
    // Build the program.
    //char options[] = "-cl-opt-disable";
    //ret = clBuildProgram(program, 1, &id, options, nullptr, nullptr);
    ret = clBuildProgram(program, 1, &id, nullptr, nullptr, nullptr);
    if (ret != CL_SUCCESS) {

        // Change for users to get more info on OpenCL build failure
        size_t len = 0;
        ret = clGetProgramBuildInfo(program, id, CL_PROGRAM_BUILD_LOG, 0, nullptr, &len);
        char *buffer = new char[len];
        ret = clGetProgramBuildInfo(program, id, CL_PROGRAM_BUILD_LOG, len, buffer, nullptr);
        if (ret != CL_SUCCESS) {
            gpuErrorMessage(tr("Error building OpenCL program... build log inaccessible"));
        }
        else {
            QString errorString(buffer);
            gpuErrorMessage("Error building OpenCL program... contents of build log:\n" + errorString);
        }
        delete [] buffer;
        return false;
    }
    state->writeToLog("Finished clBuildProgram(), reported success");

    // Create the OpenCL kernel.
    state->writeToLog("About to call clCreateKernel()");
    kernel = clCreateKernel(program, "process_block", &ret);
    if (ret != CL_SUCCESS) gpuErrorMessage(tr("Error creating OpenCL kernel."));
    state->writeToLog("Finished clCreateKernel(). End of createKernel()");
    return true;
}

void GPUInterface::gpuErrorMessage(const QString& errorMessage)
{
    QMessageBox::warning(nullptr, tr("OpenCL Error"), errorMessage);
}

/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2024 Open Ephys

    ------------------------------------------------------------------

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "AcqBoardONI.h"

#include <iostream>
#include <sstream>
#include <string>

#define INIT_STEP 64
//#define DEBUG_EMULATE_HEADSTAGES 8
//#define DEBUG_EMULATE_64CH

AcqBoardONI::AcqBoardONI() : AcquisitionBoard(),
                             chipRegisters (30000.0f)
{
    boardType = BoardType::ONI;

    impedanceMeter = std::make_unique<ImpedanceMeterONI> (this);

    evalBoard = std::make_unique<Rhd2000ONIBoard>();

    memset (auxBuffer, 0, sizeof (auxBuffer));
    memset (auxSamples, 0, sizeof (auxSamples));

    MAX_NUM_DATA_STREAMS = evalBoard->MAX_NUM_DATA_STREAMS;
    MAX_NUM_HEADSTAGES = MAX_NUM_DATA_STREAMS / 2;

    int maxNumHeadstages = 8;

    for (int i = 0; i < maxNumHeadstages; i++)
        headstages.add (new HeadstageONI (static_cast<Rhd2000ONIBoard::BoardDataSource> (i), maxNumHeadstages));

    for (int k = 0; k < 8; k++)
    {
        dacChannelsToUpdate.add (true);
        dacStream.add (0);
        setDACTriggerThreshold (k, 65534);
        dacChannels.add (0);
        dacThresholds.set (k, 0);
    }

    for (int i = 0; i < NUMBER_OF_PORTS; i++)
    {
        hasBNO[i] = false;
        bnoBuffers.add (nullptr);
    }

    isTransmitting = false;
}

AcqBoardONI::~AcqBoardONI()
{
    LOGD ("RHD2000 interface destroyed.");
}

bool AcqBoardONI::checkBoardMem() const
{
    Rhd2000ONIBoard::BoardMemState memState;
    memState = evalBoard->getBoardMemState();

    if (memState == Rhd2000ONIBoard::BOARDMEM_INIT)
    {
        LOGD ("Memory is still initializing, wait before retrying this operation.");
        CoreServices::sendStatusMessage ("Acquisition Board: Please wait, memory initializing.");
        return false;
    }
    else if (memState == Rhd2000ONIBoard::BOARDMEM_ERR)
    {
        LOGE ("On-board memory error. Try closing the GUI, un-plugging the board from power and usb and plugging everything again. If the problem persists please contact support.");
        CoreServices::sendStatusMessage ("Acquisition Board: Memory error.");
        return false;
    }
    else if (memState == Rhd2000ONIBoard::BOARDMEM_INVALID)
    {
        LOGE ("Invalid memory operation.");
        CoreServices::sendStatusMessage ("Acquisition Board: Invalid memory operation.");
        return false;
    }
    else if (memState == Rhd2000ONIBoard::BOARDMEM_OK)
        return true;

    LOGD ("Unknown memory state. Board returned ", memState);
    return false;
}

bool AcqBoardONI::detectBoard()
{
    LOGC ("Searching for ONI Acquisition Board...");
    const oni_driver_info_t* driverInfo;
    int return_code = evalBoard->open (&driverInfo);

    if (return_code == 1) // successfully opened board
    {
        LOGC ("Board opened successfully.");

        int major, minor, patch, rc;
        evalBoard->getONIVersion (&major, &minor, &patch);
        LOGC ("ONI Library version: ", major, ".", minor, ".", patch);
        LOGC ("ONI Driver: ", driverInfo->name, " Version: ", driverInfo->major, ".", driverInfo->minor, ".", driverInfo->patch, (driverInfo->pre_release ? "-" : ""), (driverInfo->pre_release ? driverInfo->pre_release : ""));
        if (evalBoard->getFTDriverInfo (&major, &minor, &patch))
        {
            LOGC ("FTDI Driver version: ", major, ".", minor, ".", patch);
        }
        if (evalBoard->getFTLibInfo (&major, &minor, &patch))
        {
            LOGC ("FTDI Library version: ", major, ".", minor, ".", patch);
        }
        if (evalBoard->getFirmwareVersion (&major, &minor, &patch, &rc))
        {
            if (rc != 0)
            {
                const char* tag;
                switch (rc & 0xC0)
                {
                    case 0x80:
                        tag = "-rc";
                        break;
                    case 0xC0:
                        tag = "-experimental";
                        break;
                    default:
                        tag = "-beta";
                }
                LOGC ("Open Ephys ECP5-ONI FPGA open. Gateware version v", major, ".", minor, ".", patch, tag, int (rc & 0x3F));
            }
            else
            {
                LOGC ("Open Ephys ECP5-ONI FPGA open. Gateware version v", major, ".", minor, ".", patch);
            }
            hasI2cSupport = CheckSemVer (major, minor, patch, 1, 5, 0);
            hasMemoryMonitorSupport = CheckSemVer (major, minor, patch, 1, 5, 1);

            if (major == 0)
            {
                ShowFirmwareUpdateMessage ("Warning: The detected version of the gateware is v"
                                           + std::to_string (major) + "." + std::to_string (minor) + "." + std::to_string (patch)
                                           + ", and should be updated to the latest version."
                                           + "\n\nTo learn how to update the gateware, please click on the link below.");
            }
        }
        oni_reg_val_t tmpId;
        if (evalBoard->getDeviceId (&tmpId))
        {
            deviceId = tmpId;
            LOGC ("Acquisition board ID = ", deviceId);
        }

        deviceFound = true;

        if (hasMemoryMonitorSupport)
        {
            evalBoard->enableMemoryMonitor (true);
            evalBoard->setMemoryMonitorSampleRate (MEMORY_MONITOR_FS);
            evalBoard->getTotalMemory (&totalMemory);
        }
        else
        {
            evalBoard->enableMemoryMonitor (false);
        }

        return true;
    }
    else
    {
        if (return_code == -1)
        {
            LOGC ("ONI FT600 driver library not found.");
        }
        else if (return_code == -2)
        {
            LOGC ("No ONI Acquisition Board found.");
        }
        else if (return_code == -3)
        {
            ShowFirmwareUpdateMessage ("Warning: The gateware on the acquisition board is not compatible with"
                                       " this version of the plugin and needs to be updated to version 2.0.0 or greater."
                                       "\n\nTo learn how to update the gateware, please click on the link below.");
        }
        deviceFound = false;
        return false;
    }
}

void InitializerThread::run()
{
    setProgress (-1.0); // Show an indeterminate progress bar
    board->initializeBoardInThread();
}

void AcqBoardONI::createCustomStreams (OwnedArray<DataBuffer>& otherBuffers)
{
    if (hasMemoryMonitorSupport)
        memBuffer = otherBuffers.add (new DataBuffer (1, 10000)); // Memory device

    for (int i = 0; i < NUMBER_OF_PORTS; i++)
    {
        if (hasBNO[i])
            bnoBuffers.set (i, otherBuffers.add (new DataBuffer (BNO_CHANNELS, 10000)));
        else
            bnoBuffers.set (i, nullptr);
    }
}

void AcqBoardONI::updateCustomStreams (OwnedArray<DataStream>& otherStreams, OwnedArray<ContinuousChannel>& otherChannels)
{
    DataStream* stream;

    if (hasMemoryMonitorSupport)
    {
        const ContinuousChannel::InputRange percentRange { -100.0f, 100.0f };

        //Memory usage device
        DataStream::Settings memStreamSettings {
            "memory_usage",
            "Hardware buffer usage on an acquisition board",
            "acq-board.memory",
            MEMORY_MONITOR_FS,
            true
        };

        stream = new DataStream (memStreamSettings);
        otherStreams.add (stream);

        ContinuousChannel::Settings channelSettings {
            ContinuousChannel::AUX,
            "MEM",
            "Hardware buffer usage",
            "acq-board.memory.continuous.percentage",
            1.0f,
            stream
        };
        otherChannels.add (new ContinuousChannel (channelSettings));
        otherChannels.getLast()->setUnits ("%");
        otherChannels.getLast()->inputRange = percentRange;
    }

    String port = "ABCD";

    //BNO
    for (int k = 0; k < NUMBER_OF_PORTS; k++)
    {
        if (hasBNO[k])
        {
            DataStream::Settings bnoStreamSettings {
                "IMU_port_" + String::charToString (port[k]),
                "Inertial measurement unit data from the BNO device on port " + String::charToString (port[k]),
                "acq-board.9dof",
                100,
                true
            };

            stream = new DataStream (bnoStreamSettings);
            otherStreams.add (stream);

            String identifier = "acq-board.9dof.continuous";

            const int numEulerStreams = 3;

            std::array<std::string, numEulerStreams> eulerIdentifiers = { "yaw",
                                                            "roll",
                                                            "pitch" };
            const std::string eulerNames = "YRP";

            const std::array<ContinuousChannel::InputRange, numEulerStreams> eulerRanges = {
                {
                    { -360.0f, 360.0f }, // Yaw
                    { -180.0f, 180.0f }, // Roll
                    { -90.0f, 90.0f } // Pitch
                }
            };

            for (int i = 0; i < numEulerStreams; i++)
            {
                ContinuousChannel::Settings channelSettings {
                    ContinuousChannel::AUX,
                    String ("Eul-") + eulerNames[i],
                    "Euler channel",
                    identifier + ".euler." + String (eulerIdentifiers[i]),
                    eulerAngleScale,
                    stream
                };
                otherChannels.add (new ContinuousChannel (channelSettings));
                otherChannels.getLast()->setUnits ("Deg.");
                otherChannels.getLast()->inputRange = eulerRanges[i];
            }

            const std::string quaternionSubtypesLower = "wxyz";
            const std::string quaternionSubtypesUpper = "WXYZ";
            const ContinuousChannel::InputRange quaternionRange { -1.0f, 1.0f };

            for (int i = 0; i < 4; i++)
            {
                ContinuousChannel::Settings channelSettings {
                    ContinuousChannel::AUX,
                    String ("Quat-") + quaternionSubtypesUpper[i],
                    "Quaternion channel",
                    identifier + ".quaternion." + quaternionSubtypesLower[i],
                    quaternionScale,
                    stream
                };
                otherChannels.add (new ContinuousChannel (channelSettings));
                otherChannels.getLast()->setUnits (""); // NB: Quaternion data is unitless by definition
                otherChannels.getLast()->inputRange = quaternionRange;
            }

            const std::string axesLower = "xyz";
            const std::string axesUpper = "XYZ";
            const ContinuousChannel::InputRange accelerationRange { -100.0f, 100.0f };

            for (int i = 0; i < 3; i++)
            {
                ContinuousChannel::Settings channelSettings {
                    ContinuousChannel::AUX,
                    String ("Acc-") + axesUpper[i],
                    "Acceleration channel",
                    identifier + ".acceleration." + axesLower[i],
                    accelerationScale,
                    stream
                };
                otherChannels.add (new ContinuousChannel (channelSettings));
                otherChannels.getLast()->setUnits ("m/s^2");
                otherChannels.getLast()->inputRange = accelerationRange;
            }

            const ContinuousChannel::InputRange gravityRange { -10.0f, 10.0f };

            for (int i = 0; i < 3; i++)
            {
                ContinuousChannel::Settings channelSettings {
                    ContinuousChannel::AUX,
                    String ("Grav-") + axesUpper[i],
                    "Gravity channel",
                    identifier + ".gravity." + axesLower[i],
                    accelerationScale,
                    stream
                };
                otherChannels.add (new ContinuousChannel (channelSettings));
                otherChannels.getLast()->setUnits ("m/s^2");
                otherChannels.getLast()->inputRange = gravityRange;
            }

            ContinuousChannel::Settings temperatureChannelSettings {
                ContinuousChannel::AUX,
                String ("Temp"),
                "Temperature channel",
                identifier + ".temperature",
                1.0f,
                stream
            };
            otherChannels.add (new ContinuousChannel (temperatureChannelSettings));
            otherChannels.getLast()->setUnits (String::fromUTF8 ("\xc2\xb0") + String("C")); // NB: "\xc2\xb0" --> degree symbol
            otherChannels.getLast()->inputRange = ContinuousChannel::InputRange { -100.0f, 100.0f };

            std::array<std::string, 4> calibrationTypesName = { "Mag", "Acc", "Gyr", "Sys" };
            std::array<std::string, 4> calibrationTypesIdentifier = { "magnetometer", "acceleration", "gyroscope", "system" };
            const ContinuousChannel::InputRange calibrationRange { -3.0f, 3.0f };

            for (int i = 0; i < 4; i++)
            {
                ContinuousChannel::Settings calibrationChannelSettings {
                    ContinuousChannel::AUX,
                    String ("Cal-") + calibrationTypesName[i],
                    "Calibration channel",
                    identifier + ".calibration." + calibrationTypesIdentifier[i],
                    1.0f,
                    stream
                };
                otherChannels.add (new ContinuousChannel (calibrationChannelSettings));
                otherChannels.getLast()->setUnits (""); // NB: Calibration data is unitless by definition
                otherChannels.getLast()->inputRange = calibrationRange;
            }
        }
    }
}

bool AcqBoardONI::initializeBoard()
{
    InitializerThread initializer (this);
    initializer.runThread();

    return true;
}

bool AcqBoardONI::initializeBoardInThread()
{
    LOGC ("Initializing ONI Acquisition Board...");

    // Initialize the board
    LOGD ("Initializing RHD2000 board.");
    LOGDD ("DBG: 0");
    evalBoard->initialize();
    // This applies the following settings:
    //  - sample rate to 30 kHz
    //  - aux command banks to zero
    //  - aux command lengths to zero
    //  - continuous run mode to 'true'
    //  - maxTimeStep to 2^32 - 1
    //  - all cable lengths to 3 feet
    //  - dspSettle to 'false'
    //  - data source mapping as 0->PortA1, 1->PortB1, 2->PortC1, 3->PortD1, etc.
    //  - enables all data streams
    //  - clears the ttlOut
    //  - disables all DACs and sets gain to 0
    LOGDD ("DBG: 1");

    setSampleRate (30000, false);

    LOGDD ("DBG: A");
    evalBoard->setCableLengthMeters (Rhd2000ONIBoard::PortA, settings.cableLength.portA);
    evalBoard->setCableLengthMeters (Rhd2000ONIBoard::PortB, settings.cableLength.portB);
    evalBoard->setCableLengthMeters (Rhd2000ONIBoard::PortC, settings.cableLength.portC);
    evalBoard->setCableLengthMeters (Rhd2000ONIBoard::PortD, settings.cableLength.portD);

    // Select RAM Bank 0 for AuxCmd3 initially, so the ADC is calibrated.
    evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortA, Rhd2000ONIBoard::AuxCmd3, 0);
    evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortB, Rhd2000ONIBoard::AuxCmd3, 0);
    evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortC, Rhd2000ONIBoard::AuxCmd3, 0);
    evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortD, Rhd2000ONIBoard::AuxCmd3, 0);
    LOGDD ("DBG: B");

    evalBoard->setMaxTimeStep (INIT_STEP);
    evalBoard->setContinuousRunMode (false);
    LOGDD ("DBG: C");
    // Start SPI interface
    evalBoard->run();
    LOGDD ("DBG: D");
    // Wait for the 60-sample run to complete
    while (evalBoard->isRunning())
    {
        ;
    }

    LOGDD ("DBG: E");

    //stopping clears the buffers, so we don't really need to read this last data
    {
        const ScopedLock lock (oniLock);
        evalBoard->stop();
    }
    LOGDD ("DBG: G");
    // Now that ADC calibration has been performed, we switch to the command sequence
    // that does not execute ADC calibration.
    LOGDD ("DBG: H");
    evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortA, Rhd2000ONIBoard::AuxCmd3, settings.fastSettleEnabled ? 2 : 1);
    evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortB, Rhd2000ONIBoard::AuxCmd3, settings.fastSettleEnabled ? 2 : 1);
    evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortC, Rhd2000ONIBoard::AuxCmd3, settings.fastSettleEnabled ? 2 : 1);
    evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortD, Rhd2000ONIBoard::AuxCmd3, settings.fastSettleEnabled ? 2 : 1);

    evalBoard->getAcquisitionClockHz (&acquisitionClockHz);

    return true;
}

bool AcqBoardONI::foundInputSource() const
{
    return deviceFound;
}

Array<const Headstage*> AcqBoardONI::getHeadstages()
{
    Array<const Headstage*> connectedHeadstages;

    for (auto headstage : headstages)
    {
        if (headstage->isConnected())
            connectedHeadstages.add (headstage);
    }

    return connectedHeadstages;
}

Array<int> AcqBoardONI::getAvailableSampleRates()
{
    Array<int> sampleRates;

    sampleRates.add (1000);
    sampleRates.add (1250);
    sampleRates.add (1500);
    sampleRates.add (2000);
    sampleRates.add (2500);
    sampleRates.add (3000);
    sampleRates.add (3333);
    sampleRates.add (4000);
    sampleRates.add (5000);
    sampleRates.add (6250);
    sampleRates.add (8000);
    sampleRates.add (10000);
    sampleRates.add (12500);
    sampleRates.add (15000);
    sampleRates.add (20000);
    sampleRates.add (25000);
    sampleRates.add (30000);

    return sampleRates;
}

void AcqBoardONI::setSampleRate(int desiredSampleRate)
{
    setSampleRate (desiredSampleRate, true);
}

void AcqBoardONI::setSampleRate (int desiredSampleRate, bool reScanDelays)
{
    Rhd2000ONIBoard::AmplifierSampleRate sampleRate;

    switch (desiredSampleRate)
    {
        case 1000:
            sampleRate = Rhd2000ONIBoard::SampleRate1000Hz;
            numUsbBlocksToRead = 1;
            settings.boardSampleRate = 1000.0f;
            break;
        case 1250:
            sampleRate = Rhd2000ONIBoard::SampleRate1250Hz;
            numUsbBlocksToRead = 1;
            settings.boardSampleRate = 1250.0f;
            break;
        case 1500:
            sampleRate = Rhd2000ONIBoard::SampleRate1500Hz;
            numUsbBlocksToRead = 1;
            settings.boardSampleRate = 1500.0f;
            break;
        case 2000:
            sampleRate = Rhd2000ONIBoard::SampleRate2000Hz;
            numUsbBlocksToRead = 1;
            settings.boardSampleRate = 2000.0f;
            break;
        case 2500:
            sampleRate = Rhd2000ONIBoard::SampleRate2500Hz;
            numUsbBlocksToRead = 1;
            settings.boardSampleRate = 2500.0f;
            break;
        case 3000:
            sampleRate = Rhd2000ONIBoard::SampleRate3000Hz;
            numUsbBlocksToRead = 2;
            settings.boardSampleRate = 3000.0f;
            break;
        case 3333:
            sampleRate = Rhd2000ONIBoard::SampleRate3333Hz;
            numUsbBlocksToRead = 2;
            settings.boardSampleRate = 3333.0f;
            break;
        case 5000:
            sampleRate = Rhd2000ONIBoard::SampleRate5000Hz;
            numUsbBlocksToRead = 3;
            settings.boardSampleRate = 5000.0f;
            break;
        case 6250:
            sampleRate = Rhd2000ONIBoard::SampleRate6250Hz;
            numUsbBlocksToRead = 3;
            settings.boardSampleRate = 6250.0f;
            break;
        case 10000:
            sampleRate = Rhd2000ONIBoard::SampleRate10000Hz;
            numUsbBlocksToRead = 6;
            settings.boardSampleRate = 10000.0f;
            break;
        case 12500:
            sampleRate = Rhd2000ONIBoard::SampleRate12500Hz;
            numUsbBlocksToRead = 7;
            settings.boardSampleRate = 12500.0f;
            break;
        case 15000:
            sampleRate = Rhd2000ONIBoard::SampleRate15000Hz;
            numUsbBlocksToRead = 8;
            settings.boardSampleRate = 15000.0f;
            break;
        case 20000:
            sampleRate = Rhd2000ONIBoard::SampleRate20000Hz;
            numUsbBlocksToRead = 12;
            settings.boardSampleRate = 20000.0f;
            break;
        case 25000:
            sampleRate = Rhd2000ONIBoard::SampleRate25000Hz;
            numUsbBlocksToRead = 14;
            settings.boardSampleRate = 25000.0f;
            break;
        case 30000:
            sampleRate = Rhd2000ONIBoard::SampleRate30000Hz;
            numUsbBlocksToRead = 16;
            settings.boardSampleRate = 30000.0f;
            break;
        default:
            LOGC ("Invalid sample rate.");
            return;
    }

    {
        const ScopedLock lock (oniLock);
        // Select per-channel amplifier sampling rate.
        evalBoard->setSampleRate (sampleRate);
    }
    LOGD ("Sample rate set to ", evalBoard->getSampleRate());

    if (reScanDelays)
    {
        checkAllCableDelays();
    }

    updateRegisters();
}

void AcqBoardONI::checkAllCableDelays()
{
    Array<bool> cableIsConnected;

    for (int cableIndex = 0; cableIndex < 4; cableIndex++)
    {
        cableIsConnected.add (headstages[cableIndex * 2]->isConnected() || headstages[cableIndex * 2 + 1]->isConnected());
    }

    LOGD ("Number of enabled data streams: ", evalBoard->getNumEnabledDataStreams());

    evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortA,
                                     Rhd2000ONIBoard::AuxCmd3,
                                     0);
    evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortB,
                                     Rhd2000ONIBoard::AuxCmd3,
                                     0);
    evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortC,
                                     Rhd2000ONIBoard::AuxCmd3,
                                     0);
    evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortD,
                                     Rhd2000ONIBoard::AuxCmd3,
                                     0);

    evalBoard->setMaxTimeStep (128 * INIT_STEP);
    evalBoard->setContinuousRunMode (false);

    std::unique_ptr<Rhd2000ONIDataBlock> dataBlock =
        std::make_unique<Rhd2000ONIDataBlock> (evalBoard->getNumEnabledDataStreams(), evalBoard->isUSB3());

    Array<int> sumGoodDelays;
    sumGoodDelays.insertMultiple (0, 0, 8);

    Array<int> indexFirstGoodDelay;
    indexFirstGoodDelay.insertMultiple (0, -1, 8);

    Array<int> indexSecondGoodDelay;
    indexSecondGoodDelay.insertMultiple (0, -1, 8);

    // Run SPI command sequence at all 16 possible FPGA MISO delay settings
    // to find optimum delay for each SPI interface cable.

    LOGD ("Checking for connected amplifier chips...");

    // Scan SPI ports
    int delay, hs, id;
    int register59Value;

    for (delay = -2; delay < 2; delay++)
    {
        LOGD ("Setting delay to: ", delay);

        int delayA = settings.optimumDelay.portA + delay;
        if (delayA < 0)
            delayA = 0;
        if (delayA > 16)
            delayA = 16;

        int delayB = settings.optimumDelay.portB + delay;
        if (delayB < 0)
            delayB = 0;
        if (delayB > 16)
            delayB = 16;

        int delayC = settings.optimumDelay.portC + delay;
        if (delayC < 0)
            delayC = 0;
        if (delayC > 16)
            delayC = 16;

        int delayD = settings.optimumDelay.portD + delay;
        if (delayD < 0)
            delayD = 0;
        if (delayD > 16)
            delayD = 16;

        evalBoard->setCableDelay (Rhd2000ONIBoard::PortA, delayA);
        evalBoard->setCableDelay (Rhd2000ONIBoard::PortB, delayB);
        evalBoard->setCableDelay (Rhd2000ONIBoard::PortC, delayC);
        evalBoard->setCableDelay (Rhd2000ONIBoard::PortD, delayD);

        // Start SPI interface.
        evalBoard->run();

        // Read the resulting single data block from the USB interface.
        evalBoard->readDataBlock (dataBlock.get(), INIT_STEP);
        {
            const ScopedLock lock (oniLock);
            evalBoard->stop();
        }

        // Read the Intan chip ID number from each RHD2000 chip found.
        // Record delay settings that yield good communication with the chip.
        for (int hs = 0; hs < headstages.size(); ++hs)
        {
            if (headstages[hs]->isConnected())
            {
                id = getIntanChipId (dataBlock.get(), headstages[hs]->getStreamIndex (0), register59Value);

                //     LOGD("hs ", hs, " id ", id, " r59 ", (int)register59Value);

                if (id == CHIP_ID_RHD2132 || id == CHIP_ID_RHD2216 || (id == CHIP_ID_RHD2164 && register59Value == REGISTER_59_MISO_A))
                {
                    LOGD ("Device ID found: ", id);

                    sumGoodDelays.set (hs, sumGoodDelays[hs] + 1);

                    if (indexFirstGoodDelay[hs] == -1)
                    {
                        indexFirstGoodDelay.set (hs, delay);
                    }
                    else if (indexSecondGoodDelay[hs] == -1)
                    {
                        indexSecondGoodDelay.set (hs, delay);
                    }
                }
            }
        }
    }

    Array<int> optimumDelay;

    optimumDelay.insertMultiple (0, -5, headstages.size());

    for (hs = 0; hs < headstages.size(); ++hs)
    {
        if (sumGoodDelays[hs] == 1 || sumGoodDelays[hs] == 2)
        {
            optimumDelay.set (hs, indexFirstGoodDelay[hs]);
        }
        else if (sumGoodDelays[hs] > 2)
        {
            optimumDelay.set (hs, indexSecondGoodDelay[hs]);
        }
    }

    if (cableIsConnected[0])
    {
        int delayShift = jmax (optimumDelay[0], optimumDelay[1]);

        evalBoard->setCableDelay (Rhd2000ONIBoard::PortA, settings.optimumDelay.portA + delayShift);

        LOGD ("Port A cable delay at ", settings.boardSampleRate, " samples/sec: ", settings.optimumDelay.portA + delayShift);
    }

    if (cableIsConnected[1])
    {
        int delayShift = jmax (optimumDelay[2], optimumDelay[3]);

        evalBoard->setCableDelay (Rhd2000ONIBoard::PortB, settings.optimumDelay.portB + delayShift);

        LOGD ("Port B cable delay at ", settings.boardSampleRate, " samples/sec: ", settings.optimumDelay.portB + delayShift);
    }

    if (cableIsConnected[2])
    {
        int delayShift = jmax (optimumDelay[4], optimumDelay[5]);

        evalBoard->setCableDelay (Rhd2000ONIBoard::PortC, settings.optimumDelay.portC + delayShift);

        LOGD ("Port C cable delay at ", settings.boardSampleRate, " samples/sec: ", settings.optimumDelay.portC + delayShift);
    }

    if (cableIsConnected[3])
    {
        int delayShift = jmax (optimumDelay[6], optimumDelay[7]);

        evalBoard->setCableDelay (Rhd2000ONIBoard::PortD, settings.optimumDelay.portD + delayShift);

        LOGD ("Port D cable delay at ", settings.boardSampleRate, " samples/sec: ", settings.optimumDelay.portD + delayShift);
    }
}

float AcqBoardONI::getSampleRate() const
{
    return settings.boardSampleRate;
}

void AcqBoardONI::updateRegisters()
{
    if (! deviceFound) //Safety to avoid crashes loading a chain with Rhythm node without a board
    {
        return;
    }
    // Set up an RHD2000 register object using this sample rate to
    // optimize MUX-related register settings.
    chipRegisters.defineSampleRate (settings.boardSampleRate);

    int commandSequenceLength;
    std::vector<int> commandList;

    //AuxCmd1 and AuxCmd2 registers are not sample-rate dependent, so it does not make sense to continuously update them
    //Thus we only update them on the first call to updateRegisters()
    if (! commonCommandsSet)
    {
        LOGD ("Uploading common commands");
        // Create a command list for the AuxCmd1 slot.  This command sequence will continuously
        // update Register 3, which controls the auxiliary digital output pin on each RHD2000 chip.
        // In concert with the v1.4 Rhythm FPGA code, this permits real-time control of the digital
        // output pin on chips on each SPI port.
        chipRegisters.setDigOutLow(); // Take auxiliary output out of HiZ mode.
        commandSequenceLength = chipRegisters.createCommandListUpdateDigOut (commandList);
        evalBoard->uploadCommandList (commandList, Rhd2000ONIBoard::AuxCmd1, 0);
        evalBoard->selectAuxCommandLength (Rhd2000ONIBoard::AuxCmd1, 0, commandSequenceLength - 1);
        evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortA, Rhd2000ONIBoard::AuxCmd1, 0);
        evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortB, Rhd2000ONIBoard::AuxCmd1, 0);
        evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortC, Rhd2000ONIBoard::AuxCmd1, 0);
        evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortD, Rhd2000ONIBoard::AuxCmd1, 0);

        // Next, we'll create a command list for the AuxCmd2 slot.  This command sequence
        // will sample the temperature sensor and other auxiliary ADC inputs.
        commandSequenceLength = chipRegisters.createCommandListTempSensor (commandList);
        evalBoard->uploadCommandList (commandList, Rhd2000ONIBoard::AuxCmd2, 0);
        evalBoard->selectAuxCommandLength (Rhd2000ONIBoard::AuxCmd2, 0, commandSequenceLength - 1);
        evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortA, Rhd2000ONIBoard::AuxCmd2, 0);
        evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortB, Rhd2000ONIBoard::AuxCmd2, 0);
        evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortC, Rhd2000ONIBoard::AuxCmd2, 0);
        evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortD, Rhd2000ONIBoard::AuxCmd2, 0);

        commonCommandsSet = true;
    }

    // Before generating register configuration command sequences, set amplifier
    // bandwidth parameters.
    settings.dsp.cutoffFreq = chipRegisters.setDspCutoffFreq (settings.dsp.cutoffFreq);
    settings.analogFilter.lowerBandwidth = chipRegisters.setLowerBandwidth (settings.analogFilter.lowerBandwidth);
    settings.analogFilter.upperBandwidth = chipRegisters.setUpperBandwidth (settings.analogFilter.upperBandwidth);
    chipRegisters.enableDsp (settings.dsp.enabled);

    // enable/disable aux inputs:
    chipRegisters.enableAux1 (settings.acquireAux);
    chipRegisters.enableAux2 (settings.acquireAux);
    chipRegisters.enableAux3 (settings.acquireAux);

    commandSequenceLength = chipRegisters.createCommandListRegisterConfig (commandList, true);
    // Upload version with ADC calibration to AuxCmd3 RAM Bank 0.
    evalBoard->uploadCommandList (commandList, Rhd2000ONIBoard::AuxCmd3, 0);
    evalBoard->selectAuxCommandLength (Rhd2000ONIBoard::AuxCmd3, 0, commandSequenceLength - 1);

    commandSequenceLength = chipRegisters.createCommandListRegisterConfig (commandList, false);
    // Upload version with no ADC calibration to AuxCmd3 RAM Bank 1.
    evalBoard->uploadCommandList (commandList, Rhd2000ONIBoard::AuxCmd3, 1);
    evalBoard->selectAuxCommandLength (Rhd2000ONIBoard::AuxCmd3, 0, commandSequenceLength - 1);

    chipRegisters.setFastSettle (true);

    commandSequenceLength = chipRegisters.createCommandListRegisterConfig (commandList, false);
    // Upload version with fast settle enabled to AuxCmd3 RAM Bank 2.
    evalBoard->uploadCommandList (commandList, Rhd2000ONIBoard::AuxCmd3, 2);
    evalBoard->selectAuxCommandLength (Rhd2000ONIBoard::AuxCmd3, 0, commandSequenceLength - 1);

    chipRegisters.setFastSettle (false);
    evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortA, Rhd2000ONIBoard::AuxCmd3, settings.fastSettleEnabled ? 2 : 1);
    evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortB, Rhd2000ONIBoard::AuxCmd3, settings.fastSettleEnabled ? 2 : 1);
    evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortC, Rhd2000ONIBoard::AuxCmd3, settings.fastSettleEnabled ? 2 : 1);
    evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortD, Rhd2000ONIBoard::AuxCmd3, settings.fastSettleEnabled ? 2 : 1);
}

int AcqBoardONI::getIntanChipId (
    Rhd2000ONIDataBlock* dataBlock,
    int stream,
    int& register59Value)
{
    bool intanChipPresent;

    if (stream < 0)
        return 0;

    // First, check ROM registers 32-36 to verify that they hold 'INTAN', and
    // the initial chip name ROM registers 24-26 that hold 'RHD'.
    // This is just used to verify that we are getting good data over the SPI
    // communication channel.
    intanChipPresent = ((char) dataBlock->auxiliaryData[stream][2][32] == 'I' && (char) dataBlock->auxiliaryData[stream][2][33] == 'N' && (char) dataBlock->auxiliaryData[stream][2][34] == 'T' && (char) dataBlock->auxiliaryData[stream][2][35] == 'A' && (char) dataBlock->auxiliaryData[stream][2][36] == 'N' && (char) dataBlock->auxiliaryData[stream][2][24] == 'R' && (char) dataBlock->auxiliaryData[stream][2][25] == 'H' && (char) dataBlock->auxiliaryData[stream][2][26] == 'D');
    //std::cout << (char) dataBlock->auxiliaryData[stream][2][32] << "-" << (char) dataBlock->auxiliaryData[stream][2][33] << "-" << (char) dataBlock->auxiliaryData[stream][2][34] << std::endl;

    // If the SPI communication is bad, return -1.  Otherwise, return the Intan
    // chip ID number stored in ROM register 63.
    if (! intanChipPresent)
    {
        register59Value = -1;
        return -1;
    }
    else
    {
        register59Value = dataBlock->auxiliaryData[stream][2][23]; // Register 59
        return dataBlock->auxiliaryData[stream][2][19]; // chip ID (Register 63)
    }
}

void PortScanner::run()
{
    setProgress (-1); // endless moving progress bar

    board->scanPortsInThread();
}

void AcqBoardONI::scanPorts()
{
    PortScanner scanner (this);
    scanner.runThread();
}

void AcqBoardONI::scanPortsInThread()
{
    if (! deviceFound) //Safety to avoid crashes if board not present
    {
        return;
    }
    if (! checkBoardMem())
        return;
    LOGDD ("DBG: SA");
    if (hasI2cSupport)
    {
        bool enableI2c[NUMBER_OF_PORTS] = { true, true, true, true };

        evalBoard->enableI2cMode (enableI2c);
        evalBoard->resetBoard();

        for (int i = 0; i < NUMBER_OF_PORTS; i += 1)
        {
            hasBNO[i] = evalBoard->isBnoConnected (i);
            evalBoard->enableBnoStream (i, hasBNO[i]);

            hasI2c[i] = evalBoard->isI2cCapable (i);
            headstageId[i] = hasI2c[i] ? evalBoard->getDeviceIdOnEeprom (i) : 0;

            if (hasBNO[i])
            {
                if (headstageId[i] == 0) // NB: 1st revision BNO capable LP headstage contains BNO but no EEPROM
                {
                    LOGD ("Found headstage with BNO but no EEPROM. Assuming low-profile 3D headstage.");

                    evalBoard->setBnoAxisMap (i, 0b00100100);
                }
                else
                {
                    std::stringstream ss;
                    ss << "0x" << std::hex << headstageId[i];
                    std::string idStr (ss.str());
                    LOGD ("Headstage ID ", idStr.c_str(), " found on port ", String::charToString ('A' + i));

                    switch (headstageId[i])
                    {
                        // TODO: Add headstages here with the required axis map
                        case 0x80000001: //Low-Profile 64ch hirose
                            evalBoard->setBnoAxisMap (i, 0b00000100100);
                            break;
                        case 0x80000002: //32ch
                            evalBoard->setBnoAxisMap (i, 0b10000011000);
                            break;
                        case 0x80000003: //16ch bipolar
                            evalBoard->setBnoAxisMap (i, 0b10000011000);
                            break;
                        default:
                            evalBoard->setBnoAxisMap (i, 0b00000100100);
                            break;
                    }
                }
            }

            enableI2c[i] = hasBNO[i] || (hasI2c[i] && headstageId[i] != 0);
        }

        evalBoard->enableI2cMode (enableI2c);
        evalBoard->resetBoard();
    }

    //Clear previous known streams
    enabledStreams.clear();

    // Scan SPI ports
    int delay, hs, id;
    int register59Value;

    for (auto headstage : headstages)
    {
        headstage->setNumStreams (0); // reset stream count
    }

    Rhd2000ONIBoard::BoardDataSource initStreamPorts[8] = {
        Rhd2000ONIBoard::PortA1,
        Rhd2000ONIBoard::PortA2,
        Rhd2000ONIBoard::PortB1,
        Rhd2000ONIBoard::PortB2,
        Rhd2000ONIBoard::PortC1,
        Rhd2000ONIBoard::PortC2,
        Rhd2000ONIBoard::PortD1,
        Rhd2000ONIBoard::PortD2
    };

    chipId.insertMultiple (0, -1, 8);
    Array<int> tmpChipId (chipId);
    LOGDD ("DBG: SB");

    float currentSampleRate = settings.boardSampleRate;

    setSampleRate (30000, false); // set to 30 kHz temporarily

    LOGDD ("DBG: SC");
    // Enable all data streams, and set sources to cover one or two chips
    // on Ports A-D.

    // THIS IS DIFFERENT FOR RECORDING CONTROLLER:
    for (int i = 0; i < 8; i++)
        evalBoard->setDataSource (i, initStreamPorts[i]);

    for (int i = 0; i < 8; i++)
        evalBoard->enableDataStream (i, true);
    {
        const ScopedLock lock (oniLock);
        evalBoard->updateStreamBlockSize();
    }

    LOGD ("Number of enabled data streams: ", evalBoard->getNumEnabledDataStreams());

    evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortA,
                                     Rhd2000ONIBoard::AuxCmd3,
                                     0);
    evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortB,
                                     Rhd2000ONIBoard::AuxCmd3,
                                     0);
    evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortC,
                                     Rhd2000ONIBoard::AuxCmd3,
                                     0);
    evalBoard->selectAuxCommandBank (Rhd2000ONIBoard::PortD,
                                     Rhd2000ONIBoard::AuxCmd3,
                                     0);

    evalBoard->setMaxTimeStep (128 * INIT_STEP);
    evalBoard->setContinuousRunMode (false);

    std::unique_ptr<Rhd2000ONIDataBlock> dataBlock =
        std::make_unique<Rhd2000ONIDataBlock> (evalBoard->getNumEnabledDataStreams(), evalBoard->isUSB3());

    Array<int> sumGoodDelays;
    sumGoodDelays.insertMultiple (0, 0, 8);

    Array<int> indexFirstGoodDelay;
    indexFirstGoodDelay.insertMultiple (0, -1, 8);

    Array<int> indexSecondGoodDelay;
    indexSecondGoodDelay.insertMultiple (0, -1, 8);

    // Run SPI command sequence at all 16 possible FPGA MISO delay settings
    // to find optimum delay for each SPI interface cable.

    LOGD ("Checking for connected amplifier chips...");

    for (delay = 0; delay < 16; delay++)
    {
        LOGD ("Setting delay to: ", delay);

        evalBoard->setCableDelay (Rhd2000ONIBoard::PortA, delay);
        evalBoard->setCableDelay (Rhd2000ONIBoard::PortB, delay);
        evalBoard->setCableDelay (Rhd2000ONIBoard::PortC, delay);
        evalBoard->setCableDelay (Rhd2000ONIBoard::PortD, delay);

        // Start SPI interface.
        evalBoard->run();

        // Wait for the 60-sample run to complete. //Not needed now.
        /*    while (evalBoard->isRunning())
        {
            ;
        }*/
        // Read the resulting single data block from the USB interface.
        evalBoard->readDataBlock (dataBlock.get(), INIT_STEP);
        {
            const ScopedLock lock (oniLock);
            evalBoard->stop();
        }

        // Read the Intan chip ID number from each RHD2000 chip found.
        // Record delay settings that yield good communication with the chip.
        for (hs = 0; hs < headstages.size(); ++hs)
        {
            id = getIntanChipId (dataBlock.get(), hs, register59Value);

                // LOGD("hs ", hs, " id ", id, " r59 ", (int)register59Value);

            if (id == CHIP_ID_RHD2132 || id == CHIP_ID_RHD2216 || (id == CHIP_ID_RHD2164 && register59Value == REGISTER_59_MISO_A))
            {
                LOGD ("Device ID found: ", id);

                sumGoodDelays.set (hs, sumGoodDelays[hs] + 1);

                if (indexFirstGoodDelay[hs] == -1)
                {
                    indexFirstGoodDelay.set (hs, delay);
                    tmpChipId.set (hs, id);
                }
                else if (indexSecondGoodDelay[hs] == -1)
                {
                    indexSecondGoodDelay.set (hs, delay);
                    tmpChipId.set (hs, id);
                }
            }
        }
    }

#if DEBUG_EMULATE_HEADSTAGES > 0
    if (tmpChipId[0] > 0)
    {
        int chipIdx = 0;
        for (int hs = 0; hs < DEBUG_EMULATE_HEADSTAGES && hs < headstages.size(); ++hs)
        {
            if (enabledStreams.size() < MAX_NUM_DATA_STREAMS)
            {
#ifdef DEBUG_EMULATE_64CH
                chipId.set (chipIdx++, CHIP_ID_RHD2164);
                chipId.set (chipIdx++, CHIP_ID_RHD2164_B);
                enableHeadstage (hs, true, 2, 32);
#else
                chipId.set (chipIdx++, CHIP_ID_RHD2132);
                enableHeadstage (hs, true, 1, 32);
#endif
            }
        }
        for (int i = 0; i < enabledStreams.size(); i++)
        {
            enabledStreams.set (i, Rhd2000ONIBoard::PortA1);
        }
    }

#else
    // Now, disable data streams where we did not find chips present.
    int chipIdx = 0;

    for (int hs = 0; hs < headstages.size(); ++hs)
    {
        if ((tmpChipId[hs] > 0) && (enabledStreams.size() < MAX_NUM_DATA_STREAMS))
        {
            chipId.set (chipIdx++, tmpChipId[hs]);

            bool bno = hasBNO[hs / 2];

            LOGD ("Enabling headstage ", hs);

            if (tmpChipId[hs] == CHIP_ID_RHD2164) //RHD2164
            {
                if (enabledStreams.size() < MAX_NUM_DATA_STREAMS - 1)
                {
                    enableHeadstage (hs, true, 2, 32, bno);
                    chipId.set (chipIdx++, CHIP_ID_RHD2164_B);
                }
                else //just one stream left
                {
                    enableHeadstage (hs, true, 1, 32, bno);
                }
            }
            else
            {
                enableHeadstage (hs, true, 1, tmpChipId[hs] == 1 ? 32 : 16, bno);
            }
        }
        else if (hs % 2 == 1 && hasBNO[hs / 2])
        {
            enableHeadstage (hs, true, 0, 0, true);
        }
        else
        {
            enableHeadstage (hs, false);
        }
    }
#endif
    updateBoardStreams();

    LOGD ("Number of enabled data streams: ", evalBoard->getNumEnabledDataStreams());

    // Set cable delay settings that yield good communication with each
    // RHD2000 chip.
    Array<int> optimumDelay;

    optimumDelay.insertMultiple (0, 0, headstages.size());

    for (hs = 0; hs < headstages.size(); ++hs)
    {
        if (sumGoodDelays[hs] == 1 || sumGoodDelays[hs] == 2)
        {
            optimumDelay.set (hs, indexFirstGoodDelay[hs]);
        }
        else if (sumGoodDelays[hs] > 2)
        {
            optimumDelay.set (hs, indexSecondGoodDelay[hs]);
        }
    }

    settings.optimumDelay.portA = jmax (optimumDelay[0], optimumDelay[1]);
    settings.optimumDelay.portB = jmax (optimumDelay[2], optimumDelay[3]);
    settings.optimumDelay.portC = jmax (optimumDelay[4], optimumDelay[5]);
    settings.optimumDelay.portD = jmax (optimumDelay[6], optimumDelay[7]);

    evalBoard->setCableDelay (Rhd2000ONIBoard::PortA, settings.optimumDelay.portA);
    evalBoard->setCableDelay (Rhd2000ONIBoard::PortB, settings.optimumDelay.portB);
    evalBoard->setCableDelay (Rhd2000ONIBoard::PortC, settings.optimumDelay.portC);
    evalBoard->setCableDelay (Rhd2000ONIBoard::PortD, settings.optimumDelay.portD);

    LOGD ("Set optimum delay for port A: ", settings.optimumDelay.portA);
    LOGD ("Set optimum delay for port B: ", settings.optimumDelay.portB);
    LOGD ("Set optimum delay for port C: ", settings.optimumDelay.portC);
    LOGD ("Set optimum delay for port D: ", settings.optimumDelay.portD);

    setSampleRate (currentSampleRate, !initialScan); // restore saved sample rate and check delays

    initialScan = false;
}

void AcqBoardONI::setCableLength (int hsNum, float length)
{
    // Set the MISO sampling delay, which is dependent on the sample rate.

    switch (hsNum)
    {
        case 0:
            evalBoard->setCableLengthFeet (Rhd2000ONIBoard::PortA, length);
            break;
        case 1:
            evalBoard->setCableLengthFeet (Rhd2000ONIBoard::PortB, length);
            break;
        case 2:
            evalBoard->setCableLengthFeet (Rhd2000ONIBoard::PortC, length);
            break;
        case 3:
            evalBoard->setCableLengthFeet (Rhd2000ONIBoard::PortD, length);
            break;
        default:
            break;
    }
}

bool AcqBoardONI::enableHeadstage (int hsNum, bool enabled, int nStr, int strChans, bool hasBNO)
{
    LOGD ("Headstage ", hsNum, ", enabled: ", enabled, ", num streams: ", nStr, ", stream channels: ", strChans);
    LOGD ("Max num headstages: ", MAX_NUM_HEADSTAGES);

    if (enabled)
    {
        headstages[hsNum]->setFirstChannel (getNumDataOutputs (ContinuousChannel::ELECTRODE));
        headstages[hsNum]->setNumStreams (nStr);
        headstages[hsNum]->setChannelsPerStream (strChans);
        headstages[hsNum]->setHasBno (hasBNO);

        if (nStr > 0)
        {
            headstages[hsNum]->setFirstStreamIndex (enabledStreams.size());
            enabledStreams.add (headstages[hsNum]->getDataStream (0));
            numChannelsPerDataStream.add (strChans);
        }

        if (nStr > 1)
        {
            enabledStreams.add (headstages[hsNum]->getDataStream (1));
            numChannelsPerDataStream.add (strChans);
        }
    }
    else
    {
        int idx = enabledStreams.indexOf (headstages[hsNum]->getDataStream (0));

        if (idx >= 0)
        {
            enabledStreams.remove (idx);
            numChannelsPerDataStream.remove (idx);
        }

        if (headstages[hsNum]->getNumStreams() > 1)
        {
            idx = enabledStreams.indexOf (headstages[hsNum]->getDataStream (1));
            if (idx >= 0)
            {
                enabledStreams.remove (idx);
                numChannelsPerDataStream.remove (idx);
            }
        }

        headstages[hsNum]->setNumStreams (0);
        headstages[hsNum]->setHasBno (hasBNO);
    }

    return true;
}

void AcqBoardONI::updateBoardStreams()
{
    for (int i = 0; i < MAX_NUM_DATA_STREAMS; i++)
    {
        if (i < enabledStreams.size())
        {
            evalBoard->enableDataStream (i, true);
            evalBoard->setDataSource (i, enabledStreams[i]);
        }
        else
        {
            evalBoard->enableDataStream (i, false);
        }
    }
    evalBoard->updateStreamBlockSize();
}

bool AcqBoardONI::isHeadstageEnabled (int hsNum) const
{
    return headstages[hsNum]->isConnected();
}

int AcqBoardONI::getActiveChannelsInHeadstage (int hsNum) const
{
    return headstages[hsNum]->getNumActiveChannels();
}

float AcqBoardONI::getBitVolts (ContinuousChannel::Type channelType) const
{
    switch (channelType)
    {
        case ContinuousChannel::ELECTRODE:
            return 0.195f;
        case ContinuousChannel::AUX:
            return 0.0000374f;
        case ContinuousChannel::ADC:
            if (deviceId == DEVICE_ID_V3)
                return v3AdcBitVal;
            else
                return 0.00015258789f;
    }

    return 1.0f;
}

void AcqBoardONI::measureImpedances()
{
    impedanceMeter->stopThreadSafely();

    impedanceMeter->runThread();
}

void AcqBoardONI::impedanceMeasurementFinished()
{
    if (impedances.valid)
    {
        LOGD ("Updating headstage impedance values");

        for (auto hs : headstages)
        {
            if (hs->isConnected())
            {
                hs->setImpedances (impedances);
            }
        }

        editor->impedanceMeasurementFinished();
    }
}

void AcqBoardONI::saveImpedances (File& file)
{
    if (impedances.valid)
    {
        std::unique_ptr<XmlElement> xml = std::unique_ptr<XmlElement> (new XmlElement ("IMPEDANCES"));

        int globalChannelNumber = -1;

        for (auto hs : headstages)
        {
            XmlElement* headstageXml = new XmlElement ("HEADSTAGE");
            headstageXml->setAttribute ("name", hs->getStreamPrefix());

            for (int ch = 0; ch < hs->getNumActiveChannels(); ch++)
            {
                globalChannelNumber++;

                XmlElement* channelXml = new XmlElement ("CHANNEL");
                channelXml->setAttribute ("name", hs->getChannelName (ch));
                channelXml->setAttribute ("number", globalChannelNumber);
                channelXml->setAttribute ("magnitude", hs->getImpedanceMagnitude (ch));
                channelXml->setAttribute ("phase", hs->getImpedancePhase (ch));
                headstageXml->addChildElement (channelXml);
            }

            xml->addChildElement (headstageXml);
        }

        xml->writeTo (file, XmlElement::TextFormat());
    }
}

void AcqBoardONI::setNamingScheme (ChannelNamingScheme scheme)
{
    channelNamingScheme = scheme;

    for (auto hs : headstages)
    {
        hs->setNamingScheme (scheme);
    }
}

ChannelNamingScheme AcqBoardONI::getNamingScheme()
{
    return channelNamingScheme;
}

void AcqBoardONI::enableAuxChannels (bool enabled)
{
    settings.acquireAux = enabled;
    updateRegisters();
}

bool AcqBoardONI::areAuxChannelsEnabled() const
{
    return settings.acquireAux;
}

void AcqBoardONI::enableAdcChannels (bool enabled)
{
    settings.acquireAdc = enabled;
}

bool AcqBoardONI::areAdcChannelsEnabled() const
{
    return settings.acquireAdc;
}

double AcqBoardONI::setUpperBandwidth (double upper)
{
    settings.analogFilter.upperBandwidth = upper;

    updateRegisters();

    return settings.analogFilter.upperBandwidth;
}

double AcqBoardONI::setLowerBandwidth (double lower)
{
    settings.analogFilter.lowerBandwidth = lower;

    updateRegisters();

    return settings.analogFilter.lowerBandwidth;
}

double AcqBoardONI::setDspCutoffFreq (double freq)
{
    settings.dsp.cutoffFreq = freq;

    updateRegisters();

    return settings.dsp.cutoffFreq;
}

double AcqBoardONI::getDspCutoffFreq() const
{
    return settings.dsp.cutoffFreq;
}

void AcqBoardONI::setDspOffset (bool state)
{
    settings.dsp.enabled = state;

    updateRegisters();
}

void AcqBoardONI::setTTLOutputMode (bool state)
{
    settings.ttlOutputMode = state;

    updateSettingsDuringAcquisition = true;
}

void AcqBoardONI::setDAChpf (float cutoff, bool enabled)
{
    settings.desiredDAChpf = cutoff;

    settings.desiredDAChpfState = enabled;

    updateSettingsDuringAcquisition = true;
}

void AcqBoardONI::setFastTTLSettle (bool state, int channel)
{
    settings.fastTTLSettleEnabled = state;

    settings.fastSettleTTLChannel = channel;

    updateSettingsDuringAcquisition = true;
}

int AcqBoardONI::setNoiseSlicerLevel (int level)
{
    settings.noiseSlicerLevel = level;

    if (deviceFound)
        evalBoard->setAudioNoiseSuppress (settings.noiseSlicerLevel);

    // Level has been checked once before this and then is checked again in setAudioNoiseSuppress.
    // This may be overkill - maybe API should change so that the final function returns the value?

    return settings.noiseSlicerLevel;
}

void AcqBoardONI::enableBoardLeds (bool enable)
{
    settings.ledsEnabled = enable;

    if (isTransmitting)
        updateSettingsDuringAcquisition = true;
    else
        evalBoard->enableBoardLeds (enable);
}

int AcqBoardONI::setClockDivider (int divide_ratio)
{
    if (! deviceFound)
        return 1;

    // Divide ratio should be 1 or an even number
    if (divide_ratio != 1 && divide_ratio % 2)
        divide_ratio--;

    // Format the divide ratio from its true value to the
    // format required by the firmware
    // Ratio    N
    // 1        0
    // >=2      Ratio/2
    if (divide_ratio == 1)
        settings.clockDivideFactor = 0;
    else
        settings.clockDivideFactor = static_cast<uint16> (divide_ratio / 2);

    if (isTransmitting)
        updateSettingsDuringAcquisition = true;
    else
        evalBoard->setClockDivider (settings.clockDivideFactor);

    return divide_ratio;
}

void AcqBoardONI::setDACTriggerThreshold (int dacChannelIndex, float threshold)
{
    dacThresholds.set (dacChannelIndex, threshold);
    dacChannelsToUpdate.set (dacChannelIndex, true);
    updateSettingsDuringAcquisition = true;

    //evalBoard->setDacThresholdVoltage(dacOutput,threshold);
}

void AcqBoardONI::connectHeadstageChannelToDAC (int headstageChannelIndex, int dacChannelIndex)
{
    if (dacChannelIndex == -1)
        return;

    if (headstageChannelIndex < getNumDataOutputs (ContinuousChannel::ELECTRODE))
    {
        int channelCount = 0;
        for (int i = 0; i < enabledStreams.size(); i++)
        {
            if (headstageChannelIndex < channelCount + numChannelsPerDataStream[i])
            {
                dacChannels.set (dacChannelIndex, headstageChannelIndex - channelCount);
                dacStream.set (dacChannelIndex, i);
                break;
            }
            else
            {
                channelCount += numChannelsPerDataStream[i];
            }
        }
        dacChannelsToUpdate.set (dacChannelIndex, true);
        updateSettingsDuringAcquisition = true;
    }
}

bool AcqBoardONI::isReady()
{
    if (! deviceFound || (getNumChannels() == 0 && getNumBnos() == 0))
        return false;

    if (! checkBoardMem())
        return false;

    return true;
}

bool AcqBoardONI::startAcquisition()
{
    impedanceMeter->waitSafely();
    dataBlock.reset (new Rhd2000ONIDataBlock (evalBoard->getNumEnabledDataStreams(), evalBoard->isUSB3()));

    LOGD ("Expecting ", getNumChannels(), " channels.");

    // reset TTL output state
    for (int k = 0; k < 16; k++)
    {
        TTL_OUTPUT_STATE[k] = 0;
    }

    //LOGD( "Number of 16-bit words in FIFO: ", evalBoard->numWordsInFifo());
    //LOGD("Is eval board running: ", evalBoard->isRunning());

    //LOGD("RHD2000 data thread starting acquisition.");

    if (1)
    {
        LOGD ("Setting continuous mode");
        evalBoard->setContinuousRunMode (true);
        LOGD ("Starting board");
        evalBoard->run();
    }

    blockSize = dataBlock->calculateDataBlockSizeInWords (evalBoard->getNumEnabledDataStreams(), evalBoard->isUSB3());
    //LOGD("Expecting blocksize of ", blockSize, " for ", evalBoard->getNumEnabledDataStreams(), " streams");

    startThread();

    isTransmitting = true;

    dataSampleNumber = 0;
    memorySampleNumber = 0;
    bnoSampleNumbers = { 0, 0, 0, 0 };

    return true;
}

bool AcqBoardONI::stopAcquisition()
{
    //LOGD("RHD2000 data thread stopping acquisition.");

    if (isThreadRunning())
    {
        signalThreadShouldExit();
    }

    if (waitForThreadToExit (500))
    {
        //LOGD("RHD2000 data thread exited.");
    }
    else
    {
        //LOGD("RHD2000 data thread failed to exit, continuing anyway...");
    }

    if (deviceFound)
    {
        const ScopedLock lock (oniLock);
        evalBoard->setContinuousRunMode (false);
        evalBoard->setMaxTimeStep (0);
        evalBoard->stop();
        evalBoard->resetBoard();
    }

    if (buffer != nullptr)
        buffer->clear();

    if (memBuffer != nullptr)
        memBuffer->clear();

    for (const auto& bnoBuffer : bnoBuffers)
    {
        if (bnoBuffer != nullptr)
            bnoBuffer->clear();
    }

    isTransmitting = false;
    updateSettingsDuringAcquisition = false;

    // remove timers
    digitalOutputTimers.clear();

    // remove commands
    while (! digitalOutputCommands.empty())
        digitalOutputCommands.pop();

    return true;
}

void AcqBoardONI::run()
{
    while (! threadShouldExit())
    {
        const int nSamps = 128; //This is relatively arbitrary. Latency could be improved by adjusting both this and the usb block size depending on channel count
        oni_frame_t* frame;
        unsigned char* bufferPtr;
        int numStreams = enabledStreams.size();
        int numChannels = getNumChannels();
        int64 sampleNumber;

        //evalBoard->printFIFOmetrics();
        for (int samp = 0; samp < nSamps; samp++)
        {
            int index = 0;
            int auxIndex, chanIndex;
            int res = evalBoard->readFrame (&frame, false);

            if (res < ONI_ESUCCESS)
            {
                LOGE ("Error reading ONI frame: ", oni_error_str (res), " code ", res);
                signalThreadShouldExit();
                oni_destroy_frame (frame);
                continue;
            }
            if (frame->dev_idx == Rhd2000ONIBoard::DEVICE_RHYTHM)
            {
                if (numChannels == 0)
                {
                    oni_destroy_frame (frame);
                    continue;
                }

                int channel = -1;

                bufferPtr = (unsigned char*) frame->data + 8; //skip ONI timestamps

                if (! Rhd2000ONIDataBlock::checkUsbHeader (bufferPtr, index))
                {
                    LOGE ("Error in Rhd2000ONIBoard::readDataBlock: Incorrect header.");
                    oni_destroy_frame (frame);
                    break;
                }

                index += 8; // magic number header width (bytes)
                uint32_t rhythmSampleNum = Rhd2000ONIDataBlock::convertUsbTimeStamp (bufferPtr, index);
                index += 4; // timestamp width
                auxIndex = index; // aux chans start at this offset
                index += 6 * numStreams; // width of the 3 aux chans

                for (int dataStream = 0; dataStream < numStreams; dataStream++)
                {
                    int nChans = numChannelsPerDataStream[dataStream];

                    chanIndex = index + 2 * dataStream;

                    if ((chipId[dataStream] == CHIP_ID_RHD2132) && (nChans == 16)) //RHD2132 16ch. headstage
                    {
                        chanIndex += 2 * RHD2132_16CH_OFFSET * numStreams;
                    }

                    for (int chan = 0; chan < nChans; chan++)
                    {
                        channel++;
                        thisSample[channel] = float (*(uint16*) (bufferPtr + chanIndex) - 32768) * 0.195f;
                        chanIndex += 2 * numStreams; // single chan width (2 bytes)
                    }
                }
                index += 64 * numStreams; // neural data width
                auxIndex += 2 * numStreams; // skip AuxCmd1 slots (see updateRegisters())
                // copy the 3 aux channels
                if (settings.acquireAux)
                {
                    for (int dataStream = 0; dataStream < numStreams; dataStream++)
                    {
                        if (chipId[dataStream] != CHIP_ID_RHD2164_B)
                        {
                            int auxNum = (rhythmSampleNum + 3) % 4;
                            if (auxNum < 3)
                            {
                                auxSamples[dataStream][auxNum] = float (*(uint16*) (bufferPtr + auxIndex) - 32768) * 0.0000374;
                            }
                            for (int chan = 0; chan < 3; chan++)
                            {
                                channel++;
                                if (auxNum == 3)
                                {
                                    auxBuffer[channel] = auxSamples[dataStream][chan];
                                }
                                thisSample[channel] = auxBuffer[channel];
                            }
                        }
                        auxIndex += 2; // single chan width (2 bytes)
                    }
                }
                index += 2 * numStreams; // skip over filler word at the end of each data stream
                // copy the 8 ADC channels
                if (settings.acquireAdc)
                {
                    bool isV3 = deviceId == DEVICE_ID_V3;

                    for (int adcChan = 0; adcChan < 8; ++adcChan)
                    {
                        channel++;
                        // ADC waveform units = volts

                        auto adcValue = float (*(uint16*) (bufferPtr + index));

                        if (isV3)
                            thisSample[channel] = -(getBitVolts (ContinuousChannel::ADC) * adcValue - 5);
                        else
                            thisSample[channel] = getBitVolts (ContinuousChannel::ADC) * adcValue - 5 - 0.4096;

                        index += 2; // single chan width (2 bytes)
                    }
                }
                else
                {
                    index += 16; // skip ADC chans (8 * 2 bytes)
                }

                uint64 ttlEventWord = *(uint64*) (bufferPtr + index) & 65535;

                index += 4;

                sampleNumber = dataSampleNumber++;
                double timestamp = static_cast<double> (frame->time) / acquisitionClockHz;

                buffer->addToBuffer (thisSample,
                                     &sampleNumber,
                                     &timestamp,
                                     &ttlEventWord,
                                     1);
            }
            else if (frame->dev_idx == Rhd2000ONIBoard::DEVICE_MEMORY)
            {
                auto data = (uint32_t*) frame->data;
                float memf = 100.0f * float (*(data + 2)) / totalMemory;
                uint64 zero = 0;
                int64 tst = memorySampleNumber++;
                double tsd = static_cast<double> (frame->time) / acquisitionClockHz;
                memBuffer->addToBuffer (
                    &memf,
                    &tst,
                    &tsd,
                    &zero,
                    1);

                editor->setPercentMemoryUsed (memf);
            }
            else if (hasBNO[0] && frame->dev_idx == Rhd2000ONIBoard::DEVICE_BNO_A)
            {
                addBnoDataToBuffer (frame, bnoBuffers[0], bnoSampleNumbers[0]++);
            }
            else if (hasBNO[1] && frame->dev_idx == Rhd2000ONIBoard::DEVICE_BNO_B)
            {
                addBnoDataToBuffer (frame, bnoBuffers[1], bnoSampleNumbers[1]++);
            }
            else if (hasBNO[2] && frame->dev_idx == Rhd2000ONIBoard::DEVICE_BNO_C)
            {
                addBnoDataToBuffer (frame, bnoBuffers[2], bnoSampleNumbers[2]++);
            }
            else if (hasBNO[3] && frame->dev_idx == Rhd2000ONIBoard::DEVICE_BNO_D)
            {
                addBnoDataToBuffer (frame, bnoBuffers[3], bnoSampleNumbers[3]++);
            }
            oni_destroy_frame (frame);
        }

        if (updateSettingsDuringAcquisition)
        {
            LOGDD ("DAC");
            for (int k = 0; k < 8; k++)
            {
                if (dacChannelsToUpdate[k])
                {
                    dacChannelsToUpdate.set (k, false);
                    if (dacChannels[k] >= 0)
                    {
                        evalBoard->enableDac (k, true);
                        evalBoard->selectDacDataStream (k, dacStream[k]);
                        evalBoard->selectDacDataChannel (k, dacChannels[k]);
                        evalBoard->setDacThreshold (k, (int) abs ((dacThresholds[k] / 0.195) + 32768), dacThresholds[k] >= 0);
                        // evalBoard->setDacThresholdVoltage(k, (int) dacThresholds[k]);
                    }
                    else
                    {
                        evalBoard->enableDac (k, false);
                    }
                }
            }

            evalBoard->setTtlMode (settings.ttlOutputMode ? 1 : 0);
            evalBoard->enableExternalFastSettle (settings.fastTTLSettleEnabled);
            evalBoard->setExternalFastSettleChannel (settings.fastSettleTTLChannel);
            evalBoard->setDacHighpassFilter (settings.desiredDAChpf);
            evalBoard->enableDacHighpassFilter (settings.desiredDAChpfState);
            evalBoard->enableBoardLeds (settings.ledsEnabled);
            evalBoard->setClockDivider (settings.clockDivideFactor);

            updateSettingsDuringAcquisition = false;
        }

        if (! digitalOutputCommands.empty())
        {
            while (! digitalOutputCommands.empty())
            {
                DigitalOutputCommand command = digitalOutputCommands.front();
                TTL_OUTPUT_STATE[command.ttlLine] = command.state;
                digitalOutputCommands.pop();
            }

            evalBoard->setTtlOut (TTL_OUTPUT_STATE);

            LOGB ("TTL OUTPUT STATE: ",
                  TTL_OUTPUT_STATE[0],
                  TTL_OUTPUT_STATE[1],
                  TTL_OUTPUT_STATE[2],
                  TTL_OUTPUT_STATE[3],
                  TTL_OUTPUT_STATE[4],
                  TTL_OUTPUT_STATE[5],
                  TTL_OUTPUT_STATE[6],
                  TTL_OUTPUT_STATE[7]);
        }
    }
}

void AcqBoardONI::addBnoDataToBuffer (oni_frame_t* frame, DataBuffer* buffer, int64 sampleNumber) const
{
    int16_t* dataPtr = (int16_t*) frame->data + 4;
    uint64 zero = 0;
    double tsd = static_cast<double> (frame->time) / acquisitionClockHz;
    std::array<float, BNO_CHANNELS> bnoSamples {};

    size_t offset = 0;

    // Euler
    for (int i = 0; i < 3; i++)
    {
        bnoSamples[offset] = float (*(dataPtr + offset)) * eulerAngleScale;
        offset++;
    }

    // Quaternion
    for (int i = 0; i < 4; i++)
    {
        bnoSamples[offset] = float (*(dataPtr + offset)) * quaternionScale;
        offset++;
    }

    // Acceleration
    for (int i = 0; i < 3; i++)
    {
        bnoSamples[offset] = float (*(dataPtr + offset)) * accelerationScale;
        offset++;
    }

    // Gravity
    for (int i = 0; i < 3; i++)
    {
        bnoSamples[offset] = float (*(dataPtr + offset)) * accelerationScale;
        offset++;
    }

    // Temperature
    bnoSamples[offset] = *((uint8_t*) (dataPtr + offset));

    // Calibration
    auto calibrationStatus = *((uint8_t*) (dataPtr + offset) + 1);

    constexpr uint8_t statusMask = 0b11;

    for (int i = 0; i < 4; i++)
    {
        bnoSamples[offset + i + 1] = (calibrationStatus & (statusMask << (2 * i))) >> (2 * i);
    }

    buffer->addToBuffer (
        bnoSamples.data(),
        &sampleNumber,
        &tsd,
        &zero,
        1);
}

void AcqBoardONI::setNumHeadstageChannels (int hsNum, int numChannels)
{
    if (headstages[hsNum]->getNumChannels() == 32)
    {
        if (numChannels < headstages[hsNum]->getNumChannels())
            headstages[hsNum]->setHalfChannels (true);
        else
            headstages[hsNum]->setHalfChannels (false);

        numChannelsPerDataStream.set (headstages[hsNum]->getStreamIndex (0), numChannels);
    }

    int channelIndex = 0;

    for (auto hs : headstages)
    {
        if (hs->isConnected())
        {
            hs->setFirstChannel (channelIndex);

            channelIndex += hs->getNumActiveChannels();
        }
    }
}

int AcqBoardONI::getChannelsInHeadstage (int hsNum) const
{
    return headstages[hsNum]->getNumChannels();
}

int AcqBoardONI::getNumBnos() const
{
    int count = 0;

    for (int i = 0; i < NUMBER_OF_PORTS; i++)
    {
        if (hasBNO[i])
            count++;
    }

    return count;
}

int AcqBoardONI::getNumDataOutputs (ContinuousChannel::Type type)
{
    if (type == ContinuousChannel::ELECTRODE)
    {
        int totalChannels = 0;

        for (auto headstage : headstages)
        {
            if (headstage->isConnected())
            {
                totalChannels += headstage->getNumActiveChannels();
            }
        }

        return totalChannels;
    }
    else if (type == ContinuousChannel::AUX)
    {
        int numAuxOutputs = 0;

        if (settings.acquireAux)
        {
            for (auto headstage : headstages)
            {
                if (headstage->isConnected())
                {
                    numAuxOutputs += 3;
                }
            }
        }

        return numAuxOutputs;
    }
    else if (type == ContinuousChannel::ADC)
    {
        if (settings.acquireAdc)
        {
            return 8;
        }
        else
        {
            return 0;
        }
    }

    return 0;
}

int AcqBoardONI::getChannelFromHeadstage (int hs, int ch)
{
    int channelCount = 0;
    int hsCount = 0;
    if (hs < 0 || hs >= headstages.size() + 1)
        return -1;
    if (hs == headstages.size()) //let's consider this the ADC channels
    {
        int adcOutputs = getNumDataOutputs (ContinuousChannel::ADC);

        if (adcOutputs > 0)
        {
            return getNumDataOutputs (ContinuousChannel::ELECTRODE) + getNumDataOutputs (ContinuousChannel::AUX) + ch;
        }
        else
            return -1;
    }
    if (headstages[hs]->isConnected())
    {
        if (ch < 0)
            return -1;
        if (ch < headstages[hs]->getNumActiveChannels())
        {
            for (int i = 0; i < hs; i++)
            {
                channelCount += headstages[i]->getNumActiveChannels();
            }
            return channelCount + ch;
        }
        else if (ch < headstages[hs]->getNumActiveChannels() + 3)
        {
            for (int i = 0; i < headstages.size(); i++)
            {
                if (headstages[i]->isConnected())
                {
                    channelCount += headstages[i]->getNumActiveChannels();
                    if (i < hs)
                        hsCount++;
                }
            }
            return channelCount + hsCount * 3 + ch - headstages[hs]->getNumActiveChannels();
        }
        else
        {
            return -1;
        }
    }
    else
    {
        return -1;
    }
}

int AcqBoardONI::getHeadstageChannel (int& hs, int ch) const
{
    int channelCount = 0;
    int hsCount = 0;

    if (ch < 0)
        return -1;

    for (int i = 0; i < headstages.size(); i++)
    {
        if (headstages[i]->isConnected())
        {
            int chans = headstages[i]->getNumActiveChannels();

            if (ch >= channelCount && ch < channelCount + chans)
            {
                hs = i;
                return ch - channelCount;
            }
            channelCount += chans;
            hsCount++;
        }
    }
    if (ch < (channelCount + hsCount * 3)) //AUX
    {
        hsCount = (ch - channelCount) / 3;

        for (int i = 0; i < headstages.size(); i++)
        {
            if (headstages[i]->isConnected())
            {
                if (hsCount == 0)
                {
                    hs = i;
                    return ch - channelCount;
                }
                hsCount--;
                channelCount++;
            }
        }
    }
    return -1;
}

bool AcqBoardONI::getMemoryMonitorSupport() const
{
    return hasMemoryMonitorSupport;
}

bool AcqBoardONI::CheckSemVer (int major, int minor, int patch, int targetMajor, int targetMinor, int targetPatch)
{
    if (major > targetMajor)
        return true;
    else if (major < targetMajor)
        return false;

    if (minor > targetMinor)
        return true;
    else if (minor < targetMinor)
        return false;

    if(patch >= targetPatch)
        return true;

    return false;
}

void AcqBoardONI::ShowFirmwareUpdateMessage (std::string message)
{
    AlertWindow alert ("Update Gateware Version",
                      message,
                      MessageBoxIconType::WarningIcon);

    auto hyperlink = std::make_unique<HyperlinkButton> ("Update Gateware", URL ("https://open-ephys.github.io/acq-board-docs/User-Manual/Gateware-Update.html"));
    hyperlink->setName ("");
    hyperlink->setSize (127, 20);
    hyperlink->setJustificationType (Justification::centred);
    hyperlink->setColour (HyperlinkButton::ColourIds::textColourId, Colours::deepskyblue);

    alert.addCustomComponent (hyperlink.get());

    alert.addButton ("Okay", 0);

    alert.runModalLoop();
}
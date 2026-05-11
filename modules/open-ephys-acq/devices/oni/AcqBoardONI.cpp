/*
    ------------------------------------------------------------------
    Copyright (C) 2024 Open Ephys
    Copyright (C) 2026 Syntalos Project

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

#include "datactl/datatypes.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#define INIT_STEP 64

AcqBoardONI::AcqBoardONI()
    : AcquisitionBoard(),
      chipRegisters(30000.0f),
      m_log(Syntalos::getLogger("oeacq.board-oni"))
{
    boardType = BoardType::ONI;

    impedanceMeter = std::make_unique<ImpedanceMeterONI>(this);

    evalBoard = std::make_unique<Rhd2000ONIBoard>();

    std::memset(auxBuffer, 0, sizeof(auxBuffer));
    std::memset(auxSamples, 0, sizeof(auxSamples));

    MAX_NUM_DATA_STREAMS = evalBoard->MAX_NUM_DATA_STREAMS;
    MAX_NUM_HEADSTAGES = MAX_NUM_DATA_STREAMS / 2;

    const int maxNumHeadstages = 8;

    headstages.reserve(maxNumHeadstages);
    for (int i = 0; i < maxNumHeadstages; i++)
        headstages.push_back(std::make_unique<HeadstageONI>(
            static_cast<Rhd2000ONIBoard::BoardDataSource>(i), maxNumHeadstages));

    dacChannelsToUpdate.assign(8, true);
    dacStream.assign(8, 0);
    dacChannels.assign(8, 0);
    dacThresholds.assign(8, 0);
    for (int k = 0; k < 8; k++)
        setDACTriggerThreshold(k, 65534);

    for (int i = 0; i < NUMBER_OF_PORTS; i++) {
        hasBNO[i] = false;
        hasI2c[i] = false;
        headstageId[i] = 0;
    }

    isTransmitting = false;
}

AcqBoardONI::~AcqBoardONI()
{
    LOG_DEBUG(m_log, "RHD2000 interface destroyed.");
}

bool AcqBoardONI::checkBoardMem() const
{
    Rhd2000ONIBoard::BoardMemState memState = evalBoard->getBoardMemState();

    if (memState == Rhd2000ONIBoard::BOARDMEM_INIT) {
        LOG_DEBUG(m_log, "Memory is still initializing, wait before retrying this operation.");
        return false;
    } else if (memState == Rhd2000ONIBoard::BOARDMEM_ERR) {
        LOG_ERROR(
            m_log,
            "On-board memory error. Try closing the GUI, un-plugging the board from power and "
            "USB, and plugging everything again. If the problem persists please contact support.");
        return false;
    } else if (memState == Rhd2000ONIBoard::BOARDMEM_INVALID) {
        LOG_ERROR(m_log, "Invalid memory operation.");
        return false;
    } else if (memState == Rhd2000ONIBoard::BOARDMEM_OK) {
        return true;
    }

    LOG_DEBUG(m_log, "Unknown memory state. Board returned {}", static_cast<int>(memState));
    return false;
}

bool AcqBoardONI::detectBoard()
{
    LOG_INFO(m_log, "Searching for ONI Acquisition Board...");
    const oni_driver_info_t *driverInfo;
    const int return_code = evalBoard->open(&driverInfo);

    if (return_code == 1) {
        LOG_INFO(m_log, "Board opened successfully.");

        int major, minor, patch, rc;
        evalBoard->getONIVersion(&major, &minor, &patch);
        LOG_INFO(m_log, "ONI Library version: {}.{}.{}", major, minor, patch);
        LOG_INFO(
            m_log,
            "ONI Driver: {} Version: {}.{}.{}{}{}",
            driverInfo->name,
            driverInfo->major,
            driverInfo->minor,
            driverInfo->patch,
            driverInfo->pre_release ? "-" : "",
            driverInfo->pre_release ? driverInfo->pre_release : "");
        if (evalBoard->getFTDriverInfo(&major, &minor, &patch))
            LOG_INFO(m_log, "FTDI Driver version: {}.{}.{}", major, minor, patch);
        if (evalBoard->getFTLibInfo(&major, &minor, &patch))
            LOG_INFO(m_log, "FTDI Library version: {}.{}.{}", major, minor, patch);
        if (evalBoard->getFirmwareVersion(&major, &minor, &patch, &rc)) {
            if (rc != 0) {
                const char *tag;
                switch (rc & 0xC0) {
                case 0x80:
                    tag = "-rc";
                    break;
                case 0xC0:
                    tag = "-experimental";
                    break;
                default:
                    tag = "-beta";
                }
                LOG_INFO(
                    m_log,
                    "Open Ephys ECP5-ONI FPGA open. Gateware version v{}.{}.{}{}{}",
                    major,
                    minor,
                    patch,
                    tag,
                    static_cast<int>(rc & 0x3F));
            } else {
                LOG_INFO(
                    m_log,
                    "Open Ephys ECP5-ONI FPGA open. Gateware version v{}.{}.{}",
                    major,
                    minor,
                    patch);
            }
            hasI2cSupport = CheckSemVer(major, minor, patch, 1, 5, 0);
            hasMemoryMonitorSupport = CheckSemVer(major, minor, patch, 1, 5, 1);

            if (major == 0)
                emitMessage(
                    MessageSeverity::Warning,
                    "The detected gateware is v" + std::to_string(major) + "."
                        + std::to_string(minor) + "." + std::to_string(patch)
                        + " and should be updated to the latest version.\n"
                          "https://open-ephys.github.io/acq-board-docs/User-Manual/Gateware-Update.html");
        }
        oni_reg_val_t tmpId;
        if (evalBoard->getDeviceId(&tmpId)) {
            deviceId = tmpId;
            LOG_INFO(m_log, "Acquisition board ID = {}", deviceId);
        }

        deviceFound = true;

        if (hasMemoryMonitorSupport) {
            evalBoard->enableMemoryMonitor(true);
            evalBoard->setMemoryMonitorSampleRate(MEMORY_MONITOR_FS);
            evalBoard->getTotalMemory(&totalMemory);
        } else {
            evalBoard->enableMemoryMonitor(false);
        }

        return true;
    } else {
        if (return_code == -1)
            LOG_INFO(m_log, "ONI FT600 driver library not found.");
        else if (return_code == -2)
            LOG_INFO(m_log, "No ONI Acquisition Board found.");
        else if (return_code == -3)
            emitMessage(
                MessageSeverity::Error,
                "The gateware on the acquisition board is not compatible with this version of "
                "the plugin and needs to be updated to v1.5.3 or greater.\n"
                "https://open-ephys.github.io/acq-board-docs/User-Manual/Gateware-Update.html");
        deviceFound = false;
        return false;
    }
}

bool AcqBoardONI::initializeBoard()
{
    return initializeBoardInThread();
}

bool AcqBoardONI::initializeBoardInThread()
{
    LOG_INFO(m_log, "Initializing ONI Acquisition Board...");
    LOG_DEBUG(m_log, "Initializing RHD2000 board.");
    evalBoard->initialize();

    setSampleRate(30000, false);

    evalBoard->setCableLengthMeters(Rhd2000ONIBoard::PortA, settings.cableLength.portA);
    evalBoard->setCableLengthMeters(Rhd2000ONIBoard::PortB, settings.cableLength.portB);
    evalBoard->setCableLengthMeters(Rhd2000ONIBoard::PortC, settings.cableLength.portC);
    evalBoard->setCableLengthMeters(Rhd2000ONIBoard::PortD, settings.cableLength.portD);

    // Select RAM Bank 0 for AuxCmd3 initially, so the ADC is calibrated.
    evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortA, Rhd2000ONIBoard::AuxCmd3, 0);
    evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortB, Rhd2000ONIBoard::AuxCmd3, 0);
    evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortC, Rhd2000ONIBoard::AuxCmd3, 0);
    evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortD, Rhd2000ONIBoard::AuxCmd3, 0);

    evalBoard->setMaxTimeStep(INIT_STEP);
    evalBoard->setContinuousRunMode(false);

    // Start SPI interface and wait for the run to complete.
    evalBoard->run();
    while (evalBoard->isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    {
        const std::lock_guard<std::mutex> lock(oniLock);
        evalBoard->stop();
    }

    // Now that ADC calibration has been performed, switch to the command sequence
    // that does not execute ADC calibration.
    evalBoard->selectAuxCommandBank(
        Rhd2000ONIBoard::PortA, Rhd2000ONIBoard::AuxCmd3, settings.fastSettleEnabled ? 2 : 1);
    evalBoard->selectAuxCommandBank(
        Rhd2000ONIBoard::PortB, Rhd2000ONIBoard::AuxCmd3, settings.fastSettleEnabled ? 2 : 1);
    evalBoard->selectAuxCommandBank(
        Rhd2000ONIBoard::PortC, Rhd2000ONIBoard::AuxCmd3, settings.fastSettleEnabled ? 2 : 1);
    evalBoard->selectAuxCommandBank(
        Rhd2000ONIBoard::PortD, Rhd2000ONIBoard::AuxCmd3, settings.fastSettleEnabled ? 2 : 1);

    evalBoard->getAcquisitionClockHz(&acquisitionClockHz);

    return true;
}

bool AcqBoardONI::foundInputSource() const
{
    return deviceFound;
}

std::vector<const Headstage *> AcqBoardONI::getHeadstages()
{
    std::vector<const Headstage *> connected;
    connected.reserve(headstages.size());
    for (const auto &hs : headstages) {
        if (hs->isConnected())
            connected.push_back(hs.get());
    }
    return connected;
}

std::vector<int> AcqBoardONI::getAvailableSampleRates()
{
    return {1000, 1250, 1500, 2000, 2500, 3000, 3333, 4000, 5000,
            6250, 8000, 10000, 12500, 15000, 20000, 25000, 30000};
}

void AcqBoardONI::setSampleRate(int desiredSampleRate)
{
    setSampleRate(desiredSampleRate, true);
}

void AcqBoardONI::setSampleRate(int desiredSampleRate, bool reScanDelays)
{
    Rhd2000ONIBoard::AmplifierSampleRate sampleRate;

    switch (desiredSampleRate) {
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
        LOG_INFO(m_log, "Invalid sample rate.");
        return;
    }

    {
        const std::lock_guard<std::mutex> lock(oniLock);
        evalBoard->setSampleRate(sampleRate);
    }
    LOG_DEBUG(m_log, "Sample rate set to {}", evalBoard->getSampleRate());

    if (reScanDelays)
        checkAllCableDelays();

    updateRegisters();
}

void AcqBoardONI::checkAllCableDelays()
{
    std::vector<bool> cableIsConnected(4, false);
    for (int cableIndex = 0; cableIndex < 4; cableIndex++) {
        cableIsConnected[cableIndex] = headstages[cableIndex * 2]->isConnected()
                                       || headstages[cableIndex * 2 + 1]->isConnected();
    }

    LOG_DEBUG(m_log, "Number of enabled data streams: {}", evalBoard->getNumEnabledDataStreams());

    evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortA, Rhd2000ONIBoard::AuxCmd3, 0);
    evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortB, Rhd2000ONIBoard::AuxCmd3, 0);
    evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortC, Rhd2000ONIBoard::AuxCmd3, 0);
    evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortD, Rhd2000ONIBoard::AuxCmd3, 0);

    evalBoard->setMaxTimeStep(128 * INIT_STEP);
    evalBoard->setContinuousRunMode(false);

    auto delayDataBlock = std::make_unique<Rhd2000ONIDataBlock>(
        evalBoard->getNumEnabledDataStreams(), evalBoard->isUSB3());

    std::vector<int> sumGoodDelays(8, 0);
    std::vector<int> indexFirstGoodDelay(8, -1);
    std::vector<int> indexSecondGoodDelay(8, -1);

    LOG_DEBUG(m_log, "Checking for connected amplifier chips...");

    int register59Value;

    for (int delay = -2; delay < 2; delay++) {
        LOG_DEBUG(m_log, "Setting delay to: {}", delay);

        const auto clamp = [](int v) { return std::clamp(v, 0, 16); };
        const int delayA = clamp(static_cast<int>(settings.optimumDelay.portA) + delay);
        const int delayB = clamp(static_cast<int>(settings.optimumDelay.portB) + delay);
        const int delayC = clamp(static_cast<int>(settings.optimumDelay.portC) + delay);
        const int delayD = clamp(static_cast<int>(settings.optimumDelay.portD) + delay);

        evalBoard->setCableDelay(Rhd2000ONIBoard::PortA, delayA);
        evalBoard->setCableDelay(Rhd2000ONIBoard::PortB, delayB);
        evalBoard->setCableDelay(Rhd2000ONIBoard::PortC, delayC);
        evalBoard->setCableDelay(Rhd2000ONIBoard::PortD, delayD);

        evalBoard->run();
        evalBoard->readDataBlock(delayDataBlock.get(), INIT_STEP);
        {
            const std::lock_guard<std::mutex> lock(oniLock);
            evalBoard->stop();
        }

        for (size_t hs = 0; hs < headstages.size(); ++hs) {
            if (!headstages[hs]->isConnected())
                continue;

            const int id = getIntanChipId(
                delayDataBlock.get(), headstages[hs]->getStreamIndex(0), register59Value);

            if (id == CHIP_ID_RHD2132 || id == CHIP_ID_RHD2216
                || (id == CHIP_ID_RHD2164 && register59Value == REGISTER_59_MISO_A)) {
                LOG_DEBUG(m_log, "Device ID found: {}", id);
                sumGoodDelays[hs] += 1;
                if (indexFirstGoodDelay[hs] == -1)
                    indexFirstGoodDelay[hs] = delay;
                else if (indexSecondGoodDelay[hs] == -1)
                    indexSecondGoodDelay[hs] = delay;
            }
        }
    }

    std::vector<int> optimumDelay(headstages.size(), -5);
    for (size_t hs = 0; hs < headstages.size(); ++hs) {
        if (sumGoodDelays[hs] == 1 || sumGoodDelays[hs] == 2)
            optimumDelay[hs] = indexFirstGoodDelay[hs];
        else if (sumGoodDelays[hs] > 2)
            optimumDelay[hs] = indexSecondGoodDelay[hs];
    }

    if (cableIsConnected[0]) {
        const int delayShift = std::max(optimumDelay[0], optimumDelay[1]);
        evalBoard->setCableDelay(Rhd2000ONIBoard::PortA, settings.optimumDelay.portA + delayShift);
        LOG_DEBUG(
            m_log,
            "Port A cable delay at {} samples/sec: {}",
            settings.boardSampleRate,
            settings.optimumDelay.portA + delayShift);
    }
    if (cableIsConnected[1]) {
        const int delayShift = std::max(optimumDelay[2], optimumDelay[3]);
        evalBoard->setCableDelay(Rhd2000ONIBoard::PortB, settings.optimumDelay.portB + delayShift);
        LOG_DEBUG(
            m_log,
            "Port B cable delay at {} samples/sec: {}",
            settings.boardSampleRate,
            settings.optimumDelay.portB + delayShift);
    }
    if (cableIsConnected[2]) {
        const int delayShift = std::max(optimumDelay[4], optimumDelay[5]);
        evalBoard->setCableDelay(Rhd2000ONIBoard::PortC, settings.optimumDelay.portC + delayShift);
        LOG_DEBUG(
            m_log,
            "Port C cable delay at {} samples/sec: {}",
            settings.boardSampleRate,
            settings.optimumDelay.portC + delayShift);
    }
    if (cableIsConnected[3]) {
        const int delayShift = std::max(optimumDelay[6], optimumDelay[7]);
        evalBoard->setCableDelay(Rhd2000ONIBoard::PortD, settings.optimumDelay.portD + delayShift);
        LOG_DEBUG(
            m_log,
            "Port D cable delay at {} samples/sec: {}",
            settings.boardSampleRate,
            settings.optimumDelay.portD + delayShift);
    }
}

float AcqBoardONI::getSampleRate() const
{
    return settings.boardSampleRate;
}

void AcqBoardONI::updateRegisters()
{
    if (!deviceFound)
        return;

    chipRegisters.defineSampleRate(settings.boardSampleRate);

    int commandSequenceLength;
    std::vector<int> commandList;

    if (!commonCommandsSet) {
        LOG_DEBUG(m_log, "Uploading common commands");
        chipRegisters.setDigOutLow();
        commandSequenceLength = chipRegisters.createCommandListUpdateDigOut(commandList);
        evalBoard->uploadCommandList(commandList, Rhd2000ONIBoard::AuxCmd1, 0);
        evalBoard->selectAuxCommandLength(Rhd2000ONIBoard::AuxCmd1, 0, commandSequenceLength - 1);
        evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortA, Rhd2000ONIBoard::AuxCmd1, 0);
        evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortB, Rhd2000ONIBoard::AuxCmd1, 0);
        evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortC, Rhd2000ONIBoard::AuxCmd1, 0);
        evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortD, Rhd2000ONIBoard::AuxCmd1, 0);

        commandSequenceLength = chipRegisters.createCommandListTempSensor(commandList);
        evalBoard->uploadCommandList(commandList, Rhd2000ONIBoard::AuxCmd2, 0);
        evalBoard->selectAuxCommandLength(Rhd2000ONIBoard::AuxCmd2, 0, commandSequenceLength - 1);
        evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortA, Rhd2000ONIBoard::AuxCmd2, 0);
        evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortB, Rhd2000ONIBoard::AuxCmd2, 0);
        evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortC, Rhd2000ONIBoard::AuxCmd2, 0);
        evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortD, Rhd2000ONIBoard::AuxCmd2, 0);

        commonCommandsSet = true;
    }

    settings.dsp.cutoffFreq = chipRegisters.setDspCutoffFreq(settings.dsp.cutoffFreq);
    settings.analogFilter.lowerBandwidth =
        chipRegisters.setLowerBandwidth(settings.analogFilter.lowerBandwidth);
    settings.analogFilter.upperBandwidth =
        chipRegisters.setUpperBandwidth(settings.analogFilter.upperBandwidth);
    chipRegisters.enableDsp(settings.dsp.enabled);

    chipRegisters.enableAux1(settings.acquireAux);
    chipRegisters.enableAux2(settings.acquireAux);
    chipRegisters.enableAux3(settings.acquireAux);

    commandSequenceLength = chipRegisters.createCommandListRegisterConfig(commandList, true);
    evalBoard->uploadCommandList(commandList, Rhd2000ONIBoard::AuxCmd3, 0);
    evalBoard->selectAuxCommandLength(Rhd2000ONIBoard::AuxCmd3, 0, commandSequenceLength - 1);

    commandSequenceLength = chipRegisters.createCommandListRegisterConfig(commandList, false);
    evalBoard->uploadCommandList(commandList, Rhd2000ONIBoard::AuxCmd3, 1);
    evalBoard->selectAuxCommandLength(Rhd2000ONIBoard::AuxCmd3, 0, commandSequenceLength - 1);

    chipRegisters.setFastSettle(true);

    commandSequenceLength = chipRegisters.createCommandListRegisterConfig(commandList, false);
    evalBoard->uploadCommandList(commandList, Rhd2000ONIBoard::AuxCmd3, 2);
    evalBoard->selectAuxCommandLength(Rhd2000ONIBoard::AuxCmd3, 0, commandSequenceLength - 1);

    chipRegisters.setFastSettle(false);
    evalBoard->selectAuxCommandBank(
        Rhd2000ONIBoard::PortA, Rhd2000ONIBoard::AuxCmd3, settings.fastSettleEnabled ? 2 : 1);
    evalBoard->selectAuxCommandBank(
        Rhd2000ONIBoard::PortB, Rhd2000ONIBoard::AuxCmd3, settings.fastSettleEnabled ? 2 : 1);
    evalBoard->selectAuxCommandBank(
        Rhd2000ONIBoard::PortC, Rhd2000ONIBoard::AuxCmd3, settings.fastSettleEnabled ? 2 : 1);
    evalBoard->selectAuxCommandBank(
        Rhd2000ONIBoard::PortD, Rhd2000ONIBoard::AuxCmd3, settings.fastSettleEnabled ? 2 : 1);
}

int AcqBoardONI::getIntanChipId(Rhd2000ONIDataBlock *dataBlock, int stream, int &register59Value)
{
    if (stream < 0)
        return 0;

    const bool intanChipPresent =
        ((char)dataBlock->auxiliaryData[stream][2][32] == 'I'
         && (char)dataBlock->auxiliaryData[stream][2][33] == 'N'
         && (char)dataBlock->auxiliaryData[stream][2][34] == 'T'
         && (char)dataBlock->auxiliaryData[stream][2][35] == 'A'
         && (char)dataBlock->auxiliaryData[stream][2][36] == 'N'
         && (char)dataBlock->auxiliaryData[stream][2][24] == 'R'
         && (char)dataBlock->auxiliaryData[stream][2][25] == 'H'
         && (char)dataBlock->auxiliaryData[stream][2][26] == 'D');

    if (!intanChipPresent) {
        register59Value = -1;
        return -1;
    }
    register59Value = dataBlock->auxiliaryData[stream][2][23]; // Register 59
    return dataBlock->auxiliaryData[stream][2][19];            // chip ID (Register 63)
}

void AcqBoardONI::scanPorts()
{
    scanPortsInThread();
}

void AcqBoardONI::scanPortsInThread()
{
    if (!deviceFound)
        return;
    if (!checkBoardMem())
        return;

    if (hasI2cSupport) {
        bool enableI2c[NUMBER_OF_PORTS] = {true, true, true, true};

        evalBoard->enableI2cMode(enableI2c);
        evalBoard->resetBoard();

        for (int i = 0; i < NUMBER_OF_PORTS; i += 1) {
            hasBNO[i] = evalBoard->isBnoConnected(i);
            evalBoard->enableBnoStream(i, hasBNO[i]);

            hasI2c[i] = evalBoard->isI2cCapable(i);
            headstageId[i] = hasI2c[i] ? evalBoard->getDeviceIdOnEeprom(i) : 0;

            if (hasBNO[i]) {
                if (headstageId[i] == 0) {
                    LOG_DEBUG(m_log,
                              "Found headstage with BNO but no EEPROM. Assuming low-profile 3D headstage.");
                    evalBoard->setBnoAxisMap(i, 0b00100100);
                } else {
                    std::stringstream ss;
                    ss << "0x" << std::hex << headstageId[i];
                    LOG_DEBUG(m_log, "Headstage ID {} found on port {}", ss.str(), char('A' + i));

                    switch (headstageId[i]) {
                    case 0x80000001:
                        evalBoard->setBnoAxisMap(i, 0b00000100100);
                        break;
                    case 0x80000002:
                        evalBoard->setBnoAxisMap(i, 0b10000011000);
                        break;
                    case 0x80000003:
                        evalBoard->setBnoAxisMap(i, 0b10000011000);
                        break;
                    default:
                        evalBoard->setBnoAxisMap(i, 0b00000100100);
                        break;
                    }
                }
            }

            enableI2c[i] = hasBNO[i] || (hasI2c[i] && headstageId[i] != 0);
        }

        evalBoard->enableI2cMode(enableI2c);
        evalBoard->resetBoard();
    }

    enabledStreams.clear();
    numChannelsPerDataStream.clear();

    int register59Value;

    for (auto &headstage : headstages)
        headstage->setNumStreams(0);

    Rhd2000ONIBoard::BoardDataSource initStreamPorts[8] = {
        Rhd2000ONIBoard::PortA1,
        Rhd2000ONIBoard::PortA2,
        Rhd2000ONIBoard::PortB1,
        Rhd2000ONIBoard::PortB2,
        Rhd2000ONIBoard::PortC1,
        Rhd2000ONIBoard::PortC2,
        Rhd2000ONIBoard::PortD1,
        Rhd2000ONIBoard::PortD2};

    // chipId is indexed by stream (0..MAX_NUM_DATA_STREAMS-1) by both this
    // routine and the impedance scan; size it to the full data-stream count
    // so out-of-range vector accesses can't trip _GLIBCXX_ASSERTIONS. The
    // upstream JUCE Array<int> tolerated out-of-bounds reads silently.
    chipId.assign(MAX_NUM_DATA_STREAMS, -1);
    // tmpChipId is indexed by headstage (0..7) — one entry per headstage.
    std::vector<int> tmpChipId(8, -1);

    const float currentSampleRate = settings.boardSampleRate;

    setSampleRate(30000, false);

    for (int i = 0; i < 8; i++)
        evalBoard->setDataSource(i, initStreamPorts[i]);

    for (int i = 0; i < 8; i++)
        evalBoard->enableDataStream(i, true);
    {
        const std::lock_guard<std::mutex> lock(oniLock);
        evalBoard->updateStreamBlockSize();
    }

    LOG_DEBUG(m_log, "Number of enabled data streams: {}", evalBoard->getNumEnabledDataStreams());

    evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortA, Rhd2000ONIBoard::AuxCmd3, 0);
    evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortB, Rhd2000ONIBoard::AuxCmd3, 0);
    evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortC, Rhd2000ONIBoard::AuxCmd3, 0);
    evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortD, Rhd2000ONIBoard::AuxCmd3, 0);

    evalBoard->setMaxTimeStep(128 * INIT_STEP);
    evalBoard->setContinuousRunMode(false);

    auto scanDataBlock = std::make_unique<Rhd2000ONIDataBlock>(
        evalBoard->getNumEnabledDataStreams(), evalBoard->isUSB3());

    std::vector<int> sumGoodDelays(8, 0);
    std::vector<int> indexFirstGoodDelay(8, -1);
    std::vector<int> indexSecondGoodDelay(8, -1);

    LOG_DEBUG(m_log, "Checking for connected amplifier chips...");

    for (int delay = 0; delay < 16; delay++) {
        LOG_DEBUG(m_log, "Setting delay to: {}", delay);

        evalBoard->setCableDelay(Rhd2000ONIBoard::PortA, delay);
        evalBoard->setCableDelay(Rhd2000ONIBoard::PortB, delay);
        evalBoard->setCableDelay(Rhd2000ONIBoard::PortC, delay);
        evalBoard->setCableDelay(Rhd2000ONIBoard::PortD, delay);

        evalBoard->run();
        evalBoard->readDataBlock(scanDataBlock.get(), INIT_STEP);
        {
            const std::lock_guard<std::mutex> lock(oniLock);
            evalBoard->stop();
        }

        for (size_t hs = 0; hs < headstages.size(); ++hs) {
            const int id =
                getIntanChipId(scanDataBlock.get(), static_cast<int>(hs), register59Value);

            if (id == CHIP_ID_RHD2132 || id == CHIP_ID_RHD2216
                || (id == CHIP_ID_RHD2164 && register59Value == REGISTER_59_MISO_A)) {
                LOG_DEBUG(m_log, "Device ID found: {}", id);
                sumGoodDelays[hs] += 1;
                if (indexFirstGoodDelay[hs] == -1) {
                    indexFirstGoodDelay[hs] = delay;
                    tmpChipId[hs] = id;
                } else if (indexSecondGoodDelay[hs] == -1) {
                    indexSecondGoodDelay[hs] = delay;
                    tmpChipId[hs] = id;
                }
            }
        }
    }

    int chipIdx = 0;
    for (size_t hs = 0; hs < headstages.size(); ++hs) {
        if ((tmpChipId[hs] > 0) && (static_cast<int>(enabledStreams.size()) < MAX_NUM_DATA_STREAMS)) {
            chipId[chipIdx++] = tmpChipId[hs];
            const bool bno = hasBNO[hs / 2];
            LOG_DEBUG(m_log, "Enabling headstage {}", hs);

            if (tmpChipId[hs] == CHIP_ID_RHD2164) {
                if (static_cast<int>(enabledStreams.size()) < MAX_NUM_DATA_STREAMS - 1) {
                    enableHeadstage(static_cast<int>(hs), true, 2, 32, bno);
                    chipId[chipIdx++] = CHIP_ID_RHD2164_B;
                } else {
                    enableHeadstage(static_cast<int>(hs), true, 1, 32, bno);
                }
            } else {
                enableHeadstage(static_cast<int>(hs), true, 1, tmpChipId[hs] == 1 ? 32 : 16, bno);
            }
        } else if (hs % 2 == 1 && hasBNO[hs / 2]) {
            enableHeadstage(static_cast<int>(hs), true, 0, 0, true);
        } else {
            enableHeadstage(static_cast<int>(hs), false);
        }
    }

    updateBoardStreams();

    LOG_DEBUG(m_log, "Number of enabled data streams: {}", evalBoard->getNumEnabledDataStreams());

    std::vector<int> optimumDelay(headstages.size(), 0);
    for (size_t hs = 0; hs < headstages.size(); ++hs) {
        if (sumGoodDelays[hs] == 1 || sumGoodDelays[hs] == 2)
            optimumDelay[hs] = indexFirstGoodDelay[hs];
        else if (sumGoodDelays[hs] > 2)
            optimumDelay[hs] = indexSecondGoodDelay[hs];
    }

    settings.optimumDelay.portA = std::max(optimumDelay[0], optimumDelay[1]);
    settings.optimumDelay.portB = std::max(optimumDelay[2], optimumDelay[3]);
    settings.optimumDelay.portC = std::max(optimumDelay[4], optimumDelay[5]);
    settings.optimumDelay.portD = std::max(optimumDelay[6], optimumDelay[7]);

    evalBoard->setCableDelay(Rhd2000ONIBoard::PortA, settings.optimumDelay.portA);
    evalBoard->setCableDelay(Rhd2000ONIBoard::PortB, settings.optimumDelay.portB);
    evalBoard->setCableDelay(Rhd2000ONIBoard::PortC, settings.optimumDelay.portC);
    evalBoard->setCableDelay(Rhd2000ONIBoard::PortD, settings.optimumDelay.portD);

    LOG_DEBUG(m_log, "Set optimum delay for port A: {}", settings.optimumDelay.portA);
    LOG_DEBUG(m_log, "Set optimum delay for port B: {}", settings.optimumDelay.portB);
    LOG_DEBUG(m_log, "Set optimum delay for port C: {}", settings.optimumDelay.portC);
    LOG_DEBUG(m_log, "Set optimum delay for port D: {}", settings.optimumDelay.portD);

    setSampleRate(currentSampleRate, !initialScan);
    initialScan = false;
}

void AcqBoardONI::setCableLength(int hsNum, float length)
{
    switch (hsNum) {
    case 0:
        evalBoard->setCableLengthFeet(Rhd2000ONIBoard::PortA, length);
        break;
    case 1:
        evalBoard->setCableLengthFeet(Rhd2000ONIBoard::PortB, length);
        break;
    case 2:
        evalBoard->setCableLengthFeet(Rhd2000ONIBoard::PortC, length);
        break;
    case 3:
        evalBoard->setCableLengthFeet(Rhd2000ONIBoard::PortD, length);
        break;
    default:
        break;
    }
}

bool AcqBoardONI::enableHeadstage(int hsNum, bool enabled, int nStr, int strChans, bool hasBno)
{
    LOG_DEBUG(
        m_log,
        "Headstage {}, enabled: {}, num streams: {}, stream channels: {}",
        hsNum,
        enabled,
        nStr,
        strChans);

    if (enabled) {
        headstages[hsNum]->setFirstChannel(getNumDataOutputs(ChannelKind::Electrode));
        headstages[hsNum]->setNumStreams(nStr);
        headstages[hsNum]->setChannelsPerStream(strChans);
        headstages[hsNum]->setHasBno(hasBno);

        if (nStr > 0) {
            headstages[hsNum]->setFirstStreamIndex(static_cast<int>(enabledStreams.size()));
            enabledStreams.push_back(headstages[hsNum]->getDataStream(0));
            numChannelsPerDataStream.push_back(strChans);
        }
        if (nStr > 1) {
            enabledStreams.push_back(headstages[hsNum]->getDataStream(1));
            numChannelsPerDataStream.push_back(strChans);
        }
    } else {
        // Remove the headstage's stream(s) from enabledStreams + numChannelsPerDataStream.
        const auto findStream = [&](Rhd2000ONIBoard::BoardDataSource s) {
            return std::find(enabledStreams.begin(), enabledStreams.end(), s);
        };
        auto it = findStream(headstages[hsNum]->getDataStream(0));
        if (it != enabledStreams.end()) {
            const auto idx = std::distance(enabledStreams.begin(), it);
            enabledStreams.erase(it);
            numChannelsPerDataStream.erase(numChannelsPerDataStream.begin() + idx);
        }
        if (headstages[hsNum]->getNumStreams() > 1) {
            it = findStream(headstages[hsNum]->getDataStream(1));
            if (it != enabledStreams.end()) {
                const auto idx = std::distance(enabledStreams.begin(), it);
                enabledStreams.erase(it);
                numChannelsPerDataStream.erase(numChannelsPerDataStream.begin() + idx);
            }
        }

        headstages[hsNum]->setNumStreams(0);
        headstages[hsNum]->setHasBno(hasBno);
    }

    return true;
}

void AcqBoardONI::updateBoardStreams()
{
    for (int i = 0; i < MAX_NUM_DATA_STREAMS; i++) {
        if (i < static_cast<int>(enabledStreams.size())) {
            evalBoard->enableDataStream(i, true);
            evalBoard->setDataSource(i, enabledStreams[i]);
        } else {
            evalBoard->enableDataStream(i, false);
        }
    }
    evalBoard->updateStreamBlockSize();
}

bool AcqBoardONI::isHeadstageEnabled(int hsNum) const
{
    return headstages[hsNum]->isConnected();
}

int AcqBoardONI::getActiveChannelsInHeadstage(int hsNum) const
{
    return headstages[hsNum]->getNumActiveChannels();
}

float AcqBoardONI::getBitVolts(ChannelKind kind) const
{
    switch (kind) {
    case ChannelKind::Electrode:
        return 0.195f;
    case ChannelKind::Aux:
        return 0.0000374f;
    case ChannelKind::Adc:
        if (deviceId == DEVICE_ID_V3)
            return static_cast<float>(v3AdcBitVal);
        return 0.00015258789f;
    }
    return 1.0f;
}

void AcqBoardONI::measureImpedances()
{
    if (!impedanceMeter)
        return;
    impedanceMeter->resetStopRequest();
    impedanceMeter->runImpedanceMeasurement(impedances);
}

void AcqBoardONI::impedanceMeasurementFinished()
{
    if (!impedances.valid)
        return;

    LOG_DEBUG(m_log, "Updating headstage impedance values");
    for (auto &hs : headstages) {
        if (hs->isConnected())
            hs->setImpedances(impedances);
    }
}

void AcqBoardONI::setNamingScheme(ChannelNamingScheme scheme)
{
    channelNamingScheme = scheme;
    for (auto &hs : headstages)
        hs->setNamingScheme(scheme);
}

ChannelNamingScheme AcqBoardONI::getNamingScheme()
{
    return channelNamingScheme;
}

void AcqBoardONI::enableAuxChannels(bool enabled)
{
    settings.acquireAux = enabled;
    updateRegisters();
}

bool AcqBoardONI::areAuxChannelsEnabled() const
{
    return settings.acquireAux;
}

void AcqBoardONI::enableAdcChannels(bool enabled)
{
    settings.acquireAdc = enabled;
}

bool AcqBoardONI::areAdcChannelsEnabled() const
{
    return settings.acquireAdc;
}

double AcqBoardONI::setUpperBandwidth(double upper)
{
    settings.analogFilter.upperBandwidth = upper;
    updateRegisters();
    return settings.analogFilter.upperBandwidth;
}

double AcqBoardONI::setLowerBandwidth(double lower)
{
    settings.analogFilter.lowerBandwidth = lower;
    updateRegisters();
    return settings.analogFilter.lowerBandwidth;
}

double AcqBoardONI::setDspCutoffFreq(double freq)
{
    settings.dsp.cutoffFreq = freq;
    updateRegisters();
    return settings.dsp.cutoffFreq;
}

double AcqBoardONI::getDspCutoffFreq() const
{
    return settings.dsp.cutoffFreq;
}

void AcqBoardONI::setDspOffset(bool state)
{
    settings.dsp.enabled = state;
    updateRegisters();
}

void AcqBoardONI::setTTLOutputMode(bool state)
{
    settings.ttlOutputMode = state;
    updateSettingsDuringAcquisition = true;
}

void AcqBoardONI::setDAChpf(float cutoff, bool enabled)
{
    settings.desiredDAChpf = cutoff;
    settings.desiredDAChpfState = enabled;
    updateSettingsDuringAcquisition = true;
}

void AcqBoardONI::setFastTTLSettle(bool state, int channel)
{
    settings.fastTTLSettleEnabled = state;
    settings.fastSettleTTLChannel = channel;
    updateSettingsDuringAcquisition = true;
}

int AcqBoardONI::setNoiseSlicerLevel(int level)
{
    settings.noiseSlicerLevel = level;
    if (deviceFound)
        evalBoard->setAudioNoiseSuppress(settings.noiseSlicerLevel);
    return settings.noiseSlicerLevel;
}

void AcqBoardONI::enableBoardLeds(bool enable)
{
    settings.ledsEnabled = enable;
    if (isTransmitting)
        updateSettingsDuringAcquisition = true;
    else
        evalBoard->enableBoardLeds(enable);
}

int AcqBoardONI::setClockDivider(int divide_ratio)
{
    if (!deviceFound)
        return 1;

    if (divide_ratio != 1 && divide_ratio % 2)
        divide_ratio--;

    if (divide_ratio == 1)
        settings.clockDivideFactor = 0;
    else
        settings.clockDivideFactor = static_cast<std::uint16_t>(divide_ratio / 2);

    if (isTransmitting)
        updateSettingsDuringAcquisition = true;
    else
        evalBoard->setClockDivider(settings.clockDivideFactor);

    return divide_ratio;
}

void AcqBoardONI::setDACTriggerThreshold(int dacChannelIndex, float threshold)
{
    dacThresholds[dacChannelIndex] = static_cast<int>(threshold);
    dacChannelsToUpdate[dacChannelIndex] = true;
    updateSettingsDuringAcquisition = true;
}

void AcqBoardONI::connectHeadstageChannelToDAC(int headstageChannelIndex, int dacChannelIndex)
{
    if (dacChannelIndex == -1)
        return;

    if (headstageChannelIndex < getNumDataOutputs(ChannelKind::Electrode)) {
        int channelCount = 0;
        for (size_t i = 0; i < enabledStreams.size(); i++) {
            if (headstageChannelIndex < channelCount + numChannelsPerDataStream[i]) {
                dacChannels[dacChannelIndex] = headstageChannelIndex - channelCount;
                dacStream[dacChannelIndex] = static_cast<int>(i);
                break;
            } else {
                channelCount += numChannelsPerDataStream[i];
            }
        }
        dacChannelsToUpdate[dacChannelIndex] = true;
        updateSettingsDuringAcquisition = true;
    }
}

bool AcqBoardONI::isReady()
{
    if (!deviceFound || (getNumChannels() == 0 && getNumBnos() == 0))
        return false;
    if (!checkBoardMem())
        return false;
    return true;
}

bool AcqBoardONI::startAcquisition()
{
    dataBlock.reset(
        new Rhd2000ONIDataBlock(evalBoard->getNumEnabledDataStreams(), evalBoard->isUSB3()));

    LOG_DEBUG(m_log, "Expecting {} channels.", getNumChannels());

    for (int k = 0; k < 16; k++)
        TTL_OUTPUT_STATE[k] = 0;

    LOG_DEBUG(m_log, "Setting continuous mode");
    evalBoard->setContinuousRunMode(true);
    LOG_DEBUG(m_log, "Starting board");
    evalBoard->run();

    blockSize = dataBlock->calculateDataBlockSizeInWords(
        evalBoard->getNumEnabledDataStreams(), evalBoard->isUSB3());

    isTransmitting = true;
    dataSampleNumber = 0;
    memorySampleNumber = 0;
    bnoSampleNumbers = {0, 0, 0, 0};

    return true;
}

bool AcqBoardONI::stopAcquisition()
{
    if (deviceFound) {
        const std::lock_guard<std::mutex> lock(oniLock);
        evalBoard->setContinuousRunMode(false);
        evalBoard->setMaxTimeStep(0);
        evalBoard->stop();
        evalBoard->resetBoard();
    }

    isTransmitting = false;
    updateSettingsDuringAcquisition = false;

    while (!m_digitalOutputCommands.empty())
        m_digitalOutputCommands.pop();

    return true;
}

bool AcqBoardONI::pumpSamples(std::span<AcqSampleChunk> sinks)
{
    if (!deviceFound || !isTransmitting)
        return false;

    constexpr int nSamps = 128;
    const int numStreams = static_cast<int>(enabledStreams.size());
    const int numChannels = getNumChannels();
    const bool isV3 = (deviceId == DEVICE_ID_V3);

    // Allocate / size every chunk for this pump cycle.
    for (auto &sink : sinks) {
        const bool active = (sink.kind == ChannelKind::Electrode)
                            || (sink.kind == ChannelKind::Aux && settings.acquireAux)
                            || (sink.kind == ChannelKind::Adc && settings.acquireAdc);
        if (!active) {
            sink.numSamples = 0;
            continue;
        }
        sink.numSamples = nSamps;
        const std::size_t needed =
            static_cast<std::size_t>(nSamps) * static_cast<std::size_t>(sink.channelsPerSample);
        if (sink.samples.size() < needed)
            sink.samples.resize(needed);
        if (sink.sampleIndices.size() < static_cast<std::size_t>(nSamps))
            sink.sampleIndices.resize(nSamps);
    }

    for (int samp = 0; samp < nSamps; samp++) {
        oni_frame_t *frame = nullptr;
        const int res = evalBoard->readFrame(&frame, false);
        if (res < ONI_ESUCCESS) {
            LOG_ERROR(m_log, "Error reading ONI frame: {} code {}", oni_error_str(res), res);
            if (frame)
                oni_destroy_frame(frame);
            return false;
        }

        if (frame->dev_idx != Rhd2000ONIBoard::DEVICE_RHYTHM) {
            // BNO frames are dropped (not exposed as a Syntalos port yet).
            // Memory-monitor frames are converted into a status-message string
            // routed up to the module so it can show buffer fill % live.
            if (frame->dev_idx == Rhd2000ONIBoard::DEVICE_MEMORY && totalMemory > 0) {
                const auto *data = static_cast<const uint32_t *>(
                    static_cast<const void *>(frame->data));
                const float memPct = 100.0f * static_cast<float>(*(data + 2))
                                     / static_cast<float>(totalMemory);
                if (m_lastReportedMemPct < 0.0f
                    || std::abs(memPct - m_lastReportedMemPct) >= 1.0f) {
                    emitMessage(MessageSeverity::Info, std::format("buffer:{}%", Syntalos::numToString(memPct)));
                    m_lastReportedMemPct = memPct;
                }
            }
            oni_destroy_frame(frame);
            // Re-try this iteration — the outer loop expects nSamps RHYTHM frames.
            samp--;
            continue;
        }

        if (numChannels == 0) {
            oni_destroy_frame(frame);
            samp--;
            continue;
        }

        unsigned char *bufferPtr = reinterpret_cast<unsigned char *>(frame->data) + 8; // skip ONI timestamps
        int index = 0;

        if (!Rhd2000ONIDataBlock::checkUsbHeader(bufferPtr, index)) {
            LOG_ERROR(m_log, "Error in Rhd2000ONIBoard::readDataBlock: Incorrect header.");
            oni_destroy_frame(frame);
            return false;
        }

        index += 8; // magic-number header width
        const uint32_t rhythmSampleNum = Rhd2000ONIDataBlock::convertUsbTimeStamp(bufferPtr, index);
        index += 4; // timestamp width
        int auxIndex = index;          // aux chans start at this offset
        index += 6 * numStreams;       // 3 aux chans * 2 bytes * numStreams

        // ---- electrode channels into the per-headstage chunks ----
        // We populate `thisSample` interleaved exactly like upstream so the AUX/ADC
        // demuxing below mirrors the original; then we copy out per chunk.
        int channel = -1;
        for (int dataStream = 0; dataStream < numStreams; dataStream++) {
            const int nChans = numChannelsPerDataStream[dataStream];
            int chanIndex = index + 2 * dataStream;
            if (chipId[dataStream] == CHIP_ID_RHD2132 && nChans == 16)
                chanIndex += 2 * RHD2132_16CH_OFFSET * numStreams;

            for (int chan = 0; chan < nChans; chan++) {
                channel++;
                thisSample[channel] =
                    static_cast<float>(*reinterpret_cast<uint16_t *>(bufferPtr + chanIndex) - 32768)
                    * 0.195f;
                chanIndex += 2 * numStreams; // single-chan stride (2 bytes * numStreams)
            }
        }

        index += 64 * numStreams;        // neural-data width
        auxIndex += 2 * numStreams;      // skip AuxCmd1 slots

        if (settings.acquireAux) {
            for (int dataStream = 0; dataStream < numStreams; dataStream++) {
                if (chipId[dataStream] != CHIP_ID_RHD2164_B) {
                    const int auxNum = (rhythmSampleNum + 3) % 4;
                    if (auxNum < 3)
                        auxSamples[dataStream][auxNum] =
                            static_cast<float>(
                                *reinterpret_cast<uint16_t *>(bufferPtr + auxIndex) - 32768)
                            * 0.0000374f;
                    for (int chan = 0; chan < 3; chan++) {
                        channel++;
                        if (auxNum == 3)
                            auxBuffer[channel] = auxSamples[dataStream][chan];
                        thisSample[channel] = auxBuffer[channel];
                    }
                }
                auxIndex += 2; // single-chan width
            }
        }
        index += 2 * numStreams; // filler word at the end of each data stream

        // ---- ADC channels ----
        std::array<uint16_t, 8> adcRaw{};
        if (settings.acquireAdc) {
            for (int adcChan = 0; adcChan < 8; ++adcChan) {
                adcRaw[adcChan] = *reinterpret_cast<uint16_t *>(bufferPtr + index);
                channel++;
                const float adcValue = static_cast<float>(adcRaw[adcChan]);
                if (isV3)
                    thisSample[channel] = -(getBitVolts(ChannelKind::Adc) * adcValue - 5);
                else
                    thisSample[channel] = getBitVolts(ChannelKind::Adc) * adcValue - 5 - 0.4096f;
                index += 2;
            }
        } else {
            index += 16;
        }

        // const uint64_t ttlEventWord = *reinterpret_cast<uint64_t *>(bufferPtr + index) & 65535;
        // (Currently unused in the Syntalos export — kept here as a pointer for when a TTL
        // input port is exposed on the module side.)

        // ---- demux thisSample into the per-headstage chunks ----
        // Walk the chunks and fill them by reading the same raw bytes the algorithm
        // above just consumed. We use thisSample[] for sanity (it's already been
        // populated with the upstream scaling) but write *raw uint16* into the chunks.
        int electrodeBase = 0;
        for (size_t hs = 0; hs < headstages.size(); ++hs) {
            const auto *headstage = headstages[hs].get();
            if (!headstage->isConnected())
                continue;

            const int hsActive = headstage->getNumActiveChannels();

            // Find the matching electrode + aux chunks for this headstage.
            for (auto &sink : sinks) {
                if (sink.numSamples == 0)
                    continue;
                if (sink.groupIndex != static_cast<int>(hs))
                    continue;

                const int row = samp;
                const int cps = sink.channelsPerSample;
                uint16_t *out = sink.samples.data()
                                + static_cast<std::size_t>(row) * static_cast<std::size_t>(cps);
                sink.sampleIndices[row] = static_cast<uint64_t>(dataSampleNumber);

                if (sink.kind == ChannelKind::Electrode) {
                    // Re-derive raw uint16 directly from bufferPtr the same way the
                    // electrode loop above did. This avoids losing precision through
                    // the 0.195-scaled float intermediate.
                    int chanCol = 0;
                    const int hsStreamStart = headstage->getStreamIndex(0);
                    const int hsNumStreams = headstage->getNumStreams();
                    // bufferPtr layout: at the top of the neural-data block we used
                    // index = (post-header + post-aux) before incrementing; but at
                    // this point in the iteration `index` has moved past it. We
                    // recompute the neural base from scratch here.
                    const int neuralBase = 8 + 4 + 6 * numStreams;
                    for (int s = 0; s < hsNumStreams; ++s) {
                        const int stream = hsStreamStart + s;
                        const int nChans = numChannelsPerDataStream[stream];
                        int chanIndex = neuralBase + 2 * stream;
                        if (chipId[stream] == CHIP_ID_RHD2132 && nChans == 16)
                            chanIndex += 2 * RHD2132_16CH_OFFSET * numStreams;
                        for (int chan = 0; chan < nChans && chanCol < cps; chan++) {
                            out[chanCol++] =
                                *reinterpret_cast<uint16_t *>(bufferPtr + chanIndex);
                            chanIndex += 2 * numStreams;
                        }
                    }
                } else if (sink.kind == ChannelKind::Aux) {
                    // 3 aux channels per headstage. Source: auxBuffer (already
                    // updated by the aux state machine above) at the headstage's
                    // first stream slot.
                    const int hsStreamStart = headstage->getStreamIndex(0);
                    // Compute the offset in the interleaved thisSample/auxBuffer
                    // layout: it's electrodeBase + (hs's electrode count) for the
                    // start of this hs's aux trio. But auxBuffer[] is indexed by
                    // the same channel counter that thisSample uses, so we need
                    // electrodeTotal + auxOffsetForHs.
                    const int electrodeTotal = getNumDataOutputs(ChannelKind::Electrode);
                    int auxOffset = 0;
                    for (size_t prev = 0; prev < hs; ++prev) {
                        if (headstages[prev]->isConnected()
                            && chipId[headstages[prev]->getStreamIndex(0)] != CHIP_ID_RHD2164_B)
                            auxOffset += 3;
                    }
                    if (chipId[hsStreamStart] != CHIP_ID_RHD2164_B) {
                        for (int chan = 0; chan < 3 && chan < cps; chan++) {
                            const float uv = auxBuffer[electrodeTotal + auxOffset + chan];
                            // Convert µV-scaled float back to offset-binary uint16 using the
                            // aux bit-volts (0.0000374 V/LSB).
                            const float counts = uv / 0.0000374f + 32768.0f;
                            const int clamped = static_cast<int>(std::clamp(counts, 0.0f, 65535.0f));
                            out[chan] = static_cast<uint16_t>(clamped);
                        }
                    } else {
                        for (int chan = 0; chan < 3 && chan < cps; chan++)
                            out[chan] = 32768;
                    }
                }
            }
            electrodeBase += hsActive;
        }

        // ADC chunk (groupIndex = -1).
        for (auto &sink : sinks) {
            if (sink.numSamples == 0)
                continue;
            if (sink.kind != ChannelKind::Adc)
                continue;

            const int row = samp;
            const int cps = sink.channelsPerSample;
            uint16_t *out = sink.samples.data()
                            + static_cast<std::size_t>(row) * static_cast<std::size_t>(cps);
            sink.sampleIndices[row] = static_cast<uint64_t>(dataSampleNumber);

            for (int adcChan = 0; adcChan < cps && adcChan < 8; ++adcChan)
                out[adcChan] = adcRaw[adcChan];
        }

        dataSampleNumber++;
        oni_destroy_frame(frame);
    }

    // ---- live settings updates that were queued from the GUI thread ----
    if (updateSettingsDuringAcquisition) {
        for (int k = 0; k < 8; k++) {
            if (dacChannelsToUpdate[k]) {
                dacChannelsToUpdate[k] = false;
                if (dacChannels[k] >= 0) {
                    evalBoard->enableDac(k, true);
                    evalBoard->selectDacDataStream(k, dacStream[k]);
                    evalBoard->selectDacDataChannel(k, dacChannels[k]);
                    evalBoard->setDacThreshold(
                        k,
                        std::abs(static_cast<int>(dacThresholds[k] / 0.195) + 32768),
                        dacThresholds[k] >= 0);
                } else {
                    evalBoard->enableDac(k, false);
                }
            }
        }
        evalBoard->setTtlMode(settings.ttlOutputMode ? 1 : 0);
        evalBoard->enableExternalFastSettle(settings.fastTTLSettleEnabled);
        evalBoard->setExternalFastSettleChannel(settings.fastSettleTTLChannel);
        evalBoard->setDacHighpassFilter(settings.desiredDAChpf);
        evalBoard->enableDacHighpassFilter(settings.desiredDAChpfState);
        evalBoard->enableBoardLeds(settings.ledsEnabled);
        evalBoard->setClockDivider(settings.clockDivideFactor);
        updateSettingsDuringAcquisition = false;
    }

    // ---- digital output drain ----
    if (!m_digitalOutputCommands.empty()) {
        while (!m_digitalOutputCommands.empty()) {
            const DigitalOutputCommand command = m_digitalOutputCommands.front();
            TTL_OUTPUT_STATE[command.ttlLine] = command.state ? 1 : 0;
            m_digitalOutputCommands.pop();
        }
        evalBoard->setTtlOut(TTL_OUTPUT_STATE);
    }

    return true;
}

void AcqBoardONI::setNumHeadstageChannels(int hsNum, int numChannels)
{
    if (headstages[hsNum]->getNumChannels() == 32) {
        headstages[hsNum]->setHalfChannels(numChannels < headstages[hsNum]->getNumChannels());
        numChannelsPerDataStream[headstages[hsNum]->getStreamIndex(0)] = numChannels;
    }

    int channelIndex = 0;
    for (auto &hs : headstages) {
        if (hs->isConnected()) {
            hs->setFirstChannel(channelIndex);
            channelIndex += hs->getNumActiveChannels();
        }
    }
}

int AcqBoardONI::getChannelsInHeadstage(int hsNum) const
{
    return headstages[hsNum]->getNumChannels();
}

int AcqBoardONI::getNumBnos() const
{
    int count = 0;
    for (int i = 0; i < NUMBER_OF_PORTS; i++) {
        if (hasBNO[i])
            count++;
    }
    return count;
}

int AcqBoardONI::getNumDataOutputs(ChannelKind kind)
{
    if (kind == ChannelKind::Electrode) {
        int totalChannels = 0;
        for (const auto &headstage : headstages) {
            if (headstage->isConnected())
                totalChannels += headstage->getNumActiveChannels();
        }
        return totalChannels;
    }
    if (kind == ChannelKind::Aux) {
        if (!settings.acquireAux)
            return 0;
        int numAuxOutputs = 0;
        for (const auto &headstage : headstages) {
            if (headstage->isConnected())
                numAuxOutputs += 3;
        }
        return numAuxOutputs;
    }
    if (kind == ChannelKind::Adc) {
        return settings.acquireAdc ? 8 : 0;
    }
    return 0;
}

int AcqBoardONI::getChannelFromHeadstage(int hs, int ch)
{
    int channelCount = 0;
    int hsCount = 0;
    if (hs < 0 || hs >= static_cast<int>(headstages.size()) + 1)
        return -1;
    if (hs == static_cast<int>(headstages.size())) {
        const int adcOutputs = getNumDataOutputs(ChannelKind::Adc);
        if (adcOutputs > 0)
            return getNumDataOutputs(ChannelKind::Electrode)
                   + getNumDataOutputs(ChannelKind::Aux) + ch;
        return -1;
    }
    if (headstages[hs]->isConnected()) {
        if (ch < 0)
            return -1;
        if (ch < headstages[hs]->getNumActiveChannels()) {
            for (int i = 0; i < hs; i++)
                channelCount += headstages[i]->getNumActiveChannels();
            return channelCount + ch;
        } else if (ch < headstages[hs]->getNumActiveChannels() + 3) {
            for (size_t i = 0; i < headstages.size(); i++) {
                if (headstages[i]->isConnected()) {
                    channelCount += headstages[i]->getNumActiveChannels();
                    if (static_cast<int>(i) < hs)
                        hsCount++;
                }
            }
            return channelCount + hsCount * 3 + ch - headstages[hs]->getNumActiveChannels();
        } else {
            return -1;
        }
    }
    return -1;
}

int AcqBoardONI::getHeadstageChannel(int &hs, int ch) const
{
    int channelCount = 0;
    int hsCount = 0;

    if (ch < 0)
        return -1;

    for (size_t i = 0; i < headstages.size(); i++) {
        if (headstages[i]->isConnected()) {
            const int chans = headstages[i]->getNumActiveChannels();
            if (ch >= channelCount && ch < channelCount + chans) {
                hs = static_cast<int>(i);
                return ch - channelCount;
            }
            channelCount += chans;
            hsCount++;
        }
    }
    if (ch < (channelCount + hsCount * 3)) {
        hsCount = (ch - channelCount) / 3;
        for (size_t i = 0; i < headstages.size(); i++) {
            if (headstages[i]->isConnected()) {
                if (hsCount == 0) {
                    hs = static_cast<int>(i);
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

bool AcqBoardONI::CheckSemVer(
    int major, int minor, int patch, int targetMajor, int targetMinor, int targetPatch)
{
    if (major > targetMajor)
        return true;
    if (major < targetMajor)
        return false;

    if (minor > targetMinor)
        return true;
    if (minor < targetMinor)
        return false;

    return patch >= targetPatch;
}

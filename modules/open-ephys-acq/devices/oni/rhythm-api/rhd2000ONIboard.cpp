#include "rhd2000ONIboard.h"
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>
#include <chrono>

Rhd2000ONIBoard::Rhd2000ONIBoard()
{
    ctx = NULL;

    int i;
    sampleRate = SampleRate30000Hz; // Rhythm FPGA boots up with 30.0 kS/s/channel sampling rate
    numDataStreams = 0;

    MAX_NUM_DATA_STREAMS = MAX_NUM_DATA_STREAMS_USB3;

    for (i = 0; i < MAX_NUM_DATA_STREAMS; ++i)
    {
        dataStreamEnabled[i] = 0;
    }

    cableDelay.resize (4, -1);
}

Rhd2000ONIBoard::~Rhd2000ONIBoard()
{
    if (ctx != NULL)
        oni_destroy_ctx (ctx);
}

bool Rhd2000ONIBoard::isUSB3()
{
    return true;
}

int Rhd2000ONIBoard::open (const oni_driver_info_t** driverInfo)
{
    ctx = oni_create_ctx ("ft600"); // "ft600" is the driver name for the usb
    if (ctx == NULL)
        return -1;
    if (driverInfo)
        getONIDriverInfo (driverInfo);

    int res = oni_init_ctx (ctx, -1);
    if (res != ONI_ESUCCESS)
    {
        oni_destroy_ctx (ctx);
        ctx = nullptr;
        if (res == ONI_EBADCONTROLLER)
            return -3;
        else
            return -2;
    }

    return 1;
}

void Rhd2000ONIBoard::getONIVersion (int* major, int* minor, int* patch)
{
    oni_version (major, minor, patch);
}

void Rhd2000ONIBoard::getONIDriverInfo (const oni_driver_info_t** driverInfo)
{
    *driverInfo = oni_get_driver_info (ctx);
}

bool Rhd2000ONIBoard::getFTDriverInfo (int* major, int* minor, int* patch)
{
    uint32_t val;
    size_t size = sizeof (val);
    if (oni_get_driver_opt (ctx, 0, &val, &size) == ONI_ESUCCESS)
    {
        *major = (val >> 24) & 0xFF;
        *minor = (val >> 16) & 0xFF;
        *patch = (val >> 0) & 0xFFFF;
        return true;
    }
    else
        return false;
}

bool Rhd2000ONIBoard::getFTLibInfo (int* major, int* minor, int* patch)
{
    uint32_t val;
    size_t size = sizeof (val);
    if (oni_get_driver_opt (ctx, 1, &val, &size) == ONI_ESUCCESS)
    {
        *major = (val >> 24) & 0xFF;
        *minor = (val >> 16) & 0xFF;
        *patch = (val >> 0) & 0xFF;
        return true;
    }
    else
        return false;
}

bool Rhd2000ONIBoard::getAcquisitionClockHz (uint32_t* acqClkHz) const
{
    uint32_t val;
    size_t size = sizeof (val);

    if (oni_get_opt (ctx, ONI_OPT_ACQCLKHZ, &val, &size) == ONI_ESUCCESS)
    {
        *acqClkHz = val;
        return true;
    }

    return false;
}

void Rhd2000ONIBoard::initialize()
{
    int i;

    resetBoard();
    setSampleRate (SampleRate30000Hz);
    selectAuxCommandBank (PortA, AuxCmd1, 0);
    selectAuxCommandBank (PortB, AuxCmd1, 0);
    selectAuxCommandBank (PortC, AuxCmd1, 0);
    selectAuxCommandBank (PortD, AuxCmd1, 0);
    selectAuxCommandBank (PortA, AuxCmd2, 0);
    selectAuxCommandBank (PortB, AuxCmd2, 0);
    selectAuxCommandBank (PortC, AuxCmd2, 0);
    selectAuxCommandBank (PortD, AuxCmd2, 0);
    selectAuxCommandBank (PortA, AuxCmd3, 0);
    selectAuxCommandBank (PortB, AuxCmd3, 0);
    selectAuxCommandBank (PortC, AuxCmd3, 0);
    selectAuxCommandBank (PortD, AuxCmd3, 0);
    selectAuxCommandLength (AuxCmd1, 0, 0);
    selectAuxCommandLength (AuxCmd2, 0, 0);
    selectAuxCommandLength (AuxCmd3, 0, 0);
    setContinuousRunMode (true);
    setMaxTimeStep (4294967295); // 4294967295 == (2^32 - 1)

    setCableLengthFeet (PortA, 3.0); // assume 3 ft cables
    setCableLengthFeet (PortB, 3.0);
    setCableLengthFeet (PortC, 3.0);
    setCableLengthFeet (PortD, 3.0);

    setDspSettle (false);

    setDataSource (0, PortA1);
    setDataSource (1, PortB1);
    setDataSource (2, PortC1);
    setDataSource (3, PortD1);
    setDataSource (4, PortA2);
    setDataSource (5, PortB2);
    setDataSource (6, PortC2);
    setDataSource (7, PortD2);

    setDataSource (8, PortA1);
    setDataSource (9, PortB1);
    setDataSource (10, PortC1);
    setDataSource (11, PortD1);
    setDataSource (12, PortA2);
    setDataSource (13, PortB2);
    setDataSource (14, PortC2);
    setDataSource (15, PortD2);

    enableDataStream (0, true); // start with only one data stream enabled
    for (i = 1; i < MAX_NUM_DATA_STREAMS; i++)
    {
        enableDataStream (i, false);
    }
    updateStreamBlockSize();

    clearTtlOut();

    enableDac (0, false);
    enableDac (1, false);
    enableDac (2, false);
    enableDac (3, false);
    enableDac (4, false);
    enableDac (5, false);
    enableDac (6, false);
    enableDac (7, false);
    selectDacDataStream (0, 0);
    selectDacDataStream (1, 0);
    selectDacDataStream (2, 0);
    selectDacDataStream (3, 0);
    selectDacDataStream (4, 0);
    selectDacDataStream (5, 0);
    selectDacDataStream (6, 0);
    selectDacDataStream (7, 0);
    selectDacDataChannel (0, 0);
    selectDacDataChannel (1, 0);
    selectDacDataChannel (2, 0);
    selectDacDataChannel (3, 0);
    selectDacDataChannel (4, 0);
    selectDacDataChannel (5, 0);
    selectDacDataChannel (6, 0);
    selectDacDataChannel (7, 0);

    setDacManual (32768); // midrange value = 0 V

    setDacGain (0);
    setAudioNoiseSuppress (0);

    setTtlMode (1); // Digital outputs 0-7 are DAC comparators; 8-15 under manual control

    setDacThreshold (0, 32768, true);
    setDacThreshold (1, 32768, true);
    setDacThreshold (2, 32768, true);
    setDacThreshold (3, 32768, true);
    setDacThreshold (4, 32768, true);
    setDacThreshold (5, 32768, true);
    setDacThreshold (6, 32768, true);
    setDacThreshold (7, 32768, true);

    enableExternalFastSettle (false);
    setExternalFastSettleChannel (0);

    enableExternalDigOut (PortA, false);
    enableExternalDigOut (PortB, false);
    enableExternalDigOut (PortC, false);
    enableExternalDigOut (PortD, false);
    setExternalDigOutChannel (PortA, 0);
    setExternalDigOutChannel (PortB, 0);
    setExternalDigOutChannel (PortC, 0);
    setExternalDigOutChannel (PortD, 0);

    enableBoardLeds (true);
}

bool Rhd2000ONIBoard::setSampleRate (AmplifierSampleRate newSampleRate)
{
    /*
    * Sample rate in the new board is controlled by a register in the Rhythm hub manager.
    * To control it, 3 main clocks are created through PLLs. Clock can be chosen from one
    * of these and, optionally, be devided by any even number.
    * To access the register, device address is 510 register 8192. Its value is as follows:
    * Bit0: 0 = Clock directly from one of the PLL outputs. 1 = Clock will be divided
    * Bits1-2: "00" or "11" 84Mhz clock for 30KS/s. "01" 70MHz Clock for 25KS/s. "10" 56MHz clock for 20KS/s
    * Bits3-6: if bit 0 is "1" the selected clock will be divided by this 2*(value + 1) (i.e.: 0 is a /2)
    */
    int divEn;
    int clockSel;
    int divider;
    switch (newSampleRate)
    {
        case SampleRate1000Hz:
            divEn = 1;
            clockSel = 2;
            divider = 9;
            break;
        case SampleRate1250Hz:
            divEn = 1;
            clockSel = 1;
            divider = 9;
            break;
        case SampleRate1500Hz:
            divEn = 1;
            clockSel = 0;
            divider = 9;
            break;
        case SampleRate2000Hz:
            divEn = 1;
            clockSel = 2;
            divider = 4;
            break;
        case SampleRate2500Hz:
            divEn = 1;
            clockSel = 1;
            divider = 4;
            break;
        case SampleRate3000Hz:
            divEn = 1;
            clockSel = 0;
            divider = 4;
            break;
        case SampleRate3333Hz:
            divEn = 1;
            clockSel = 2;
            divider = 2;
            break;
        case SampleRate5000Hz:
            divEn = 1;
            clockSel = 2;
            divider = 1;
            break;
        case SampleRate6250Hz:
            divEn = 1;
            clockSel = 1;
            divider = 1;
            break;
        case SampleRate10000Hz:
            divEn = 1;
            clockSel = 2;
            divider = 0;
            break;
        case SampleRate12500Hz:
            divEn = 1;
            clockSel = 1;
            divider = 0;
            break;
        case SampleRate15000Hz:
            divEn = 1;
            clockSel = 0;
            divider = 0;
            break;
        case SampleRate20000Hz:
            divEn = 0;
            clockSel = 2;
            divider = 0;
            break;
        case SampleRate25000Hz:
            divEn = 0;
            clockSel = 1;
            divider = 0;
            break;
        case SampleRate30000Hz:
            divEn = 0;
            clockSel = 0;
            divider = 0;
            break;
        default:
            return false;
    }

    oni_reg_val_t val = ((divider & 0xF) << 3) + ((clockSel & 0x3) << 1) + (divEn & 0x1);
    int res;
    res = oni_write_reg (ctx, RHYTHM_HUB_MANAGER, HUB_CLOCK_SEL, val);
    if (res != ONI_ESUCCESS)
        return false;
    do
    {
        res = oni_read_reg (ctx, RHYTHM_HUB_MANAGER, HUB_CLOCK_BUSY, &val);
        if (res != ONI_ESUCCESS)
            return false;
    } while (val != 0);

    sampleRate = newSampleRate;
    return true;
}

// Returns the current per-channel sampling rate (in Hz) as a floating-point number.
double Rhd2000ONIBoard::getSampleRate() const
{
    switch (sampleRate)
    {
        case SampleRate1000Hz:
            return 1000.0;
            break;
        case SampleRate1250Hz:
            return 1250.0;
            break;
        case SampleRate1500Hz:
            return 1500.0;
            break;
        case SampleRate2000Hz:
            return 2000.0;
            break;
        case SampleRate2500Hz:
            return 2500.0;
            break;
        case SampleRate3000Hz:
            return 3000.0;
            break;
        case SampleRate3333Hz:
            return (10000.0 / 3.0);
            break;
        case SampleRate5000Hz:
            return 5000.0;
            break;
        case SampleRate6250Hz:
            return 6250.0;
            break;
        case SampleRate10000Hz:
            return 10000.0;
            break;
        case SampleRate12500Hz:
            return 12500.0;
            break;
        case SampleRate15000Hz:
            return 15000.0;
            break;
        case SampleRate20000Hz:
            return 20000.0;
            break;
        case SampleRate25000Hz:
            return 25000.0;
            break;
        case SampleRate30000Hz:
            return 30000.0;
            break;
        default:
            return -1.0;
    }
}

void Rhd2000ONIBoard::enableMemoryMonitor(bool enable)
{
    int rc = oni_write_reg (ctx, DEVICE_MEMORY, (oni_reg_addr_t) MemoryMonitorRegisters::ENABLE, enable ? 1 : 0);

    if (rc != ONI_ESUCCESS)
    {
        std::cout << oni_error_str (rc) << std::endl;
        return;
    }
}

bool Rhd2000ONIBoard::setMemoryMonitorSampleRate (int sampleRate)
{
    oni_reg_val_t clkHz;
    int rc = oni_read_reg (ctx, DEVICE_MEMORY, (oni_reg_addr_t) MemoryMonitorRegisters::CLK_HZ, &clkHz);
    if (rc != ONI_ESUCCESS)
    {
        std::cout << oni_error_str (rc) << "\n";
        return false;
    }

    rc = oni_write_reg (ctx, DEVICE_MEMORY, (oni_reg_addr_t) MemoryMonitorRegisters::CLK_DIV, clkHz / sampleRate);

    if (rc != ONI_ESUCCESS)
    {
        std::cout << oni_error_str (rc) << "\n";
        return false;
    }

    return true;
}

bool Rhd2000ONIBoard::getTotalMemory (uint32_t* memory)
{
    int rc = oni_read_reg (ctx, DEVICE_MEMORY, (oni_reg_addr_t) MemoryMonitorRegisters::TOTAL_MEM, memory);

    if (rc != ONI_ESUCCESS)
    {
        std::cout << oni_error_str (rc) << "\n";
        return false;
    }

    return true;
}

// Print a command list to the console in readable form.
void Rhd2000ONIBoard::printCommandList (const std::vector<int>& commandList) const
{
    unsigned int i;
    int cmd, channel, reg, data;

    std::cout << std::endl;
    for (i = 0; i < commandList.size(); ++i)
    {
        cmd = commandList[i];
        if (cmd < 0 || cmd > 0xffff)
        {
            std::cout << "  command[" << i << "] = INVALID COMMAND: " << cmd << std::endl;
        }
        else if ((cmd & 0xc000) == 0x0000)
        {
            channel = (cmd & 0x3f00) >> 8;
            std::cout << "  command[" << i << "] = CONVERT(" << channel << ")" << std::endl;
        }
        else if ((cmd & 0xc000) == 0xc000)
        {
            reg = (cmd & 0x3f00) >> 8;
            std::cout << "  command[" << i << "] = READ(" << reg << ")" << std::endl;
        }
        else if ((cmd & 0xc000) == 0x8000)
        {
            reg = (cmd & 0x3f00) >> 8;
            data = (cmd & 0x00ff);
            std::cout << "  command[" << i << "] = WRITE(" << reg << ",";
            std::cout << std::hex << std::uppercase << std::internal << std::setfill ('0') << std::setw (2) << data << std::nouppercase << std::dec;
            std::cout << ")" << std::endl;
        }
        else if (cmd == 0x5500)
        {
            std::cout << "  command[" << i << "] = CALIBRATE" << std::endl;
        }
        else if (cmd == 0x6a00)
        {
            std::cout << "  command[" << i << "] = CLEAR" << std::endl;
        }
        else
        {
            std::cout << "  command[" << i << "] = INVALID COMMAND: ";
            std::cout << std::hex << std::uppercase << std::internal << std::setfill ('0') << std::setw (4) << cmd << std::nouppercase << std::dec;
            std::cout << std::endl;
        }
    }
    std::cout << std::endl;
}

void Rhd2000ONIBoard::uploadCommandList (const std::vector<int>& commandList, AuxCmdSlot auxCommandSlot, int bank)
{
    // CHECK

    if (auxCommandSlot != AuxCmd1 && auxCommandSlot != AuxCmd2 && auxCommandSlot != AuxCmd3)
    {
        std::cerr << "Error in Rhd2000ONIBoard::uploadCommandList: auxCommandSlot out of range.\n";
        return;
    }

    if ((bank < 0) || (bank > 15))
    {
        std::cerr << "Error in Rhd2000ONIBoard::uploadCommandList: bank out of range.\n";
        return;
    }

    oni_reg_addr_t base_address = 0x4000;

    for (unsigned int i = 0; i < commandList.size(); ++i)
    {
        oni_reg_addr_t bank_select = (bank << 10);

        oni_reg_addr_t aux_select;

        switch (auxCommandSlot)
        {
            case AuxCmd1:
                aux_select = (0 << 14);
                break;
            case AuxCmd2:
                aux_select = (1 << 14);
                break;
            case AuxCmd3:
                aux_select = (2 << 14);
                break;
        }

        oni_write_reg (ctx, DEVICE_RHYTHM, base_address + bank_select + aux_select + i, commandList[i]);
    }
}

// Select an auxiliary command slot (AuxCmd1, AuxCmd2, or AuxCmd3) and bank (0-15) for a particular SPI port
// (PortA, PortB, PortC, or PortD) on the FPGA.
void Rhd2000ONIBoard::selectAuxCommandBank (BoardPort port, AuxCmdSlot auxCommandSlot, int bank)
{
    int bitShift;

    if (auxCommandSlot != AuxCmd1 && auxCommandSlot != AuxCmd2 && auxCommandSlot != AuxCmd3)
    {
        std::cerr << "Error in Rhd2000ONIBoard::selectAuxCommandBank: auxCommandSlot out of range." << std::endl;
        return;
    }
    if (bank < 0 || bank > 15)
    {
        std::cerr << "Error in Rhd2000ONIBoard::selectAuxCommandBank: bank out of range." << std::endl;
        return;
    }

    switch (port)
    {
        case PortA:
            bitShift = 0;
            break;
        case PortB:
            bitShift = 4;
            break;
        case PortC:
            bitShift = 8;
            break;
        case PortD:
            bitShift = 12;
            break;
    }

    switch (auxCommandSlot)
    {
        case AuxCmd1:
            oni_write_reg_mask (ctx, DEVICE_RHYTHM, AUXCMD_BANK_1, bank << bitShift, 0x000f << bitShift);
            break;
        case AuxCmd2:
            oni_write_reg_mask (ctx, DEVICE_RHYTHM, AUXCMD_BANK_2, bank << bitShift, 0x000f << bitShift);
            break;
        case AuxCmd3:
            oni_write_reg_mask (ctx, DEVICE_RHYTHM, AUXCMD_BANK_3, bank << bitShift, 0x000f << bitShift);
            break;
    }
}

// Specify a command sequence length (endIndex = 0-1023) and command loop index (0-1023) for a particular
// auxiliary command slot (AuxCmd1, AuxCmd2, or AuxCmd3).
void Rhd2000ONIBoard::selectAuxCommandLength (AuxCmdSlot auxCommandSlot, int loopIndex, int endIndex)
{
    if (auxCommandSlot != AuxCmd1 && auxCommandSlot != AuxCmd2 && auxCommandSlot != AuxCmd3)
    {
        std::cerr << "Error in Rhd2000ONIBoard::selectAuxCommandLength: auxCommandSlot out of range." << std::endl;
        return;
    }

    if (loopIndex < 0 || loopIndex > 1023)
    {
        std::cerr << "Error in Rhd2000ONIBoard::selectAuxCommandLength: loopIndex out of range." << std::endl;
        return;
    }

    if (endIndex < 0 || endIndex > 1023)
    {
        std::cerr << "Error in Rhd2000ONIBoard::selectAuxCommandLength: endIndex out of range." << std::endl;
        return;
    }

    switch (auxCommandSlot)
    {
        case AuxCmd1:
            oni_write_reg (ctx, DEVICE_RHYTHM, LOOP_AUXCMD_INDEX_1, loopIndex);
            oni_write_reg (ctx, DEVICE_RHYTHM, MAX_AUXCMD_INDEX_1, endIndex);
            break;
        case AuxCmd2:
            oni_write_reg (ctx, DEVICE_RHYTHM, LOOP_AUXCMD_INDEX_2, loopIndex);
            oni_write_reg (ctx, DEVICE_RHYTHM, MAX_AUXCMD_INDEX_2, endIndex);
            break;
        case AuxCmd3:
            oni_write_reg (ctx, DEVICE_RHYTHM, LOOP_AUXCMD_INDEX_3, loopIndex);
            oni_write_reg (ctx, DEVICE_RHYTHM, MAX_AUXCMD_INDEX_3, endIndex);
            break;
    }
}

void Rhd2000ONIBoard::resetBoard()
{
    using namespace std::chrono_literals;
    std::this_thread::sleep_for (50ms);
    uint32_t val = 1;
    oni_set_opt (ctx, ONI_OPT_RESET, &val, sizeof (val));
    oni_set_opt (ctx, ONI_OPT_BLOCKREADSIZE, &usbReadBlockSize, sizeof (usbReadBlockSize));
}

void Rhd2000ONIBoard::setContinuousRunMode (bool continuousMode)
{
    oni_reg_val_t val = continuousMode ? 1 << SPI_RUN_CONTINUOUS : 0;
    oni_write_reg_mask (ctx, DEVICE_RHYTHM, MODE, val, 1 << SPI_RUN_CONTINUOUS);
}

// Set maxTimeStep for cases where continuousMode == false.
void Rhd2000ONIBoard::setMaxTimeStep (unsigned int maxTimeStep)
{
    oni_write_reg (ctx, DEVICE_RHYTHM, MAX_TIMESTEP, maxTimeStep);
}

void Rhd2000ONIBoard::run()
{
    oni_reg_val_t reg = 2;
    oni_set_opt (ctx, ONI_OPT_RESETACQCOUNTER, &reg, sizeof (oni_size_t));
}

void Rhd2000ONIBoard::stop()
{
    oni_size_t reg = 0;
    oni_set_opt (ctx, ONI_OPT_RUNNING, &reg, sizeof (reg));
}

bool Rhd2000ONIBoard::isRunning() const
{
    oni_reg_val_t val = 0;
    if (oni_read_reg (ctx, DEVICE_RHYTHM, SPI_RUNNING, &val) != ONI_ESUCCESS)
        return false;
    return val;
}

// Set the delay for sampling the MISO line on a particular SPI port (PortA - PortD), in integer clock
// steps, where each clock step is 1/2800 of a per-channel sampling period.
// Note: Cable delay must be updated after sampleRate is changed, since cable delay calculations are
// based on the clock frequency!
void Rhd2000ONIBoard::setCableDelay (BoardPort port, int delay)
{
    int bitShift;

    if (delay < 0 || delay > 15)
    {
        std::cerr << "Warning in Rhd2000ONIBoard::setCableDelay: delay out of range: " << delay << std::endl;
    }

    if (delay < 0)
        delay = 0;
    if (delay > 15)
        delay = 15;

    switch (port)
    {
        case PortA:
            bitShift = 0;
            cableDelay[0] = delay;
            break;
        case PortB:
            bitShift = 4;
            cableDelay[1] = delay;
            break;
        case PortC:
            bitShift = 8;
            cableDelay[2] = delay;
            break;
        case PortD:
            bitShift = 12;
            cableDelay[3] = delay;
            break;
        default:
            std::cerr << "Error in Rhd2000ONIBoard::setCableDelay: unknown port." << std::endl;
    }

    oni_write_reg_mask (ctx, DEVICE_RHYTHM, CABLE_DELAY, delay << bitShift, 0x000f << bitShift);
}

// Set the delay for sampling the MISO line on a particular SPI port (PortA - PortD) based on the length
// of the cable between the FPGA and the RHD2000 chip (in meters).
// Note: Cable delay must be updated after sampleRate is changed, since cable delay calculations are
// based on the clock frequency!
void Rhd2000ONIBoard::setCableLengthMeters (BoardPort port, double lengthInMeters)
{
    int delay;
    double tStep, cableVelocity, distance, timeDelay;
    const double speedOfLight = 299792458.0; // units = meters per second
    const double xilinxLvdsOutputDelay = 1.9e-9; // 1.9 ns Xilinx LVDS output pin delay
    const double xilinxLvdsInputDelay = 1.4e-9; // 1.4 ns Xilinx LVDS input pin delay
    const double rhd2000Delay = 9.0e-9; // 9.0 ns RHD2000 SCLK-to-MISO delay
    const double misoSettleTime = 6.7e-9; // 6.7 ns delay after MISO changes, before we sample it

    tStep = 1.0 / (2800.0 * getSampleRate()); // data clock that samples MISO has a rate 35 x 80 = 2800x higher than the sampling rate
    // cableVelocity = 0.67 * speedOfLight;  // propogation velocity on cable: version 1.3 and earlier
    cableVelocity = 0.555 * speedOfLight; // propogation velocity on cable: version 1.4 improvement based on cable measurements
    distance = 2.0 * lengthInMeters; // round trip distance data must travel on cable
    timeDelay = (distance / cableVelocity) + xilinxLvdsOutputDelay + rhd2000Delay + xilinxLvdsInputDelay + misoSettleTime;

    delay = (int) floor (((timeDelay / tStep) + 1.0) + 0.5);

    if (delay < 1)
        delay = 1; // delay of zero is too short (due to I/O delays), even for zero-length cables
    delay = delay + 1; //For extra clock step

    setCableDelay (port, delay);
}

// Same function as above, but accepts lengths in feet instead of meters
void Rhd2000ONIBoard::setCableLengthFeet (BoardPort port, double lengthInFeet)
{
    setCableLengthMeters (port, 0.3048 * lengthInFeet); // convert feet to meters
}

// Estimate cable length based on a particular delay used in setCableDelay.
// (Note: Depends on sample rate.)
double Rhd2000ONIBoard::estimateCableLengthMeters (int delay) const
{
    delay = delay - 1; //Extra clock step for timings in ECP5 board
    double tStep, cableVelocity, distance;
    const double speedOfLight = 299792458.0; // units = meters per second
    const double xilinxLvdsOutputDelay = 1.9e-9; // 1.9 ns Xilinx LVDS output pin delay
    const double xilinxLvdsInputDelay = 1.4e-9; // 1.4 ns Xilinx LVDS input pin delay
    const double rhd2000Delay = 9.0e-9; // 9.0 ns RHD2000 SCLK-to-MISO delay
    const double misoSettleTime = 6.7e-9; // 6.7 ns delay after MISO changes, before we sample it

    tStep = 1.0 / (2800.0 * getSampleRate()); // data clock that samples MISO has a rate 35 x 80 = 2800x higher than the sampling rate
    // cableVelocity = 0.67 * speedOfLight;  // propogation velocity on cable: version 1.3 and earlier
    cableVelocity = 0.555 * speedOfLight; // propogation velocity on cable: version 1.4 improvement based on cable measurements

    // distance = cableVelocity * (delay * tStep - (xilinxLvdsOutputDelay + rhd2000Delay + xilinxLvdsInputDelay));  // version 1.3 and earlier
    distance = cableVelocity * ((((double) delay) - 1.0) * tStep - (xilinxLvdsOutputDelay + rhd2000Delay + xilinxLvdsInputDelay + misoSettleTime)); // version 1.4 improvement
    if (distance < 0.0)
        distance = 0.0;

    return (distance / 2.0);
}

// Turn on or off DSP settle function in the FPGA.  (Only executes when CONVERT commands are sent.)
void Rhd2000ONIBoard::setDspSettle (bool enabled)
{
    oni_reg_val_t val = enabled ? 1 << DSP_SETTLE : 0;
    oni_write_reg_mask (ctx, DEVICE_RHYTHM, MODE, val, 1 << DSP_SETTLE);
}

void Rhd2000ONIBoard::setDataSource (int stream, BoardDataSource dataSource)
{
    if (stream < 0 || stream > (MAX_NUM_DATA_STREAMS - 1))
    {
        std::cerr << "Error in Rhd2000ONIBoard::setDataSource: stream out of range." << std::endl;
        return;
    }

    oni_reg_addr_t reg = DATA_STREAM_1_8_SEL + int (stream / 8);
    oni_reg_val_t bitShift = 4 * (stream % 8);
    oni_write_reg_mask (ctx, DEVICE_RHYTHM, reg, dataSource << bitShift, 0x000f << bitShift);
}

void Rhd2000ONIBoard::enableDataStream (int stream, bool enabled)
{
    if (stream < 0 || stream > (MAX_NUM_DATA_STREAMS - 1))
    {
        std::cerr << "Error in Rhd2000ONIBoard::setDataSource: stream out of range." << std::endl;
        return;
    }

    if (enabled)
    {
        if (dataStreamEnabled[stream] == 0)
        {
            oni_write_reg_mask (ctx, DEVICE_RHYTHM, DATA_STREAM_EN, 0x0001 << stream, 0x0001 << stream);
            dataStreamEnabled[stream] = 1;
            ++numDataStreams;
        }
    }
    else
    {
        if (dataStreamEnabled[stream] == 1)
        {
            oni_write_reg_mask (ctx, DEVICE_RHYTHM, DATA_STREAM_EN, 0x0000 << stream, 0x0001 << stream);
            dataStreamEnabled[stream] = 0;
            numDataStreams--;
        }
    }
}

void Rhd2000ONIBoard::updateStreamBlockSize()
{
    //At this moment, we can only change device transfer sizes after a reset. In the future, we might
    //implement dynamic frames, so this could change.
    resetBoard();
}

// Returns the number of enabled data streams.
int Rhd2000ONIBoard::getNumEnabledDataStreams() const
{
    return numDataStreams;
}

// Enable or disable AD5662 DAC channel (0-7)
void Rhd2000ONIBoard::enableDac (int dacChannel, bool enabled)
{
    if (dacChannel < 0 || dacChannel > 7)
    {
        std::cerr << "Error in Rhd2000ONIBoard::enableDac: dacChannel out of range." << std::endl;
        return;
    }
    oni_reg_addr_t reg = DAC_SEL_1 + dacChannel;
    oni_reg_val_t val = enabled ? 0x0400 : 0;
    oni_write_reg_mask (ctx, DEVICE_RHYTHM, reg, val, 0x0400);
}

// Set the gain level of all eight DAC channels to 2^gain (gain = 0-7).
void Rhd2000ONIBoard::setDacGain (int gain)
{
    if (gain < 0 || gain > 7)
    {
        std::cerr << "Error in Rhd2000ONIBoard::setDacGain: gain out of range." << std::endl;
        return;
    }
    oni_write_reg_mask (ctx, DEVICE_RHYTHM, DAC_CTL, gain << 7, 0x07 << 7);
}

// Suppress the noise on DAC channels 0 and 1 (the audio channels) between
// +16*noiseSuppress and -16*noiseSuppress LSBs.  (noiseSuppress = 0-127).
void Rhd2000ONIBoard::setAudioNoiseSuppress (int noiseSuppress)
{
    if (noiseSuppress < 0 || noiseSuppress > 127)
    {
        std::cerr << "Error in Rhd2000ONIBoard::setAudioNoiseSuppress: noiseSuppress out of range." << std::endl;
        return;
    }

    oni_write_reg_mask (ctx, DEVICE_RHYTHM, DAC_CTL, noiseSuppress, 0x7F);
}

// Assign a particular data stream (0-15) to a DAC channel (0-15).  Setting stream
// to 16 selects DacManual value;
void Rhd2000ONIBoard::selectDacDataStream (int dacChannel, int stream)
{
    if (dacChannel < 0 || dacChannel > 7)
    {
        std::cerr << "Error in Rhd2000ONIBoard::selectDacDataStream: dacChannel out of range." << std::endl;
        return;
    }

    if (stream < 0 || stream > MAX_NUM_DATA_STREAMS + 1)
    {
        std::cerr << "Error in Rhd2000ONIBoard::selectDacDataStream: stream out of range." << std::endl;
        return;
    }

    oni_reg_addr_t reg = DAC_SEL_1 + dacChannel;
    oni_reg_val_t val = stream << 5;
    oni_write_reg_mask (ctx, DEVICE_RHYTHM, reg, val, 0x1F << 5);
}

// Assign a particular amplifier channel (0-31) to a DAC channel (0-7).
void Rhd2000ONIBoard::selectDacDataChannel (int dacChannel, int dataChannel)
{
    if (dacChannel < 0 || dacChannel > 7)
    {
        std::cerr << "Error in Rhd2000ONIBoard::selectDacDataChannel: dacChannel out of range." << std::endl;
        return;
    }

    if (dataChannel < 0 || dataChannel > 31)
    {
        std::cerr << "Error in Rhd2000ONIBoard::selectDacDataChannel: dataChannel out of range." << std::endl;
        return;
    }

    oni_reg_addr_t reg = DAC_SEL_1 + dacChannel;
    oni_reg_val_t val = dataChannel;
    oni_write_reg_mask (ctx, DEVICE_RHYTHM, reg, val, 0x1F);
}

// Enable external triggering of amplifier hardware 'fast settle' function (blanking).
// If external triggering is enabled, the fast settling of amplifiers on all connected
// chips will be controlled in real time via one of the 16 TTL inputs.
void Rhd2000ONIBoard::enableExternalFastSettle (bool enable)
{
    oni_reg_val_t val = enable ? 1 << 4 : 0;
    oni_write_reg_mask (ctx, DEVICE_RHYTHM, EXTERNAL_FAST_SETTLE, val, 1 << 4);
}

// Select which of the TTL inputs 0-15 is used to perform a hardware 'fast settle' (blanking)
// of the amplifiers if external triggering of fast settling is enabled.
void Rhd2000ONIBoard::setExternalFastSettleChannel (int channel)
{
    if (channel < 0 || channel > 15)
    {
        std::cerr << "Error in Rhd2000ONIBoard::setExternalFastSettleChannel: channel " << channel << " out of range." << std::endl;
        return;
    }

    oni_reg_val_t val = channel;
    oni_write_reg_mask (ctx, DEVICE_RHYTHM, EXTERNAL_FAST_SETTLE, val, 0x0F);
}

// Enable external control of RHD2000 auxiliary digital output pin (auxout).
// If external control is enabled, the digital output of all chips connected to a
// selected SPI port will be controlled in real time via one of the 16 TTL inputs.
void Rhd2000ONIBoard::enableExternalDigOut (BoardPort port, bool enable)
{
    oni_reg_addr_t reg = EXTERNAL_DIGOUT_A + port;
    oni_reg_val_t val = enable ? 1 << 4 : 0;
    oni_write_reg_mask (ctx, DEVICE_RHYTHM, reg, val, 1 << 4);
}

// Select which of the TTL inputs 0-15 is used to control the auxiliary digital output
// pin of the chips connected to a particular SPI port, if external control of auxout is enabled.
void Rhd2000ONIBoard::setExternalDigOutChannel (BoardPort port, int channel)
{
    if (channel < 0 || channel > 15)
    {
        std::cerr << "Error in Rhd2000ONIBoard::setExternalDigOutChannel: channel out of range." << std::endl;
        return;
    }

    oni_reg_addr_t reg = EXTERNAL_DIGOUT_A + port;
    oni_reg_val_t val = channel;
    oni_write_reg_mask (ctx, DEVICE_RHYTHM, reg, val, 0x0F);
}

// Enable optional FPGA-implemented digital high-pass filters associated with DAC outputs
// on USB interface board.. These one-pole filters can be used to record wideband neural data
// while viewing only spikes without LFPs on the DAC outputs, for example.  This is useful when
// using the low-latency FPGA thresholds to detect spikes and produce digital pulses on the TTL
// outputs, for example.
void Rhd2000ONIBoard::enableDacHighpassFilter (bool enable)
{
    oni_reg_val_t val = enable ? 1 << 16 : 0;
    oni_write_reg_mask (ctx, DEVICE_RHYTHM, HPF, val, 1 << 16);
}

// Set cutoff frequency (in Hz) for optional FPGA-implemented digital high-pass filters
// associated with DAC outputs on USB interface board.  These one-pole filters can be used
// to record wideband neural data while viewing only spikes without LFPs on the DAC outputs,
// for example.  This is useful when using the low-latency FPGA thresholds to detect spikes
// and produce digital pulses on the TTL outputs, for example.
void Rhd2000ONIBoard::setDacHighpassFilter (double cutoff)
{
    double b;
    int filterCoefficient;
    const double pi = 3.1415926535897;

    // Note that the filter coefficient is a function of the amplifier sample rate, so this
    // function should be called after the sample rate is changed.
    b = 1.0 - exp (-2.0 * pi * cutoff / getSampleRate());

    // In hardware, the filter coefficient is represented as a 16-bit number.
    filterCoefficient = (int) floor (65536.0 * b + 0.5);

    if (filterCoefficient < 1)
    {
        filterCoefficient = 1;
    }
    else if (filterCoefficient > 65535)
    {
        filterCoefficient = 65535;
    }

    oni_reg_val_t val = filterCoefficient;
    oni_write_reg_mask (ctx, DEVICE_RHYTHM, HPF, val, 0xFFFF);
}

// Set thresholds for DAC channels; threshold output signals appear on TTL outputs 0-7.
// The parameter 'threshold' corresponds to the RHD2000 chip ADC output value, and must fall
// in the range of 0 to 65535, where the 'zero' level is 32768.
// If trigPolarity is true, voltages equaling or rising above the threshold produce a high TTL output.
// If trigPolarity is false, voltages equaling or falling below the threshold produce a high TTL output.
void Rhd2000ONIBoard::setDacThreshold (int dacChannel, int threshold, bool trigPolarity)
{
    if (dacChannel < 0 || dacChannel > 7)
    {
        std::cerr << "Error in Rhd2000ONIBoard::setDacThreshold: dacChannel out of range." << std::endl;
        return;
    }

    if (threshold < 0 || threshold > 65535)
    {
        std::cerr << "Error in Rhd2000ONIBoard::setDacThreshold: threshold out of range." << std::endl;
        return;
    }

    oni_reg_addr_t reg = DAC_THRESH_1 + dacChannel;
    oni_reg_val_t val = (threshold & 0x0FFFF) + ((trigPolarity ? 1 : 0) << 16);

    oni_write_reg (ctx, DEVICE_RHYTHM, reg, val);
}

// Set the TTL output mode of the board.
// mode = 0: All 16 TTL outputs are under manual control
// mode = 1: Top 8 TTL outputs are under manual control;
//           Bottom 8 TTL outputs are outputs of DAC comparators
void Rhd2000ONIBoard::setTtlMode (int mode)
{
    if (mode < 0 || mode > 1)
    {
        std::cerr << "Error in Rhd2000ONIBoard::setTtlMode: mode out of range." << std::endl;
        return;
    }

    oni_write_reg_mask (ctx, DEVICE_RHYTHM, MODE, mode << TTL_OUT_MODE, 1 << TTL_OUT_MODE);
}

bool Rhd2000ONIBoard::isStreamEnabled (int streamIndex)
{
    if (streamIndex < 0 || streamIndex > (MAX_NUM_DATA_STREAMS - 1))
        return false;

    return dataStreamEnabled[streamIndex];
}

void Rhd2000ONIBoard::enableBoardLeds (bool enable)
{
    oni_reg_val_t val = enable ? 1 << LED_ENABLE : 0;
    oni_write_reg_mask (ctx, DEVICE_RHYTHM, MODE, val, 1 << LED_ENABLE);
}

// Ratio    divide_factor
// 1        0
// >=2      Ratio/2
void Rhd2000ONIBoard::setClockDivider (int divide_factor)
{
    oni_reg_val_t val = divide_factor;
    oni_write_reg (ctx, DEVICE_RHYTHM, SYNC_CLKOUT_DIVIDE, val);
}

int Rhd2000ONIBoard::oni_write_reg_mask (const oni_ctx ctx, oni_dev_idx_t dev_idx, oni_reg_addr_t addr, oni_reg_val_t value, unsigned int mask)
{
    int res;
    oni_reg_val_t val;

    res = oni_read_reg (ctx, dev_idx, addr, &val);
    if (res != ONI_ESUCCESS)
        return res;

    val = (val & ~mask) | (value & mask);
    res = oni_write_reg (ctx, dev_idx, addr, val);
    return res;
}

int Rhd2000ONIBoard::readFrame (oni_frame_t** frame, bool filterRhythm)
{
    int res;
    bool found = false;
    do
    {
        res = oni_read_frame (ctx, frame);
        if (res < ONI_ESUCCESS)
            return res;
        if (filterRhythm == false || (*frame)->dev_idx == DEVICE_RHYTHM)
        {
            found = true;
        }
        else
        {
            oni_destroy_frame (*frame);
        }

    } while (! found);
    return res;
}

//TODO: This method is outdated and uses unncessary buffering, but it's required for the legacy datablock structure. Fortunately, it's only
//used for initialization and headstage search. A further rework should eliminate the need for it.
bool Rhd2000ONIBoard::readDataBlock (Rhd2000ONIDataBlock* dataBlock, int nSamples)
{
    oni_frame_t* frame;
    size_t offset = 0;
    size_t bufSize = Rhd2000ONIDataBlock::calculateDataBlockSizeInWords (numDataStreams, true, nSamples);
    unsigned char* usbBuffer = (unsigned char*) malloc (sizeof (uint16_t) * bufSize);
    if (usbBuffer == NULL)
    {
        std::cerr << "Error allocating usb buffer in readDataBlock" << std::endl;
        return false;
    }
    for (int i = 0; i < nSamples; i++)
    {
        if (readFrame (&frame) < ONI_ESUCCESS)
        {
            free (usbBuffer);
            return false;
        }

        memcpy (usbBuffer + offset, frame->data + 8, frame->data_sz - 8);
        offset += frame->data_sz - 8;
        oni_destroy_frame (frame);
    }
    dataBlock->fillFromUsbBuffer (usbBuffer, 0, numDataStreams, nSamples);
    free (usbBuffer);
    return true;
}

//Same as readDataBlock
bool Rhd2000ONIBoard::readDataBlocks (int numBlocks, std::queue<Rhd2000ONIDataBlock>& dataQueue)
{
    int nSamples = numBlocks * Rhd2000ONIDataBlock::getSamplesPerDataBlock (true);
    size_t bufSize = Rhd2000ONIDataBlock::calculateDataBlockSizeInWords (numDataStreams, true, nSamples);
    unsigned char* usbBuffer = (unsigned char*) malloc (sizeof (uint16_t) * bufSize);
    if (usbBuffer == NULL)
    {
        std::cerr << "Error allocating usb buffer in readDataBlocks" << std::endl;
        return false;
    }

    Rhd2000ONIDataBlock* dataBlock;
    oni_frame_t* frame;
    size_t offset = 0;
    ;
    for (int i = 0; i < nSamples; i++)
    {
        if (readFrame (&frame) < ONI_ESUCCESS)
        {
            free (usbBuffer);
            return false;
        }
        memcpy (usbBuffer + offset, frame->data + 8, frame->data_sz - 8);
        offset += frame->data_sz - 8;
        oni_destroy_frame (frame);
    }

    dataBlock = new Rhd2000ONIDataBlock (numDataStreams, true);
    for (int i = 0; i < numBlocks; ++i)
    {
        dataBlock->fillFromUsbBuffer (usbBuffer, i, numDataStreams);
        dataQueue.push (*dataBlock);
    }
    delete dataBlock;
    free (usbBuffer);
    return true;
}

void Rhd2000ONIBoard::setTtlOut (int ttlOutArray[16])
{
    int32_t ttlOut = 0;

    for (int i = 0; i < 16; ++i)
    {
        if (ttlOutArray[i] > 0)
            ttlOut += 1 << i;
    }

    oni_frame_t* frame;

    int res = oni_create_frame (ctx, &frame, DEVICE_TTL, &ttlOut, sizeof (ttlOut));
    if (res > ONI_ESUCCESS)
    {
        oni_write_frame (ctx, frame);
        oni_destroy_frame (frame);
    }
    else
        std::cerr << "Error creating frame for TTL writing " << res << ": " << oni_error_str (res) << std::endl;
}

void Rhd2000ONIBoard::clearTtlOut()
{
    int32_t ttlOut = 0;

    oni_frame_t* frame;

    int res = oni_create_frame (ctx, &frame, DEVICE_TTL, &ttlOut, sizeof (ttlOut));
    if (res > ONI_ESUCCESS)
    {
        oni_write_frame (ctx, frame);
        oni_destroy_frame (frame);
    }
    else
        std::cerr << "Error creating frame for TTL clearing " << res << ": " << oni_error_str (res) << std::endl;
}

void Rhd2000ONIBoard::setDacManual (int value)
{
    if (value < 0 || value > 65535)
    {
        std::cerr << "Error in Rhd2000ONIBoard::setDacManual: value out of range." << std::endl;
        return;
    }
    value = value & 0xFFFF;
    oni_size_t dacValues[4] {};
    for (int i = 0; i < 4; i++)
    {
        dacValues[i] = std::min ((value + 5) / (10 * bitVal16), static_cast<double> (std::numeric_limits<uint16_t>::max()));
    }
    oni_frame_t* frame;
    int res = oni_create_frame (ctx, &frame, DEVICE_DAC, dacValues, 4 * sizeof (oni_size_t));
    if (res > ONI_ESUCCESS)
    {
        oni_write_frame (ctx, frame);
        oni_destroy_frame (frame);
    }
    else
        std::cerr << "Error creating frame for DAC writing " << res << ": " << oni_error_str (res) << std::endl;
}

bool Rhd2000ONIBoard::getFirmwareVersion (int* major, int* minor, int* patch, int* rc) const
{
    if (! ctx)
        return false;
    oni_reg_val_t val;
    if (oni_read_reg (ctx, 254, 2, &val) != ONI_ESUCCESS)
        return false;
    *patch = val & 0xFF;
    *minor = (val >> 8) & 0xFF;
    *major = (val >> 16) & 0xFF;
    if (rc)
    {
        *rc = (val >> 24) & 0xFF;
    }
    return true;
}

bool Rhd2000ONIBoard::getDeviceId (oni_reg_val_t* id)
{
    constexpr int deviceIndex = 254;
    constexpr int registerAddress = 0;
    int rc = oni_read_reg (ctx, deviceIndex, registerAddress, id);

    return rc == ONI_ESUCCESS;
}

Rhd2000ONIBoard::BoardMemState Rhd2000ONIBoard::getBoardMemState() const
{
    if (! ctx)
        return BOARDMEM_INVALID;
    oni_reg_val_t val;
    if (oni_read_reg (ctx, 254, 0x1000, &val) != ONI_ESUCCESS)
        return BOARDMEM_INVALID;
    return static_cast<BoardMemState> (val & 0x03);
}

bool Rhd2000ONIBoard::enableI2cMode (bool enablePort[4])
{
    if (! ctx)
        return false;

    const oni_reg_addr_t ENABLE_I2C_ADDRESS = 0x00001001;
    oni_reg_val_t val = 0x0;

    for (int i = 3; i >= 0; i--)
    {
        val |= static_cast<int> (enablePort[i]) << i;
    }

    if (oni_write_reg (ctx, DEVICE_INFO, ENABLE_I2C_ADDRESS, val) != ONI_ESUCCESS)
        return false;

    return true;
}

bool Rhd2000ONIBoard::isI2cCapable (const uint32_t port)
{
    if (! ctx || port > 3)
        return false;

    oni_reg_val_t val;
    oni_dev_idx_t device = DEVICE_I2C_RAW_A + port * 2;

    int result = oni_read_reg (ctx, device, (oni_reg_addr_t) I2cRawRegisters::I2C_BUS_READY, &val);

    if (result != ONI_ESUCCESS)
        return false;

    return val > 0;
}

int Rhd2000ONIBoard::readByte (oni_dev_idx_t device, uint32_t i2cDevAddress, oni_reg_addr_t i2cRegAddress, oni_reg_val_t* value, bool sixteenBitAddress)
{
    if (! ctx)
        return 1;

    uint32_t registerAddress = (i2cRegAddress << 7) | (i2cDevAddress & 0x7F);
    registerAddress |= sixteenBitAddress ? 0x80000000 : 0;

    int result = oni_read_reg (ctx, device, registerAddress, value);

    return result;
}

int Rhd2000ONIBoard::readEepromByte (oni_dev_idx_t i2cRawAddress, uint32_t address, uint8_t& byte)
{
    uint32_t i2cAddress = 0x50;

    oni_reg_val_t value;

    int result = readByte (i2cRawAddress, i2cAddress, address, &value, true);

    if (result == 0)
        byte = (uint8_t) (value & 0xFF);
    else
        byte = 0;

    return result;
}

uint32_t Rhd2000ONIBoard::getDeviceIdOnEeprom (const uint32_t port)
{
    if (! ctx || port > 3)
        return 0;

    uint8_t val;
    int res;

    oni_dev_idx_t i2cRawAddress = DEVICE_I2C_RAW_A + port * 2;

    // NB: Confirm EEPROM first bytes contain OESH
    const char identifier[] = "OESH";

    for (unsigned int i = 0; i < strlen (identifier); i += 1)
    {
        res = readEepromByte (i2cRawAddress, i, val);

        if (res != 0 || val != identifier[i])
            return 0;
    }
    //Check that EEPROM format is v1 or v2
    res = readEepromByte (i2cRawAddress, 4, val);
    if (res != 0 || (val != 1 && val != 2))
        return 0;

    const uint32_t deviceIdStartAddress = 8;
    uint32_t data = 0;

    for (unsigned int i = 0; i < sizeof (uint32_t); i++)
    {
        res = readEepromByte (i2cRawAddress, deviceIdStartAddress + i, val);

        if (res != 0)
            return 0;

        data += val << (8 * i);
    }

    return data;
}

bool Rhd2000ONIBoard::isBnoConnected (const uint32_t port)
{
    if (! ctx || port > 3)
        return false;

    oni_reg_val_t val = 2; // Value >= 2 means there is a BNO scan in progress
    oni_dev_idx_t device = DEVICE_BNO_A + port * 2;
    int result;

    using namespace std::chrono;
    auto startTime = high_resolution_clock::now();

    while (val >= 2 && duration_cast<seconds> (duration<double> (high_resolution_clock::now() - startTime)) < 1s)
    {
        result = oni_read_reg (ctx, device, (oni_reg_addr_t) BnoRegisters::BNO_STATUS, &val);

        if (result != ONI_ESUCCESS)
            return false;
    }

    return val == 1;
}

void Rhd2000ONIBoard::enableBnoStream (const uint32_t port, bool enabled)
{
    if (! ctx || port > 3)
        return;

    oni_dev_idx_t device = DEVICE_BNO_A + port * 2;

    int rc = oni_write_reg (ctx, device, (oni_reg_addr_t) BnoRegisters::ENABLE_BNO, static_cast<int> (enabled));
}

bool Rhd2000ONIBoard::isBnoEnabled (const uint32_t port)
{
    if (! ctx || port > 3)
        return false;

    oni_dev_idx_t device = DEVICE_BNO_A + port * 2;
    oni_reg_val_t val;

    if (oni_read_reg (ctx, device, (oni_reg_addr_t) BnoRegisters::ENABLE_BNO, &val) != ONI_ESUCCESS)
        return false;

    return static_cast<bool> (val);
}

void Rhd2000ONIBoard::setBnoAxisMap (const uint32_t port, int axisMap)
{
    if (! ctx || port > 3)
        return;

    oni_dev_idx_t device = DEVICE_BNO_A + port * 2;

    oni_write_reg (ctx, device, (uint32_t) BnoRegisters::AXIS_MAP, axisMap);
}


void Rhd2000ONIBoard::debug_printDevTable() const
{
    oni_size_t tableSize;
    size_t optSize = sizeof (tableSize);
    oni_get_opt (ctx, ONI_OPT_NUMDEVICES, &tableSize, &optSize);
    optSize = sizeof (oni_device_t) * tableSize;
    oni_device_t* devs = (oni_device_t*)malloc(optSize);
    if (devs == NULL)
        return;

    oni_get_opt (ctx, ONI_OPT_DEVICETABLE, devs, &optSize);
    for (int i = 0; i < tableSize; i++)
    {
        printf ("N: %d Idx %x Id %x Ver: %x Rd %x Wr %x\n", i, devs[i].idx, devs[i].id, devs[i].version, devs[i].read_size, devs[i].write_size);
    }

    free (devs);
}
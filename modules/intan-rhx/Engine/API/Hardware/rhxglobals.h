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

#ifndef RHXGLOBALS_H
#define RHXGLOBALS_H

#include <QString>

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS

const QString OrganizationName = "Intan Technologies";
const QString OrganizationDomain = "intantech.com";
const QString ApplicationName = "IntanRHX";
const QString ApplicationCopyrightYear = "2020-2021";

const int GRADIENTWIDTH = 300;
const int GRADIENTHEIGHT = 10;
const int BUFFERWIDTH = 25;
const int BUFFERHEIGHT = 20;
const int HASHMARKLENGTH = 5;

// Software version number (e.g., version 1.3.5)
#define SOFTWARE_MAIN_VERSION_NUMBER 3
#define SOFTWARE_SECONDARY_VERSION_NUMBER 0
#define SOFTWARE_TERTIARY_VERSION_NUMBER 3

const QString SoftwareVersion = QString::number(SOFTWARE_MAIN_VERSION_NUMBER) + "." +
                                QString::number(SOFTWARE_SECONDARY_VERSION_NUMBER) + "." +
                                QString::number(SOFTWARE_TERTIARY_VERSION_NUMBER);

// FPGA configuration bitfiles
const QString ConfigFileRHDBoard = "ConfigRHDInterfaceBoard.bit";
const QString ConfigFileRHDController = "ConfigRHDController.bit";
const QString ConfigFileRHSController = "ConfigRHSController.bit";
const QString ConfigFileXEM6010Tester = "ConfigXEM6010Tester.bit";

// Special Unicode characters, as QString data type
const QString DeltaSymbol = QString((QChar)0x0394);
const QString MuSymbol = QString((QChar)0x03bc);
const QString MicroVoltsSymbol = MuSymbol + QString("V");
const QString MicroAmpsSymbol = MuSymbol + QString("A");
const QString MicroSecondsSymbol = MuSymbol + QString("s");
const QString OmegaSymbol = QString((QChar)0x03a9);
const QString AngleSymbol = QString((QChar)0x2220);
const QString DegreeSymbol = QString((QChar)0x00b0);
const QString PlusMinusSymbol = QString((QChar)0x00b1);
const QString SqrtSymbol = QString((QChar)0x221a);
const QString EnDashSymbol = QString((QChar)0x2013);
const QString EmDashSymbol = QString((QChar)0x2014);
const QString EllipsisSymbol = QString((QChar)0x2026);
const QString CopyrightSymbol = QString((QChar)0x00a9);

const QString MuSymbolTCP = QString::fromStdWString(L"\u00b5");
const QString PlusMinusSymbolTCP = QString::fromStdWString(L"\u00b1");

const QString EndOfLine = QString("\n");

enum ControllerType {
    ControllerRecordUSB2 = 0,
    ControllerRecordUSB3 = 1,
    ControllerStimRecordUSB2 = 2
};

enum DemoSelections {
    DemoUSBInterfaceBoard,
    DemoRecordingController,
    DemoStimRecordController,
    DemoPlayback
};

const QString ControllerTypeString[3] = {
    "RHD USB Interface Board",
    "RHD Recording Controller",
    "RHS Stimulation/Recording Controller"
};

const QString ControllerTypeSettingsGroup[3] = {
    "RHDUSBInterfaceBoard",
    "RHDRecordingController",
    "RHSStimRecordingController"
};

enum AmplifierSampleRate {
    SampleRate1000Hz = 0,
    SampleRate1250Hz = 1,
    SampleRate1500Hz = 2,
    SampleRate2000Hz = 3,
    SampleRate2500Hz = 4,
    SampleRate3000Hz = 5,
    SampleRate3333Hz = 6,
    SampleRate4000Hz = 7,
    SampleRate5000Hz = 8,
    SampleRate6250Hz = 9,
    SampleRate8000Hz = 10,
    SampleRate10000Hz = 11,
    SampleRate12500Hz = 12,
    SampleRate15000Hz = 13,
    SampleRate20000Hz = 14,
    SampleRate25000Hz = 15,
    SampleRate30000Hz = 16
};

const QString SampleRateString[17] = {
    "1.0 kHz",
    "1.25 kHz",
    "1.5 kHz",
    "2 kHz",
    "2.5 kHz",
    "3.0 kHz",
    "3.33 kHz",
    "4 kHz",
    "5 kHz",
    "6.25 kHz",
    "8 kHz",
    "10 kHz",
    "12.5 kHz",
    "15 kHz",
    "20 kHz",
    "25 kHz",
    "30 kHz"
};

enum StimStepSize {
    StimStepSizeMin = 0,
    StimStepSize10nA = 1,
    StimStepSize20nA = 2,
    StimStepSize50nA = 3,
    StimStepSize100nA = 4,
    StimStepSize200nA = 5,
    StimStepSize500nA = 6,
    StimStepSize1uA = 7,
    StimStepSize2uA = 8,
    StimStepSize5uA = 9,
    StimStepSize10uA = 10,
    StimStepSizeMax = 11
};

const QString StimStepSizeString[12] = {
    "minimum step size (imprecise)",
    PlusMinusSymbol + "2.55 " + MicroAmpsSymbol + " range (10 nA step size)",
    PlusMinusSymbol + "5.10 " + MicroAmpsSymbol + " range (20 nA step size)",
    PlusMinusSymbol + "12.75 " + MicroAmpsSymbol + " range (50 nA step size)",
    PlusMinusSymbol + "25.5 " + MicroAmpsSymbol + " range (0.1 " + MicroAmpsSymbol + " step size)",
    PlusMinusSymbol + "51.0 " + MicroAmpsSymbol + " range (0.2 " + MicroAmpsSymbol + " step size)",
    PlusMinusSymbol + "127.5 " + MicroAmpsSymbol + " range (0.5 " + MicroAmpsSymbol + " step size)",
    PlusMinusSymbol + "255 " + MicroAmpsSymbol + " range (1 " + MicroAmpsSymbol + " step size)",
    PlusMinusSymbol + "510 " + MicroAmpsSymbol + " range (2 " + MicroAmpsSymbol + " step size)",
    PlusMinusSymbol + "1.275 mA range (5 " + MicroAmpsSymbol + " step size)",
    PlusMinusSymbol + "2.550 mA range (10 " + MicroAmpsSymbol + " step size)",
    "maximum step size (imprecise)"
};

enum BoardPort {
    PortA = 0,
    PortB = 1,
    PortC = 2,
    PortD = 3,
    PortE = 4,  // Port E used only with some USB3 controllers
    PortF = 5,  // Port F used only with some USB3 controllers
    PortG = 6,  // Port G used only with some USB3 controllers
    PortH = 7   // Port H used only with some USB3 controllers
};

enum BoardDataSource {  // used only with ControllerRecordUSB2
    Unassigned = -1,
    PortA1 = 0,
    PortA2 = 1,
    PortB1 = 2,
    PortB2 = 3,
    PortC1 = 4,
    PortC2 = 5,
    PortD1 = 6,
    PortD2 = 7,
    PortA1Ddr = 8,
    PortA2Ddr = 9,
    PortB1Ddr = 10,
    PortB2Ddr = 11,
    PortC1Ddr = 12,
    PortC2Ddr = 13,
    PortD1Ddr = 14,
    PortD2Ddr = 15
};

enum StimShape {
    Biphasic = 0,
    BiphasicWithInterphaseDelay = 1,
    Triphasic = 2,
    Monophasic = 3
};

enum ChipType {
    NoChip = -1,
    RHD2132Chip = 1,
    RHD2216Chip = 2,
    RHD2164Chip = 4,
    RHS2116Chip = 32,
    RHD2164MISOBChip = 1000
};

enum SignalType {
    AmplifierSignal = 0,
    AuxInputSignal = 1,
    SupplyVoltageSignal = 2,
    BoardAdcSignal = 3,
    BoardDacSignal = 4,
    BoardDigitalInSignal = 5,
    BoardDigitalOutSignal = 6
};

enum FileFormat {
    FileFormatIntan,
    FileFormatFilePerSignalType,
    FileFormatFilePerChannel
};

enum BoardMode {
    RHDUSBInterfaceBoard,
    RHSController,
    RHDController,
    CLAMPController,
    UnknownUSB2Device,
    UnknownUSB3Device
};

const unsigned int FIFOCapacityInWords = 67108864;

// Maximum number of data blocks to read at once (limited by low-frequency impedance measurements)
const int MaxNumBlocksToRead = 56;

// Intan 4-bit hardware board mode identifier
const int RHDUSBInterfaceBoardMode = 0;
const int RHDControllerBoardMode = 13;
const int RHSControllerBoardMode = 14;
const int CLAMPControllerBoardMode = 15;

// RHD2164 MISO ID numbers from ROM register 59
const int Register59MISOA = 53;
const int Register59MISOB = 58;

const int BytesPerWord = 2;

// Trigonometric constants
const double Pi = 3.14159265359;
const double TwoPi = 6.28318530718;
const double DegreesToRadians = 0.0174532925199;
const double RadiansToDegrees = 57.2957795132;

const float PiF = 3.14159265359F;
const float TwoPiF = 6.28318530718F;
const float DegreesToRadiansF = 0.0174532925199F;
const float RadiansToDegreesF = 57.2957795132F;

// log10(2) through log10(9) for logarithmic axes in plots
const double Log10_2 = 0.301029995664;
const double Log10_3 = 0.477121254720;
const double Log10_4 = 0.602059991328;
const double Log10_5 = 0.698970004336;
const double Log10_6 = 0.778151250384;
const double Log10_7 = 0.845098040014;
const double Log10_8 = 0.903089986992;
const double Log10_9 = 0.954242509439;

// Saved data file constants
const uint32_t DataFileMagicNumberRHD = 0xc6912702;
const uint32_t DataFileMagicNumberRHS = 0xd69127ac;
const int16_t DataFileMainVersionNumber = 3;
const int16_t DataFileSecondaryVersionNumber = 0;
const uint32_t SpikeFileMagicNumberAllChannels = 0x18f8474b;
const uint32_t SpikeFileMagicNumberSingleChannel = 0x18f88c00;

// TCP Waveform Output magic number
const uint32_t TCPWaveformMagicNumber = 0x2ef07a08;

// TCP Spike Output magic number
const uint32_t TCPSpikeMagicNumber = 0x3ae2710f;

// Parameter setting error messages
const QString ReadOnlyErrorMessage = "cannot be changed once software has started.";
const QString RunningErrorMessage = "cannot be set while controller is running.";
const QString RecordingErrorMessage = "cannot be set while controller is recording.";
const QString NonStimErrorMessage = "cannot be set with a non-stim controller";
const QString NonStimRunningErrorMessage = "cannot be set with a non-stim controller, or while controller is running";
const QString StimErrorMessage = "cannot be set with a stim controller";
const QString StimRunningErrorMessage = "cannot be set with a stim controller, or while controller is running";

#endif // RHXGLOBALS_H

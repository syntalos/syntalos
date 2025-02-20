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

#include <iostream>
#include <cmath>
#include <vector>
#include <queue>
#include <limits>
#include "rhxregisters.h"

RHXRegisters::RHXRegisters(ControllerType type_, double sampleRate_, StimStepSize stimStep_) :
    type(type_),
    sampleRate(sampleRate_),
    stimStep(stimStep_)
{
    if (type == ControllerRecordUSB2 || type == ControllerRecordUSB3) {
        ampPwr.resize(64);
        ampFastSettle.resize(1);
    } else {
        ampPwr.resize(16);
        ampFastSettle.resize(16);
        ampFLSelect.resize(16);
        dcAmpPwr.resize(16);
        complianceMonitor.resize(16);
        stimOn.resize(16);
        stimPol.resize(16);
        chargeRecoverySwitch.resize(16);
        cLChargeRecoveryEn.resize(16);
        negCurrentMag.resize(16);
        negCurrentTrim.resize(16);
        posCurrentMag.resize(16);
        posCurrentTrim.resize(16);
    }

    setSamplingParameters();

    // Set default values for all register settings.
    if (type == ControllerRecordUSB2 || type == ControllerRecordUSB3) {
        setDefaultRHDSettings();
    } else {
        setDefaultRHSSettings();
    }
}

// Size of on-FPGA auxiliary command RAM banks
int RHXRegisters::maxCommandLength(ControllerType type)
{
    if (type == ControllerStimRecord) return 8192;
    else return 1024;
}

int RHXRegisters::maxCommandLength() const
{
    return maxCommandLength(type);
}

// Maximum number of amplifier channels per chip
int RHXRegisters::maxNumChannelsPerChip(ControllerType type)
{
    if (type == ControllerStimRecord) return 16;
    else return 64;
}

int RHXRegisters::maxNumChannelsPerChip() const
{
    return maxNumChannelsPerChip(type);
}

// Set sampling-rate-dependent registers correctly.
void RHXRegisters::setSamplingParameters()
{
    if (type == ControllerStimRecord) {
        if (sampleRate < 6001.0) {
            muxBias = 40;
            adcBufferBias = 32;
        } else if (sampleRate < 7001.0) {
            muxBias = 40;
            adcBufferBias = 16;
        } else if (sampleRate < 8751.0) {
            muxBias = 40;
            adcBufferBias = 8;
        } else if (sampleRate < 11001.0) {
            muxBias = 32;
            adcBufferBias = 8;
        } else if (sampleRate < 14001.0) {
            muxBias = 26;
            adcBufferBias = 8;
        } else if (sampleRate < 17501.0) {
            muxBias = 18;
            adcBufferBias = 4;
        } else if (sampleRate < 22001.0) {
            muxBias = 16;
            adcBufferBias = 3;
        } else  {
            muxBias = 5;
            adcBufferBias = 3;
        }
    } else {
        muxLoad = 0;
        if (sampleRate < 3334.0) {
            muxBias = 40;
            adcBufferBias = 32;
        } else if (sampleRate < 4001.0) {
            muxBias = 40;
            adcBufferBias = 16;
        } else if (sampleRate < 5001.0) {
            muxBias = 40;
            adcBufferBias = 8;
        } else if (sampleRate < 6251.0) {
            muxBias = 32;
            adcBufferBias = 8;
        } else if (sampleRate < 8001.0) {
            muxBias = 26;
            adcBufferBias = 8;
        } else if (sampleRate < 10001.0) {
            muxBias = 18;
            adcBufferBias = 4;
        } else if (sampleRate < 12501.0) {
            muxBias = 16;
            adcBufferBias = 3;
        } else if (sampleRate < 15001.0) {
            muxBias = 7;
            adcBufferBias = 3;
        } else {
            muxBias = 4;
            adcBufferBias = 2;
        }
    }
}

void RHXRegisters::setDefaultRHDSettings()
{
    adcReferenceBw = 3;         // ADC reference generator bandwidth (0 [highest BW] - 3 [lowest BW]);
                                // always set to 3
    setFastSettle(false);       // amplifier fast settle (off = normal operation)
    ampVrefEnable = 1;          // enable amplifier voltage references (0 = power down; 1 = enable);
                                // 1 = normal operation
    adcComparatorBias = 3;      // ADC comparator preamp bias current (0 [lowets] - 3 [highest], only
                                // valid for comparator select = 2,3); always set to 3
    adcComparatorSelect = 2;    // ADC comparator select; always set to 2

    vddSenseEnable = 1;         // supply voltage sensor enable (0 = disable; 1 = enable)
    // adcBufferBias = 32;      // ADC reference buffer bias current (0 [highest current] - 63 [lowest current]);
                                // This value should be set according to ADC sampling rate; set in setSampleRate()

    // muxBias = 40;            // ADC input MUS bias current (0 [highest current] - 63 [lowest current]);
                                // This value should be set according to ADC sampling rate; set in setSampleRate()

    // muxLoad = 0;             // MUX capacitance load at ADC input (0 [min CL] - 7 [max CL]); LSB = 3 pF
                                // Set in setSampleRate()

    tempS1 = 0;                 // temperature sensor S1 (0-1); 0 = power saving mode when temperature sensor is
                                // not in use
    tempS2 = 0;                 // temperature sensor S2 (0-1); 0 = power saving mode when temperature sensor is
                                // not in use
    tempEn = 0;                 // temperature sensor enable (0 = disable; 1 = enable)
    setDigOutHiZ();             // auxiliary digital output state

    weakMiso = 1;               // weak MISO (0 = MISO line is HiZ when CS is inactive; 1 = MISO line is weakly
                                // driven when CS is inactive)
    twosComp = 0;               // two's complement ADC results (0 = unsigned offset representation; 1 = signed
                                // representation)
    absMode = 0;                // absolute value mode (0 = normal output; 1 = output passed through abs(x) function)
    enableDsp(true);            // DSP offset removal enable/disable
    setDspCutoffFreq(1.0);      // DSP offset removal HPF cutoff frequency

    zcheckDacPower = 1;         // impedance testing DAC power-up (0 = power down; 1 = power up)
    zcheckLoad = 0;             // impedance testing dummy load (0 = normal operation; 1 = insert 60 pF to ground)
    setZcheckScale(ZcheckCs100fF);  // impedance testing scale factor (100 fF, 1.0 pF, or 10.0 pF)
    zcheckConnAll = 0;          // impedance testing connect all (0 = normal operation; 1 = connect all electrodes together)
    setZcheckPolarity(ZcheckPositiveInput); // impedance testing polarity select (RHD2216 only) (0 = test positive inputs;
                                // 1 = test negative inputs)
    enableZcheck(false);        // impedance testing enable/disable

    setZcheckChannel(0);        // impedance testing amplifier select (0-63)

    offChipRH1 = 0;             // bandwidth resistor RH1 on/off chip (0 = on chip; 1 = off chip)
    offChipRH2 = 0;             // bandwidth resistor RH2 on/off chip (0 = on chip; 1 = off chip)
    offChipRL = 0;              // bandwidth resistor RL on/off chip (0 = on chip; 1 = off chip)
    adcAux1En = 1;              // enable ADC aux1 input (when RH1 is on chip) (0 = disable; 1 = enable)
    adcAux2En = 1;              // enable ADC aux2 input (when RH2 is on chip) (0 = disable; 1 = enable)
    adcAux3En = 1;              // enable ADC aux3 input (when RL is on chip) (0 = disable; 1 = enable)

    setUpperBandwidth(7500.0);  // set upper bandwidth of amplifiers
    setLowerBandwidth(1.0);     // set lower bandwidth of amplifiers

    powerUpAllAmps();           // turn on all amplifiers
}

void RHXRegisters::setDefaultRHSSettings()
{
    setDigOutHiZ(DigOut1);      // auxiliary digital output states
    setDigOutHiZ(DigOut2);
    setDigOutHiZ(DigOutOD);

    weakMiso = 1;               // weak MISO (0 = MISO line is HiZ when CS is inactive; 1 = MISO line is weakly
                                // driven when CS is inactive)
    twosComp = 0;               // two's complement ADC results (0 = unsigned offset representation; 1 = signed
                                // representation)
    absMode = 0;                // absolute value mode (0 = normal output; 1 = output passed through abs(x) function)
    enableDsp(true);            // DSP offset removal enable/disable
    setDspCutoffFreq(1.0);      // DSP offset removal HPF cutoff frequency

    zcheckDacPower = 1;         // impedance testing DAC power-up (0 = power down; 1 = power up)
    zcheckLoad = 0;             // impedance testing dummy load (0 = normal operation; 1 = insert 60 pF to ground)
    setZcheckScale(ZcheckCs100fF);  // impedance testing scale factor (100 fF, 1.0 pF, or 10.0 pF)

    enableZcheck(false);        // impedance testing enable/disable

    setZcheckChannel(0);        // impedance testing amplifier select (0-15)

    setUpperBandwidth(7500.0);  // set upper bandwidth of amplifiers
    setLowerBandwidth(1.0, 0);  // set lower bandwidth of amplifiers
    setLowerBandwidth(1000.0, 1); // set lower bandwidth of amplifiers for amplifier settle function

    setStimStepSize(stimStep);  // set stimulation current DAC step size
    setChargeRecoveryCurrentLimit(CurrentLimit10nA);        // set charge recovery current limit
    setChargeRecoveryTargetVoltage(0);                      // set charge recovery target voltage

    for (int channel = 0; channel < 16; channel++) {
        setPosStimMagnitude(channel, 0, 0);             // set all stimulation magnitudes to zero
        setNegStimMagnitude(channel, 0, 0);
    }

    for (int channel = 0; channel < 16; channel++) {
        ampFastSettle[channel] = 0;
        ampFLSelect[channel] = 0;
        stimOn[channel] = 0;
        stimPol[channel] = 0;
        chargeRecoverySwitch[channel] = 0;
        cLChargeRecoveryEn[channel] = 0;
    }

    powerUpAllAmps();           // turn on all amplifiers
    powerUpAllDCAmps();         // turn on all DC amplifiers
    setStimEnable(true);        // enable stimulation
}

// Enable or disable amplifier fast settle function; drive amplifiers to baseline if enabled.
void RHXRegisters::setFastSettle(bool enabled)
{
    for (unsigned int i = 0; i < ampFastSettle.size(); ++i) {
        ampFastSettle[i] = (enabled ? 1 : 0);
    }
}

// Drive auxiliary digital output low.
void RHXRegisters::setDigOutLow(DigOut pin)
{
    switch (pin) {
    case(DigOut1):
        digOut1 = 0;
        digOut1HiZ = 0;
        break;
    case(DigOut2):
        digOut2 = 0;
        digOut2HiZ = 0;
        break;
    case (DigOutOD) :
        digOutOD = 1;
        break;
    }
}

// Drive auxiliary digital output high.
void RHXRegisters::setDigOutHigh(DigOut pin)
{
    switch (pin) {
    case(DigOut1) :
        digOut1 = 1;
        digOut1HiZ = 0;
        break;
    case(DigOut2) :
        digOut2 = 1;
        digOut2HiZ = 0;
        break;
    case (DigOutOD) :
        digOutOD = 0;
        break;
    }
}

// Set auxiliary digital output to high-impedance (HiZ) state.
void RHXRegisters::setDigOutHiZ(DigOut pin)
{
    switch (pin) {
    case(DigOut1) :
        digOut1 = 0;
        digOut1HiZ = 1;
        break;
    case(DigOut2) :
        digOut2 = 0;
        digOut2HiZ = 1;
        break;
    case (DigOutOD) :
        digOutOD = 0;
        break;
    }
}

// Enable or disable ADC auxiliary input 1.
void RHXRegisters::enableAux1(bool enabled)
{
    adcAux1En = (enabled ? 1 : 0);
}

// Enable or disable ADC auxiliary input 2.
void RHXRegisters::enableAux2(bool enabled)
{
    adcAux2En = (enabled ? 1 : 0);
}

// Enable or disable ADC auxiliary input 3.
void RHXRegisters::enableAux3(bool enabled)
{
    adcAux3En = (enabled ? 1 : 0);
}

// Enable or disable DSP offset removal filter.
void RHXRegisters::enableDsp(bool enabled)
{
    dspEn = (enabled ? 1 : 0);
}

// Return a size-16 vector containing all possible cutoff frequencies for the on-chip DSP offset
// removal filter (a one-pole highpass filter).
std::vector<double> RHXRegisters::getDspFreqTable(double sampleRate_)
{
    std::vector<double> fCutoff(16, 0.0);
    // Note: fCutoff[0] = 0.0 here, but this index should not be used.
    for (int n = 1; n < 16; ++n) {
        double x = pow(2.0, (double) n);
        fCutoff[n] = sampleRate_ * log(x / (x - 1.0)) / TwoPi;
    }
    return fCutoff;
}

// Set the DSP offset removal filter cutoff frequency as closely to the requested
// newDspCutoffFreq (in Hz) as possible; returns the actual cutoff frequency (in Hz).
double RHXRegisters::setDspCutoffFreq(double newDspCutoffFreq)
{
    std::vector<double> fCutoff = getDspFreqTable();
    double logNewDspCutoffFreq = log10(newDspCutoffFreq);

    // Find the closest value to the requested cutoff frequency (on a logarithmic scale).
    if (newDspCutoffFreq > fCutoff[1]) {
        dspCutoffFreq = 1;
    } else if (newDspCutoffFreq < fCutoff[15]) {
        dspCutoffFreq = 15;
    } else {
        double minLogDiff = 10000000.0;
        for (int n = 1; n < 16; n++) {
            double logFCutoff = log10(fCutoff[n]);
            if (fabs(logNewDspCutoffFreq - logFCutoff) < minLogDiff) {
                minLogDiff = fabs(logNewDspCutoffFreq - logFCutoff);
                dspCutoffFreq = n;
            }
        }
    }
    return fCutoff[dspCutoffFreq];
}

// Return the current value of the DSP offset removal cutoff frequency (in Hz).
double RHXRegisters::getDspCutoffFreq() const
{
    double x = pow(2.0, (double) dspCutoffFreq);

    return sampleRate * log(x / (x - 1.0)) / TwoPi;
}

// Enable or disable impedance checking mode.
void RHXRegisters::enableZcheck(bool enabled)
{
    zcheckEn = (enabled ? 1: 0);
}

// Power up or down impedance checking DAC.
void RHXRegisters::setZcheckDacPower(bool enabled)
{
    zcheckDacPower = (enabled ? 1 : 0);
}

// Select the series capacitor used to convert the voltage waveform generated by the on-chip
// DAC into an AC current waveform that stimulates a selected electrode for impedance testing
// (ZcheckCs100fF, ZcheckCs1pF, or Zcheck10pF).
void RHXRegisters::setZcheckScale(ZcheckCs scale)
{
    switch (scale) {
        case ZcheckCs100fF:
            zcheckScale = 0x00;     // Cs = 0.1 pF
            break;
        case ZcheckCs1pF:
            zcheckScale = 0x01;     // Cs = 1.0 pF
            break;
        case ZcheckCs10pF:
            zcheckScale = 0x03;     // Cs = 10.0 pF
            break;
    }
}

// Select impedance testing of positive or negative amplifier inputs (RHD2216 only), based
// on the variable polarity (ZcheckPositiveInput or ZcheckNegativeInput).
void RHXRegisters::setZcheckPolarity(ZcheckPolarity polarity)
{
    switch (polarity) {
        case ZcheckPositiveInput:
            zcheckSelPol = 0;
            break;
    case ZcheckNegativeInput:
            zcheckSelPol = 1;
            break;
    }
}

// Select the amplifier channel for impedance testing.
int RHXRegisters::setZcheckChannel(int channel)
{
    if (channel < 0 || channel > maxNumChannelsPerChip() - 1) {
        return -1;
    } else {
        zcheckSelect = channel;
        return zcheckSelect;
    }
}

// Power up or down selected amplifier on chip.
void RHXRegisters::setAmpPowered(int channel, bool powered)
{
    if (channel >= 0 && channel <= maxNumChannelsPerChip() - 1) {
        ampPwr[channel] = (powered ? 1 : 0);
    }
}

// Power up all amplifiers on chip.
void RHXRegisters::powerUpAllAmps()
{
    for (int channel = 0; channel < maxNumChannelsPerChip(); ++channel) {
        ampPwr[channel] = 1;
    }
}

// Power down all amplifiers on chip.
void RHXRegisters::powerDownAllAmps()
{
    for (int channel = 0; channel < maxNumChannelsPerChip(); ++channel) {
        ampPwr[channel] = 0;
    }
}

// Power up or down selected DC amplifier on chip.
void RHXRegisters::setDCAmpPowered(int channel, bool powered)
{
    if (channel >= 0 && channel < maxNumChannelsPerChip()) {
        dcAmpPwr[channel] = (powered ? 1 : 0);
    }
}

// Power up all DC amplifiers on chip.
void RHXRegisters::powerUpAllDCAmps()
{
    for (int channel = 0; channel < maxNumChannelsPerChip(); ++channel) {
        dcAmpPwr[channel] = 1;
    }
}

// Power down all DC amplifiers on chip.
void RHXRegisters::powerDownAllDCAmps()
{
    for (int channel = 0; channel < maxNumChannelsPerChip(); ++channel) {
        dcAmpPwr[channel] = 0;
    }
}

// Enable or disable stimulation globally.
void RHXRegisters::setStimEnable(bool enable)
{
    if (enable) {
        stimEnableA = 0xaaaa;
        stimEnableB = 0x00ff;
    } else {
        stimEnableA = 0x0000;
        stimEnableB = 0x0000;
    }
}

int RHXRegisters::getRegisterValue(int reg) const
{
    if (type == ControllerStimRecord) return getRHSRegisterValue(reg);
    else return getRHDRegisterValue(reg);
}

// Returns the value of a selected RAM register (0-250) on the RHS2116 chip, based
// on the current register variables in the class instance.
int RHXRegisters::getRHSRegisterValue(int reg) const
{
    int regout;
    const int ZcheckDac = 128;  // midrange

    switch (reg) {
    case 0:
        regout = (adcBufferBias << 6) + muxBias;
        break;
    case 1:
        regout = (digOutOD << 12) + (digOut2 << 11) + (digOut2HiZ << 10) + (digOut1 << 9) +
            (digOut1HiZ << 8) + (weakMiso << 7) + (twosComp << 6) + (absMode << 5) +
            (dspEn << 4) + dspCutoffFreq;
        break;
    case 2:
        regout = (zcheckSelect << 8) + (zcheckDacPower << 6) + (zcheckLoad << 5) +
            (zcheckScale << 3) + zcheckEn;
        break;
    case 3:
        regout = ZcheckDac;
        break;
    case 4:
        regout = (rH1Dac2 << 6) + rH1Dac1;
        break;
    case 5:
        regout = (rH2Dac2 << 6) + rH2Dac1;
        break;
    case 6:
        regout = (rLDac3A << 13) + (rLDac2A << 7) + rLDac1A;
        break;
    case 7:
        regout = (rLDac3B << 13) + (rLDac2B << 7) + rLDac1B;
        break;
    case 8:
        regout = vectorToWord(ampPwr);
        break;
    case 10:
        regout = vectorToWord(ampFastSettle);
        break;
    case 12:
        regout = vectorToWord(ampFLSelect);
        break;
    case 32:
        regout = stimEnableA;
        break;
    case 33:
        regout = stimEnableB;
        break;
    case 34:
        regout = (stimStepSel3 << 13) + (stimStepSel2 << 7) + stimStepSel1;
        break;
    case 35:
        regout = (stimPBias << 4) + stimNBias;
        break;
    case 36:
        regout = chargeRecoveryDac;
        break;
    case 37:
        regout = (chargeRecoveryCurrentLimitSel3 << 13) + (chargeRecoveryCurrentLimitSel2 << 7) +
            chargeRecoveryCurrentLimitSel1;
        break;
    case 38:
        regout = vectorToWord(dcAmpPwr);
        break;
    // Register 40 is read only.
    case 42:
        regout = vectorToWord(stimOn);
        break;
    case 44:
        regout = vectorToWord(stimPol);
        break;
    case 46:
        regout = vectorToWord(chargeRecoverySwitch);
        break;
    case 48:
        regout = vectorToWord(cLChargeRecoveryEn);
        break;
    // Register 50 is read only.
    case 64:
        regout = (negCurrentTrim[0] << 8) + negCurrentMag[0];
        break;
    case 65:
        regout = (negCurrentTrim[1] << 8) + negCurrentMag[1];
        break;
    case 66:
        regout = (negCurrentTrim[2] << 8) + negCurrentMag[2];
        break;
    case 67:
        regout = (negCurrentTrim[3] << 8) + negCurrentMag[3];
        break;
    case 68:
        regout = (negCurrentTrim[4] << 8) + negCurrentMag[4];
        break;
    case 69:
        regout = (negCurrentTrim[5] << 8) + negCurrentMag[5];
        break;
    case 70:
        regout = (negCurrentTrim[6] << 8) + negCurrentMag[6];
        break;
    case 71:
        regout = (negCurrentTrim[7] << 8) + negCurrentMag[7];
        break;
    case 72:
        regout = (negCurrentTrim[8] << 8) + negCurrentMag[8];
        break;
    case 73:
        regout = (negCurrentTrim[9] << 8) + negCurrentMag[9];
        break;
    case 74:
        regout = (negCurrentTrim[10] << 8) + negCurrentMag[10];
        break;
    case 75:
        regout = (negCurrentTrim[11] << 8) + negCurrentMag[11];
        break;
    case 76:
        regout = (negCurrentTrim[12] << 8) + negCurrentMag[12];
        break;
    case 77:
        regout = (negCurrentTrim[13] << 8) + negCurrentMag[13];
        break;
    case 78:
        regout = (negCurrentTrim[14] << 8) + negCurrentMag[14];
        break;
    case 79:
        regout = (negCurrentTrim[15] << 8) + negCurrentMag[15];
        break;
    case 96:
        regout = (posCurrentTrim[0] << 8) + posCurrentMag[0];
        break;
    case 97:
        regout = (posCurrentTrim[1] << 8) + posCurrentMag[1];
        break;
    case 98:
        regout = (posCurrentTrim[2] << 8) + posCurrentMag[2];
        break;
    case 99:
        regout = (posCurrentTrim[3] << 8) + posCurrentMag[3];
        break;
    case 100:
        regout = (posCurrentTrim[4] << 8) + posCurrentMag[4];
        break;
    case 101:
        regout = (posCurrentTrim[5] << 8) + posCurrentMag[5];
        break;
    case 102:
        regout = (posCurrentTrim[6] << 8) + posCurrentMag[6];
        break;
    case 103:
        regout = (posCurrentTrim[7] << 8) + posCurrentMag[7];
        break;
    case 104:
        regout = (posCurrentTrim[8] << 8) + posCurrentMag[8];
        break;
    case 105:
        regout = (posCurrentTrim[9] << 8) + posCurrentMag[9];
        break;
    case 106:
        regout = (posCurrentTrim[10] << 8) + posCurrentMag[10];
        break;
    case 107:
        regout = (posCurrentTrim[11] << 8) + posCurrentMag[11];
        break;
    case 108:
        regout = (posCurrentTrim[12] << 8) + posCurrentMag[12];
        break;
    case 109:
        regout = (posCurrentTrim[13] << 8) + posCurrentMag[13];
        break;
    case 110:
        regout = (posCurrentTrim[14] << 8) + posCurrentMag[14];
        break;
    case 111:
        regout = (posCurrentTrim[15] << 8) + posCurrentMag[15];
        break;
    default:
        regout = -1;
    }
    return regout;
}

// Return the value of a selected RAM register (0-21) on the RHD2000 chip, based
// on the current register variables in the class instance.
int RHXRegisters::getRHDRegisterValue(int reg) const
{
    int regout;
    const int ZcheckDac = 128;  // midrange

    switch (reg) {
    case 0:
        regout = (adcReferenceBw << 6) + (ampFastSettle[0] << 5) + (ampVrefEnable << 4) +
                (adcComparatorBias << 2) + adcComparatorSelect;
        break;
    case 1:
        regout = (vddSenseEnable << 6) + adcBufferBias;
        break;
    case 2:
        regout = muxBias;
        break;
    case 3:
        regout = (muxLoad << 5) + (tempS2 << 4) + (tempS1 << 3) + (tempEn << 2) +
                (digOut1HiZ << 1) + digOut1;
        break;
    case 4:
        regout = (weakMiso << 7) + (twosComp << 6) + (absMode << 5) + (dspEn << 4) +
                dspCutoffFreq;
        break;
    case 5:
        regout = (zcheckDacPower << 6) + (zcheckLoad << 5) + (zcheckScale << 3) +
                (zcheckConnAll << 2) + (zcheckSelPol << 1) + zcheckEn;
        break;
    case 6:
        regout = ZcheckDac;
        break;
    case 7:
        regout = zcheckSelect;
        break;
    case 8:
        regout = (offChipRH1 << 7) + rH1Dac1;
        break;
    case 9:
        regout = (adcAux1En << 7) + rH1Dac2;
        break;
    case 10:
        regout = (offChipRH2 << 7) + rH2Dac1;
        break;
    case 11:
        regout = (adcAux2En << 7) + rH2Dac2;
        break;
    case 12:
        regout = (offChipRL << 7) + rLDac1B;
        break;
    case 13:
        regout = (adcAux3En << 7) + (rLDac3B << 6) + rLDac2B;
        break;
    case 14:
        regout = (ampPwr[7] << 7) + (ampPwr[6] << 6) + (ampPwr[5] << 5) + (ampPwr[4] << 4) +
                (ampPwr[3] << 3) + (ampPwr[2] << 2) + (ampPwr[1] << 1) + ampPwr[0];
        break;
    case 15:
        regout = (ampPwr[15] << 7) + (ampPwr[14] << 6) + (ampPwr[13] << 5) + (ampPwr[12] << 4) +
                (ampPwr[11] << 3) + (ampPwr[10] << 2) + (ampPwr[9] << 1) + ampPwr[0];
        break;
    case 16:
        regout = (ampPwr[23] << 7) + (ampPwr[22] << 6) + (ampPwr[21] << 5) + (ampPwr[20] << 4) +
                (ampPwr[19] << 3) + (ampPwr[18] << 2) + (ampPwr[17] << 1) + ampPwr[16];
        break;
    case 17:
        regout = (ampPwr[31] << 7) + (ampPwr[30] << 6) + (ampPwr[29] << 5) + (ampPwr[28] << 4) +
                (ampPwr[27] << 3) + (ampPwr[26] << 2) + (ampPwr[25] << 1) + ampPwr[24];
        break;
    case 18:
        regout = (ampPwr[39] << 7) + (ampPwr[38] << 6) + (ampPwr[37] << 5) + (ampPwr[36] << 4) +
                (ampPwr[35] << 3) + (ampPwr[34] << 2) + (ampPwr[33] << 1) + ampPwr[32];
        break;
    case 19:
        regout = (ampPwr[47] << 7) + (ampPwr[46] << 6) + (ampPwr[45] << 5) + (ampPwr[44] << 4) +
                (ampPwr[43] << 3) + (ampPwr[42] << 2) + (ampPwr[41] << 1) + ampPwr[40];
        break;
    case 20:
        regout = (ampPwr[55] << 7) + (ampPwr[54] << 6) + (ampPwr[53] << 5) + (ampPwr[52] << 4) +
                (ampPwr[51] << 3) + (ampPwr[50] << 2) + (ampPwr[49] << 1) + ampPwr[48];
        break;
    case 21:
        regout = (ampPwr[63] << 7) + (ampPwr[62] << 6) + (ampPwr[61] << 5) + (ampPwr[60] << 4) +
                (ampPwr[59] << 3) + (ampPwr[58] << 2) + (ampPwr[57] << 1) + ampPwr[56];
        break;
    default:
        regout = -1;
    }
    return regout;
}

// Convert a 16-bit vector to a 16-bit word.
int RHXRegisters::vectorToWord(const std::vector<int> &v)
{
    int word = 0;

    for (int i = 0; i < 16; i++) {
        if (v[i]) word += (1 << i);
    }

    return word;
}


// Return the value of the RH1 resistor (in ohms) corresponding to a particular upper
// bandwidth value (in Hz).
double RHXRegisters::rH1FromUpperBandwidth(double upperBandwidth) const
{
    double log10f = log10(upperBandwidth);

    return 0.9730 * pow(10.0, (8.0968 - 1.1892 * log10f + 0.04767 * log10f * log10f));
}

// Return the value of the RH2 resistor (in ohms) corresponding to a particular upper
// bandwidth value (in Hz).
double RHXRegisters::rH2FromUpperBandwidth(double upperBandwidth) const
{
    double log10f = log10(upperBandwidth);

    return 1.0191 * pow(10.0, (8.1009 - 1.0821 * log10f + 0.03383 * log10f * log10f));
}

// Return the value of the RL resistor (in ohms) corresponding to a particular lower
// bandwidth value (in Hz).
double RHXRegisters::rLFromLowerBandwidth(double lowerBandwidth) const
{
    double log10f = log10(lowerBandwidth);

    if (lowerBandwidth < 4.0) {
        return 1.0061 * pow(10.0, (4.9391 - 1.2088 * log10f + 0.5698 * log10f * log10f +
            0.1442 * log10f * log10f * log10f));
    } else {
        return 1.0061 * pow(10.0, (4.7351 - 0.5916 * log10f + 0.08482 * log10f * log10f));
    }
}

// Return the amplifier upper bandwidth (in Hz) corresponding to a particular value
// of the resistor RH1 (in ohms).
double RHXRegisters::upperBandwidthFromRH1(double rH1) const
{
    double a, b, c;

    a = 0.04767;
    b = -1.1892;
    c = 8.0968 - log10(rH1 / 0.9730);

    return pow(10.0, ((-b - sqrt(b * b - 4 * a * c)) / (2 * a)));
}

// Return the amplifier upper bandwidth (in Hz) corresponding to a particular value
// of the resistor RH2 (in ohms).
double RHXRegisters::upperBandwidthFromRH2(double rH2) const
{
    double a, b, c;

    a = 0.03383;
    b = -1.0821;
    c = 8.1009 - log10(rH2 / 1.0191);

    return pow(10.0, ((-b - sqrt(b * b - 4 * a * c)) / (2 * a)));
}

// Return the amplifier lower bandwidth (in Hz) corresponding to a particular value
// of the resistor RL (in ohms).
double RHXRegisters::lowerBandwidthFromRL(double rL) const
{
    double a, b, c;

    // Quadratic fit below is invalid for values of RL less than 5.1 kOhm.
    if (rL < 5100.0) {
        rL = 5100.0;
    }

    if (rL < 30000.0) {
        a = 0.08482;
        b = -0.5916;
        c = 4.7351 - log10(rL / 1.0061);
    } else {
        a = 0.3303;
        b = -1.2100;
        c = 4.9873 - log10(rL / 1.0061);
    }

    return pow(10.0, ((-b - sqrt(b * b - 4 * a * c)) / (2 * a)));
}

// Set the on-chip RH1 and RH2 DAC values appropriately to set a particular amplifier
// upper bandwidth (in Hz).  Return an estimate of the actual upper bandwidth achieved.
double RHXRegisters::setUpperBandwidth(double upperBandwidth)
{
    const double RH1Base = 2200.0;
    const double RH1Dac1Unit = 600.0;
    const double RH1Dac2Unit = 29400.0;
    const int RH1Dac1Steps = 63;
    const int RH1Dac2Steps = 31;

    const double RH2Base = 8700.0;
    const double RH2Dac1Unit = 763.0;
    const double RH2Dac2Unit = 38400.0;
    const int RH2Dac1Steps = 63;
    const int RH2Dac2Steps = 31;

    // Upper bandwidths higher than 30 kHz don't work well with the RHS amplifiers.
    if (upperBandwidth > 30000.0) {
        upperBandwidth = 30000.0;
    }

    double rH1Target = rH1FromUpperBandwidth(upperBandwidth);

    rH1Dac1 = 0;
    rH1Dac2 = 0;
    double rH1Actual = RH1Base;

    for (int i = 0; i < RH1Dac2Steps; ++i) {
        if (rH1Actual < rH1Target - (RH1Dac2Unit - RH1Dac1Unit / 2)) {
            rH1Actual += RH1Dac2Unit;
            ++rH1Dac2;
        }
    }

    for (int i = 0; i < RH1Dac1Steps; ++i) {
        if (rH1Actual < rH1Target - (RH1Dac1Unit / 2)) {
            rH1Actual += RH1Dac1Unit;
            ++rH1Dac1;
        }
    }

    double rH2Target = rH2FromUpperBandwidth(upperBandwidth);

    rH2Dac1 = 0;
    rH2Dac2 = 0;
    double rH2Actual = RH2Base;

    for (int i = 0; i < RH2Dac2Steps; ++i) {
        if (rH2Actual < rH2Target - (RH2Dac2Unit - RH2Dac1Unit / 2)) {
            rH2Actual += RH2Dac2Unit;
            ++rH2Dac2;
        }
    }

    for (int i = 0; i < RH2Dac1Steps; ++i) {
        if (rH2Actual < rH2Target - (RH2Dac1Unit / 2)) {
            rH2Actual += RH2Dac1Unit;
            ++rH2Dac1;
        }
    }

    double actualUpperBandwidth1 = upperBandwidthFromRH1(rH1Actual);
    double actualUpperBandwidth2 = upperBandwidthFromRH2(rH2Actual);

    // Upper bandwidth estimates calculated from actual RH1 value and acutal RH2 value
    // should be very close; we will take their geometric mean to get a single
    // number.
    double actualUpperBandwidth = sqrt(actualUpperBandwidth1 * actualUpperBandwidth2);

    /*
    cout << '\n';
    cout << "RHXRegisters::setUpperBandwidth\n";
    cout << fixed << setprecision(1);

    cout << "RH1 DAC2 = " << rH1Dac2 << ", DAC1 = " << rH1Dac1 << '\n';
    cout << "RH1 target: " << rH1Target << " Ohms\n";
    cout << "RH1 actual: " << rH1Actual << " Ohms\n";

    cout << "RH2 DAC2 = " << rH2Dac2 << ", DAC1 = " << rH2Dac1 << '\n';
    cout << "RH2 target: " << rH2Target << " Ohms\n";
    cout << "RH2 actual: " << rH2Actual << " Ohms\n";

    cout << "Upper bandwidth target: " << upperBandwidth << " Hz\n";
    cout << "Upper bandwidth actual: " << actualUpperBandwidth << " Hz\n";

    cout << '\n';
    cout << setprecision(6);
    cout.unsetf(ios::floatfield);
    */

    return actualUpperBandwidth;
}

// Set the on-chip RL DAC values appropriately to set a particular amplifier
// lower bandwidth (in Hz).  If select == 1, fL_A is set; if select == 0, fL_B
// is set (RHS chips only).  Return an estimate of the actual lower bandwidth achieved.
// For RHD chips, the default value of select = 0 should be used.
double RHXRegisters::setLowerBandwidth(double lowerBandwidth, int select)
{
    const double RLBase = 3500.0;
    const double RLDac1Unit = 175.0;
    const double RLDac2Unit = 12700.0;
    const double RLDac3Unit = 3000000.0;
    const int RLDac1Steps = 127;
    const int RLDac2Steps = 63;

    // Lower bandwidths higher than 1.5 kHz don't work well with the RHX amplifiers.
    if (lowerBandwidth > 1500.0) {
        lowerBandwidth = 1500.0;
    }

    double rLTarget = rLFromLowerBandwidth(lowerBandwidth);

    int rLDac1 = 0;
    int rLDac2 = 0;
    int rLDac3 = 0;
    double rLActual = RLBase;

    if (lowerBandwidth < 0.15) {
        rLActual += RLDac3Unit;
        ++rLDac3;
    }

    for (int i = 0; i < RLDac2Steps; ++i) {
        if (rLActual < rLTarget - (RLDac2Unit - RLDac1Unit / 2)) {
            rLActual += RLDac2Unit;
            ++rLDac2;
        }
    }

    for (int i = 0; i < RLDac1Steps; ++i) {
        if (rLActual < rLTarget - (RLDac1Unit / 2)) {
            rLActual += RLDac1Unit;
            ++rLDac1;
        }
    }

    double actualLowerBandwidth = lowerBandwidthFromRL(rLActual);

    if (select) {
        rLDac1A = rLDac1;
        rLDac2A = rLDac2;
        rLDac3A = rLDac3;
    } else {
        rLDac1B = rLDac1;
        rLDac2B = rLDac2;
        rLDac3B = rLDac3;
    }

    /*
    cout << '\n';
    cout << fixed << setprecision(1);
    cout << "RHXRegisters::setLowerBandwidth\n";

    cout << "RL DAC3 = " << rLDac3 << ", DAC2 = " << rLDac2 << ", DAC1 = " << rLDac1 << '\n';
    cout << "RL target: " << rLTarget << " Ohms\n";
    cout << "RL actual: " << rLActual << " Ohms\n";

    cout << setprecision(3);

    cout << "Lower bandwidth target: " << lowerBandwidth << " Hz\n";
    cout << "Lower bandwidth actual: " << actualLowerBandwidth << " Hz\n";

    cout << '\n';
    cout << setprecision(6);
    cout.unsetf(ios::floatfield);
    */

    return actualLowerBandwidth;
}

// Return a MOSI command (CALIBRATE or CLEAR or COMPLIANCE MONITOR RESET).
unsigned int RHXRegisters::createRHXCommand(RHXCommandType commandType)
{
    if (type == ControllerStimRecord) {
        switch (commandType) {
        case RHXCommandCalibrate:
            return 0x55000000;   // 01010101 00000000 00000000 00000000
            break;
        case RHXCommandCalClear:
            return 0x6a000000;   // 01101010 00000000 00000000 00000000
            break;
        case RHXCommandComplianceReset:
            return 0xd0ff0000;   // 11010000 11111111 00000000 00000000 (Read from Register 255 with M flag set).
            break;
        default:
            std::cerr << "Error in RHXRegisters::createRHXCommand: " <<
                "Only 'Calibrate', 'Clear Calibration', or 'Compliance Monitor Reset' commands take zero arguments.\n";
            return 0xffffffff;
        }
    } else {
        switch (commandType) {
        case RHXCommandCalibrate:
            return 0x5500;   // 0101010100000000
            break;
        case RHXCommandCalClear:
            return 0x6a00;   // 0110101000000000
            break;
        default:
            std::cerr << "Error in RHXRegisters::createRHXCommand: " <<
                    "Only 'Calibrate' or 'Clear Calibration' commands take zero arguments.\n";
            return 0xffffffff;
        }
    }
}

// Return a MOSI command (CONVERT or READ).
unsigned int RHXRegisters::createRHXCommand(RHXCommandType commandType, unsigned int arg1)
{
    if (type == ControllerStimRecord) {
        switch (commandType) {
        case RHXCommandConvert:
            if (arg1 > 15) {
                std::cerr << "Error in RHXRegisters::createRHX0Command: " <<
                    "Channel number out of range.\n";
                return 0xffffffff;
            }
            return 0x00000000 + (arg1 << 16);  // 00umdh00 00cccccc 00000000 00000000; if the command is 'Convert',
                                               // arg1 is the channel number.
            break;
        case RHXCommandRegRead:
            if (arg1 > 255) {
                std::cerr << "Error in RHXRegisters::createRHXCommand: " <<
                    "Register address out of range.\n";
                return 0xffffffff;
            }
            return 0xc0000000 + (arg1 << 16);  // 11um0000 rrrrrrrr 00000000 00000000; if the command is 'Register Read',
                                               // arg1 is the register address.
            break;
        default:
            std::cerr << "Error in RHXRegisters::createRHXCommand: " <<
                "Only 'Convert' and 'Register Read' commands take one argument.\n";
            return 0xffffffff;
        }
    } else {
        switch (commandType) {
        case RHXCommandConvert:
            if (((int)arg1 < 0) || (arg1 > 63)) {
                std::cerr << "Error in RHXRegisters::createRHXCommand: " <<
                        "Channel number out of range.\n";
                return -1;
            }
            return 0x0000 + (arg1 << 8);  // 00cccccc0000000h; if the command is 'Convert',
                                          // arg1 is the channel number.
        case RHXCommandRegRead:
            if (((int)arg1 < 0) || (arg1 > 63)) {
                std::cerr << "Error in RHXRegisters::createRHXCommand: " <<
                        "Register address out of range.\n";
                return -1;
            }
            return 0xc000 + (arg1 << 8);  // 11rrrrrr00000000; if the command is 'Register Read',
                                          // arg1 is the register address.
            break;
        default:
            std::cerr << "Error in RHXRegisters::createRHXCommand: " <<
                    "Only 'Convert' and 'Register Read' commands take one argument.\n";
            return 0xffffffff;
        }
    }
}

// Return a MOSI command (WRITE).
unsigned int RHXRegisters::createRHXCommand(RHXCommandType commandType, unsigned int arg1, unsigned int arg2)
{
    if (type == ControllerStimRecord) {
        switch (commandType) {
        case RHXCommandRegWrite:
            if (arg1 > 255) {
                std::cerr << "Error in RHXRegisters::createRHXCommand: " <<
                    "Register address out of range.\n";
                return 0xffffffff;
            }
            if (arg2 > 65535) {
                std::cerr << "Error in RHXRegisters::createRHXCommand: " <<
                    "Register data out of range.\n";
                return 0xffffffff;
            }
            return 0x80000000 + (arg1 << 16) + arg2; // 10um0000 rrrrrrrr dddddddd dddddddd; if the command is 'Register Write',
                                                     // arg1 is the register address and arg2 is the data.
            break;
        case RHXCommandConvert:
            if (arg1 > 15) {
                std::cerr << "Error in RHXRegisters::createRHXCommand: " <<
                    "Channel number out of range.\n";
                return 0xffffffff;
            }
            return 0x00000000 + (arg2 << 26) + (arg1 << 16);  // 00umdh00 00cccccc 00000000 00000000; if the command is 'Convert',
                                                              // arg1 is the channel number and arg2 is the H flag.
            break;
        default:
            std::cerr << "Error in RHXRegisters::createRHXCommand: " <<
                "Only 'Register Write' and 'Convert' commands take two arguments.\n";
            return 0xffffffff;
        }
    } else {
        switch (commandType) {
        case RHXCommandRegWrite:
            if (((int)arg1 < 0) || (arg1 > 63)) {
                std::cerr << "Error in RHXRegisters::createRHXCommand: " <<
                        "Register address out of range.\n";
                return -1;
            }
            if (((int)arg2 < 0) || (arg2 > 255)) {
                std::cerr << "Error in RHXRegisters::createRHXCommand: " <<
                        "Register data out of range.\n";
                return -1;
            }
            return 0x8000 + (arg1 << 8) + arg2; // 10rrrrrrdddddddd; if the command is 'Register Write',
                                                // arg1 is the register address and arg1 is the data.
            break;
        default:
            std::cerr << "Error in RHXRegisters::createRHXCommand: " <<
                    "Only 'Register Write' commands take two arguments.\n";
            return 0xffffffff;
        }
    }
}

// Return a MOSI command (WRITE).
unsigned int RHXRegisters::createRHXCommand(RHXCommandType commandType, unsigned int arg1, unsigned int arg2,
                                            unsigned int uFlag, unsigned int mFlag)
{
    if (type == ControllerStimRecord) {
        switch (commandType) {
        case RHXCommandRegWrite:
            if (arg1 > 255) {
                std::cerr << "Error in RHXRegisters::createRHXCommand: " <<
                    "Register address out of range.\n";
                return 0xffffffff;
            }
            if (arg2 > 65535) {
                std::cerr << "Error in RHXRegisters::createRHXCommand: " <<
                    "Register data out of range.\n";
                return 0xffffffff;
            }
            return 0x80000000 + (uFlag << 29) + (mFlag << 28) + (arg1 << 16) + arg2; // 10um0000 rrrrrrrr dddddddd dddddddd; if the command is 'Register Write',
                                                     // arg1 is the register address and arg2 is the data.
            break;
        default:
            std::cerr << "Error in RHXRegisters::createRHXCommand: " <<
                "Only 'Register Write' commands take four arguments.\n";
            return 0xffffffff;
        }
    } else {
        std::cerr << "Error in RHXRegisters::createRHXCommand: " <<
            "Only RHS2000 'Register Write' commands take four arguments.\n";
        return 0xffffffff;
    }
}

// Set the stimulation current DAC step size.
void RHXRegisters::setStimStepSize(StimStepSize step)
{
    switch (step) {
    case StimStepSizeMin:
        stimStepSel1 = 127;
        stimStepSel2 = 63;
        stimStepSel3 = 3;
        stimPBias = 6;
        stimNBias = 6;
        break;
    case StimStepSize10nA:
        stimStepSel1 = 64;
        stimStepSel2 = 19;
        stimStepSel3 = 3;
        stimPBias = 6;
        stimNBias = 6;
        break;
    case StimStepSize20nA:
        stimStepSel1 = 40;
        stimStepSel2 = 40;
        stimStepSel3 = 1;
        stimPBias = 7;
        stimNBias = 7;
        break;
    case StimStepSize50nA:
        stimStepSel1 = 64;
        stimStepSel2 = 40;
        stimStepSel3 = 0;
        stimPBias = 7;
        stimNBias = 7;
        break;
    case StimStepSize100nA:
        stimStepSel1 = 30;
        stimStepSel2 = 20;
        stimStepSel3 = 0;
        stimPBias = 7;
        stimNBias = 7;
        break;
    case StimStepSize200nA:
        stimStepSel1 = 25;
        stimStepSel2 = 10;
        stimStepSel3 = 0;
        stimPBias = 8;
        stimNBias = 8;
        break;
    case StimStepSize500nA:
        stimStepSel1 = 101;
        stimStepSel2 = 3;
        stimStepSel3 = 0;
        stimPBias = 9;
        stimNBias = 9;
        break;
    case StimStepSize1uA:
        stimStepSel1 = 98;
        stimStepSel2 = 1;
        stimStepSel3 = 0;
        stimPBias = 10;
        stimNBias = 10;
        break;
    case StimStepSize2uA:
        stimStepSel1 = 94;
        stimStepSel2 = 0;
        stimStepSel3 = 0;
        stimPBias = 11;
        stimNBias = 11;
        break;
    case StimStepSize5uA:
        stimStepSel1 = 38;
        stimStepSel2 = 0;
        stimStepSel3 = 0;
        stimPBias = 14;
        stimNBias = 14;
        break;
    case StimStepSize10uA:
        stimStepSel1 = 15;
        stimStepSel2 = 0;
        stimStepSel3 = 0;
        stimPBias = 15;
        stimNBias = 15;
        break;
    case StimStepSizeMax:
    default:
        stimStepSel1 = 0;
        stimStepSel2 = 0;
        stimStepSel3 = 0;
        stimPBias = 15;
        stimNBias = 15;
        break;
    }
}

// Convert stimulation step size enum to floating point current value (in amps).
double RHXRegisters::stimStepSizeToDouble(StimStepSize step)
{
    switch (step) {
    case StimStepSize10nA:
        return 10.0e-9;
    case StimStepSize20nA:
        return 20.0e-9;
    case StimStepSize50nA:
        return 50.0e-9;
    case StimStepSize100nA:
        return 100.0e-9;
    case StimStepSize200nA:
        return 200.0e-9;
    case StimStepSize500nA:
        return 500.0e-9;
    case StimStepSize1uA:
        return 1.0e-6;
    case StimStepSize2uA:
        return 2.0e-6;
    case StimStepSize5uA:
        return 5.0e-6;
    case StimStepSize10uA:
        return 10.0e-6;
    case StimStepSizeMin:
    case StimStepSizeMax:
    default:
        return std::numeric_limits<double>::quiet_NaN();
    }
}

// Set positive stimulation magnitude (0 to 255, in DAC steps) and trim (-128 to +127) for a channel (0-15).
int RHXRegisters::setPosStimMagnitude(int channel, int magnitude, int trim)
{
    if ((channel < 0) || (channel > 15)) {
        std::cerr << "Error in Rhs2000Registers::setPosStimMagnitude: channel out of range.\n";
        return -1;
    }
    if ((magnitude < 0) || (magnitude > 255)) {
        std::cerr << "Error in Rhs2000Registers::setPosStimMagnitude: magnitude out of range.\n";
        return -1;
    }
    if ((trim < -128) || (trim > 127)) {
        std::cerr << "Error in Rhs2000Registers::setPosStimMagnitude: trim out of range.\n";
        return -1;
    }
    posCurrentMag[channel] = magnitude;
    posCurrentTrim[channel] = trim + 128;
    return 0;
}

// Set negative stimulation magnitude (0 to 255, in DAC steps) and trim (-128 to +127) for a channel (0-15).
int RHXRegisters::setNegStimMagnitude(int channel, int magnitude, int trim)
{
    if ((channel < 0) || (channel > 15)) {
        std::cerr << "Error in Rhs2000Registers::setNegStimMagnitude: channel out of range.\n";
        return -1;
    }
    if ((magnitude < 0) || (magnitude > 255)) {
        std::cerr << "Error in Rhs2000Registers::setNegStimMagnitude: magnitude out of range.\n";
        return -1;
    }
    if ((trim < -128) || (trim > 127)) {
        std::cerr << "Error in Rhs2000Registers::setNegStimMagnitude: trim out of range.\n";
        return -1;
    }
    negCurrentMag[channel] = magnitude;
    negCurrentTrim[channel] = trim + 128;
    return 0;
}

// Set charge recovery current limit.
void RHXRegisters::setChargeRecoveryCurrentLimit(ChargeRecoveryCurrentLimit limit)
{
    switch (limit) {
    case CurrentLimitMin:
        chargeRecoveryCurrentLimitSel1 = 127;
        chargeRecoveryCurrentLimitSel2 = 63;
        chargeRecoveryCurrentLimitSel3 = 3;
        break;
    case CurrentLimit1nA:
        chargeRecoveryCurrentLimitSel1 = 0;
        chargeRecoveryCurrentLimitSel2 = 30;
        chargeRecoveryCurrentLimitSel3 = 2;
        break;
    case CurrentLimit2nA:
        chargeRecoveryCurrentLimitSel1 = 0;
        chargeRecoveryCurrentLimitSel2 = 15;
        chargeRecoveryCurrentLimitSel3 = 1;
        break;
    case CurrentLimit5nA:
        chargeRecoveryCurrentLimitSel1 = 0;
        chargeRecoveryCurrentLimitSel2 = 31;
        chargeRecoveryCurrentLimitSel3 = 0;
        break;
    case CurrentLimit10nA:
        chargeRecoveryCurrentLimitSel1 = 50;
        chargeRecoveryCurrentLimitSel2 = 15;
        chargeRecoveryCurrentLimitSel3 = 0;
        break;
    case CurrentLimit20nA:
        chargeRecoveryCurrentLimitSel1 = 78;
        chargeRecoveryCurrentLimitSel2 = 7;
        chargeRecoveryCurrentLimitSel3 = 0;
        break;
    case CurrentLimit50nA:
        chargeRecoveryCurrentLimitSel1 = 22;
        chargeRecoveryCurrentLimitSel2 = 3;
        chargeRecoveryCurrentLimitSel3 = 0;
        break;
    case CurrentLimit100nA:
        chargeRecoveryCurrentLimitSel1 = 56;
        chargeRecoveryCurrentLimitSel2 = 1;
        chargeRecoveryCurrentLimitSel3 = 0;
        break;
    case CurrentLimit200nA:
        chargeRecoveryCurrentLimitSel1 = 71;
        chargeRecoveryCurrentLimitSel2 = 0;
        chargeRecoveryCurrentLimitSel3 = 0;
        break;
    case CurrentLimit500nA:
        chargeRecoveryCurrentLimitSel1 = 26;
        chargeRecoveryCurrentLimitSel2 = 0;
        chargeRecoveryCurrentLimitSel3 = 0;
        break;
    case CurrentLimit1uA:
        chargeRecoveryCurrentLimitSel1 = 9;
        chargeRecoveryCurrentLimitSel2 = 0;
        chargeRecoveryCurrentLimitSel3 = 0;
        break;
    case CurrentLimitMax:
        chargeRecoveryCurrentLimitSel1 = 0;
        chargeRecoveryCurrentLimitSel2 = 0;
        chargeRecoveryCurrentLimitSel3 = 0;
        break;
    }
}

// Convert current limit enum to floating point current value (in amps).
double RHXRegisters::chargeRecoveryCurrentLimitToDouble(ChargeRecoveryCurrentLimit limit)
{
    switch (limit) {
    case CurrentLimit1nA:
        return 1.0e-9;
    case CurrentLimit2nA:
        return 2.0e-9;
    case CurrentLimit5nA:
        return 5.0e-9;
    case CurrentLimit10nA:
        return 10.0e-9;
    case CurrentLimit20nA:
        return 20.0e-9;
    case CurrentLimit50nA:
        return 50.0e-9;
    case CurrentLimit100nA:
        return 100.0e-9;
    case CurrentLimit200nA:
        return 200.0e-9;
    case CurrentLimit500nA:
        return 500.0e-9;
    case CurrentLimit1uA:
        return 1.0e-6;
    case CurrentLimitMin:
    case CurrentLimitMax:
    default:
        return std::numeric_limits<double>::quiet_NaN();
    }
}

// Set the target voltage for current-limited charge recovery.  The parameter
// vTarget should specify a voltage in the range of -1.225 to +1.215 (units = volts).
// Return actual value of target voltage.
double RHXRegisters::setChargeRecoveryTargetVoltage(double vTarget)
{
    double dacStep = 1.225 / 128.0;
    int value;

    value = ((int)floor(vTarget / dacStep + 0.5)) + 128;
    if (value < 0) value = 0;
    if (value > 255) value = 255;

    chargeRecoveryDac = value;
    return dacStep * (value - 128);
}

// Create a list of numCommands commands to program most RAM registers on a RHD2000 chip, read those values
// back to confirm programming, read ROM registers, and (if calibrate == true) run ADC calibration.
// Returns the length of the command list.  numCommands must be 60 or greater.
int RHXRegisters::createCommandListRHDRegisterConfig(std::vector<unsigned int> &commandList, bool calibrate,
                                                     int numCommands)
{
    if (type != ControllerRecordUSB2 && type != ControllerRecordUSB3) return -1;
    if (numCommands < 60) return -1;

    commandList.clear();    // If command list already exists, erase it and start a new one

    // Start with a few dummy commands in case chip is still powering up.
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 63));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 63));

    // Program RAM registers.
    commandList.push_back(createRHXCommand(RHXCommandRegWrite,  0, getRegisterValue( 0)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite,  1, getRegisterValue( 1)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite,  2, getRegisterValue( 2)));
    // Don't program Register 3 (MUX Load, Temperature Sensor, and Auxiliary Digital Output);
    // Control temperature sensor in another command stream.
    commandList.push_back(createRHXCommand(RHXCommandRegWrite,  4, getRegisterValue( 4)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite,  5, getRegisterValue( 5)));
    // Don't program Register 6 (Impedance Check DAC) here; create DAC waveform in another command stream.
    commandList.push_back(createRHXCommand(RHXCommandRegWrite,  7, getRegisterValue( 7)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite,  8, getRegisterValue( 8)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite,  9, getRegisterValue( 9)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 10, getRegisterValue(10)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 11, getRegisterValue(11)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 12, getRegisterValue(12)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 13, getRegisterValue(13)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 14, getRegisterValue(14)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 15, getRegisterValue(15)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 16, getRegisterValue(16)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 17, getRegisterValue(17)));

    // Read ROM registers.
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 63));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 62));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 61));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 60));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 59));

    // Read chip name from ROM.
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 48));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 49));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 50));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 51));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 52));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 53));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 54));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 55));

    // Read Intan name from ROM.
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 40));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 41));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 42));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 43));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 44));

    // Read back RAM registers to confirm programming.
    for (int reg = 0; reg < 18; ++reg) {
        commandList.push_back(createRHXCommand(RHXCommandRegRead,  reg));
    }

    // Optionally, run ADC calibration (should only be run once after board is plugged in).
    if (calibrate) {
        commandList.push_back(createRHXCommand(RHXCommandCalibrate));
    } else {
        commandList.push_back(createRHXCommand(RHXCommandRegRead, 63));
    }

    // Program amplifier 31-63 power up/down registers in case a RHD2164 is connected.
    // Note: We don't read these registers back, since they are only 'visible' on MISO B.
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 18, getRegisterValue(18)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 19, getRegisterValue(19)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 20, getRegisterValue(20)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 21, getRegisterValue(21)));

    // End with a dummy command.
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 63));

    for (int i = 0; i < (numCommands - 60); ++i) {
        commandList.push_back(createRHXCommand(RHXCommandRegRead, 63));
    }

    return (int)commandList.size();
}

// Create a list of 128 commands to program most RAM registers on an RHS2116 chip, read those values
// back to confirm programming, and read ROM registers.
// If updateStimParams == true, update stimulation amplitudes and other charge-recovery parameters.
// Return the length of the command list.
int RHXRegisters::createCommandListRHSRegisterConfig(std::vector<unsigned int> &commandList, bool updateStimParams)
{
    if (type != ControllerStimRecord) return -1;

    commandList.clear();    // If command list already exists, erase it and start a new one.

                            // Start with a few dummy commands in case chip is still powering up.
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));

    // Program 53 RAM registers.
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 0, getRegisterValue(0)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 1, getRegisterValue(1)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 2, getRegisterValue(2)));
    // Don't program Register 3 (Impedance Check DAC) here; create DAC waveform in another command stream.
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 4, getRegisterValue(4)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 5, getRegisterValue(5)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 6, getRegisterValue(6)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 7, getRegisterValue(7)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 8, getRegisterValue(8)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 10, getRegisterValue(10)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 12, getRegisterValue(12)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 32, getRegisterValue(32)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 33, getRegisterValue(33)));
    if (updateStimParams) {
        commandList.push_back(createRHXCommand(RHXCommandRegWrite, 34, getRegisterValue(34)));
        commandList.push_back(createRHXCommand(RHXCommandRegWrite, 35, getRegisterValue(35)));
        commandList.push_back(createRHXCommand(RHXCommandRegWrite, 36, getRegisterValue(36)));
        commandList.push_back(createRHXCommand(RHXCommandRegWrite, 37, getRegisterValue(37)));
    } else {
        commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));
        commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));
        commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));
        commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));
    }
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 38, getRegisterValue(38)));
    // Register 40 (Compliance Monitor) is read only; clear it here:
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 40, 0, 0, 1));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 42, getRegisterValue(42)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 44, getRegisterValue(44)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 46, getRegisterValue(46)));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 48, getRegisterValue(48)));
    // Register 50 (Fault Current Detect) is read only.

    if (updateStimParams) {
        for (int reg = 64; reg < 80; ++reg) {
            commandList.push_back(createRHXCommand(RHXCommandRegWrite, reg, getRegisterValue(reg)));
        }
        for (int reg = 96; reg < 111; ++reg) {
            commandList.push_back(createRHXCommand(RHXCommandRegWrite, reg, getRegisterValue(reg)));
        }
        // Update all triggered registers with last WRITE command.
        commandList.push_back(createRHXCommand(RHXCommandRegWrite, 111, getRegisterValue(111), 1, 0));
    } else {
        for (int i = 0; i < 32; ++i) {
            commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));
        }
    }

    // Read 5 ROM registers.
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 254));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 253));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 252));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 251));

    // Read back 56 RAM registers to confirm programming.
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 0));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 1));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 2));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 3));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 4));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 5));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 6));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 7));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 8));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 10));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 12));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 32));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 33));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 34));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 35));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 36));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 37));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 38));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 40));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 42));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 44));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 46));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 48));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 50));
    for (int reg = 64; reg < 80; ++reg) {
        commandList.push_back(createRHXCommand(RHXCommandRegRead, reg));
    }
    for (int reg = 96; reg < 112; ++reg) {
        commandList.push_back(createRHXCommand(RHXCommandRegRead, reg));
    }

    // ADC calibration should not be performed on RHS2116; rather, CLEAR command should be sent.
    commandList.push_back(createRHXCommand(RHXCommandCalClear));

    // End with dummy commands.
    for (int i = 0; i < 10; ++i) {
        commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));
    }

    return (int)commandList.size();
}

// Read all registers from chip without changing any values.
int RHXRegisters::createCommandListRHSRegisterRead(std::vector<unsigned int> &commandList)
{
    if (type != ControllerStimRecord) return -1;

    commandList.clear();    // If command list already exists, erase it and start a new one.

                            // Start with a few dummy commands in case chip is still powering up.
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));

    for (int i = 0; i < 54; i++) {
        commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));
    }

    // Read 5 ROM registers.
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 254));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 253));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 252));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 251));

    // Read back 56 RAM registers to confirm programming.
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 0));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 1));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 2));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 3));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 4));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 5));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 6));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 7));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 8));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 10));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 12));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 32));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 33));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 34));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 35));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 36));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 37));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 38));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 40));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 42));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 44));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 46));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 48));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 50));
    for (int reg = 64; reg < 80; ++reg) {
        commandList.push_back(createRHXCommand(RHXCommandRegRead, reg));
    }
    for (int reg = 96; reg < 112; ++reg) {
        commandList.push_back(createRHXCommand(RHXCommandRegRead, reg));
    }
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));

    // End with dummy commands.
    for (int i = 0; i < 10; ++i) {
        commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));
    }

    return (int)commandList.size();
}

// Create a list of dummy commands with a specific command. Return the length of the command list (which should be n).
int RHXRegisters::createCommandListDummy(std::vector <unsigned int> &commandList, int n, unsigned int cmd)
{
    commandList.clear();

    for (int i = 0; i < n; i++) {
        commandList.push_back(cmd);
    }

    return (int)commandList.size();
}

// Set positive and negative stimulation magnitude and trim parameters for a single channel.
// Return the length of the command list (which should be 128).
int RHXRegisters::createCommandListSetStimMagnitudes(std::vector<unsigned int> &commandList, int channel,
                                                         int posMag, int posTrim, int negMag, int negTrim)
{
    if (type != ControllerStimRecord) return -1;

    commandList.clear();    // If command list already exists, erase it and start a new one.

                            // Start with two dummy commands.
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));

    setPosStimMagnitude(channel, posMag, posTrim);
    setNegStimMagnitude(channel, negMag, negTrim);

    // Now, configure RAM registers.
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 96 + channel, getRegisterValue(96 + channel), 1, 0)); // positive register; update
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 64 + channel, getRegisterValue(64 + channel), 1, 0)); // negative register; update

    // More dummy commands to make 128 total commands
    while (commandList.size() < 128) {
        commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));
    }

    return (int)commandList.size();
}

int RHXRegisters::createCommandListSetStimMagnitudesAllChannels(std::vector<unsigned int> &commandList, int posMag, int posTrim, int negMag, int negTrim)
{
    if (type != ControllerStimRecord) return -1;

    commandList.clear(); // If command list already exists, erase it and start a new one.

    // Start with two dummy commands.
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));

    for (int channel = 0; channel < 16; channel++) {
        setPosStimMagnitude(channel, posMag, posTrim);
        setNegStimMagnitude(channel, negMag, negTrim);
        commandList.push_back(createRHXCommand(RHXCommandRegWrite, 96 + channel, getRegisterValue(96 + channel), 1, 0)); // positive register; update
        commandList.push_back(createRHXCommand(RHXCommandRegWrite, 64 + channel, getRegisterValue(64 + channel), 1, 0)); // negative register; update
    }

    // More dummy commands to make 128 total commands
    while (commandList.size() < 128) {
        commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));
    }

    return (int)commandList.size();
}

// Set charge recovery current limit and target voltage (Registers 36 and 37).
// Return the length of the command list (which should be 128).
int RHXRegisters::createCommandListConfigChargeRecovery(std::vector<unsigned int> &commandList,
                                                            ChargeRecoveryCurrentLimit currentLimit, double targetVoltage)
{
    if (type != ControllerStimRecord) return -1;

    commandList.clear();    // If command list already exists, erase it and start a new one.

                            // Start with two dummy commands.
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));

    setChargeRecoveryCurrentLimit(currentLimit);		// Set charge recovery current limit.
    setChargeRecoveryTargetVoltage(targetVoltage);      // Set charge recovery target voltage.

    // Now, configure RAM registers.
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 36, getRegisterValue(36), 0, 0));
    commandList.push_back(createRHXCommand(RHXCommandRegWrite, 37, getRegisterValue(37), 0, 0));

    // More dummy commands to make 128 total commands.
    while (commandList.size() < 128) {
        commandList.push_back(createRHXCommand(RHXCommandRegRead, 255));
    }

    return (int)commandList.size();
}

// Create a list of up to 8192 commands to generate a sine wave of particular frequency (in Hz) and
// amplitude (in DAC steps, 0-128) using the on-chip impedance testing voltage DAC.  If frequency is set to zero,
// a DC baseline waveform is created.
// Return the length of the command list.
int RHXRegisters::createCommandListZcheckDac(std::vector<unsigned int> &commandList, double frequency, double amplitude)
{
    commandList.clear();    // if command list already exists, erase it and start a new one

    if ((amplitude < 0.0) || (amplitude > 128.0)) {
        std::cerr << "Error in RHXRegisters::createCommandListZcheckDac: Amplitude out of range.\n";
        return -1;
    }
    if (frequency < 0.0) {
        std::cerr << "Error in RHXRegisters::createCommandListZcheckDac: Negative frequency not allowed.\n";
        return -1;
    } else if (frequency > sampleRate / 4.0) {
        std::cerr << "Error in RHXRegisters::createCommandListZcheckDac: " <<
            "Frequency too high relative to sampling rate.\n";
        return -1;
    }

    unsigned int dacRegister = (type == ControllerStimRecord) ? 3 : 6;
    if (frequency == 0.0) {
        for (int i = 0; i < maxCommandLength(); ++i) {
            commandList.push_back(createRHXCommand(RHXCommandRegWrite, dacRegister, 128));
        }
    } else {
        int period = (int)floor(sampleRate / frequency + 0.5);
        if (period > maxCommandLength()) {
            std::cerr << "Error in RHXRegisters::createCommandListZcheckDac: Frequency too low.\n";
            return -1;
        } else {
            double t = 0.0;
            for (int i = 0; i < period; ++i) {
                int value = (int)floor(amplitude * sin(TwoPi * frequency * t) + 128.0 + 0.5);
                if (value < 0) {
                    value = 0;
                } else if (value > 255) {
                    value = 255;
                }
                commandList.push_back(createRHXCommand(RHXCommandRegWrite, dacRegister, value));
                t += 1.0 / sampleRate;
            }
        }
    }

    return (int)commandList.size();
}

// Create a list of RHD commands to sample auxiliary ADC inputs 1-3 at 1/4 the amplifier sampling
// rate.  The reading of a ROM register is interleaved to allow for data frame alignment.
// Return the length of the command list.  numCommands should be evenly divisible by four.
int RHXRegisters::createCommandListRHDSampleAuxIns(std::vector<unsigned int> &commandList, int numCommands)
{
    if (type != ControllerRecordUSB2 && type != ControllerRecordUSB3) return -1;
    if (numCommands < 4) return -1;

    commandList.clear();    // If command list already exists, erase it and start a new one.

    for (int i = 0; i < (numCommands/4) - 2; ++i) {
        commandList.push_back(createRHXCommand(RHXCommandConvert, 32)); // sample AuxIn1
        commandList.push_back(createRHXCommand(RHXCommandConvert, 33)); // sample AuxIn2
        commandList.push_back(createRHXCommand(RHXCommandConvert, 34)); // sample AuxIn3
        commandList.push_back(createRHXCommand(RHXCommandRegRead, 40)); // Read ROM register; should return 0x0049.
    }

    // Last two times:
    commandList.push_back(createRHXCommand(RHXCommandConvert, 32)); // sample AuxIn1
    commandList.push_back(createRHXCommand(RHXCommandConvert, 33)); // sample AuxIn2
    commandList.push_back(createRHXCommand(RHXCommandConvert, 34)); // sample AuxIn3
    commandList.push_back(createRHXCommand(RHXCommandConvert, 48)); // Read Supply Voltage Sensor

    commandList.push_back(createRHXCommand(RHXCommandConvert, 32)); // sample AuxIn1
    commandList.push_back(createRHXCommand(RHXCommandConvert, 33)); // sample AuxIn2
    commandList.push_back(createRHXCommand(RHXCommandConvert, 34)); // sample AuxIn3
    commandList.push_back(createRHXCommand(RHXCommandRegRead, 40)); // Read ROM register; should return 0x0049.

    return (int)commandList.size();
}

// Create a list of commands to update RHD Register 3 (controlling the auxiliary digital ouput
// pin) every sampling period.
// Return the length of the command list.
int RHXRegisters::createCommandListRHDUpdateDigOut(std::vector<unsigned int> &commandList, int numCommands)
{
    if (type != ControllerRecordUSB2 && type != ControllerRecordUSB3) return -1;
    if (numCommands < 1) return -1;

    commandList.clear();    // If command list already exists, erase it and start a new one.

    for (int i = 0; i < numCommands; ++i) {
        commandList.push_back(createRHXCommand(RHXCommandRegWrite, 3, getRegisterValue(3)));
    }

    return (int)commandList.size();
}

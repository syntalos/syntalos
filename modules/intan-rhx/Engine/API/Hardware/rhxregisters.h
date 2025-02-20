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

#ifndef RHXREGISTERS_H
#define RHXREGISTERS_H

#include "rhxglobals.h"
#include "rhxdatablock.h"
#include <vector>

class RHXRegisters
{
public:
    RHXRegisters(ControllerType type_, double sampleRate_, StimStepSize stimStep_ = StimStepSize500nA);

    void setFastSettle(bool enabled);

    enum DigOut {
        DigOut1,
        DigOut2,
        DigOutOD
    };

    void setDigOutLow(DigOut pin = DigOut1);
    void setDigOutHigh(DigOut pin = DigOut1);
    void setDigOutHiZ(DigOut pin = DigOut1);

    void enableAux1(bool enabled);
    void enableAux2(bool enabled);
    void enableAux3(bool enabled);

    void enableDsp(bool enabled);
    static std::vector<double> getDspFreqTable(double sampleRate_);
    std::vector<double> getDspFreqTable() const { return getDspFreqTable(sampleRate); }
    double setDspCutoffFreq(double newDspCutoffFreq);
    double getDspCutoffFreq() const;

    void enableZcheck(bool enabled);
    void setZcheckDacPower(bool enabled);

    enum ZcheckCs {
        ZcheckCs100fF,
        ZcheckCs1pF,
        ZcheckCs10pF
    };

    enum ZcheckPolarity {
        ZcheckPositiveInput,
        ZcheckNegativeInput
    };

    void setZcheckScale(ZcheckCs scale);
    void setZcheckPolarity(ZcheckPolarity polarity);
    int setZcheckChannel(int channel);

    void setAmpPowered(int channel, bool powered);
    void powerUpAllAmps();
    void powerDownAllAmps();
    void setDCAmpPowered(int channel, bool powered);    // ControllerStimRecord only
    void powerUpAllDCAmps();                            // ControllerStimRecord only
    void powerDownAllDCAmps();                          // ControllerStimRecord only

    void setStimEnable(bool enable);                    // ControllerStimRecord only

    void setStimStepSize(StimStepSize step);                        // ControllerStimRecord only
    static double stimStepSizeToDouble(StimStepSize step);          // ControllerStimRecord only
    int setPosStimMagnitude(int channel, int magnitude, int trim);  // ControllerStimRecord only
    int setNegStimMagnitude(int channel, int magnitude, int trim);  // ControllerStimRecord only

    enum ChargeRecoveryCurrentLimit {
        CurrentLimitMin = 0,
        CurrentLimit1nA,
        CurrentLimit2nA,
        CurrentLimit5nA,
        CurrentLimit10nA,
        CurrentLimit20nA,
        CurrentLimit50nA,
        CurrentLimit100nA,
        CurrentLimit200nA,
        CurrentLimit500nA,
        CurrentLimit1uA,
        CurrentLimitMax
    };

    void setChargeRecoveryCurrentLimit(ChargeRecoveryCurrentLimit limit);               // ControllerStimRecord only
    static double chargeRecoveryCurrentLimitToDouble(ChargeRecoveryCurrentLimit limit); // ControllerStimRecord only
    double setChargeRecoveryTargetVoltage(double vTarget);                              // ControllerStimRecord only

    int getRegisterValue(int reg) const;

    double setUpperBandwidth(double upperBandwidth);
    double setLowerBandwidth(double lowerBandwidth, int select = 0);

    int createCommandListRHDRegisterConfig(std::vector<unsigned int> &commandList, bool calibrate, int numCommands);
    int createCommandListRHDSampleAuxIns(std::vector<unsigned int> &commandList, int numCommands);
    int createCommandListRHDUpdateDigOut(std::vector<unsigned int> &commandList, int numCommands);

    int createCommandListRHSRegisterConfig(std::vector<unsigned int> &commandList, bool updateStimParams);
    int createCommandListRHSRegisterRead(std::vector<unsigned int> &commandList);
    int createCommandListSetStimMagnitudes(std::vector<unsigned int> &commandList, int channel,
                                           int posMag, int posTrim, int negMag, int negTrim);
    int createCommandListSetStimMagnitudesAllChannels(std::vector<unsigned int> &commandList, int posMag, int posTrim, int negMag, int negTrim);
    int createCommandListConfigChargeRecovery(std::vector<unsigned int> &commandList, ChargeRecoveryCurrentLimit currentLimit,
                                              double targetVoltage);

    int createCommandListZcheckDac(std::vector<unsigned int> &commandList, double frequency, double amplitude);
    int createCommandListDummy(std::vector <unsigned int> &commandList, int n, unsigned int cmd);

    enum RHXCommandType {
        RHXCommandConvert,
        RHXCommandCalibrate,
        RHXCommandCalClear,
        RHXCommandRegWrite,
        RHXCommandRegRead,
        RHXCommandComplianceReset  // ControllerStimRecord only
    };

    unsigned int createRHXCommand(RHXCommandType commandType);
    unsigned int createRHXCommand(RHXCommandType commandType, unsigned int arg1);
    unsigned int createRHXCommand(RHXCommandType commandType, unsigned int arg1, unsigned int arg2);
    unsigned int createRHXCommand(RHXCommandType commandType, unsigned int arg1, unsigned int arg2,
                                  unsigned int uFlag, unsigned int mFlag);

    int maxCommandLength() const;
    static int maxCommandLength(ControllerType type);
    int maxNumChannelsPerChip() const;
    static int maxNumChannelsPerChip(ControllerType type);

private:
    ControllerType type;
    double sampleRate;
    StimStepSize stimStep;

    // Register variables
    int adcReferenceBw;
    int ampVrefEnable;
    int adcComparatorBias;
    int adcComparatorSelect;
    int vddSenseEnable;
    int adcBufferBias;
    int muxBias;
    int muxLoad;
    int tempS1;
    int tempS2;
    int tempEn;
    int digOutOD;
    int digOut1;
    int digOut1HiZ;
    int digOut2;
    int digOut2HiZ;
    int weakMiso;
    int twosComp;
    int absMode;
    int dspEn;
    int dspCutoffFreq;
    int zcheckDacPower;
    int zcheckLoad;
    int zcheckScale;
    int zcheckConnAll;
    int zcheckSelPol;
    int zcheckEn;
    int zcheckSelect;
    int offChipRH1;
    int offChipRH2;
    int offChipRL;
    int adcAux1En;
    int adcAux2En;
    int adcAux3En;
    int rH1Dac1;
    int rH1Dac2;
    int rH2Dac1;
    int rH2Dac2;
    int rLDac1A;
    int rLDac2A;
    int rLDac3A;
    int rLDac1B;
    int rLDac2B;
    int rLDac3B;
    int stimEnableA;
    int stimEnableB;
    int stimStepSel1;
    int stimStepSel2;
    int stimStepSel3;
    int stimNBias;
    int stimPBias;
    int chargeRecoveryDac;
    int chargeRecoveryCurrentLimitSel1;
    int chargeRecoveryCurrentLimitSel2;
    int chargeRecoveryCurrentLimitSel3;
    std::vector<int> ampPwr;
    std::vector<int> ampFastSettle;
    std::vector<int> ampFLSelect;
    std::vector<int> dcAmpPwr;
    std::vector<int> complianceMonitor;
    std::vector<int> stimOn;
    std::vector<int> stimPol;
    std::vector<int> chargeRecoverySwitch;
    std::vector<int> cLChargeRecoveryEn;
    std::vector<int> negCurrentMag;
    std::vector<int> negCurrentTrim;
    std::vector<int> posCurrentMag;
    std::vector<int> posCurrentTrim;

    double rH1FromUpperBandwidth(double upperBandwidth) const;
    double rH2FromUpperBandwidth(double upperBandwidth) const;
    double rLFromLowerBandwidth(double lowerBandwidth) const;
    double upperBandwidthFromRH1(double rH1) const;
    double upperBandwidthFromRH2(double rH2) const;
    double lowerBandwidthFromRL(double rL) const;

    void setSamplingParameters();
    void setDefaultRHDSettings();
    void setDefaultRHSSettings();
    int getRHDRegisterValue(int reg) const;
    int getRHSRegisterValue(int reg) const;
    static int vectorToWord(const std::vector<int> &v);
};

#endif // RHXREGISTERS_H

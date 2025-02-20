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

#ifndef SYNTHDATABLOCKGENERATOR_H
#define SYNTHDATABLOCKGENERATOR_H

#include "abstractrhxcontroller.h"
#include "randomnumber.h"
#include "rhxdatablock.h"
#include <QElapsedTimer>
#include <cstdint>
#include <vector>

class AbstractSynthSource
{
public:
    AbstractSynthSource(RandomNumber* randomGenerator_, double sampleRate);
    virtual ~AbstractSynthSource();
    virtual void reset() = 0;
    virtual uint16_t nextSample() = 0;

protected:
    double tStepMsec;
    double tMsec;
    RandomNumber* randomGenerator;

    uint16_t convertToAmpADCValue(double electrodeMicrovolts) const;
};

class NeuralSynthSource : public AbstractSynthSource
{
public:
    NeuralSynthSource(RandomNumber* randomGenerator_, double sampleRate, int nUnits_);
    void reset() override;
    uint16_t nextSample() override;

private:
    int nUnits;
    std::vector<double> spikeAmplitude;
    std::vector<double> spikeDurationMsec;
    std::vector<double> spikeRateHz;
    std::vector<bool> firing;
    std::vector<double> spikeTimeMsec;

    const double NoiseRMSLevelMicroVolts = 5.0;  // 5 uV rms typical cortical background noise
    const double SpikeRefractoryPeriodMsec = 5.0;
    const double LFPFrequencyHz = 2.3;
    const double LFPModulationHz = 0.5;

    double LFPVoltage() const;
    double nextSpikeVoltage(int unit);
};

class ECGSynthSource : public AbstractSynthSource
{
public:
    ECGSynthSource(RandomNumber* randomGenerator_, double sampleRate);
    void reset() override;
    uint16_t nextSample() override;

private:
    double ecgAmplitude;
};

class ADCSynthSource : public AbstractSynthSource
{
public:
    ADCSynthSource(RandomNumber* randomGenerator_, double sampleRate, double freqHz_, double amplitude_ = 1.0);
    void reset() override;
    uint16_t nextSample() override;

private:
    double freqHz;
    double amplitude;
};

class DigitalSynthSource : public AbstractSynthSource
{
public:
    DigitalSynthSource(RandomNumber* randomGenerator_, double sampleRate, double freqHz);
    void reset() override;
    uint16_t nextSample() override;

private:
    double periodMsec;
};

class SynthDataBlockGenerator
{
public:
    SynthDataBlockGenerator(ControllerType type_, double sampleRate_);
    ~SynthDataBlockGenerator();

    long readSynthDataBlocksRaw(int numBlocks, uint8_t* buffer, int numDataStreams);
    void reset();

private:
    ControllerType type;
    double sampleRate;
    RandomNumber* randomGenerator;
    uint16_t* usbWords;
    uint32_t tIndex;
    QElapsedTimer timer;
    double dataBlockPeriodInNsec;
    double timeDeficitInNsec;
    std::vector<std::vector<AbstractSynthSource*> > synthSources;
    std::vector<ADCSynthSource*> adcSynthSources;
    std::vector<DigitalSynthSource*> digitalSynthSources;

    uint16_t auxInSample;
    uint16_t vddSample;
    uint16_t dcAmpSample;

    void createSynthDataBlock(int numBlocks, int numDataStreams);
};

#endif // SYNTHDATABLOCKGENERATOR_H

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
#include "rhxglobals.h"
#include "abstractrhxcontroller.h"
#include "synthdatablockgenerator.h"

AbstractSynthSource::AbstractSynthSource(RandomNumber* randomGenerator_, double sampleRate) :
    tStepMsec(1.0e3 / sampleRate),
    tMsec(0.0),
    randomGenerator(randomGenerator_)
{
    randomGenerator->setGaussianAccuracy(6);
}

AbstractSynthSource::~AbstractSynthSource()
{

}

uint16_t AbstractSynthSource::convertToAmpADCValue(double electrodeMicrovolts) const
{
    int result = round(electrodeMicrovolts / 0.195) + 32768;
    if (result < 0) result = 0;
    else if (result > 65535) result = 65535;
    return (uint16_t) result;
}


NeuralSynthSource::NeuralSynthSource(RandomNumber* randomGenerator_, double sampleRate, int nUnits_) :
    AbstractSynthSource(randomGenerator_, sampleRate),
    nUnits(nUnits_)
{
    spikeAmplitude.resize(nUnits);
    spikeDurationMsec.resize(nUnits);
    spikeRateHz.resize(nUnits);
    firing.resize(nUnits);
    spikeTimeMsec.resize(nUnits);
    for (int i = 0; i < nUnits; ++i) {
        spikeAmplitude[i] = randomGenerator->randomUniform(-500.0, -200.0);
        spikeDurationMsec[i] = randomGenerator->randomUniform(0.3, 1.7);
        spikeRateHz[i] = randomGenerator->randomLogUniform(0.1, 50.0);
    }

    // Mimic virtual method reset() here in the constructor
    tMsec = 0.0;
    for (int i = 0; i < nUnits; ++i) {
        firing[i] = false;
        spikeTimeMsec[i] = 0.0;
    }
}

void NeuralSynthSource::reset()
{
    tMsec = 0.0;
    for (int i = 0; i < nUnits; ++i) {
        firing[i] = false;
        spikeTimeMsec[i] = 0.0;
    }
}

double NeuralSynthSource::nextSpikeVoltage(int unit)
{
    double result = 0.0;
    if (spikeTimeMsec[unit] < spikeDurationMsec[unit]) {
        result = spikeAmplitude[unit] * exp(-2.0 * (spikeTimeMsec[unit])) *
                sin(TwoPi * spikeTimeMsec[unit] / spikeDurationMsec[unit]);
        spikeTimeMsec[unit] += tStepMsec;
    } else if (spikeTimeMsec[unit] < spikeDurationMsec[unit] + SpikeRefractoryPeriodMsec) {
        spikeTimeMsec[unit] += tStepMsec;
    } else {
        firing[unit] = false;
        spikeTimeMsec[unit] = 0.0;
    }
    return result;
}

double NeuralSynthSource::LFPVoltage() const
{
    // double amplitude = 5000.0;
    double amplitude = 100.0 + 80.0 * sin(TwoPi * (tMsec / 1000.0) * LFPModulationHz);
    return amplitude * sin(TwoPi * (tMsec / 1000.0) * LFPFrequencyHz);
}

uint16_t NeuralSynthSource::nextSample()
{
    double result = NoiseRMSLevelMicroVolts * randomGenerator->randomGaussian();
    for (int i = 0; i < nUnits; ++i) {
        if (firing[i]) {
            result += nextSpikeVoltage(i);
        } else {
            double spikeModulationFactor = (1000.0 - fmod(tMsec, 1000.0)) / 1000.0;
            if (randomGenerator->randomUniform() < (spikeModulationFactor * spikeRateHz[i] * tStepMsec / 1000.0)) {
                firing[i] = true;
            }
        }
    }
    //result = LFPVoltage();
    result += LFPVoltage();
    tMsec += tStepMsec;
    return convertToAmpADCValue(result);
}


ECGSynthSource::ECGSynthSource(RandomNumber* randomGenerator_, double sampleRate) :
    AbstractSynthSource(randomGenerator_, sampleRate)
{
    ecgAmplitude = randomGenerator->randomUniform(500.0, 3000.0);
    // Mimic virtual method reset() here in the constructor
    tMsec = 0.0;
}

void ECGSynthSource::reset()
{
    tMsec = 0.0;
}

uint16_t ECGSynthSource::nextSample()
{
    double ecgValue = 0.0;
    // Piece together half sine waves to model QRS complex, P wave, and T wave.
    if (tMsec < 80.0) {
        ecgValue = 0.04 * sin(TwoPi * tMsec / 160.0); // P wave
    } else if (tMsec > 100.0 && tMsec < 120.0) {
        ecgValue = -0.25 * sin(TwoPi * (tMsec - 100.0) / 40.0); // Q
    } else if (tMsec > 120.0 && tMsec < 180.0) {
        ecgValue = 1.0 * sin(TwoPi * (tMsec - 120.0) / 120.0); // R
    } else if (tMsec > 180.0 && tMsec < 260.0) {
        ecgValue = -0.12 * sin(TwoPi * (tMsec - 180.0) / 160.0); // S
    } else if (tMsec > 340.0 && tMsec < 400.0) {
        ecgValue = 0.06 * sin(TwoPi * (tMsec - 340.0) / 120.0); // T wave
    }

    // Repeat ECG waveform with regular period.
    tMsec += tStepMsec;
    if (tMsec > 840.0) {
        tMsec = 0.0;
    }

    // Multiply basic ECG waveform by channel-specific amplitude, and add 2.4 uVrms noise.
    ecgValue = ecgAmplitude * ecgValue + 2.4 * randomGenerator->randomGaussian();

    return convertToAmpADCValue(ecgValue);
}


ADCSynthSource::ADCSynthSource(RandomNumber* randomGenerator_, double sampleRate, double freqHz_, double amplitude_) :
    AbstractSynthSource(randomGenerator_, sampleRate),
    freqHz(freqHz_),
    amplitude(amplitude_)
{
    // Mimic virtual method reset() here in the constructor
    tMsec = 0.0;
}

void ADCSynthSource::reset()
{
    tMsec = 0.0;
}

uint16_t ADCSynthSource::nextSample()
{
    int result = round(32768.0 * amplitude * sin(TwoPi * (tMsec / 1000.0) * freqHz)) + 32768;
    if (result < 0) result = 0;
    else if (result > 65535) result = 65535;
    tMsec += tStepMsec;
    return (uint16_t) result;
}


DigitalSynthSource::DigitalSynthSource(RandomNumber* randomGenerator_, double sampleRate, double freqHz) :
    AbstractSynthSource(randomGenerator_, sampleRate)
{
    periodMsec = 1000.0 / freqHz;
    // Mimic virtual method reset() here in the constructor
    tMsec = 0.0;
}

void DigitalSynthSource::reset()
{
    tMsec = 0.0;
}

uint16_t DigitalSynthSource::nextSample()
{
    uint16_t result = 0U;
    if (tMsec > periodMsec / 2.0) result = 1U;
    tMsec += tStepMsec;
    if (tMsec > periodMsec) tMsec = 0.0;
    return result;
}


SynthDataBlockGenerator::SynthDataBlockGenerator(ControllerType type_, double sampleRate_) :
    type(type_),
    sampleRate(sampleRate_)
{
    int bufferSizeInWords = MaxNumBlocksToRead *
            RHXDataBlock::dataBlockSizeInWords(type, AbstractRHXController::maxNumDataStreams(type));
    std::cout << "SynthDataBlockGenerator: Allocating " << BytesPerWord * bufferSizeInWords / 1.0e6 << " MBytes for synthetic USB data generator." << std::endl;
    usbWords = nullptr;
    usbWords = new uint16_t [bufferSizeInWords];
    dataBlockPeriodInNsec = 1.0e9 * ((double)RHXDataBlock::samplesPerDataBlock(type)) / sampleRate;
    randomGenerator = new RandomNumber();

    synthSources.resize(AbstractRHXController::maxNumDataStreams(type));
    for (unsigned int stream = 0; stream < synthSources.size(); ++stream) {
        synthSources[stream].resize(RHXDataBlock::channelsPerStream(type));
        for (int channel = 0; channel < RHXDataBlock::channelsPerStream(type); ++channel) {
            if (sampleRate > 4999.9) {
                synthSources[stream][channel] = new NeuralSynthSource(randomGenerator, sampleRate, 2);
            } else {
                synthSources[stream][channel] = new ECGSynthSource(randomGenerator, sampleRate);
            }
            synthSources[stream][channel]->reset();
        }
    }
    adcSynthSources.resize(2);
    adcSynthSources[0] = new ADCSynthSource(randomGenerator, sampleRate, 10.0, 1.0);
    adcSynthSources[1] = new ADCSynthSource(randomGenerator, sampleRate, 100.0, 0.5);

    digitalSynthSources.resize(2);
    digitalSynthSources[0] = new DigitalSynthSource(randomGenerator, sampleRate, 1.0);
    digitalSynthSources[1] = new DigitalSynthSource(randomGenerator, sampleRate, 0.1);

    double auxIn = 1.7;
    auxInSample = (uint16_t) round(auxIn / 0.0000374);
    double vdd = 3.3;
    vddSample = (uint16_t) round(vdd / 0.0000748);

    double dcAmpVoltage = 1.0;
    dcAmpSample = (uint16_t) round(-dcAmpVoltage / 0.01923 + 512.0);

    reset();
}

SynthDataBlockGenerator::~SynthDataBlockGenerator()
{
    delete [] usbWords;
    delete randomGenerator;

    for (unsigned int stream = 0; stream < synthSources.size(); ++stream) {
        for (unsigned int channel = 0; channel < synthSources[stream].size(); ++channel) {
            delete synthSources[stream][channel];
        }
    }

    for (unsigned int i = 0; i < adcSynthSources.size(); ++i) {
        delete adcSynthSources[i];
    }

    for (unsigned int i = 0; i < digitalSynthSources.size(); ++i) {
        delete digitalSynthSources[i];
    }
}

void SynthDataBlockGenerator::reset()
{
    tIndex = 0;
    timeDeficitInNsec = 0.0;
    timer.start();
}

// Synthesize a certain number of USB data blocks, if the appropriate time has elapsed, and writes the raw bytes
// to a buffer.  Return total number of bytes read.
long SynthDataBlockGenerator::readSynthDataBlocksRaw(int numBlocks, uint8_t* buffer, int numDataStreams)
{
    double elapsedTime = (double)timer.nsecsElapsed();
    double targetTime = (double)numBlocks * dataBlockPeriodInNsec;
    double excessTime = elapsedTime - (targetTime - timeDeficitInNsec);

    if (excessTime < 0.0) return 0; // Not enough time has passed; wait for the data to be ready

    timer.start();

    timeDeficitInNsec = excessTime; // Remember excess time and subtract it from next time meausurement;
                                    // We need to do this to maintain the sample rate accurately.
    if (timeDeficitInNsec > targetTime) {   // But don't let the deficit be too large in any one pass.
        timeDeficitInNsec = targetTime;
    }

    createSynthDataBlock(numBlocks, numDataStreams);

    long numWords = numBlocks * RHXDataBlock::dataBlockSizeInWords(type, numDataStreams);
    uint16_t* pRead = usbWords;
    uint8_t* pWrite = buffer;
    for (long i = 0; i < numWords; ++i) {
        *pWrite = (uint8_t) (*pRead & 0x00ffU);
        pWrite++;
        *pWrite = (uint8_t) ((*pRead & 0xff00U) >> 8);
        pWrite++;
        pRead++;
    }

    return BytesPerWord * numWords;
}

void SynthDataBlockGenerator::createSynthDataBlock(int numBlocks, int numDataStreams)
{
    uint16_t* pWrite = usbWords;
    for (int block = 0; block < numBlocks; ++block) {
        for (int sample = 0; sample < RHXDataBlock::samplesPerDataBlock(type); ++sample) {
            // Write header magic number.
            uint64_t header = RHXDataBlock::headerMagicNumber(type);
            pWrite[0] = (uint16_t) ((header & 0x000000000000ffffUL) >> 0);
            pWrite[1] = (uint16_t) ((header & 0x00000000ffff0000UL) >> 16);
            pWrite[2] = (uint16_t) ((header & 0x0000ffff00000000UL) >> 32);
            pWrite[3] = (uint16_t) ((header & 0xffff000000000000UL) >> 48);
            pWrite += 4;

            // Write timestamp.
            pWrite[0] = (uint16_t) ((tIndex & 0x0000ffffU) >> 0);
            pWrite[1] = (uint16_t) ((tIndex & 0xffff0000U) >> 16);
            pWrite += 2;

            // Write amplifier and auxiliary data.
            switch (type) {
            case ControllerRecordUSB2:
            case ControllerRecordUSB3:
                // Write auxiliary command 0-2 results.
                for (int channel = 0; channel < 3; ++channel) {
                    for (int stream = 0; stream < numDataStreams; ++stream) {
                        if (channel == 1) {
                            switch (sample % 4) {
                            case 0:
                                *pWrite = (sample == 124) ? vddSample : 0x0049U;  // ROM register 40 read result (or Vdd)
                                break;
                            case 1:
                                *pWrite = auxInSample;   // AuxIn1 result
                                break;
                            case 2:
                                *pWrite = auxInSample;   // AuxIn2 result
                                break;
                            case 3:
                                *pWrite = auxInSample;   // AuxIn3 result
                                break;
                            }
                        } else {
                            *pWrite = 0U;
                        }
                        pWrite++;
                    }
                }
                // Write amplifier data (same for all streams to save CPU time).
                for (int channel = 0; channel < RHXDataBlock::channelsPerStream(type); ++channel) {
                    uint16_t value = synthSources[0][channel]->nextSample();
                    for (int stream = 0; stream < numDataStreams; ++stream) {
                        *pWrite = value;
                        pWrite++;
                    }
                }
                break;
            case ControllerStimRecord:
                // Write auxiliary command 1-3 results.
                for (int channel = 1; channel < 4; ++channel) {
                    for (int stream = 0; stream < numDataStreams; ++stream) {
                        pWrite[0] = 0U;
                        pWrite[1] = 0U;
                        pWrite += 2;
                    }
                }
                // Write amplifier data (same for all streams to save CPU time).
                for (int channel = 0; channel < RHXDataBlock::channelsPerStream(type); ++channel) {
                    uint16_t value = synthSources[0][channel]->nextSample();
                    for (int stream = 0; stream < numDataStreams; ++stream) {
                        pWrite[0] = (uint16_t) dcAmpSample;   // DC amplifier result; same on all channels here
                        pWrite[1] = value; // AC amplifier result
                        pWrite += 2;
                    }
                }
                // Write auxiliary command 0 results.
                for (int stream = 0; stream < numDataStreams; ++stream) {
                    pWrite[0] = 0U;
                    pWrite[1] = 0U;
                    pWrite += 2;
                }
                break;
            }

            // Write filler words.
            int numFillerWords = 0;
            if (type == ControllerRecordUSB2) {
                numFillerWords = numDataStreams;
            } else if (type == ControllerRecordUSB3) {
                numFillerWords = numDataStreams % 4;
            }
            for (int i = 0; i < numFillerWords; ++i) {
                *pWrite = (uint16_t) 0U;
                pWrite++;
            }

            // Write stimulation data (ControllerStimRecord only).
            if (type == ControllerStimRecord) {
                // Write stimulation on/off data.
                for (int stream = 0; stream < numDataStreams; ++stream) {
                    *pWrite = 0U;
                    // *pWrite = (block == 0) ? 1U : 0U;   // TEMP
                    pWrite++;
                }
                // Write stimulation polarity data.
                for (int stream = 0; stream < numDataStreams; ++stream) {
                    *pWrite = 0U;
                    pWrite++;
                }
                // Write amplifier settle data.
                for (int stream = 0; stream < numDataStreams; ++stream) {
                    *pWrite = 0U;
                    // *pWrite = (block == 1 || block == 2) ? 1U : 0U;   // TEMP
                    pWrite++;
                }
                // Write charge recovery data.
                for (int stream = 0; stream < numDataStreams; ++stream) {
                    *pWrite = 0U;
                    // *pWrite = (block == 2) ? 1U : 0U;   // TEMP
                    pWrite++;
                }
            }

            // Write Analog Out data (ControllerStimRecord only).
            if (type == ControllerStimRecord) {
                for (int i = 0; i < 8; ++i) {
                    *pWrite = (uint16_t) 32768U;
                    pWrite++;
                }
            }

            // Write Analog In data.
            *pWrite = adcSynthSources[0]->nextSample();
            pWrite++;
            *pWrite = adcSynthSources[1]->nextSample();
            pWrite++;
            for (int i = 2; i < 8; ++i) {
                if (type == ControllerRecordUSB2) {
                    *pWrite = (uint16_t) 0;
                } else {
                    *pWrite = (uint16_t) 32768U;
                }
                pWrite++;
            }

            // Write Digital In data.
            *pWrite = digitalSynthSources[0]->nextSample() + 2 * digitalSynthSources[1]->nextSample();
            pWrite++;

            // Write Digital Out data.
            *pWrite = (uint16_t) 0U;
            pWrite++;

            tIndex++;
        }
    }
}

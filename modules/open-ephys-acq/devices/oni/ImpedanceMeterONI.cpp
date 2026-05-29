/*
    ------------------------------------------------------------------
    Copyright (C) 2021 Open Ephys
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

#include "ImpedanceMeterONI.h"

#include "AcqBoardONI.h"

#include <cmath>
#include <vector>

#define PI                 3.14159265359
#define TWO_PI             6.28318530718
#define DEGREES_TO_RADIANS 0.0174532925199
#define RADIANS_TO_DEGREES 57.2957795132

#define CHECK_EXIT    \
    if (shouldStop()) \
    return

ImpedanceMeterONI::ImpedanceMeterONI(AcqBoardONI *board_)
    : ImpedanceMeter(),
      acquisitionBoard(board_),
      m_log(Syntalos::getLogger("oeacq.impedance-oni"))
{
    // 120 blocks is enough headroom to perform electrode impedance measurements
    // at very low frequencies.
    const int maxNumBlocks = 120;
    int numStreams = 8;

    allocateDoubleArray3D(amplifierPreFilter, numStreams, 32, MAX_SAMPLES_PER_DATA_BLOCK * maxNumBlocks);
}

ImpedanceMeterONI::~ImpedanceMeterONI() = default;

float ImpedanceMeterONI::updateImpedanceFrequency(float desiredImpedanceFreq, bool &impedanceFreqValid)
{
    int impedancePeriod;
    double lowerBandwidthLimit, upperBandwidthLimit;
    float actualImpedanceFreq;

    upperBandwidthLimit = acquisitionBoard->settings.analogFilter.upperBandwidth / 1.5;
    lowerBandwidthLimit = acquisitionBoard->settings.analogFilter.lowerBandwidth * 1.5;

    if (acquisitionBoard->settings.dsp.enabled) {
        if (acquisitionBoard->settings.dsp.cutoffFreq > acquisitionBoard->settings.analogFilter.lowerBandwidth) {
            lowerBandwidthLimit = acquisitionBoard->settings.dsp.cutoffFreq * 1.5;
        }
    }

    if (desiredImpedanceFreq > 0.0) {
        impedancePeriod = (acquisitionBoard->settings.boardSampleRate / desiredImpedanceFreq);
        if (impedancePeriod >= 4 && impedancePeriod <= 1024 && desiredImpedanceFreq >= lowerBandwidthLimit
            && desiredImpedanceFreq <= upperBandwidthLimit) {
            actualImpedanceFreq = acquisitionBoard->settings.boardSampleRate / impedancePeriod;
            impedanceFreqValid = true;
        } else {
            actualImpedanceFreq = 0.0;
            impedanceFreqValid = false;
        }
    } else {
        actualImpedanceFreq = 0.0;
        impedanceFreqValid = false;
    }

    return actualImpedanceFreq;
}

int ImpedanceMeterONI::loadAmplifierData(std::queue<Rhd2000ONIDataBlock> &dataQueue, int numBlocks, int numDataStreams)
{
    int block, t, channel, stream;
    int indexAmp = 0;

    for (block = 0; block < numBlocks; ++block) {
        // Load and scale RHD2000 amplifier waveforms (sampled at amplifier sampling rate).
        for (t = 0; t < SAMPLES_PER_DATA_BLOCK(acquisitionBoard->evalBoard->isUSB3()); ++t) {
            for (channel = 0; channel < 32; ++channel) {
                for (stream = 0; stream < numDataStreams; ++stream) {
                    // Amplifier waveform units = microvolts.
                    amplifierPreFilter[stream][channel]
                                      [indexAmp] = 0.195
                                                   * (dataQueue.front().amplifierData[stream][channel][t] - 32768);
                }
            }
            ++indexAmp;
        }
        // We are done with this Rhd2000DataBlock object; remove it from dataQueue.
        dataQueue.pop();
    }

    return 0;
}

void ImpedanceMeterONI::measureComplexAmplitude(
    std::vector<std::vector<std::vector<double>>> &measuredMagnitude,
    std::vector<std::vector<std::vector<double>>> &measuredPhase,
    int capIndex,
    int stream,
    int chipChannel,
    int numBlocks,
    double sampleRate,
    double frequency,
    int numPeriods)
{
    int period = (sampleRate / frequency);
    int startIndex = 0;
    int endIndex = startIndex + numPeriods * period - 1;

    // Move the measurement window to the end of the waveform to ignore start-up transient.
    while (endIndex < SAMPLES_PER_DATA_BLOCK(acquisitionBoard->evalBoard->isUSB3()) * numBlocks - period) {
        startIndex += period;
        endIndex += period;
    }

    double iComponent, qComponent;

    // Measure real (iComponent) and imaginary (qComponent) amplitude of frequency component.
    amplitudeOfFreqComponent(
        iComponent,
        qComponent,
        amplifierPreFilter[stream][chipChannel],
        startIndex,
        endIndex,
        sampleRate,
        frequency);
    // Calculate magnitude and phase from real (I) and imaginary (Q) components.
    measuredMagnitude[stream][chipChannel][capIndex] = std::sqrt(iComponent * iComponent + qComponent * qComponent);
    measuredPhase[stream][chipChannel][capIndex] = RADIANS_TO_DEGREES * std::atan2(qComponent, iComponent);
}

void ImpedanceMeterONI::amplitudeOfFreqComponent(
    double &realComponent,
    double &imagComponent,
    const std::vector<double> &data,
    int startIndex,
    int endIndex,
    double sampleRate,
    double frequency)
{
    int length = endIndex - startIndex + 1;
    const double k = TWO_PI * frequency / sampleRate; // precalculate for speed

    // Perform correlation with sine and cosine waveforms.
    double meanI = 0.0;
    double meanQ = 0.0;
    for (int t = startIndex; t <= endIndex; ++t) {
        meanI += data.at(t) * std::cos(k * t);
        meanQ += data.at(t) * -1.0 * std::sin(k * t);
    }
    meanI /= static_cast<double>(length);
    meanQ /= static_cast<double>(length);

    realComponent = 2.0 * meanI;
    imagComponent = 2.0 * meanQ;
}

void ImpedanceMeterONI::factorOutParallelCapacitance(
    double &impedanceMagnitude,
    double &impedancePhase,
    double frequency,
    double parasiticCapacitance)
{
    // First, convert from polar coordinates to rectangular coordinates.
    double measuredR = impedanceMagnitude * std::cos(DEGREES_TO_RADIANS * impedancePhase);
    double measuredX = impedanceMagnitude * std::sin(DEGREES_TO_RADIANS * impedancePhase);

    double capTerm = TWO_PI * frequency * parasiticCapacitance;
    double xTerm = capTerm * (measuredR * measuredR + measuredX * measuredX);
    double denominator = capTerm * xTerm + 2 * capTerm * measuredX + 1;
    double trueR = measuredR / denominator;
    double trueX = (measuredX + xTerm) / denominator;

    // Now, convert from rectangular coordinates back to polar coordinates.
    impedanceMagnitude = std::sqrt(trueR * trueR + trueX * trueX);
    impedancePhase = RADIANS_TO_DEGREES * std::atan2(trueX, trueR);
}

void ImpedanceMeterONI::empiricalResistanceCorrection(
    double &impedanceMagnitude,
    double &impedancePhase,
    double boardSampleRate)
{
    // First, convert from polar coordinates to rectangular coordinates.
    double impedanceR = impedanceMagnitude * std::cos(DEGREES_TO_RADIANS * impedancePhase);
    double impedanceX = impedanceMagnitude * std::sin(DEGREES_TO_RADIANS * impedancePhase);

    // Empirically derived correction factor (i.e., no physical basis for this equation).
    impedanceR /= 10.0 * std::exp(-boardSampleRate / 2500.0) * std::cos(TWO_PI * boardSampleRate / 15000.0) + 1.0;

    // Now, convert from rectangular coordinates back to polar coordinates.
    impedanceMagnitude = std::sqrt(impedanceR * impedanceR + impedanceX * impedanceX);
    impedancePhase = RADIANS_TO_DEGREES * std::atan2(impedanceX, impedanceR);
}

void ImpedanceMeterONI::runImpedanceMeasurement(Impedances &impedances)
{
    // Save the current board settings; the impedance algorithm needs the
    // sample rate fixed at 30 kHz with default analog filters / DSP off.
    LOG_DEBUG(m_log, "Storing board settings...");
    const double lowerBandwidth = acquisitionBoard->settings.analogFilter.lowerBandwidth;
    const double upperBandwidth = acquisitionBoard->settings.analogFilter.upperBandwidth;
    const bool dspEnabled = acquisitionBoard->settings.dsp.enabled;
    const float boardSampleRate = acquisitionBoard->settings.boardSampleRate;

    acquisitionBoard->setSampleRate(30000.0f);
    acquisitionBoard->setUpperBandwidth(7500);
    acquisitionBoard->setLowerBandwidth(1);
    acquisitionBoard->setDspOffset(false);

    LOG_DEBUG(m_log, "Running impedance measurement");
    runImpedanceMeasurementInternal(impedances);

    LOG_DEBUG(m_log, "Restoring board settings");
    restoreBoardSettings();

    acquisitionBoard->setSampleRate(boardSampleRate);
    acquisitionBoard->setUpperBandwidth(upperBandwidth);
    acquisitionBoard->setLowerBandwidth(lowerBandwidth);
    acquisitionBoard->setDspOffset(dspEnabled);

    LOG_DEBUG(m_log, "Impedance measurement finished");
    acquisitionBoard->impedanceMeasurementFinished();

    reportProgress(1.0);
}

void ImpedanceMeterONI::runImpedanceMeasurementInternal(Impedances &impedances)
{
    int commandSequenceLength, stream, channel, capRange;
    std::vector<int> commandList;

    reportProgress(0.0);

    int numdataStreams = acquisitionBoard->evalBoard->getNumEnabledDataStreams();
    LOG_DEBUG(m_log, "ImpedanceMeterONI: Num enabled streams = {}", numdataStreams);

    bool rhd2164ChipPresent = false;
    int chOffset;

    std::vector<int> enabledStreams;

    for (stream = 0; stream < acquisitionBoard->MAX_NUM_DATA_STREAMS; ++stream) {
        CHECK_EXIT;

        if (acquisitionBoard->evalBoard->isStreamEnabled(stream)) {
            enabledStreams.push_back(stream);
        }

        if (acquisitionBoard->chipId[stream] == CHIP_ID_RHD2164_B) {
            rhd2164ChipPresent = true;
        }
    }

    bool validImpedanceFreq;
    LOG_DEBUG(m_log, "ImpedanceMeterONI: Updating impedance frequency to 1000");
    float actualImpedanceFreq = updateImpedanceFrequency(1000.0, validImpedanceFreq);

    if (!validImpedanceFreq) {
        LOG_DEBUG(m_log, "Invalid frequency");
        return;
    }

    // Create a command list for the AuxCmd1 slot.
    commandSequenceLength = acquisitionBoard->chipRegisters.createCommandListZcheckDac(
        commandList,
        actualImpedanceFreq,
        128.0);
    CHECK_EXIT;
    LOG_DEBUG(m_log, "ImpedanceMeterONI: Updating command list");
    acquisitionBoard->evalBoard->uploadCommandList(commandList, Rhd2000ONIBoard::AuxCmd1, 1);
    acquisitionBoard->evalBoard->selectAuxCommandLength(Rhd2000ONIBoard::AuxCmd1, 0, commandSequenceLength - 1);

    if (acquisitionBoard->settings.fastTTLSettleEnabled) {
        acquisitionBoard->evalBoard->enableExternalFastSettle(false);
    }

    CHECK_EXIT;
    acquisitionBoard->evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortA, Rhd2000ONIBoard::AuxCmd1, 1);
    acquisitionBoard->evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortB, Rhd2000ONIBoard::AuxCmd1, 1);
    acquisitionBoard->evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortC, Rhd2000ONIBoard::AuxCmd1, 1);
    acquisitionBoard->evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortD, Rhd2000ONIBoard::AuxCmd1, 1);

    // Select number of periods to measure impedance over.
    int numPeriods = (0.020 * actualImpedanceFreq); // Test each channel for at least 20 ms…
    if (numPeriods < 5)
        numPeriods = 5; // …but always measure across no fewer than 5 complete periods.
    double period = acquisitionBoard->settings.boardSampleRate / actualImpedanceFreq;
    int numBlocks = std::ceil(
        (numPeriods + 2) * period
        / static_cast<float>(SAMPLES_PER_DATA_BLOCK(acquisitionBoard->evalBoard->isUSB3()))); // +2 periods to settle
    LOG_DEBUG(m_log, "ImpedanceMeterONI: Initial numBlocks = {}", numBlocks);
    if (numBlocks < 2)
        numBlocks = 2; // need first block for command to switch channels to take effect.
    LOG_DEBUG(m_log, "ImpedanceMeterONI: Padded numBlocks = {}", numBlocks);

    CHECK_EXIT;
    acquisitionBoard->settings.dsp.cutoffFreq = acquisitionBoard->chipRegisters.setDspCutoffFreq(
        acquisitionBoard->settings.dsp.cutoffFreq);
    acquisitionBoard->settings.analogFilter.lowerBandwidth = acquisitionBoard->chipRegisters.setLowerBandwidth(
        acquisitionBoard->settings.analogFilter.lowerBandwidth);
    acquisitionBoard->settings.analogFilter.upperBandwidth = acquisitionBoard->chipRegisters.setUpperBandwidth(
        acquisitionBoard->settings.analogFilter.upperBandwidth);
    acquisitionBoard->chipRegisters.enableDsp(acquisitionBoard->settings.dsp.enabled);
    acquisitionBoard->chipRegisters.enableZcheck(true);
    LOG_DEBUG(m_log, "ImpedanceMeterONI: Z check enabled");

    commandSequenceLength = acquisitionBoard->chipRegisters.createCommandListRegisterConfig(commandList, false);
    CHECK_EXIT;
    // Upload version with no ADC calibration to AuxCmd3 RAM Bank 1.
    acquisitionBoard->evalBoard->uploadCommandList(commandList, Rhd2000ONIBoard::AuxCmd3, 3);
    acquisitionBoard->evalBoard->selectAuxCommandLength(Rhd2000ONIBoard::AuxCmd3, 0, commandSequenceLength - 1);
    acquisitionBoard->evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortA, Rhd2000ONIBoard::AuxCmd3, 3);
    acquisitionBoard->evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortB, Rhd2000ONIBoard::AuxCmd3, 3);
    acquisitionBoard->evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortC, Rhd2000ONIBoard::AuxCmd3, 3);
    acquisitionBoard->evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortD, Rhd2000ONIBoard::AuxCmd3, 3);

    CHECK_EXIT;
    acquisitionBoard->evalBoard->setContinuousRunMode(false);

    acquisitionBoard->evalBoard->setMaxTimeStep(
        128 * SAMPLES_PER_DATA_BLOCK(acquisitionBoard->evalBoard->isUSB3()) * numBlocks);

    // Create matrices of doubles of size (numStreams x 32 x 3) to store complex amplitudes
    // of all amplifier channels (32 on each data stream) at three different Cseries values.
    std::vector<std::vector<std::vector<double>>> measuredMagnitude;
    std::vector<std::vector<std::vector<double>>> measuredPhase;

    measuredMagnitude.resize(acquisitionBoard->evalBoard->getNumEnabledDataStreams());
    measuredPhase.resize(acquisitionBoard->evalBoard->getNumEnabledDataStreams());

    for (int i = 0; i < acquisitionBoard->evalBoard->getNumEnabledDataStreams(); ++i) {
        measuredMagnitude[i].resize(32);
        measuredPhase[i].resize(32);

        for (int j = 0; j < 32; ++j) {
            measuredMagnitude[i][j].resize(3);
            measuredPhase[i][j].resize(3);
        }
    }

    double distance, minDistance, current, cSeries;
    double impedanceMagnitude, impedancePhase;

    const double bestAmplitude = 250.0; // we favor voltage readings closest to 250 µV: not too large, not too small
    const double dacVoltageAmplitude = 128 * (1.225 / 256); // assumes the DAC amplitude was set to 128
    const double parasiticCapacitance = 14.0e-12;           // 14 pF: estimate of on-chip parasitic capacitance
    double relativeFreq = actualImpedanceFreq / acquisitionBoard->settings.boardSampleRate;

    int bestAmplitudeIndex;

    // We execute three complete electrode impedance measurements: one each with
    // Cseries set to 0.1 pF, 1 pF, and 10 pF.  Then we select the best measurement
    // for each channel so that we achieve a wide impedance measurement range.
    for (capRange = 0; capRange < 3; ++capRange) {
        switch (capRange) {
        case 0:
            acquisitionBoard->chipRegisters.setZcheckScale(Rhd2000ONIRegisters::ZcheckCs100fF);
            cSeries = 0.1e-12;
            break;
        case 1:
            acquisitionBoard->chipRegisters.setZcheckScale(Rhd2000ONIRegisters::ZcheckCs1pF);
            cSeries = 1.0e-12;
            break;
        case 2:
            acquisitionBoard->chipRegisters.setZcheckScale(Rhd2000ONIRegisters::ZcheckCs10pF);
            cSeries = 10.0e-12;
            break;
        }

        // Check all 32 channels across all active data streams.
        for (channel = 0; channel < 32; ++channel) {
            CHECK_EXIT;

            acquisitionBoard->chipRegisters.setZcheckChannel(channel);
            commandSequenceLength = acquisitionBoard->chipRegisters.createCommandListRegisterConfig(commandList, false);
            // Upload version with no ADC calibration to AuxCmd3 RAM Bank 1.
            acquisitionBoard->evalBoard->uploadCommandList(commandList, Rhd2000ONIBoard::AuxCmd3, 3);

            acquisitionBoard->evalBoard->run();

            std::queue<Rhd2000ONIDataBlock> dataQueue;
            acquisitionBoard->evalBoard->readDataBlocks(numBlocks, dataQueue);
            {
                const std::lock_guard<std::mutex> lock(acquisitionBoard->oniLock);
                acquisitionBoard->evalBoard->stop();
            }

            loadAmplifierData(dataQueue, numBlocks, numdataStreams);

            for (stream = 0; stream < numdataStreams; ++stream) {
                reportProgress(
                    static_cast<double>(capRange) / 3.0 + (static_cast<double>(channel) / 32.0 / 3.0)
                    + (static_cast<double>(stream) / static_cast<double>(numdataStreams) / 32.0 / 3.0));

                if (acquisitionBoard->chipId[stream] != CHIP_ID_RHD2164_B) {
                    measureComplexAmplitude(
                        measuredMagnitude,
                        measuredPhase,
                        capRange,
                        stream,
                        channel,
                        numBlocks,
                        acquisitionBoard->settings.boardSampleRate,
                        actualImpedanceFreq,
                        numPeriods);
                }
            }

            // If an RHD2164 chip is plugged in, we have to set the Zcheck select register to channels 32-63
            // and repeat the previous steps.
            if (rhd2164ChipPresent) {
                CHECK_EXIT;
                acquisitionBoard->chipRegisters.setZcheckChannel(channel + 32); // address channels 32-63
                commandSequenceLength = acquisitionBoard->chipRegisters.createCommandListRegisterConfig(
                    commandList,
                    false);
                acquisitionBoard->evalBoard->uploadCommandList(commandList, Rhd2000ONIBoard::AuxCmd3, 3);

                acquisitionBoard->evalBoard->run();
                acquisitionBoard->evalBoard->readDataBlocks(numBlocks, dataQueue);
                {
                    const std::lock_guard<std::mutex> lock(acquisitionBoard->oniLock);
                    acquisitionBoard->evalBoard->stop();
                }
                loadAmplifierData(dataQueue, numBlocks, numdataStreams);

                for (stream = 0; stream < acquisitionBoard->evalBoard->getNumEnabledDataStreams(); ++stream) {
                    if (acquisitionBoard->chipId[stream] == CHIP_ID_RHD2164_B) {
                        measureComplexAmplitude(
                            measuredMagnitude,
                            measuredPhase,
                            capRange,
                            stream,
                            channel,
                            numBlocks,
                            acquisitionBoard->settings.boardSampleRate,
                            actualImpedanceFreq,
                            numPeriods);
                    }
                }
            }
        }
    }

    impedances.streams.clear();
    impedances.channels.clear();
    impedances.magnitudes.clear();
    impedances.phases.clear();

    for (stream = 0; stream < acquisitionBoard->evalBoard->getNumEnabledDataStreams(); ++stream) {
        if ((acquisitionBoard->chipId[stream] == CHIP_ID_RHD2132)
            && (acquisitionBoard->numChannelsPerDataStream[stream] == 16))
            chOffset = RHD2132_16CH_OFFSET;
        else
            chOffset = 0;

        for (channel = 0; channel < acquisitionBoard->numChannelsPerDataStream[stream]; ++channel) {
            minDistance = 9.9e99; // ridiculously large number
            for (capRange = 0; capRange < 3; ++capRange) {
                // Find the measured amplitude that is closest to bestAmplitude on a logarithmic scale
                distance = std::abs(std::log(measuredMagnitude[stream][channel + chOffset][capRange] / bestAmplitude));
                if (distance < minDistance) {
                    bestAmplitudeIndex = capRange;
                    minDistance = distance;
                }
            }
            switch (bestAmplitudeIndex) {
            case 0:
                cSeries = 0.1e-12;
                break;
            case 1:
                cSeries = 1.0e-12;
                break;
            case 2:
                cSeries = 10.0e-12;
                break;
            }

            // Calculate current amplitude produced by on-chip voltage DAC.
            current = TWO_PI * actualImpedanceFreq * dacVoltageAmplitude * cSeries;

            // Calculate impedance magnitude from calculated current and measured voltage.
            impedanceMagnitude = 1.0e-6 * (measuredMagnitude[stream][channel + chOffset][bestAmplitudeIndex] / current)
                                 * (18.0 * relativeFreq * relativeFreq + 1.0);

            // Calculate impedance phase, with small correction factor accounting for the
            // 3-command SPI pipeline delay.
            impedancePhase = measuredPhase[stream][channel + chOffset][bestAmplitudeIndex] + (360.0 * (3.0 / period));

            // Factor out on-chip parasitic capacitance from impedance measurement.
            factorOutParallelCapacitance(impedanceMagnitude, impedancePhase, actualImpedanceFreq, parasiticCapacitance);

            // Perform empirical resistance correction to improve accuracy at sample rates below 15 kS/s.
            empiricalResistanceCorrection(
                impedanceMagnitude,
                impedancePhase,
                acquisitionBoard->settings.boardSampleRate);

            impedances.streams.push_back(enabledStreams[stream]);
            impedances.channels.push_back(channel + chOffset);
            impedances.magnitudes.push_back(impedanceMagnitude);
            impedances.phases.push_back(impedancePhase);
        }
    }

    impedances.valid = true;
}

void ImpedanceMeterONI::restoreBoardSettings()
{
    acquisitionBoard->evalBoard->setContinuousRunMode(false);
    acquisitionBoard->evalBoard->setMaxTimeStep(0);

    // Switch back to flatline.
    acquisitionBoard->evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortA, Rhd2000ONIBoard::AuxCmd1, 0);
    acquisitionBoard->evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortB, Rhd2000ONIBoard::AuxCmd1, 0);
    acquisitionBoard->evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortC, Rhd2000ONIBoard::AuxCmd1, 0);
    acquisitionBoard->evalBoard->selectAuxCommandBank(Rhd2000ONIBoard::PortD, Rhd2000ONIBoard::AuxCmd1, 0);

    acquisitionBoard->evalBoard->selectAuxCommandLength(Rhd2000ONIBoard::AuxCmd1, 0, 1);

    acquisitionBoard->evalBoard->selectAuxCommandBank(
        Rhd2000ONIBoard::PortA,
        Rhd2000ONIBoard::AuxCmd3,
        acquisitionBoard->settings.fastSettleEnabled ? 2 : 1);
    acquisitionBoard->evalBoard->selectAuxCommandBank(
        Rhd2000ONIBoard::PortB,
        Rhd2000ONIBoard::AuxCmd3,
        acquisitionBoard->settings.fastSettleEnabled ? 2 : 1);
    acquisitionBoard->evalBoard->selectAuxCommandBank(
        Rhd2000ONIBoard::PortC,
        Rhd2000ONIBoard::AuxCmd3,
        acquisitionBoard->settings.fastSettleEnabled ? 2 : 1);
    acquisitionBoard->evalBoard->selectAuxCommandBank(
        Rhd2000ONIBoard::PortD,
        Rhd2000ONIBoard::AuxCmd3,
        acquisitionBoard->settings.fastSettleEnabled ? 2 : 1);

    if (acquisitionBoard->settings.fastTTLSettleEnabled) {
        acquisitionBoard->evalBoard->enableExternalFastSettle(true);
    }
}

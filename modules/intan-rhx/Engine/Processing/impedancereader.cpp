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

#include <QProgressDialog>
#include <QMessageBox>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <cmath>
#include <deque>
#include <iostream>
#include "signalsources.h"
#include "impedancereader.h"

ImpedanceReader::ImpedanceReader(SystemState* state_, AbstractRHXController* rhxController_) :
    state(state_),
    rhxController(rhxController_)
{
}

bool ImpedanceReader::measureImpedances()
{
    // We can't really measure impedances in playback mode, so just return false.
    if (state->playback->getValue()) return false;

    if (state->synthetic->getValue()) {
        // Plug in dummy impedance values in synthetic mode and return true.
        runDemoImpedanceMeasurement();
        return true;
    }

    bool rhd2164ChipPresent = false;
    for (int stream = 0; stream < (int) state->chipType.size(); ++stream) {
        if (state->chipType[stream] == RHD2164MISOBChip) {
            rhd2164ChipPresent = true;
        }
    }

    // If this is an RHD system: disable external fast settling, since this interferes with DAC commands in AuxCmd1.
    ControllerType controllerType = state->getControllerTypeEnum();
    if (state->getControllerTypeEnum() == ControllerRecordUSB2 || state->getControllerTypeEnum() == ControllerRecordUSB3) {
        rhxController->enableExternalFastSettle(false);

        rhxController->enableExternalDigOut(PortA, false);
        rhxController->enableExternalDigOut(PortB, false);
        rhxController->enableExternalDigOut(PortC, false);
        rhxController->enableExternalDigOut(PortD, false);
        if (state->numSPIPorts > 4) {
            rhxController->enableExternalDigOut(PortE, false);
            rhxController->enableExternalDigOut(PortF, false);
            rhxController->enableExternalDigOut(PortG, false);
            rhxController->enableExternalDigOut(PortH, false);
        }
    }

    // Disable DACs
    for (int i = 0; i < 8; i++) {
        rhxController->enableDac(i, false);
    }

    // Create a progress bar to let user know how long this will take.
    int maxProgress = (controllerType == ControllerStimRecord) ? 50 : 98;
    QProgressDialog progress(QObject::tr("Measuring Electrode Impedances"), QString(), 0, maxProgress);
    progress.setWindowTitle(QObject::tr("Progress"));
    progress.setMinimumDuration(0);
    progress.setModal(true);
    progress.setValue(0);

    // Create a command list for the AuxCmd1 slot.
    RHXRegisters chipRegisters(controllerType, state->sampleRate->getNumericValue(), state->getStimStepSizeEnum());
    std::vector<unsigned int> commandList;

    int commandSequenceLength = chipRegisters.createCommandListZcheckDac(commandList, state->actualImpedanceFreq->getValue(),
                                                                         128.0);

    rhxController->uploadCommandList(commandList, AbstractRHXController::AuxCmd1, 1);
    rhxController->selectAuxCommandLength(AbstractRHXController::AuxCmd1, 0, commandSequenceLength - 1);

    if (controllerType == ControllerRecordUSB2 || controllerType == ControllerRecordUSB3) {
        rhxController->selectAuxCommandBankAllPorts(AbstractRHXController::AuxCmd1, 1);
    }

    progress.setValue(1);

    // Select number of periods to measure impedance over
    int numPeriods = round(0.020 * state->actualImpedanceFreq->getValue()); // Test each channel for at least 20 msec...
    if (numPeriods < 5) numPeriods = 5; // ...but always measure across no fewer than 5 complete periods
    double period = state->sampleRate->getNumericValue() / state->actualImpedanceFreq->getValue();
    int numBlocks = ceil((numPeriods + 2) * period / (double) RHXDataBlock::samplesPerDataBlock(controllerType)); // + 2 periods to give time to settle initially
    if (numBlocks < 2) numBlocks = 2;   // need first block for command to switch channels to take effect


     chipRegisters.setDspCutoffFreq(state->desiredDspCutoffFreq->getValue());
    chipRegisters.setLowerBandwidth(state->desiredLowerBandwidth->getValue(), 0);
    chipRegisters.setUpperBandwidth(state->desiredUpperBandwidth->getValue());
    chipRegisters.enableDsp(state->dspEnabled->getValue());
    chipRegisters.enableZcheck(true);
    if (controllerType == ControllerStimRecord) {
        commandSequenceLength = chipRegisters.createCommandListRHSRegisterConfig(commandList, false);
    } else {
        commandSequenceLength = chipRegisters.createCommandListRHDRegisterConfig(commandList, false,
                                                                                 RHXDataBlock::samplesPerDataBlock(controllerType));
    }

    // Upload version with no ADC calibration to AuxCmd3 RAM.
    rhxController->uploadCommandList(commandList, AbstractRHXController::AuxCmd3, 3);
    rhxController->selectAuxCommandLength(AbstractRHXController::AuxCmd3, 0, commandSequenceLength - 1);

    if (controllerType == ControllerRecordUSB2 || controllerType == ControllerRecordUSB3) {
        rhxController->selectAuxCommandBankAllPorts(AbstractRHXController::AuxCmd3, 3);
    }

    rhxController->setContinuousRunMode(false);
    rhxController->setMaxTimeStep(RHXDataBlock::samplesPerDataBlock(controllerType) * numBlocks);

    // Create matrices of doubles of size (numStreams x numChannelsPerStream x 3) to store complex amplitudes
    // of all amplifier channels (32 or 16 on each data stream) at three different Cseries values
    int numChannelsPerStream = RHXDataBlock::channelsPerStream(controllerType);
    std::vector<std::vector<std::vector<ComplexPolar> > > measuredImpedance;
    measuredImpedance.resize(rhxController->getNumEnabledDataStreams());
    for (int i = 0; i < rhxController->getNumEnabledDataStreams(); ++i) {
        measuredImpedance[i].resize(numChannelsPerStream);
        for (int j = 0; j < numChannelsPerStream; ++j) {
            measuredImpedance[i][j].resize(3);
        }
    }

    // Data files used to examine data used in impedance measurement, only necessary for testing
//    QFile cap0File("cap0.dat");
//    cap0File.open(QIODevice::WriteOnly);
//    QDataStream cap0Stream(&cap0File);
//    cap0Stream.setVersion(QDataStream::Qt_5_11);
//    cap0Stream.setByteOrder(QDataStream::LittleEndian);
//    cap0Stream.setFloatingPointPrecision(QDataStream::DoublePrecision);

//    QFile cap1File("cap1.dat");
//    cap1File.open(QIODevice::WriteOnly);
//    QDataStream cap1Stream(&cap1File);
//    cap1Stream.setVersion(QDataStream::Qt_5_11);
//    cap1Stream.setByteOrder(QDataStream::LittleEndian);
//    cap1Stream.setFloatingPointPrecision(QDataStream::DoublePrecision);

//    QFile cap2File("cap2.dat");
//    cap2File.open(QIODevice::WriteOnly);
//    QDataStream cap2Stream(&cap2File);
//    cap2Stream.setVersion(QDataStream::Qt_5_11);
//    cap2Stream.setByteOrder(QDataStream::LittleEndian);
//    cap2Stream.setFloatingPointPrecision(QDataStream::DoublePrecision);

    // We execute three complete electrode impedance measurements: one each with
    // Cseries set to 0.1 pF, 1 pF, and 10 pF.  Then we select the best measurement
    // for each channel so that we achieve a wide impedance measurement range.
    for (int capRange = 0; capRange < 3; ++capRange) {
        switch (capRange) {
        case 0:
            chipRegisters.setZcheckScale(RHXRegisters::ZcheckCs100fF);
            break;
        case 1:
            chipRegisters.setZcheckScale(RHXRegisters::ZcheckCs1pF);
            break;
        case 2:
            chipRegisters.setZcheckScale(RHXRegisters::ZcheckCs10pF);
            break;
        }

        // Check all channels across all active data streams.
        for (int channel = 0; channel < numChannelsPerStream; ++channel) {
            progress.setValue(numChannelsPerStream * capRange + channel + 2);

            chipRegisters.setZcheckChannel(channel);
            if (controllerType == ControllerStimRecord) {
                //commandSequenceLength = chipRegisters.createCommandListRHSRegisterConfig(commandList, false);
                chipRegisters.createCommandListRHSRegisterConfig(commandList, false);
            } else {
                //commandSequenceLength = chipRegisters.createCommandListRHDRegisterConfig(commandList, false,
                                                                                         //RHXDataBlock::samplesPerDataBlock(controllerType));
                chipRegisters.createCommandListRHDRegisterConfig(commandList, false,
                                                                 RHXDataBlock::samplesPerDataBlock(controllerType));
            }

            // Upload version with no ADC calibration to AuxCmd3 RAM bank
            rhxController->uploadCommandList(commandList, AbstractRHXController::AuxCmd3, 3);

            rhxController->run();
            while (rhxController->isRunning()) {
                qApp->processEvents();
            }
            std::deque<RHXDataBlock*> dataQueue;
            rhxController->readDataBlocks(numBlocks, dataQueue);

            for (int stream = 0; stream < rhxController->getNumEnabledDataStreams(); ++stream) {
                if (state->chipType[stream] != RHD2164MISOBChip) {
                    // Measure impedances, and pass measureComplexAmplitude() a file to write to. Only necessary for testing.
                    // Otherwise, just call measureComplexAmplitude() without this file for all capRange and channel values.
//                    if (channel == 0 && stream == 0) {
//                        if (capRange == 0) {
//                            measuredImpedance[stream][channel][capRange] =
//                                    measureComplexAmplitude(dataQueue, stream, channel, state->sampleRate->getNumericValue(),
//                                                            state->actualImpedanceFreq->getValue(), numPeriods, &cap0Stream);
//                        } else if (capRange == 1) {
//                            measuredImpedance[stream][channel][capRange] =
//                                    measureComplexAmplitude(dataQueue, stream, channel, state->sampleRate->getNumericValue(),
//                                                            state->actualImpedanceFreq->getValue(), numPeriods, &cap1Stream);
//                        } else {
//                            measuredImpedance[stream][channel][capRange] =
//                                    measureComplexAmplitude(dataQueue, stream, channel, state->sampleRate->getNumericValue(),
//                                                            state->actualImpedanceFreq->getValue(), numPeriods, &cap2Stream);
//                        }
//                    }

//                    else {
//                    measuredImpedance[stream][channel][capRange] =
//                            measureComplexAmplitude(dataQueue, stream, channel, state->sampleRate->getNumericValue(),
//                                                    state->actualImpedanceFreq->getValue(), numPeriods);
//                    }
                    measuredImpedance[stream][channel][capRange] =
                            measureComplexAmplitude(dataQueue, stream, channel, state->sampleRate->getNumericValue(),
                                                    state->actualImpedanceFreq->getValue(), numPeriods);
                }
            }
            while (!dataQueue.empty()) {
                delete dataQueue.back();
                dataQueue.pop_back();
            }

            // If an RHD2164 chip is plugged in, we have to set the Zcheck select register to channels 32-63
            // and repeat the previous steps.
            if (rhd2164ChipPresent) {
                chipRegisters.setZcheckChannel(channel + 32);  // Address channels 32-63.
                //commandSequenceLength =
                        //chipRegisters.createCommandListRHDRegisterConfig(commandList, false,
                                                                         //RHXDataBlock::samplesPerDataBlock(controllerType));
                chipRegisters.createCommandListRHDRegisterConfig(commandList, false,
                                                                 RHXDataBlock::samplesPerDataBlock(controllerType));
                // Upload version with no ADC calibration to AuxCmd3 RAM Bank 1.
                rhxController->uploadCommandList(commandList, AbstractRHXController::AuxCmd3, 3);

                rhxController->run();
                while (rhxController->isRunning()) {
                    qApp->processEvents();
                }
                rhxController->readDataBlocks(numBlocks, dataQueue);

                for (int stream = 0; stream < rhxController->getNumEnabledDataStreams(); ++stream) {
                    if (state->chipType[stream] == RHD2164MISOBChip) {
                        measuredImpedance[stream][channel][capRange] =
                                measureComplexAmplitude(dataQueue, stream, channel, state->sampleRate->getNumericValue(),
                                                        state->actualImpedanceFreq->getValue(), numPeriods);
                    }
                }
                while (!dataQueue.empty()) {
                    delete dataQueue.back();
                    dataQueue.pop_back();
                }
            }
        }
    }

    // Close data files used to examine data used in impedance measurement, only necessary for testing
//    cap0File.close();
//    cap1File.close();
//    cap2File.close();

    const double DacVoltageAmplitude = 128.0 * (1.225 / 256.0); // this assumes the DAC amplitude was set to 128

    double parasiticCapacitance;  // Estimate of on-chip parasitic capicitance, including effective amplifier input capacitance.
    if (controllerType == ControllerStimRecord) {
        parasiticCapacitance = 12.0e-12;  // 12 pF
    } else {
        parasiticCapacitance = 15.0e-12;  // 15 pF
    }

    double relativeFreq = state->actualImpedanceFreq->getValue() / state->sampleRate->getNumericValue();
    double saturationVoltage = approximateSaturationVoltage(state->actualImpedanceFreq->getValue(),
                                                            state->actualUpperBandwidth->getValue());

    for (int stream = 0; stream < rhxController->getNumEnabledDataStreams(); ++stream) {
        for (int channel = 0; channel < numChannelsPerStream; ++channel) {
            Channel* signalChannel = state->signalSources->getAmplifierChannel(stream, channel);
            if (signalChannel) {
                // Make sure chosen capacitor is below saturation voltage
                int bestAmplitudeIndex;
                if (measuredImpedance[stream][channel][2].magnitude < saturationVoltage) {
                    bestAmplitudeIndex = 2;
                } else if (measuredImpedance[stream][channel][1].magnitude < saturationVoltage) {
                    bestAmplitudeIndex = 1;
                } else {
                    bestAmplitudeIndex = 0;
                }

                // If C2 and C3 are too close, C3 is probably saturated. Ignore C3.
                double capRatio = measuredImpedance[stream][channel][1].magnitude / measuredImpedance[stream][channel][2].magnitude;
                if (capRatio > 0.2) {
                    if (bestAmplitudeIndex == 2) {
                        bestAmplitudeIndex = 1;
                    }
                }

                double cSeries = 0.0;
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

                // Calculate current amplitude produced by on-chip voltage DAC
                double current = TwoPi * state->actualImpedanceFreq->getValue() * DacVoltageAmplitude * cSeries;

                ComplexPolar impedance;

                // Calculate impedance magnitude from calculated current and measured voltage.
                impedance.magnitude = 1.0e-6 * (measuredImpedance[stream][channel][bestAmplitudeIndex].magnitude / current) *
                        (18.0 * relativeFreq * relativeFreq + 1.0);

                // Calculate impedance phase, with small correction factor accounting for the 3-command SPI pipeline delay.
                impedance.phase = measuredImpedance[stream][channel][bestAmplitudeIndex].phase + (360.0 * (3.0 / period));

                // Factor out on-chip parasitic capacitance from impedance measurement.
                impedance = factorOutParallelCapacitance(impedance, state->actualImpedanceFreq->getValue(), parasiticCapacitance);

                if (controllerType == ControllerStimRecord) {
                    // Multiply by a factor of 10%: empirical tests indicate that RHS chips usually underestimate impedance
                    // by about 10%
                    impedance.magnitude = 1.1 * impedance.magnitude;
                }

                signalChannel->setImpedance(impedance.magnitude, impedance.phase);
            }
        }
    }

    rhxController->setContinuousRunMode(false);
    rhxController->setMaxTimeStep(0);
    rhxController->flush();

    progress.setValue(progress.maximum());

    // Switch back to flatline
    rhxController->selectAuxCommandLength(AbstractRHXController::AuxCmd1, 0, RHXDataBlock::samplesPerDataBlock(controllerType) - 1);

    if (controllerType == ControllerRecordUSB2 || controllerType == ControllerRecordUSB3) {
        rhxController->selectAuxCommandBankAllPorts(AbstractRHXController::AuxCmd1, 0);

        bool enabled = state->manualFastSettleEnabled->getValue();
        rhxController->selectAuxCommandBankAllPorts(AbstractRHXController::AuxCmd3, enabled ? 2 : 1);

        // Re-enable external fast settling, if selected.
        rhxController->enableExternalFastSettle(state->externalFastSettleEnabled->getValue());

        // Re-enable auxiliary digital output control, if selected.
        rhxController->enableExternalDigOut(PortA, state->signalSources->portGroupByIndex(0)->auxDigOutEnabled->getValue());
        rhxController->enableExternalDigOut(PortB, state->signalSources->portGroupByIndex(1)->auxDigOutEnabled->getValue());
        rhxController->enableExternalDigOut(PortC, state->signalSources->portGroupByIndex(2)->auxDigOutEnabled->getValue());
        rhxController->enableExternalDigOut(PortD, state->signalSources->portGroupByIndex(3)->auxDigOutEnabled->getValue());
        if (state->numSPIPorts > 4) {
            rhxController->enableExternalDigOut(PortE, state->signalSources->portGroupByIndex(4)->auxDigOutEnabled->getValue());
            rhxController->enableExternalDigOut(PortF, state->signalSources->portGroupByIndex(5)->auxDigOutEnabled->getValue());
            rhxController->enableExternalDigOut(PortG, state->signalSources->portGroupByIndex(6)->auxDigOutEnabled->getValue());
            rhxController->enableExternalDigOut(PortH, state->signalSources->portGroupByIndex(7)->auxDigOutEnabled->getValue());
        }
    }

    // Re-enable DACs
    rhxController->enableDac(0, state->analogOut1Channel->getValueString().toLower() != "off");
    rhxController->enableDac(1, state->analogOut2Channel->getValueString().toLower() != "off");
    rhxController->enableDac(2, state->analogOut3Channel->getValueString().toLower() != "off");
    rhxController->enableDac(3, state->analogOut4Channel->getValueString().toLower() != "off");
    rhxController->enableDac(4, state->analogOut5Channel->getValueString().toLower() != "off");
    rhxController->enableDac(5, state->analogOut6Channel->getValueString().toLower() != "off");
    rhxController->enableDac(6, state->analogOut7Channel->getValueString().toLower() != "off");
    rhxController->enableDac(7, state->analogOut8Channel->getValueString().toLower() != "off");

    state->impedancesHaveBeenMeasured->setValue(true);
    state->displayLabelText->setValue("Impedance Magnitude");
    return true;
}

// Use a 2nd order Low Pass Filter to model the approximate voltage at which the amplifiers saturate
// which depends on the impedance frequency and the amplifier bandwidth.
double ImpedanceReader::approximateSaturationVoltage(double actualZFreq, double highCutoff)
{
    if (actualZFreq < 0.2 * highCutoff) {
        return 5000.0;
    } else {
        return 5000.0 * sqrt(1.0 / (1.0 + pow(3.3333 * actualZFreq / highCutoff, 4.0)));
    }
}

// Given a measured complex impedance that is the result of an electrode impedance in parallel
// with a parasitic capacitance (i.e., due to the amplifier input capacitance and other
// capacitances associated with the chip bondpads), this function factors out the effect of the
// parasitic capacitance to return the acutal electrode impedance.
ComplexPolar ImpedanceReader::factorOutParallelCapacitance(ComplexPolar impedance, double frequency,
                                                              double parasiticCapacitance)
{
    // First, convert from polar coordinates to rectangular coordinates.
    double measuredR = impedance.magnitude * cos(DegreesToRadians * impedance.phase);
    double measuredX = impedance.magnitude * sin(DegreesToRadians * impedance.phase);

    double capTerm = TwoPi * frequency * parasiticCapacitance;
    double xTerm = capTerm * (measuredR * measuredR + measuredX * measuredX);
    double denominator = capTerm * xTerm + 2.0 * capTerm * measuredX + 1.0;
    double trueR = measuredR / denominator;
    double trueX = (measuredX + xTerm) / denominator;

    // Now, convert from rectangular coordinates back to polar coordinates.
    ComplexPolar result;
    result.magnitude = sqrt(trueR * trueR + trueX * trueX);
    result.phase = RadiansToDegrees * atan2(trueX, trueR);
    return result;
}

ComplexPolar ImpedanceReader::measureComplexAmplitude(const std::deque<RHXDataBlock*> &dataQueue, int stream, int chipChannel,
                                                      double sampleRate, double frequency, int numPeriods, QDataStream *outStream) const
{
    int samplesPerDataBlock = RHXDataBlock::samplesPerDataBlock(state->getControllerTypeEnum());
    int numBlocks = (int) dataQueue.size();

    // Copy waveform data from data blocks.
    std::vector<double> waveform(samplesPerDataBlock * numBlocks);
    int index = 0;
    for (int block = 0; block < numBlocks; ++block) {
        for (int t = 0; t < samplesPerDataBlock; ++t) {
            waveform[index++] = 0.195 * (double)(dataQueue[block]->amplifierData(stream, chipChannel, t) - 32768);
            if (outStream) {
                *outStream << 0.195 * (double)(dataQueue[block]->amplifierData(stream, chipChannel, t) - 32768);
            }
        }
    }

    if (state->notchFreq->getValue().toLower() != "none") {
        double notchFreq = state->notchFreq->getNumericValue();
        applyNotchFilter(waveform, notchFreq, (double) NotchBandwidth, sampleRate);
    }

    int period = round(sampleRate / frequency);
    int startIndex = 0;
    int endIndex = startIndex + numPeriods * period - 1;

    // Move the measurement window to the end of the waveform to ignore start-up transient.
    while (endIndex < samplesPerDataBlock * numBlocks - period) {
        startIndex += period;
        endIndex += period;
    }

    return amplitudeOfFreqComponent(waveform, startIndex, endIndex, sampleRate, frequency);
}

void ImpedanceReader::applyNotchFilter(std::vector<double> &waveform, double fNotch, double bandwidth, double sampleRate) const
{
    double d = exp(-1.0 * Pi * bandwidth / sampleRate);
    double b = (1.0 + d * d) * cos(TwoPi * fNotch / sampleRate);
    double b0 = (1.0 + d * d) / 2.0;
    double b1 = -b;
    double b2 = b0;
    double a1 = b1;
    double a2 = d * d;
    int length = (int) waveform.size();

    double prevPrevIn = waveform[0];
    double prevIn = waveform[1];
    for (int t = 2; t < length; ++t) {
        double in = waveform[t];
        waveform[t] = b0 * in + b1 * prevIn + b2 * prevPrevIn - a1 * waveform[t - 1] - a2 * waveform[t - 2];  // Direct Form 1 implementation
        prevPrevIn = prevIn;
        prevIn = in;
    }
}

ComplexPolar ImpedanceReader::amplitudeOfFreqComponent(const std::vector<double> &waveform, int startIndex, int endIndex,
                                                       double sampleRate, double frequency)
{
    const double K = TwoPi * frequency / sampleRate;  // precalculate for speed

    // Perform correlation with sine and cosine waveforms.
    double meanI = 0.0;
    double meanQ = 0.0;
    for (int t = startIndex; t <= endIndex; ++t) {
        meanI += waveform[t] * cos(K * t);
        meanQ += waveform[t] * -1.0 * sin(K * t);
    }
    double length = (double)(endIndex - startIndex + 1);
    meanI /= length;
    meanQ /= length;

    double realComponent = 2.0 * meanI;
    double imagComponent = 2.0 * meanQ;

    ComplexPolar result;
    result.magnitude = sqrt(realComponent * realComponent + imagComponent * imagComponent);
    result.phase = RadiansToDegrees * atan2(imagComponent, realComponent);
    return result;
}

bool ImpedanceReader::saveImpedances()
{
    QString csvFileName = state->impedanceFilename->getFullFilename();
    if (csvFileName.right(4).toLower() != ".csv") {
        csvFileName += ".csv";
    }
    QFile csvFile(csvFileName);
    if (!csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&csvFile);

    out << "Channel Number,Channel Name,Port,Enabled,";
    out << "Impedance Magnitude at " << state->actualImpedanceFreq->getValue() << " Hz (ohms),";
    out << "Impedance Phase at " << state->actualImpedanceFreq->getValue() << " Hz (degrees),";
    out << "Series RC equivalent R (Ohms),";
    out << "Series RC equivalent C (Farads)" << EndOfLine;

    int numChannelsPerStream = RHXDataBlock::channelsPerStream(state->getControllerTypeEnum());
    for (int stream = 0; stream < rhxController->getNumEnabledDataStreams(); ++stream) {
        for (int channel = 0; channel < numChannelsPerStream; ++channel) {
            Channel* signalChannel = state->signalSources->getAmplifierChannel(stream, channel);
            if (signalChannel) {
                double equivalentR = signalChannel->getImpedanceMagnitude() *
                        cos(DegreesToRadians * signalChannel->getImpedancePhase());
                double equivalentC = 1.0 /
                        (TwoPi * state->actualImpedanceFreq->getValue() * signalChannel->getImpedanceMagnitude() *
                         -1.0 * sin(DegreesToRadians * signalChannel->getImpedancePhase()));

                out.setRealNumberNotation(QTextStream::ScientificNotation);
                out.setRealNumberPrecision(2);

                out << signalChannel->getNativeName() << ",";
                out << signalChannel->getCustomName() << ",";
                out << signalChannel->getGroupName() << ",";
                out << signalChannel->isEnabled() << ",";
                out << signalChannel->getImpedanceMagnitude() << ",";

                out.setRealNumberNotation(QTextStream::FixedNotation);
                out.setRealNumberPrecision(0);

                out << signalChannel->getImpedancePhase() << ",";

                out.setRealNumberNotation(QTextStream::ScientificNotation);
                out.setRealNumberPrecision(2);

                out << equivalentR << ",";
                out << equivalentC << EndOfLine;
            }
        }
    }

    csvFile.close();
    return true;
}

void ImpedanceReader::runDemoImpedanceMeasurement()
{
    ControllerType controllerType = state->getControllerTypeEnum();
    int numChannelsPerStream = RHXDataBlock::channelsPerStream(controllerType);

    // Create a progress bar to let user know how long this will take.
    int maxProgress = (controllerType == ControllerStimRecord) ? 50 : 98;
    QProgressDialog progress(QObject::tr("Measuring Electrode Impedances"), QString(), 0, maxProgress);
    progress.setWindowTitle(QObject::tr("Progress"));
    progress.setMinimumDuration(0);
    progress.setModal(true);
    progress.setValue(0);

    QElapsedTimer timer;
    timer.start();
    for (int i = 1; i < maxProgress; ++i) {
        progress.setValue(i);
        while (timer.nsecsElapsed() < qint64(50000000)) {
            qApp->processEvents();
        }
        timer.restart();
    }

    for (int channel = 0; channel < numChannelsPerStream; ++channel) {
        for (int stream = 0; stream < rhxController->getNumEnabledDataStreams(); ++stream) {
            Channel* signalChannel = state->signalSources->getAmplifierChannel(stream, channel);
            if (signalChannel) {
                signalChannel->setImpedance(100.0e3, -70.0);  // Dummy impedance values for demo mode.
            }
        }
    }
    progress.setValue(progress.maximum());

    state->impedancesHaveBeenMeasured->setValue(true);
    state->displayLabelText->setValue("Impedance Magnitude");
}

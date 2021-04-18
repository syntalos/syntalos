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

#include <QString>
#include <QFile>
#include <QDataStream>
#include <QTime>
#include <iostream>
#include <cmath>
#include "abstractrhxcontroller.h"
#include "savemanager.h"

using namespace std;

SaveManager::SaveManager(WaveformFifo* waveformFifo_, SystemState* state_) :
    waveformFifo(waveformFifo_),
    state(state_),
    signalSources(state_->signalSources)
{
    type = state->getControllerTypeEnum();
    timeStampOffset = 0;
    liveNotesFile = nullptr;
}

SaveManager::~SaveManager()
{
    if (liveNotesFile) {
        liveNotesFile->close();
        delete liveNotesFile;
    }
}

int64_t SaveManager::writeIntanFileHeader(SaveFile* saveFile)
{
    int64_t numBytesInitial = saveFile->getNumBytesWritten();

    if (type == ControllerStimRecordUSB2) {
        saveFile->writeUInt32(DataFileMagicNumberRHS);
    } else {
        saveFile->writeUInt32(DataFileMagicNumberRHD);
    }
    saveFile->writeInt16(DataFileMainVersionNumber);
    saveFile->writeInt16(DataFileSecondaryVersionNumber);

    saveFile->writeDouble(state->sampleRate->getNumericValue());
    saveFile->writeInt16(state->dspEnabled->getValue());
    saveFile->writeDouble(state->actualDspCutoffFreq->getValue());
    saveFile->writeDouble(state->actualLowerBandwidth->getValue());

    if (type == ControllerStimRecordUSB2) {
        saveFile->writeDouble(state->actualLowerSettleBandwidth->getValue());
    }
    saveFile->writeDouble(state->actualUpperBandwidth->getValue());

    saveFile->writeDouble(state->desiredDspCutoffFreq->getValue());
    saveFile->writeDouble(state->desiredLowerBandwidth->getValue());
    if (type == ControllerStimRecordUSB2) {
        saveFile->writeDouble(state->desiredLowerSettleBandwidth->getValue());
    }
    saveFile->writeDouble(state->desiredUpperBandwidth->getValue());

    saveFile->writeInt16(state->notchFreq->getIndex());

    saveFile->writeDouble(state->desiredImpedanceFreq->getValue());
    saveFile->writeDouble(state->actualImpedanceFreq->getValue());

    if (type == ControllerStimRecordUSB2) {
        saveFile->writeInt16(state->useFastSettle->getValue());
        saveFile->writeInt16(state->chargeRecoveryMode->getValue());

        saveFile->writeDouble(RHXRegisters::stimStepSizeToDouble(state->getStimStepSizeEnum()));
        saveFile->writeDouble(RHXRegisters::chargeRecoveryCurrentLimitToDouble(state->getChargeRecoveryCurrentLimitEnum()));
        saveFile->writeDouble(state->chargeRecoveryTargetVoltage->getValue());
    }

    saveFile->writeQString(state->note1->getValueString());
    saveFile->writeQString(state->note2->getValueString());
    saveFile->writeQString(state->note3->getValueString());

    if (type == ControllerStimRecordUSB2) {
        saveFile->writeInt16(state->saveDCAmplifierWaveforms->getValue());
    } else {
        saveFile->writeInt16(0);
    }

    saveFile->writeInt16(AbstractRHXController::boardMode(type));

    saveFile->writeQString(QString("n/a"));  // No good way to report global software reference in RHX code.

    saveFile->writeSignalSources(signalSources);

    return saveFile->getNumBytesWritten() - numBytesInitial;
}

void SaveManager::writeLiveNote(const QString& note, int64_t numSamplesRecorded)
{
    if (!liveNotesFile) {  // If live notes file has not yet been created, do so now.
        liveNotesFile = new SaveFile(liveNotesFileName, 128);
        if (!state->note1->getValueString().isEmpty()) {
            writeLiveNoteEntry(0, state->note1->getValueString());
        }
        if (!state->note2->getValueString().isEmpty()) {
            writeLiveNoteEntry(0, state->note2->getValueString());
        }
        if (!state->note3->getValueString().isEmpty()) {
            writeLiveNoteEntry(0, state->note3->getValueString());
        }
    }
    if (!liveNotesFile->isOpen()) {
        liveNotesFile->openForAppend();
    }
    writeLiveNoteEntry(numSamplesRecorded, note);
}

void SaveManager::writeLiveNoteEntry(uint64_t timestamp, const QString& note)
{
    if (liveNotesFile->isOpen()) { // Be very careful with live notes file so adding a note doesn't crash the software during
                                    // a recording session under any circumstance.
        QString timestampString = QString::number(timestamp);
        int timeInSeconds = round((double) timestamp / state->sampleRate->getNumericValue());
        QTime recordTime(0, 0);
        QString timeString = recordTime.addSecs(timeInSeconds).toString("HH:mm:ss");
        liveNotesFile->writeQStringAsAsciiText(timestampString + ", " + timeString + ", " + note + "\r\n");
    } else {
        cerr << "SaveManager::writeLiveNoteEntry: live notes file " << liveNotesFileName.toStdString() << " is not open.\n";
    }
}

bool SaveManager::setPosStimAmplitude(int stream, int channel, int amplitude)
{
    int index = saveList.getAmplifierIndexFromStreamChannel(stream, channel);
    if (index < 0) {
        return false;
    }
    amplitude = qBound(0, amplitude, 255);
    posStimAmplitudes[index] = (uint8_t) amplitude;
//    cout << "SaveManager::setPosStimAmplitude(" << index << ", " << amplitude << ")" << EndOfLine;
    return true;
}

bool SaveManager::setNegStimAmplitude(int stream, int channel, int amplitude)
{
    int index = saveList.getAmplifierIndexFromStreamChannel(stream, channel);
    if (index < 0) {
        return false;
    }
    amplitude = qBound(0, amplitude, 255);
    negStimAmplitudes[index] = (uint8_t) amplitude;
//    cout << "SaveManager::setNegStimAmplitude(" << index << ", " << amplitude << ")" << EndOfLine;
    return true;
}

QString SaveManager::getDateTimeStamp()
{
    QDateTime dateTime = QDateTime::currentDateTime();
    QString dateTimeStamp = "_" + dateTime.toString("yyMMdd") + "_" + dateTime.toString("HHmmss");
    return dateTimeStamp;
}

QString SaveManager::intanFileExtension() const
{
    if (type == ControllerStimRecordUSB2) return QString(".rhs");
    else return QString(".rhd");
}

uint16_t SaveManager::convertAmplifierValue(float voltage) const  // voltage in microvolts
{
    int result = ((int) round(voltage / 0.195F)) + 32768;
    if (result < 0) result = 0;
    else if (result > 65535) result = 65535;
    return (uint16_t) result;
}

void SaveManager::convertAmplifierValue(uint16_t* dest, const float* voltage, int numSamples) const  // voltage in microvolts
{
    int result;
    uint16_t* pWrite = dest;
    for (int i = 0; i < numSamples; ++i) {
        result = ((int) round(voltage[i] / 0.195F)) + 32768;
        if (result < 0) result = 0;
        else if (result > 65535) result = 65535;
        *pWrite = result;
        ++pWrite;
    }
}

uint16_t SaveManager::convertDcAmplifierValue(float voltage) const  // voltage in volts
{
    int result = ((int) round(voltage / -0.01923F)) + 512;
    if (result < 0) result = 0;
    else if (result > 65535) result = 65535;
    return (uint16_t) result;
}

void SaveManager::convertDcAmplifierValue(uint16_t* dest, const float* voltage, int numSamples) const  // voltage in volts
{
    int result;
    uint16_t* pWrite = dest;
    for (int i = 0; i < numSamples; ++i) {
        result = ((int) round(voltage[i] / -0.01923F)) + 512;
        if (result < 0) result = 0;
        else if (result > 65535) result = 65535;
        *pWrite = result;
        ++pWrite;
    }
}

uint16_t SaveManager::convertAuxInputValue(float voltage) const   // voltage in volts
{
    int result = ((int) round(voltage / 0.0000374F));
    if (result < 0) result = 0;
    else if (result > 65535) result = 65535;
    return (uint16_t) result;
}

void SaveManager::convertAuxInputValue(uint16_t* dest, const float* voltage, int numSamples) const  // voltage in volts
{
    int result;
    uint16_t* pWrite = dest;
    for (int i = 0; i < numSamples; ++i) {
        result = ((int) round(voltage[i] / 0.0000374F));
        if (result < 0) result = 0;
        else if (result > 65535) result = 65535;
        *pWrite = result;
        ++pWrite;
    }
}

// Special function to combine amplifier data and auxiliary input data (converting from voltage in volts)
// into single uint16 array.
void SaveManager::mergeAmpAndAuxValues(uint16_t* dest, const uint16_t* ampSigned, const float* auxVoltage, int numSamples,
                                       int numAmpChannels, int numAuxChannels) const
{
    int result;
    uint16_t* pWrite = dest;
    for (int i = 0; i < numSamples; ++i) {
        for (int j = 0; j < numAmpChannels; ++j) {
            *pWrite = (*ampSigned);
            ++pWrite;
            ++ampSigned;
        }
        for (int j = 0; j < numAuxChannels; ++j) {
            result = ((int) round((*auxVoltage) / 0.0000374F));
            if (result < 0) result = 0;
            else if (result > 65535) result = 65535;
            *pWrite = result;
            ++pWrite;
            ++auxVoltage;
        }
    }
}

uint16_t SaveManager::convertSupplyVoltageValue(float voltage) const   // voltage in volts
{
    int result = ((int) round(voltage / 0.0000748F));
    if (result < 0) result = 0;
    else if (result > 65535) result = 65535;
    return (uint16_t) result;
}

void SaveManager::convertSupplyVoltageValue(uint16_t* dest, const float* voltage, int numSamples) const  // voltage in volts
{
    int result;
    uint16_t* pWrite = dest;
    for (int i = 0; i < numSamples; ++i) {
        result = ((int) round(voltage[i] / 0.0000748F));
        if (result < 0) result = 0;
        else if (result > 65535) result = 65535;
        *pWrite = result;
        ++pWrite;
    }
}

uint16_t SaveManager::convertBoardAdcValue(float voltage) const   // voltage in volts
{
    int result;
    if (type == ControllerRecordUSB2) {
        result = ((int) round(voltage / 50.354e-6F));
    } else {
        result = ((int) round(voltage / 312.5e-6F)) + 32768;
    }
    if (result < 0) result = 0;
    else if (result > 65535) result = 65535;
    return (uint16_t) result;
}

void SaveManager::convertBoardAdcValue(uint16_t* dest, const float* voltage, int numSamples) const  // voltage in volts
{
    int result;
    uint16_t* pWrite = dest;
    float scale = 312.5e-6F;
    int offset = 32768;
    if (type == ControllerRecordUSB2) {
        scale = 50.354e-6F;
        offset = 0;
    }
    for (int i = 0; i < numSamples; ++i) {
        result = ((int) round(voltage[i] / scale)) + offset;
        if (result < 0) result = 0;
        else if (result > 65535) result = 65535;
        *pWrite = (uint16_t) result;
        ++pWrite;
    }
}

// ControllerStimRecordUSB2 only
uint16_t SaveManager::convertBoardDacValue(float voltage) const   // voltage in volts
{
    int result = ((int) round(voltage / 312.5e-6F)) + 32768;
    if (result < 0) result = 0;
    else if (result > 65535) result = 65535;
    return (uint16_t) result;
}

// ControllerStimRecordUSB2 only
void SaveManager::convertBoardDacValue(uint16_t* dest, const float* voltage, int numSamples) const  // voltage in volts
{
    int result;
    uint16_t* pWrite = dest;
    for (int i = 0; i < numSamples; ++i) {
        result = ((int) round(voltage[i] / 312.5e-6F)) + 32768;
        if (result < 0) result = 0;
        else if (result > 65535) result = 65535;
        *pWrite = (uint16_t) result;
        ++pWrite;
    }
}

void SaveManager::getAllWaveformPointers()
{
    saveList = signalSources->getSaveSignalList();
//    saveList.print();

    amplifierGPUWaveform.resize(saveList.amplifier.size());
    amplifierLowpassGPUWaveform.resize(saveList.amplifier.size());
    amplifierHighpassGPUWaveform.resize(saveList.amplifier.size());
    spikeWaveform.resize(saveList.amplifier.size());
    for (int i = 0; i < (int) saveList.amplifier.size(); ++i) {
        amplifierGPUWaveform[i] = waveformFifo->getGpuWaveformAddress(saveList.amplifier[i] + "|WIDE");
        amplifierLowpassGPUWaveform[i] = waveformFifo->getGpuWaveformAddress(saveList.amplifier[i] + "|LOW");
        amplifierHighpassGPUWaveform[i] = waveformFifo->getGpuWaveformAddress(saveList.amplifier[i] + "|HIGH");
        spikeWaveform[i] = waveformFifo->getDigitalWaveformPointer(saveList.amplifier[i] + "|SPK");
    }

    if (type == ControllerStimRecordUSB2) {
        dcAmplifierWaveform.resize(saveList.amplifier.size());
        stimFlagsWaveform.resize(saveList.amplifier.size());
        posStimAmplitudes.resize(saveList.amplifier.size());
        fill(posStimAmplitudes.begin(), posStimAmplitudes.end(), 0);
        negStimAmplitudes.resize(saveList.amplifier.size());
        fill(negStimAmplitudes.begin(), negStimAmplitudes.end(), 0);
        for (int i = 0; i < (int) saveList.amplifier.size(); ++i) {
            dcAmplifierWaveform[i] = waveformFifo->getAnalogWaveformPointer(saveList.amplifier[i] + "|DC");
            stimFlagsWaveform[i] = waveformFifo->getDigitalWaveformPointer(saveList.amplifier[i] + "|STIM");
            Channel* channel = signalSources->channelByName(saveList.amplifier[i]);
            double stepSizeuA = RHXRegisters::stimStepSizeToDouble(state->getStimStepSizeEnum()) * 1e6;
            if (channel) {
                int firstPhaseAmplitude = round(channel->stimParameters->firstPhaseAmplitude->getValue() / stepSizeuA);
                firstPhaseAmplitude = qBound(0, firstPhaseAmplitude, 255);
                int secondPhaseAmplitude = round(channel->stimParameters->secondPhaseAmplitude->getValue() / stepSizeuA);
                secondPhaseAmplitude = qBound(0, secondPhaseAmplitude, 255);
                if (channel->stimParameters->stimPolarity->getIndex() == NegativeFirst) {
                    negStimAmplitudes[i] = (uint8_t) firstPhaseAmplitude;
                    posStimAmplitudes[i] = (uint8_t) secondPhaseAmplitude;
                } else {
                    posStimAmplitudes[i] = (uint8_t) firstPhaseAmplitude;
                    negStimAmplitudes[i] = (uint8_t) secondPhaseAmplitude;
                }
            }
        }
    }

    auxInputWaveform.resize(saveList.auxInput.size());
    for (int i = 0; i < (int) saveList.auxInput.size(); ++i) {
        auxInputWaveform[i] = waveformFifo->getAnalogWaveformPointer(saveList.auxInput[i]);
    }

    supplyVoltageWaveform.resize(saveList.supplyVoltage.size());
    for (int i = 0; i < (int) saveList.supplyVoltage.size(); ++i) {
        supplyVoltageWaveform[i] = waveformFifo->getAnalogWaveformPointer(saveList.supplyVoltage[i]);
    }

    boardAdcWaveform.resize(saveList.boardAdc.size());
    for (int i = 0; i < (int) saveList.boardAdc.size(); ++i) {
        boardAdcWaveform[i] = waveformFifo->getAnalogWaveformPointer(saveList.boardAdc[i]);
    }

    boardDacWaveform.resize(saveList.boardDac.size());
    for (int i = 0; i < (int) saveList.boardDac.size(); ++i) {
        boardDacWaveform[i] = waveformFifo->getAnalogWaveformPointer(saveList.boardDac[i]);
    }

    boardDigitalInWaveform = waveformFifo->getDigitalWaveformPointer("DIGITAL-IN-WORD");
    boardDigitalOutWaveform = waveformFifo->getDigitalWaveformPointer("DIGITAL-OUT-WORD");
}

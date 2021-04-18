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
#ifndef SAVEMANAGER_H
#define SAVEMANAGER_H

#include "waveformfifo.h"
#include "systemstate.h"
#include "signalsources.h"
#include "rhxdatablock.h"
#include "savefile.h"

class SaveManager
{
public:
    SaveManager(WaveformFifo* waveformFifo_, SystemState* state_);
    virtual ~SaveManager();

    virtual bool openAllSaveFiles() = 0;
    virtual int64_t writeToSaveFiles(int numSamples, int timeIndex = 0) = 0;
    virtual void closeAllSaveFiles() = 0;
    virtual bool mustSaveCompleteDataBlocks() const { return false; }
    virtual int maxSamplesInFile() const { return 0; }  // returning zero disables the maximum samples per file constraint
    virtual double bytesPerMinute() const = 0;

    inline void setTimeStampOffset(uint32_t offset) { timeStampOffset = (int) offset; }
    int64_t writeIntanFileHeader(SaveFile* saveFile);   // Returns number of bytes written

    QString saveFileDateTimeStamp() const { return dateTimeStamp; }
    void writeLiveNote(const QString& note, int64_t numSamplesRecorded);

    bool setPosStimAmplitude(int stream, int channel, int amplitude);
    bool setNegStimAmplitude(int stream, int channel, int amplitude);

protected:
    WaveformFifo* waveformFifo;
    SystemState* state;
    SignalSources* signalSources;
    ControllerType type;
    int timeStampOffset;

    SignalList saveList;
    vector<GpuWaveformAddress> amplifierGPUWaveform;
    vector<GpuWaveformAddress> amplifierLowpassGPUWaveform;
    vector<GpuWaveformAddress> amplifierHighpassGPUWaveform;
    vector<float*> dcAmplifierWaveform;
    vector<uint16_t*> spikeWaveform;
    vector<uint16_t*> stimFlagsWaveform;
    vector<uint8_t> posStimAmplitudes;
    vector<uint8_t> negStimAmplitudes;
    vector<float*> auxInputWaveform;
    vector<float*> supplyVoltageWaveform;
    vector<float*> boardAdcWaveform;
    vector<float*> boardDacWaveform;
    uint16_t* boardDigitalInWaveform;
    uint16_t* boardDigitalOutWaveform;

    QString dateTimeStamp;
    SaveFile* liveNotesFile;
    QString liveNotesFileName;

    static QString getDateTimeStamp();
    void getAllWaveformPointers();
    QString intanFileExtension() const;

    uint16_t convertAmplifierValue(float voltage) const;
    void convertAmplifierValue(uint16_t* dest, const float* voltage, int numSamples) const;
    uint16_t convertDcAmplifierValue(float voltage) const;
    void convertDcAmplifierValue(uint16_t* dest, const float* voltage, int numSamples) const;
    uint16_t convertAuxInputValue(float voltage) const;
    void convertAuxInputValue(uint16_t* dest, const float* voltage, int numSamples) const;
    void mergeAmpAndAuxValues(uint16_t* dest, const uint16_t* ampSigned, const float* auxVoltage, int numSamples,
                              int numAmpChannels, int numAuxChannels) const;
    uint16_t convertSupplyVoltageValue(float voltage) const;
    void convertSupplyVoltageValue(uint16_t* dest, const float* voltage, int numSamples) const;
    uint16_t convertBoardAdcValue(float voltage) const;
    void convertBoardAdcValue(uint16_t* dest, const float* voltage, int numSamples) const;
    uint16_t convertBoardDacValue(float voltage) const;
    void convertBoardDacValue(uint16_t* dest, const float* voltage, int numSamples) const;

private:
    void writeLiveNoteEntry(uint64_t timestamp, const QString& note);
};

#endif // SAVEMANAGER_H

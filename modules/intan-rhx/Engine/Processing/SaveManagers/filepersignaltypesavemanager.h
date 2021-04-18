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

#ifndef FILEPERSIGNALTYPESAVEMANAGER_H
#define FILEPERSIGNALTYPESAVEMANAGER_H

#include "waveformfifo.h"
#include "systemstate.h"
#include "savemanager.h"

// One file per signal type file format
class FilePerSignalTypeSaveManager : public SaveManager
{
public:
    FilePerSignalTypeSaveManager(WaveformFifo* waveformFifo_, SystemState* state_);
    ~FilePerSignalTypeSaveManager();

    bool openAllSaveFiles() override;
    int64_t writeToSaveFiles(int numSamples, int timeIndex = 0) override;
    void closeAllSaveFiles() override;
    double bytesPerMinute() const override;

private:
    SaveFile* infoFile;
    SaveFile* timeStampFile;
    SaveFile* amplifierFile;
    SaveFile* lowpassAmplifierFile;
    SaveFile* highpassAmplifierFile;
    SaveFile* spikeFile;
    SaveFile* auxInputFile;
    SaveFile* supplyVoltageFile;
    SaveFile* dcAmplifierFile;
    SaveFile* stimFile;
    SaveFile* analogInputFile;
    SaveFile* analogOutputFile;
    SaveFile* digitalInputFile;
    SaveFile* digitalOutputFile;

    bool saveAuxInsWithAmps;
    bool saveSpikeSnapshot;
    int samplesPreDetect;
    int samplesPostDetect;
};

#endif // FILEPERSIGNALTYPESAVEMANAGER_H

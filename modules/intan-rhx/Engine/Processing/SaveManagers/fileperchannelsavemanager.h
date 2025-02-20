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

#ifndef FILEPERCHANNELSAVEMANAGER_H
#define FILEPERCHANNELSAVEMANAGER_H

#include "waveformfifo.h"
#include "systemstate.h"
#include "savemanager.h"

// One file per channel file format
class FilePerChannelSaveManager : public SaveManager
{
public:
    FilePerChannelSaveManager(WaveformFifo* waveformFifo_, SystemState* state_);
    ~FilePerChannelSaveManager();

    bool openAllSaveFiles() override;
    int64_t writeToSaveFiles(int numSamples, int timeIndex = 0) override;
    void closeAllSaveFiles() override;
    double bytesPerMinute() const override;

private:
    SaveFile* infoFile;
    SaveFile* timeStampFile;
    std::vector<SaveFile*> amplifierFiles;
    std::vector<SaveFile*> lowpassAmplifierFiles;
    std::vector<SaveFile*> highpassAmplifierFiles;
    std::vector<SaveFile*> spikeFiles;
    std::vector<SaveFile*> auxInputFiles;
    std::vector<SaveFile*> supplyVoltageFiles;
    std::vector<SaveFile*> dcAmplifierFiles;
    std::vector<SaveFile*> stimFiles;
    std::vector<SaveFile*> analogInputFiles;
    std::vector<SaveFile*> analogOutputFiles;
    std::vector<SaveFile*> digitalInputFiles;
    std::vector<int> digitalInputFileIndices;
    std::vector<SaveFile*> digitalOutputFiles;
    std::vector<int> digitalOutputFileIndices;

    bool saveSpikeSnapshot;
    int samplesPreDetect;
    int samplesPostDetect;

    int *spikeCounter;

    int *mostRecentSpikeTimestamp;
    int tenthOfSecondTimestamps;
    int *lastForceFlushTimestamp;
};

#endif // FILEPERCHANNELSAVEMANAGER_H

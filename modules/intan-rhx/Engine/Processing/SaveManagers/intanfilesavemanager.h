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

#ifndef INTANFILESAVEMANAGER_H
#define INTANFILESAVEMANAGER_H

#include "waveformfifo.h"
#include "systemstate.h"
#include "savemanager.h"

// Intan save file format (*.rhd, *.rhs)
class IntanFileSaveManager : public SaveManager
{
public:
    IntanFileSaveManager(WaveformFifo* waveformFifo_, SystemState* state_);
    ~IntanFileSaveManager();

    bool openAllSaveFiles() override;
    int64_t writeToSaveFiles(int numSamples, int timeIndex = 0) override;
    void closeAllSaveFiles() override;
    bool mustSaveCompleteDataBlocks() const override { return true; }
    int maxSamplesInFile() const override;
    double bytesPerMinute() const override;

private:
    SaveFile* saveFile;

    QString subdirName;
};

#endif // INTANFILESAVEMANAGER_H

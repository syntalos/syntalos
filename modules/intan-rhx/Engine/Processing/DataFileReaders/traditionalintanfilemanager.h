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

#ifndef TRADITIONALINTANFILEMANAGER_H
#define TRADITIONALINTANFILEMANAGER_H

#include <QFile>
#include <QString>
#include <vector>
#include "datafilemanager.h"
#include "datafile.h"

using namespace std;

class TraditionalIntanFileManager : public DataFileManager
{
public:
    TraditionalIntanFileManager(const QString& fileName_, IntanHeaderInfo* info_, bool &canReadFile, QString& report,
                                DataFileReader* parent);
    ~TraditionalIntanFileManager();

    int64_t jumpToTimeStamp(int64_t target) override;
    void loadDataFrame() override;
    void loadNextDataBlock();
    QFile* openLiveNotes();

    QString currentFileName() const override;

    struct consecutiveFile {
        QString fileName;
        int64_t numSamplesInFile;
    };

private:
    DataFile* dataFile;
    vector<consecutiveFile> consecutiveFiles;
    int consecutiveFileIndex;
    bool atEndOfCurrentFile;
    int samplesPerDataBlock;
    int positionInDataBlock;

    //  Buffers for loading entire data block into memory.
    vector<int32_t> timeStampBuffer;
    vector<uint16_t> amplifierDataBuffer;
    vector<uint16_t> dcAmplifierDataBuffer;
    vector<uint16_t> stimDataBuffer;
    vector<uint16_t> auxInputDataBuffer;
    vector<uint16_t> supplyVoltageDataBuffer;
    vector<uint16_t> analogInDataBuffer;
    vector<uint16_t> analogOutDataBuffer;
    vector<uint16_t> digitalInDataBuffer;
    vector<uint16_t> digitalOutDataBuffer;
    vector<int16_t> tempSensorBuffer;
};

#endif // TRADITIONALINTANFILEMANAGER_H

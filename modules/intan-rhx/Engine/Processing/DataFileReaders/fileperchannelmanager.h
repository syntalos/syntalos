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

#ifndef FILEPERCHANNELMANAGER_H
#define FILEPERCHANNELMANAGER_H

#include <QFile>
#include <QString>
#include <vector>
#include "datafilemanager.h"
#include "datafile.h"

class SystemState;


class FilePerChannelManager : public DataFileManager
{
public:
    FilePerChannelManager(const QString& fileName_, IntanHeaderInfo* info_, bool& canReadFile, QString& report,
                          DataFileReader* parent);
    ~FilePerChannelManager();

    long readDataBlocksRaw(int numBlocks, uint8_t* buffer) override;
    int64_t getLastTimeStamp() override;
    int64_t jumpToTimeStamp(int64_t target) override;
    void loadDataFrame() override;
    QFile* openLiveNotes();
    int64_t blocksPresent() override;

private:
    DataFile* timeFile;
    std::vector<std::vector<DataFile*> > amplifierFiles;
    std::vector<std::vector<DataFile*> > dcAmplifierFiles;
    std::vector<std::vector<DataFile*> > stimFiles;
    std::vector<std::vector<DataFile*> > auxInputFiles;
    std::vector<DataFile*> supplyVoltageFiles;
    std::vector<DataFile*> analogInFiles;
    std::vector<DataFile*> analogOutFiles;
    std::vector<DataFile*> digitalInFiles;
    std::vector<DataFile*> digitalOutFiles;

    void updateEndOfData();
};

#endif // FILEPERCHANNELMANAGER_H

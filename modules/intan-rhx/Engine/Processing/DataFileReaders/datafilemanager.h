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

#ifndef DATAFILEMANAGER_H
#define DATAFILEMANAGER_H

#include <QString>
#include <vector>
#include <map>

using namespace std;

struct IntanHeaderInfo;
class DataFileReader;

class DataFileManager
{
public:
    DataFileManager(const QString& fileName_, IntanHeaderInfo* info_, DataFileReader* parent);
    virtual ~DataFileManager();

    int64_t getFirstTimeStamp() const { return firstTimeStamp; }
    int64_t getLastTimeStamp() const { return lastTimeStamp; }
    int64_t getCurrentTimeStamp() const { return readIndex + firstTimeStamp; }
    int64_t getTotalNumSamples() const { return totalNumSamples; }

    QString timeString(int64_t samples);

    bool liveNotesLoaded() const { return !liveNotes.empty(); }
    QString getLastLiveNote();

    long readDataBlocksRaw(int numBlocks, uint8_t* buffer);
    virtual int64_t jumpToTimeStamp(int64_t target) = 0;
    virtual void loadDataFrame() = 0;
    void readLiveNotes(QFile* liveNotesFile);

    virtual QString currentFileName() const { return fileName; }

    struct StimData {
        StimData() : amplitude(0), stimOn(0), stimPol(0), ampSettle(0), chargeRecov(0), complianceLimit(0) {}
        uint16_t amplitude;
        uint16_t stimOn;
        uint16_t stimPol;
        uint16_t ampSettle;
        uint16_t chargeRecov;
        uint16_t complianceLimit;
        void clear() { amplitude = 0; stimOn = 0; stimPol = 0; ampSettle = 0; chargeRecov = 0; complianceLimit = 0; }
    };

protected:
    QString fileName;
    IntanHeaderInfo* info;
    DataFileReader* dataFileReader;

    vector<vector<bool> > amplifierWasSaved;
    vector<vector<bool> > dcAmplifierWasSaved;
    vector<vector<bool> > stimWasSaved;
    vector<vector<bool> > auxInputWasSaved;
    vector<bool> supplyVoltageWasSaved;
    vector<bool> analogInWasSaved;
    vector<bool> analogOutWasSaved;
    vector<bool> digitalInWasSaved;
    vector<bool> digitalOutWasSaved;

    int64_t totalNumSamples;
    int64_t readIndex;
    int64_t firstTimeStamp;
    int64_t lastTimeStamp;

    // Single data frame
    int32_t timeStamp;
    vector<vector<uint16_t> > amplifierData;
    vector<vector<uint16_t> > dcAmplifierData;
    vector<vector<StimData> > stimData;
    vector<vector<bool> > posStimAmplitudeFound;
    vector<vector<bool> > negStimAmplitudeFound;
    vector<vector<uint16_t> > auxInputData;
    vector<uint16_t> supplyVoltageData;
    vector<uint16_t> analogInData;
    vector<uint16_t> analogOutData;
    uint16_t digitalInData;
    uint16_t digitalOutData;

    // Live notes
    map<string, string> liveNotes;
    QString lastLiveNote;
};

#endif // DATAFILEMANAGER_H

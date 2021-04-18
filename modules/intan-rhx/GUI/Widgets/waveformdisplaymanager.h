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

#ifndef WAVEFORMDISPLAYMANAGER_H
#define WAVEFORMDISPLAYMANAGER_H

#include <map>
#include <string>
#include "minmax.h"
#include "waveformfifo.h"
#include "systemstate.h"

using namespace std;

const float ScaleFactorY = 10.0F;

enum YScaleType {
    WidebandYScale,
    LowpassYScale,
    HighpassYScale,
    DCYScale,
    AuxInputYScale,
    SupplyVoltageYScale,
    AnalogIOYScale,
    DigitalIOYScale,
    RasterYScale,
    UnknownYScale
};

struct YScaleUsed
{
public:
    YScaleUsed() :
        widepassYScaleUsed(false),
        lowpassYScaleUsed(false),
        highpassYScaleUsed(false),
        auxInputYScaleUsed(false),
        dcYScaleUsed(false),
        analogIOYScaleUsed(false)
    {}

    bool widepassYScaleUsed;
    bool lowpassYScaleUsed;
    bool highpassYScaleUsed;
    bool auxInputYScaleUsed;
    bool dcYScaleUsed;
    bool analogIOYScaleUsed;
};

struct WaveformDisplayDataStore
{
public:
    WaveformDisplayDataStore() :
        hasStimFlags(false),
        isRaster(false),
        yScaleType(UnknownYScale)
    {}

    // Internal state
    bool hasStimFlags;
    bool isRaster;
    YScaleType yScaleType;
    bool hasAlreadyLoaded;
    bool isOutOfDate;

    // Data stores: raw y coordinates in sequence
    vector<MinMax<float> > yMinMaxData;
    vector<float> yData;
    vector<uint16_t> stimFlags;
    vector<uint16_t> rasterData;

    // Data stores: screen coordinates
    vector<QLineF> verticalLines;
    vector<QPointF> points;
};

class WaveformDisplayManager
{
public:
    WaveformDisplayManager(SystemState* state_, int maxWidthInPixels_, int numRefreshZones_);
    ~WaveformDisplayManager();

    bool addWaveform(const QString& waveName, bool isStim = false, bool isRaster = false);  // Return false if waveName is already in waveform list.
    bool removeWaveform(const QString& waveName);  // Returns false if waveName is not in waveform list.
    inline void removeAllWaveforms() { data.clear(); }

    inline void setMaxWidthInPixels(int maxWidthInPixels_) { maxWidthInPixels = maxWidthInPixels_; calculateParameters(); }
    inline void setTScaleInMsec(int tScaleInMsec_, int numRefreshZones_ = 1) { tScaleInMsec = tScaleInMsec_;
                                                                               numRefreshZones = numRefreshZones_;
                                                                               calculateParameters(); }
    void resetAll();

    void prepForLoadingNewData();
    void prepForLoadingOldData(int startTime);
    void loadNewData(const WaveformFifo* waveformFifo, const QString& waveName) const;
    void loadOldData(const WaveformFifo* waveformFifo, const QString& waveName, int startTime) const;
    YScaleUsed finishLoading();

    float getYScaleFactor(YScaleType yScaleType) const;
    float getYScaleFactor(const QString& waveName) const;

    void draw(QPainter &painter, const QString& waveName, QPoint position, QColor color);
    void drawDivider(QPainter &painter, int yPos, int xStart, int xEnd);

    inline int getWidthInPixels() const { return widthInPixels; }
    int getRefreshXPosition() const;
    int getValidDataXPosition() const;

    inline int getSamplesPerRefresh() const { return samplesPerZone; }
    inline int getSamplesPerRefresh(int tScaleInMsec_, int numRefreshZones_) const
        { return round(sampleRate * ((double)tScaleInMsec_ / 1000.0) / (double)numRefreshZones_); }

    inline int getSamplesPerFullRefresh() const { return samplesPerZone * numRefreshZones; }
//    inline int getSamplesPerFullRefresh(int tScaleInMsec_, int numRefreshZones_) const
//        { int samplesPerZone_ = round(sampleRate * ((double)tScaleInMsec_ / 1000.0) / (double)numRefreshZones_);
//          return samplesPerZone_* numRefreshZones_; }

private:
    SystemState *state;

    // Input parameters
    int maxWidthInPixels;
    int numRefreshZones;
    double sampleRate;
    int tScaleInMsec;

    // Calculated display data parameters
    int zoneWidthInPixels;
    int widthInPixels;
    int samplesPerZone;
    float pixelsPerSample;
    bool useVerticalLines;
    int length;
    int zoneLength;

    // State while running
    bool oldDataPresent;
    bool sweepFirstTime;
    int validDataIndex;

    // Waveform data mapped to waveform name
    map<string, WaveformDisplayDataStore*> data;

    const QColor StimColor = QColor(255, 155, 155);
    const QColor ComplianceLimitColor = QColor(255, 0, 0);
    const QColor AmpSettleColor = QColor(255, 255, 215);
    const QColor ChargeRecovColor = QColor(215, 255, 215);
    QColor stimFlagsColor(uint16_t stimFlags, QColor backgroundColor) const;
    QColor supplyVoltageColor(MinMax<float> yMinMax) const;

    void calculateParameters();
    void loadDataSegment(const WaveformFifo* waveformFifo, const QString& waveName,  WaveformDisplayDataStore* ds,
                         int displayStartPos, int displayEndPos, int startTime) const;
    void reset(WaveformDisplayDataStore* ds);
};

#endif // WAVEFORMDISPLAYMANAGER_H

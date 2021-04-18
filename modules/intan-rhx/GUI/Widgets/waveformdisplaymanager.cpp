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

#include <QPainter>
#include "waveformdisplaymanager.h"

WaveformDisplayManager::WaveformDisplayManager(SystemState* state_, int maxWidthInPixels_, int numRefreshZones_) :
    state(state_),
    maxWidthInPixels(maxWidthInPixels_),
    numRefreshZones(numRefreshZones_)
{
    state->writeToLog("Beginning of WaveformDisplayManager ctor");
    sampleRate = state->sampleRate->getNumericValue();
    tScaleInMsec = (int) state->tScale->getNumericValue();

    oldDataPresent = false;
    sweepFirstTime = true;
    validDataIndex = 0;

    state->writeToLog("About to call calculateParameters()");
    calculateParameters();
    state->writeToLog("Completed calculateParameters(). End of ctor");
}

WaveformDisplayManager::~WaveformDisplayManager()
{
    if (!data.empty()) {
        map<string, WaveformDisplayDataStore*>::const_iterator it = data.begin();
        while (it != data.end()) {
            delete it->second;
            ++it;
        }
    }
}

void WaveformDisplayManager::calculateParameters()
{
    state->writeToLog("Beginning of calculateParameters()");
    zoneWidthInPixels = floor((double)maxWidthInPixels / (double)numRefreshZones);
    state->writeToLog("zoneWidthInPixels: " + QString::number(zoneWidthInPixels));
    widthInPixels = numRefreshZones * zoneWidthInPixels;
    state->writeToLog("widthInPixels: " + QString::number(widthInPixels));
    samplesPerZone = round(sampleRate * ((double)tScaleInMsec / 1000.0) / (double)numRefreshZones);
    state->writeToLog("samplesPerZone: " + QString::number(samplesPerZone));
    pixelsPerSample = (float)zoneWidthInPixels / (float)samplesPerZone;
    state->writeToLog("pixelsPerSample: " + QString::number(pixelsPerSample));
    useVerticalLines = pixelsPerSample < 1.0F;
    length = useVerticalLines ? widthInPixels : (samplesPerZone * numRefreshZones);
    state->writeToLog("length: " + QString::number(length));
    zoneLength = length / numRefreshZones;
    state->writeToLog("zoneLength: " + QString::number(zoneLength));
    resetAll();
    state->writeToLog("Completed resetAll(). End of calculateParameters()");

//    cout << EndOfLine;
//    cout << "WaveformDisplayManager::calculateParameters:" << EndOfLine;
//    cout << "   zone width in pixels = " << zoneWidthInPixels << EndOfLine;
//    cout << "   width in pixels = " << widthInPixels << EndOfLine;
//    cout << "   samples per zone = " << samplesPerZone << EndOfLine;
//    cout << "   pixels per sample = " << pixelsPerSample << EndOfLine;
//    cout << "   use vertical lines = " << useVerticalLines << EndOfLine;
//    cout << "   length = " << length << EndOfLine;
//    cout << EndOfLine;
}

bool WaveformDisplayManager::addWaveform(const QString& waveName, bool isStim, bool isRaster)
{
    string name = waveName.toStdString();
    if (data.find(name) != data.end()) return false;  // No repeats!  Do not read from the Waveform FIFO twice!

    WaveformDisplayDataStore* ds = new WaveformDisplayDataStore;
    ds->isRaster = isRaster;
    ds->hasStimFlags = isStim && !isRaster;
    QString filterText = waveName.section('|', 1, 1);
    if (filterText == "WIDE") ds->yScaleType = WidebandYScale;
    else if (filterText == "LOW") ds->yScaleType = LowpassYScale;
    else if (filterText == "HIGH") ds->yScaleType = HighpassYScale;
    else if (filterText == "SPK") ds->yScaleType = RasterYScale;
    else if (filterText == "DC") ds->yScaleType = DCYScale;
    else if (waveName.mid(1,4) == "-AUX") ds->yScaleType = AuxInputYScale;
    else if (waveName.mid(1,4) == "-VDD") ds->yScaleType = SupplyVoltageYScale;
    else if (waveName.left(7) == "ANALOG-") ds->yScaleType = AnalogIOYScale;
    else if (waveName.left(8) == "DIGITAL-") ds->yScaleType = DigitalIOYScale;
    else {
        ds->yScaleType = UnknownYScale;
        cout << "Warning: Unknown Y Scale for waveform " << name << '\n';
    }

    data[name] = ds;
    reset(ds);
    return true;
}

bool WaveformDisplayManager::removeWaveform(const QString& waveName)
{
    string name = waveName.toStdString();
    if (data.find(name) == data.end()) return false;
    delete data.at(name);
    data.erase(name);
    return true;
}

// Call this method before calling loadNewData() on waveforms.
void WaveformDisplayManager::prepForLoadingNewData()
{
    if (state->rollMode->getValue()) {  // Roll mode
        validDataIndex -= zoneLength;
        if (validDataIndex < 0) validDataIndex = 0;
    } else {  // Sweep mode
        validDataIndex += zoneLength;
        if (validDataIndex > length) {
            validDataIndex = zoneLength;
            sweepFirstTime = false;
        }
    }

    // Mark all WaveformDisplayDataStore objects with hasAlreadyLoaded = false.
    map<string, WaveformDisplayDataStore*>::const_iterator it = data.begin();
    while (it != data.end()) {
        it->second->hasAlreadyLoaded = false;
        ++it;
    }
}

// Call this method before calling loadOldData() on waveforms.
void WaveformDisplayManager::prepForLoadingOldData(int startTime)
{
    sweepFirstTime = true;
    if (-startTime > samplesPerZone * numRefreshZones) {
        validDataIndex = length;
    } else {
        validDataIndex = zoneLength * floor(-startTime / samplesPerZone);
    }

    // Mark all WaveformDisplayDataStore objects with hasAlreadyLoaded = false.
    map<string, WaveformDisplayDataStore*>::const_iterator it = data.begin();
    while (it != data.end()) {
        it->second->hasAlreadyLoaded = false;
        ++it;
    }
}

// Call this method after calling loadNewData() on waveforms, and before calling draw().
YScaleUsed WaveformDisplayManager::finishLoading()
{
    oldDataPresent = true;
    YScaleUsed yScaleUsed;

    // If WaveformDisplayDataStore object has not loaded new data, mark it as out of date
    map<string, WaveformDisplayDataStore*>::const_iterator it = data.begin();
    while (it != data.end()) {
        if (!(it->second->hasAlreadyLoaded)) {
            it->second->isOutOfDate = true;
        } else {
            YScaleType yScaleType = it->second->yScaleType;
            if (yScaleType == WidebandYScale) yScaleUsed.widepassYScaleUsed = true;
            else if (yScaleType == LowpassYScale) yScaleUsed.lowpassYScaleUsed = true;
            else if (yScaleType == HighpassYScale) yScaleUsed.highpassYScaleUsed = true;
            else if (yScaleType == AuxInputYScale) yScaleUsed.auxInputYScaleUsed = true;
            else if (yScaleType == AnalogIOYScale) yScaleUsed.analogIOYScaleUsed = true;
            else if (yScaleType == DCYScale) yScaleUsed.dcYScaleUsed = true;
        }
        ++it;
    }
    return yScaleUsed;
}

void WaveformDisplayManager::loadNewData(const WaveformFifo* waveformFifo, const QString& waveName) const
{
    map<string, WaveformDisplayDataStore*>::const_iterator it = data.find(waveName.toStdString());
    if (it == data.end()) {
        cout << "WaveformDisplayManager::loadNewData: Could not find waveName " << waveName.toStdString() << '\n';
        return;
    }
    WaveformDisplayDataStore* ds = it->second;
    if (!ds) return;

    if (ds->hasAlreadyLoaded) return;   // Don't load the same data twice.

    if (ds->isOutOfDate) {  // If display data is out of date, load old data from waveform FIFO to catch up.
        int displayStartPos, displayEndPos, startTime;
        if (state->rollMode->getValue()) {  // Roll mode
            displayStartPos = max(zoneLength, validDataIndex + zoneLength);  // No need to load first display zone; we just shift it away.
            displayStartPos = min(displayStartPos, length);
            displayEndPos = length;
            if (useVerticalLines) {
                startTime = -(displayEndPos - displayStartPos) * samplesPerZone / zoneWidthInPixels;
            } else {
                startTime = -(displayEndPos - displayStartPos);
            }
            loadDataSegment(waveformFifo, waveName, ds, displayStartPos, displayEndPos, startTime);
        } else {  // Sweep mode
            displayStartPos = 0;
            displayEndPos = max(0, validDataIndex - zoneLength);
            startTime = -displayEndPos * samplesPerZone / zoneWidthInPixels;
            loadDataSegment(waveformFifo, waveName, ds, displayStartPos, displayEndPos, startTime);
            if (!sweepFirstTime) {
                displayStartPos = displayEndPos;
                displayEndPos = length;
                if (useVerticalLines) {
                    startTime -= (displayEndPos - displayStartPos) * samplesPerZone / zoneWidthInPixels;
                } else {
                    startTime -= (displayEndPos - displayStartPos);
                }
                loadDataSegment(waveformFifo, waveName, ds, displayStartPos, displayEndPos, startTime);
            }
        }
        ds->isOutOfDate = false;
    }

    if (state->rollMode->getValue()) {  // Roll mode
        // Shift old data to the left.
        if (numRefreshZones > 1) {
            if (ds->isRaster) {
                for (int i = validDataIndex; i < length - zoneLength; ++i) {
                    ds->rasterData[i] = ds->rasterData[i + zoneLength];
                }
            } else {
                if (useVerticalLines) {
                    // TODO: Using .data() to get pointer may speed things up a little bit.  Try again, and in more places?
                    // MinMax<float>* directData = ds->yMinMaxData.data();
                    for (int x = validDataIndex; x < length - zoneLength; ++x) {
                        // directData[x] = directData[x + zoneLength];
                        ds->yMinMaxData[x] = ds->yMinMaxData[x + zoneLength];
                    }
                } else {
                    for (int i = validDataIndex; i < length - zoneLength; ++i) {
                        ds->yData[i] = ds->yData[i + zoneLength];
                    }
                }
                if (ds->hasStimFlags) {
                    for (int i = validDataIndex; i < length - zoneLength; ++i) {
                        ds->stimFlags[i] = ds->stimFlags[i + zoneLength];
                    }
                }
            }
        }

        // Now, load new data.
        loadDataSegment(waveformFifo, waveName, ds, length - zoneLength, length, 0);
    } else {  // Sweep mode
        // Load new data.
        loadDataSegment(waveformFifo, waveName, ds, validDataIndex - zoneLength, validDataIndex, 0);
    }
    ds->hasAlreadyLoaded = true;
}

void WaveformDisplayManager::loadOldData(const WaveformFifo* waveformFifo, const QString& waveName, int startTime) const
{
    map<string, WaveformDisplayDataStore*>::const_iterator it = data.find(waveName.toStdString());
    if (it == data.end()) {
        cout << "WaveformDisplayManager::loadOldData: Could not find waveName " << waveName.toStdString() << '\n';
        return;
    }
    WaveformDisplayDataStore* ds = it->second;
    if (!ds) return;

    if (ds->hasAlreadyLoaded) return;   // Don't load the same data twice.


    loadDataSegment(waveformFifo, waveName, ds, 0, validDataIndex, startTime);
    ds->isOutOfDate = false;
    ds->hasAlreadyLoaded = true;
}

void WaveformDisplayManager::loadDataSegment(const WaveformFifo* waveformFifo, const QString& waveName,
                                             WaveformDisplayDataStore* ds, int displayStartPos, int displayEndPos,
                                             int startTime) const
{
    int displaySpan = displayEndPos - displayStartPos;
    // Note: displaySpan should be an integer multiple of zoneWidthInPixels if useVerticalLines is true.
    //       displaySpan should be an integer multiple of zoneLength if useVerticalLines is false.

    if (displaySpan == 0) return;

    bool gpuMode = false;
    GpuWaveformAddress gpuWaveformAddress;
    float* waveform = nullptr;
    uint16_t* rasterData = nullptr;
    uint16_t* stimFlags = nullptr;

    if (ds->isRaster) {
        rasterData = waveformFifo->getDigitalWaveformPointer(waveName.toStdString());
    } else {
        gpuWaveformAddress = waveformFifo->getGpuWaveformAddress(waveName.toStdString());
        if (gpuWaveformAddress.waveformIndex >= 0) {
            gpuMode = true;
        } else {
            waveform = waveformFifo->getAnalogWaveformPointer(waveName.toStdString());
        }
        if (ds->hasStimFlags) {
            stimFlags = waveformFifo->getDigitalWaveformPointer(waveName.section('|', 0, 0).toStdString() + "|STIM");
        }
    }

    if (useVerticalLines) {  // Samples per pixel > 1
        int sampleSpan = (displaySpan / zoneWidthInPixels) * samplesPerZone;
        int pixelsToGo = displaySpan;
        int samplesToGo = sampleSpan;
        int timeIndex = startTime;

        if (ds->isRaster) {
            for (int x = displayStartPos; x < displayEndPos; ++x) {
                int samples = round((double)samplesToGo / (double)pixelsToGo);
                ds->rasterData[x] = waveformFifo->getRasterData(WaveformFifo::ReaderDisplay, rasterData, timeIndex, samples);
                timeIndex += samples;
                samplesToGo -= samples;
                --pixelsToGo;
            }
        } else {
            MinMax<float> y;
            if (oldDataPresent && startTime == 0) {
                int lastIndex = displayStartPos - 1;
                if (lastIndex < 0) lastIndex = length - 1;
                y = ds->yMinMaxData[lastIndex];
            } else {
                y.swap();
            }

            for (int x = displayStartPos; x < displayEndPos; ++x) {
                y.swap();
                int samples = round((double)samplesToGo / (double)pixelsToGo);
                if (gpuMode) {
                    waveformFifo->getMinMaxGpuAmplifierData(y, WaveformFifo::ReaderDisplay, gpuWaveformAddress, timeIndex, samples);
                } else {
                    waveformFifo->getMinMaxData(y, WaveformFifo::ReaderDisplay, waveform, timeIndex, samples);
                }
                ds->yMinMaxData[x] = y;
                if (ds->hasStimFlags) {
                    ds->stimFlags[x] = waveformFifo->getStimData(WaveformFifo::ReaderDisplay, stimFlags, timeIndex, samples);
                }
                timeIndex += samples;
                samplesToGo -= samples;
                --pixelsToGo;
            }
        }
    } else {  // Samples per pixel <= 1
        if (ds->isRaster) {
            waveformFifo->copyDigitalData(WaveformFifo::ReaderDisplay, &ds->rasterData[displayStartPos], rasterData,
                                          startTime, displaySpan);
        } else {
            if (gpuMode) {
                waveformFifo->copyGpuAmplifierData(WaveformFifo::ReaderDisplay, &ds->yData[displayStartPos], gpuWaveformAddress,
                                                   startTime, displaySpan);
            } else {
                waveformFifo->copyAnalogData(WaveformFifo::ReaderDisplay, &ds->yData[displayStartPos], waveform,
                                             startTime, displaySpan);
            }
            if (ds->hasStimFlags) {
                waveformFifo->copyDigitalData(WaveformFifo::ReaderDisplay, &ds->stimFlags[displayStartPos], stimFlags,
                                              startTime, displaySpan);
            }
        }
    }
}

float WaveformDisplayManager::getYScaleFactor(const QString& waveName) const
{
    map<string, WaveformDisplayDataStore*>::const_iterator it = data.find(waveName.toStdString());
    if (it == data.end()) return 0.0F;
    WaveformDisplayDataStore* ds = it->second;
    if (!ds) return 0.0F;
    return getYScaleFactor(ds->yScaleType);
}

float WaveformDisplayManager::getYScaleFactor(YScaleType yScaleType) const
{
    float yScale;
    switch (yScaleType) {
    case WidebandYScale:
        yScale = (float) state->yScaleWide->getNumericValue();
        break;
    case LowpassYScale:
        yScale = (float) state->yScaleLow->getNumericValue();
        break;
    case HighpassYScale:
        yScale = (float) state->yScaleHigh->getNumericValue();
        break;
    case DCYScale:
        yScale = (float) state->yScaleDC->getNumericValue();
        break;
    case AuxInputYScale:
        yScale = (float) state->yScaleAux->getNumericValue();
        break;
    case SupplyVoltageYScale:
        yScale = 0.5F;
        break;
    case AnalogIOYScale:
        yScale = (float) state->yScaleAnalog->getNumericValue();
        break;
    default:
        yScale = 1.0F;
    }
    return ScaleFactorY / yScale;
}

void WaveformDisplayManager::draw(QPainter &painter, const QString& waveName, QPoint position, QColor color)
{
    map<string, WaveformDisplayDataStore*>::const_iterator it = data.find(waveName.toStdString());
    if (it == data.end()) return;
    WaveformDisplayDataStore* ds = it->second;
    if (!ds) return;
    if (ds->isOutOfDate) return;

    painter.setPen(color);
    const QColor BackgroundColor = QColor(state->backgroundColor->getValueString());
    const uint16_t StimPolFlagOnly = 0x0100u;

    const int RasterHeight = 6;
    const int YRasterTop = position.y() - RasterHeight;
    const int YRasterBot = position.y() + RasterHeight;

    const int StimMarkerHeight = 6;
    const int YStimMarkerTop = position.y() - StimMarkerHeight;
    const int YStimMarkerBot = position.y() + StimMarkerHeight;

    float yScaleFactor = -getYScaleFactor(ds->yScaleType);

    bool supplyVoltageMode = false;
    MinMax<float> yMinMax;

    float yOffset = (float) position.y();
    if (waveName.mid(1,4) == "-AUX") {
        yOffset -= yScaleFactor * 1.7F;  // Center auxiliary input waveforms on nominal zero-gee bias level of accelerometers.
    } else if (waveName.mid(1,4) == "-VDD") {
        yOffset -= yScaleFactor * 3.3F;  // Center supply voltage waveforms on nominal 3.3V chip power supply level.
        supplyVoltageMode = true;
    } else if (waveName.left(8) == "DIGITAL-") {
        yOffset -= yScaleFactor * 0.5F;
    }

    const float EpsilonX = 0.49F;    // Help to resolve plotting issue where short line segments were invisible.

    if (!state->rollMode->getValue() || state->sweeping) {  // Sweep mode (or rewinding/fast forwarding)
        float x = (float)position.x();
        if (useVerticalLines) {
            if (ds->isRaster) {
                for (int i = 0; i < length; ++i) {  // Draw rasters.
                    if (ds->rasterData[i] != 0) {
                        if (i <= validDataIndex || !sweepFirstTime) {
                            painter.drawLine(QPointF(x, YRasterTop), QPointF(x, YRasterBot));
                        }
                    }
                    ++x;
                }
            } else {
                for (int i = 0; i < length; ++i) {
                    if (ds->hasStimFlags) {
                        if (ds->stimFlags[i] != 0 && ds->stimFlags[i] != StimPolFlagOnly) {  // Draw stim flags.
                            if (i <= validDataIndex || !sweepFirstTime) {
                                painter.setPen(stimFlagsColor(ds->stimFlags[i], BackgroundColor));
                                painter.drawLine(QPointF(x, YStimMarkerTop), QPointF(x, YStimMarkerBot));
                            }
                        }
                    }
                    ds->verticalLines[i] = QLineF(x, yScaleFactor * ds->yMinMaxData[i].minVal + yOffset,
                                                  x + EpsilonX, yScaleFactor * ds->yMinMaxData[i].maxVal + yOffset);
                    if (supplyVoltageMode && i < validDataIndex) {
                        yMinMax.update(ds->yMinMaxData[i].minVal);
                        yMinMax.update(ds->yMinMaxData[i].maxVal);
                    }
                    ++x;
                }
                // Draw main waveform.
                painter.setPen(supplyVoltageMode ? supplyVoltageColor(yMinMax) : color);
                painter.drawLines(&ds->verticalLines[0], validDataIndex);
                if (!sweepFirstTime && (validDataIndex < length)) {
                    painter.drawLines(&ds->verticalLines[validDataIndex], length - validDataIndex);
                }
            }
        } else {
            if (ds->isRaster) {
                for (int i = 0; i < length; ++i) {  // Draw rasters.
                    if (ds->rasterData[i] != 0) {
                        if (i <= validDataIndex || !sweepFirstTime) {
                            painter.drawLine(QPointF(x, YRasterTop), QPointF(x, YRasterBot));
                        }
                    }
                    x += pixelsPerSample;
                }
            } else {
                for (int i = 0; i < length; ++i) {
                    if (ds->hasStimFlags) {
                        if (ds->stimFlags[i] != 0 && ds->stimFlags[i] != StimPolFlagOnly) {  // Draw stim flags.
                            if (i <= validDataIndex || !sweepFirstTime) {
                                painter.setPen(stimFlagsColor(ds->stimFlags[i], BackgroundColor));
                                for (float w = 0; w < pixelsPerSample; ++w) {
                                    painter.drawLine(QPointF(x + w, YStimMarkerTop), QPointF(x + w, YStimMarkerBot));
                                }
                            }
                        }
                    }
                    ds->points[i] = QPointF(x, yScaleFactor * ds->yData[i] + yOffset);
                    if (supplyVoltageMode && i < validDataIndex) {
                        yMinMax.update(ds->yData[i]);
                    }
                    x += pixelsPerSample;
                }
                // Draw main waveform.
                painter.setPen(supplyVoltageMode ? supplyVoltageColor(yMinMax) : color);
                painter.drawPolyline(&ds->points[0], validDataIndex);
                if (!sweepFirstTime && (validDataIndex < length)) {
                    painter.drawPolyline(&ds->points[validDataIndex], length - validDataIndex);
                }
            }
        }
    } else {  // Roll mode
        if (validDataIndex >= length) return;
        float x = (float)(validDataIndex + position.x());
        if (useVerticalLines) {
            if (ds->isRaster) {
                for (int i = validDataIndex; i < length; ++i) {  // Draw rasters.
                    if (ds->rasterData[i] != 0) {
                        painter.drawLine(QPointF(x, YRasterTop), QPointF(x, YRasterBot));
                    }
                    ++x;
                }
            } else {
                for (int i = validDataIndex; i < length; ++i) {
                    if (ds->hasStimFlags) {
                        if (ds->stimFlags[i] != 0 && ds->stimFlags[i] != StimPolFlagOnly) {  // Draw stim flags.
                            painter.setPen(stimFlagsColor(ds->stimFlags[i], BackgroundColor));
                            painter.drawLine(QPointF(x, YStimMarkerTop), QPointF(x, YStimMarkerBot));
                        }
                    }
                    ds->verticalLines[i] = QLineF(x, yScaleFactor * ds->yMinMaxData[i].minVal + yOffset,
                                                  x + EpsilonX, yScaleFactor * ds->yMinMaxData[i].maxVal + yOffset);
                    if (supplyVoltageMode) {
                        yMinMax.update(ds->yMinMaxData[i].minVal);
                        yMinMax.update(ds->yMinMaxData[i].maxVal);
                    }
                    ++x;
                }
                // Draw main waveform.
                painter.setPen(supplyVoltageMode ? supplyVoltageColor(yMinMax) : color);
                painter.drawLines(&ds->verticalLines[validDataIndex], length - validDataIndex);
            }
        } else {
            if (ds->isRaster) {
                for (int i = validDataIndex; i < length; ++i) {  // Draw rasters.
                    if (ds->rasterData[i] != 0) {
                        painter.drawLine(QPointF(x, YRasterTop), QPointF(x, YRasterBot));
                    }
                    x += pixelsPerSample;
                }
            } else {
                for (int i = validDataIndex; i < length; ++i) {
                    if (ds->hasStimFlags) {
                        if (ds->stimFlags[i] != 0 && ds->stimFlags[i] != StimPolFlagOnly) {  // Draw stim flags.
                            painter.setPen(stimFlagsColor(ds->stimFlags[i], BackgroundColor));
                            for (float w = 0; w < pixelsPerSample; ++w) {
                                painter.drawLine(QPointF(x + w, YStimMarkerTop), QPointF(x + w, YStimMarkerBot));
                            }
                        }
                    }
                    ds->points[i] = QPointF(x, yScaleFactor * ds->yData[i] + yOffset);
                    if (supplyVoltageMode) {
                        yMinMax.update(ds->yData[i]);
                    }
                    x += pixelsPerSample;
                }
                // Draw main waveform.
                painter.setPen(supplyVoltageMode ? supplyVoltageColor(yMinMax) : color);
                painter.drawPolyline(&ds->points[validDataIndex], length - validDataIndex);
            }
        }
    }
}

void WaveformDisplayManager::drawDivider(QPainter &painter, int yPos, int xStart, int xEnd)
{
    painter.fillRect(QRect(xStart, yPos - 1, xEnd - xStart, 3), QBrush(Qt::darkGray));
}

QColor WaveformDisplayManager::stimFlagsColor(uint16_t stimFlags, QColor backgroundColor) const
{
    const uint16_t ComplianceFlag = 0x8000u;
    const uint16_t ChargeRecoveryFlag = 0x4000u;
    const uint16_t AmpSettleFlag = 0x2000u;
    const uint16_t StimPolFlag = 0x0100u;
    const uint16_t StimOnFlag = 0x0001u;

    if (stimFlags == 0 || stimFlags == StimPolFlag) return backgroundColor;
    if (stimFlags & ComplianceFlag) return ComplianceLimitColor;
    if (stimFlags & StimOnFlag) return StimColor;
    if (stimFlags & ChargeRecoveryFlag) return ChargeRecovColor;
    if (stimFlags & AmpSettleFlag) return AmpSettleColor;
    return backgroundColor;
}

QColor WaveformDisplayManager::supplyVoltageColor(MinMax<float> yMinMax) const
{
    if ((yMinMax.minVal < 2.9) || (yMinMax.maxVal > 3.6)) {
        if ((yMinMax.minVal < 0.0) || (yMinMax.maxVal > 5.0)) {   // Unrealistic values; impossible for on-chip ADC to generate these.
            return Qt::gray;
        } else {
            return Qt::red;
        }
    } else if (yMinMax.maxVal < 3.2) {
        return Qt::yellow;
    } else {
        return Qt::green;
    }
}

void WaveformDisplayManager::reset(WaveformDisplayDataStore* ds)
{
    if (!ds) return;

    if (!ds->isRaster) {
        ds->rasterData.clear();
        if (useVerticalLines) {
            ds->yMinMaxData.resize(length);
            ds->verticalLines.resize(length);
            ds->yData.clear();
            ds->points.clear();
        } else {
            ds->yData.resize(length);
            ds->points.resize(length);
            ds->yMinMaxData.clear();
            ds->verticalLines.clear();
        }
        if (ds->hasStimFlags) {
            ds->stimFlags.resize(length);
        } else {
            ds->stimFlags.clear();
        }
    } else {
        ds->rasterData.resize(length);
        ds->yMinMaxData.clear();
        ds->verticalLines.clear();
        ds->yData.clear();
        ds->points.clear();
        ds->stimFlags.clear();
    }
    ds->isOutOfDate = false;
}

void WaveformDisplayManager::resetAll()
{
    oldDataPresent = false;
    sweepFirstTime = true;
    validDataIndex = state->rollMode->getValue() ? length : 0;

    if (data.empty()) return;
    map<string, WaveformDisplayDataStore*>::const_iterator it = data.begin();
    while (it != data.end()) {
        reset(it->second);
        ++it;
    }
}

int WaveformDisplayManager::getRefreshXPosition() const
{
    if (state->rollMode->getValue()) {  // Roll mode
        return widthInPixels;
    } else { // Sweep mode
        if (useVerticalLines) {
            return validDataIndex;
        } else {
            return zoneWidthInPixels * (validDataIndex / zoneLength);
        }
    }
}

int WaveformDisplayManager::getValidDataXPosition() const
{
    if (!state->rollMode->getValue() && !sweepFirstTime) {
        return widthInPixels;
    }
    if (useVerticalLines) {
        return validDataIndex;
    } else {
        return zoneWidthInPixels * (validDataIndex / zoneLength);
    }
}

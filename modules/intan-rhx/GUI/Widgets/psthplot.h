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

#ifndef PSTHPLOT_H
#define PSTHPLOT_H

#define PSTHPLOT_X_SIZE 600
#define PSTHPLOT_Y_SIZE 550

#include <QtWidgets>
#include <vector>
#include <deque>
#include "systemstate.h"
#include "plotutilities.h"
#include "waveformfifo.h"
#include "rhxglobals.h"

class PSTHPlot : public QWidget
{
    Q_OBJECT
public:
    explicit PSTHPlot(SystemState* state_, QWidget *parent = nullptr);

    void setWaveform(const std::string& waveName_);
    QString getWaveform() const { return QString::fromStdString(waveName); }
    bool updateWaveforms(WaveformFifo* waveformFifo, int numSamples);
    void resetPSTH();
    void deleteLastRaster();

    bool saveMatFile(const QString& fileName) const;
    bool saveCsvFile(QString fileName) const;
    bool savePngFile(const QString& fileName) const;

    QSize minimumSizeHint() const override { return QSize(PSTHPLOT_X_SIZE, PSTHPLOT_Y_SIZE); }
    QSize sizeHint() const override { return QSize(PSTHPLOT_X_SIZE, PSTHPLOT_Y_SIZE); }

public slots:
    void updateFromState();

protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    SystemState* state;
    std::string waveName;

    std::deque<uint16_t> spikeTrainQueue;
    std::deque<uint16_t> digitalWaveformQueue;
    std::vector<std::vector<uint8_t> > rasters;
    std::vector<uint16_t> triggerWaveform;
    std::vector<float> histogram;
    std::vector<float> histogramTScale;
    QImage rasterImage;
    QImage histogramImage;

    int preTriggerTimeSpan;
    int postTriggerTimeSpan;
    int binSize;
    int maxNumTrials;
    int numTrials;
    double maxValueHistogram;

    int rasterPlotWidth;
    bool lastMouseWasInFrame;
    QRect digitalFrame;
    QRect rasterFrame;
    QRect histogramFrame;

    QImage image;

    int numBins() const { return (preTriggerTimeSpan + postTriggerTimeSpan) / binSize; }
    int numTStepsPerBin() const { return qRound(binSize * state->sampleRate->getNumericValue() / 1000.0); }
    int numTStepsTotal() const { return numBins() * numTStepsPerBin(); }
    int numTStepsPreTrigger() const { return (preTriggerTimeSpan / binSize) * numTStepsPerBin(); }
    int numTStepsPostTrigger() const { return (postTriggerTimeSpan / binSize) * numTStepsPerBin(); }
    void addNewRaster(int timeIndex);
    void adjustRasterPlotWidth(int width);
    void resizeRasters();
    void redrawRasterImage();
    void calculateHistogram();
};

#endif // PSTHPLOT_H

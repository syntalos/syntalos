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

#ifndef SPECTROGRAMPLOT_H
#define SPECTROGRAMPLOT_H

#define SPECPLOT_X_SIZE 600
#define SPECPLOT_Y_SIZE 550

#include <QtWidgets>
#include <vector>
#include <deque>
#include "systemstate.h"
#include "plotutilities.h"
#include "waveformfifo.h"
#include "rhxglobals.h"
#include "fastfouriertransform.h"

class SpectrogramPlot : public QWidget
{
    Q_OBJECT
public:
    explicit SpectrogramPlot(SystemState* state_, QWidget *parent = nullptr);
    ~SpectrogramPlot();

    void setWaveform(const std::string& waveName_);
    QString getWaveform() const;
    bool updateWaveforms(WaveformFifo* waveformFifo, int numSamples);
    void resetSpectrogram();

    double getDeltaTimeMsec() const { return 1000.0 * tStep; }
    double getDeltaFreqHz() const { return frequencyScale[1]; }

    bool saveMatFile(const QString& fileName) const;
    bool saveCsvFile(QString fileName) const;
    bool savePngFile(const QString& fileName) const;

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

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

    std::deque<float> amplifierWaveformQueue;
    std::deque<float> amplifierWaveformRecordQueue;
    std::deque<uint16_t> digitalWaveformQueue;
    std::deque<uint32_t> waveformTimeStampQueue;

    FastFourierTransform* fftEngine;
    int fftSize;

    double tScale;
    int tSize;
    int tIndex;
    int numValidTStepsInSpectrogram;
    bool spectrogramFull;
    double tStep;

    float* fftInputBuffer;
    std::vector<float> frequencyScale;
    int fMinIndex;
    int fMaxIndex;
    std::vector<float> timeScale;
    std::vector<float> psdSpectrum;
    std::vector<std::vector<float> > psdSpectrogram;
    QImage psdRawImage;

    double psdScaleMin;
    double psdScaleMax;
    ColorScale* colorScale;
    QImage image;
    QRect scopeFrame;
    bool lastMouseWasInFrame;

    QString psdUnitsMicro;

    void setNewFftSize(int fftSize_);
    void updateFMinMaxIndex();
    void setNewTimeScale(double tScale_);
};

#endif // SPECTROGRAMPLOT_H

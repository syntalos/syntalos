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

#ifndef SPIKEPLOT_H
#define SPIKEPLOT_H

#define SPIKEPLOT_X_SIZE 480
#define SPIKEPLOT_Y_SIZE 481

#include <QtWidgets>
#include <deque>
#include <vector>
#include <map>
#include "systemstate.h"
#include "plotutilities.h"
#include "waveformfifo.h"

using namespace std;

struct SpikePlotHistory
{
    deque<vector<float> > snippets;
    deque<int> spikeIds;

    deque<vector<float> > snapshotSnippets;
    deque<int> snapshotSpikeIds;
};


class SpikePlot : public QWidget
{
    Q_OBJECT
public:
    explicit SpikePlot(SystemState* state_, QWidget *parent = nullptr);
    ~SpikePlot();

    void setWaveform(const string& waveName);
    QString getWaveform();
    bool updateWaveforms(WaveformFifo* waveformFifo, int numSamples);
    void clearSpikes();

    void takeSnapshot();
    void clearSnapshot();

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

public slots:
    void updateFromState();

protected:
    void paintEvent(QPaintEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    SystemState* state;
    Channel* channel;
    SpikePlotHistory* history;
    int samplesPreDetect;
    int samplesPostDetect;
    double tStepMsec;

    QRect scopeFrame;
    QImage image;

    map<string, SpikePlotHistory*> spikeHistoryMap;

    const QColor SnapshotColor = QColor(140, 83, 25);

    double latestRmsCalculation;
    int latestSpikeRateCalculation;

    CoordinateTranslator ct;

    void updateCoordinateTranslator();
};

#endif // SPIKEPLOT_H

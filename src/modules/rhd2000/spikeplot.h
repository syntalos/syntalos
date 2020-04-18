//  ------------------------------------------------------------------------
//
//  This file is part of the Intan Technologies RHD2000 Interface
//  Version 1.5.2
//  Copyright (C) 2013-2017 Intan Technologies
//
//  ------------------------------------------------------------------------
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef SPIKEPLOT_H
#define SPIKEPLOT_H

#define SPIKEPLOT_X_SIZE 320
#define SPIKEPLOT_Y_SIZE 346

#include <QWidget>

using namespace std;

class SignalProcessor;
class SpikeScopeDialog;
class SignalChannel;

class SpikePlot : public QWidget
{
    Q_OBJECT
public:
    explicit SpikePlot(SignalProcessor *inSignalProcessor, SignalChannel *initialChannel,
                       SpikeScopeDialog *inSpikeScopeDialog, QWidget *parent = 0);
    void setYScale(int newYScale);
    void setSampleRate(double newSampleRate);
    void updateWaveform(int numBlocks);
    void setMaxNumSpikeWaveforms(int num);
    void clearScope();
    void setVoltageTriggerMode(bool voltageMode);
    void setVoltageThreshold(int threshold);
    void setDigitalTriggerChannel(int channel);
    void setDigitalEdgePolarity(bool risingEdge);
    void setNewChannel(SignalChannel* newChannel);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

signals:

public slots:

protected:
    void paintEvent(QPaintEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void drawAxisLines();
    void drawAxisText();
    void updateSpikePlot(double rms);
    void initializeDisplay();

    SignalProcessor *signalProcessor;
    SpikeScopeDialog *spikeScopeDialog;

    QVector<QVector<double> > spikeWaveform;
    QVector<double> spikeWaveformBuffer;
    QVector<int> digitalInputBuffer;
    int spikeWaveformIndex;
    int numSpikeWaveforms;
    int maxNumSpikeWaveforms;
    bool voltageTriggerMode;
    int voltageThreshold;
    int digitalTriggerChannel;
    bool digitalEdgePolarity;

    int preTriggerTSteps;
    int totalTSteps;
    bool startingNewChannel;
    int rmsDisplayPeriod;

    SignalChannel *selectedChannel;

    QRect frame;

    double tStepMsec;
    int yScale;
    double savedRms;

    QPixmap pixmap;

    QVector<QVector<QColor> > scopeColors;

};

#endif // SPIKEPLOT_H

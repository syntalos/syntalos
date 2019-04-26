//  ------------------------------------------------------------------------
//
//  This file is part of the Intan Technologies RHD2000 Interface
//  Version 1.4.2
//  Copyright (C) 2013 Intan Technologies
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

#ifndef WAVEPLOT_H
#define WAVEPLOT_H

#include <QWidget>
#include "signalgroup.h"
#include "qtincludes.h"

using namespace std;

class Rhd2000DataBlock;
class SignalProcessor;
class SignalSources;
class IntanUI;

class TracePlotProxy;

class WavePlot : public QWidget
{
    Q_OBJECT

public:
    WavePlot(SignalProcessor *inSignalProcessor, SignalSources *inSignalSources,
             IntanUI *inIntanUI, QWidget *parent = 0);

    void initialize(int startingPort);
    void passFilteredData();
    void refreshScreen();

    SignalChannel* selectedChannel();
    SignalChannel* selectedChannel(int index);
    QString getChannelName();
    QString getChannelName(int port, int index);
    QString getNativeChannelName();
    QString getNativeChannelName(int port, int index);
    int setPort(int port);
    void setChannelName(QString name);
    void setChannelName(QString name, int port, int index);
    void sortChannelsByName();
    void sortChannelsByNumber();
    bool isSelectedChannelEnabled();
    void setSelectedChannelEnable(bool enabled);
    void toggleSelectedChannelEnable();
    void enableAllChannels();
    void disableAllChannels();
    void setImpedanceLabels(bool enabled);
    void setPointPlotMode(bool enabled);

    int setNumFrames(int index);
    int setNumFrames(int index, int port);
    int setTopLeftFrame(int newTopLeftFrame, int port);
    int getTopLeftFrame(int port);
    int getNumFramesIndex(int port);
    void setYScale(int newYScale);
    void setTScale(int newTScale);
    void setSampleRate(double newSampleRate);
    void setNumUsbBlocksToPlot(int numBlocks);

    QSize minimumSizeHint() const;
    QSize sizeHint() const;

    bool isPortEnabled(int port) const;
    void setPlotProxy(TracePlotProxy *pp);
    int getChannelCount(int port) const;
    
signals:
    void selectedChannelChanged(SignalChannel* newChannel);

public slots:

protected:
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);
    void keyPressEvent(QKeyEvent *event);
    void closeEvent(QCloseEvent *event);
    void resizeEvent(QResizeEvent* event);

private:
    void refreshPixmap();
    void drawAxes(QPainter &painter, int frameNumber);
    void drawAxisLines(QPainter &painter, int frameNumber);
    void drawAxisText(QPainter &painter, int frameNumber);
    void drawWaveforms();
    void highlightFrame(int frameIndex, bool eraseOldFrame);
    void changeSelectedFrame(int newSelectedFrame, bool pageUpDown);
    int findClosestFrame(QPoint p);
    int distanceSquared(QPoint a, QPoint b);
    void drawDragIndicator(int frameIndex, bool erase);
    void expandYScale();
    void contractYScale();
    void expandTScale();
    void contractTScale();

    void allocateDoubleArray3D(QVector<QVector<QVector<double> > > &array3D,
                               int xSize, int ySize, int zSize);
    void allocateDoubleArray2D(QVector<QVector<double> > &array2D,
                               int xSize, int ySize);
    void allocateIntArray2D(QVector<QVector<int> > &array2D,
                            int xSize, int ySize);

    SignalProcessor *signalProcessor;
    SignalSources *signalSources;
    IntanUI *intanUI;
    TracePlotProxy *plotProxy;

    QPixmap pixmap;

    QVector<double> plotDataOld;
    QVector<QVector<QRect> > frameList;
    QVector<int> frameNumColumns;
    QVector<int> numFramesIndex;
    QVector<int> selectedFrame;
    QVector<int> topLeftFrame;
    int selectedPort;
    int yScale;
    int tScale;
    double sampleRate;
    double tPosition;
    int numUsbBlocksToPlot;
    bool dragging;
    int dragToIndex;
    bool impedanceLabels;
    bool pointPlotMode;

    void createFrames(unsigned int frameIndex, unsigned int maxX, unsigned int maxY);
    void createAllFrames();
};

#endif // WAVEPLOT_H

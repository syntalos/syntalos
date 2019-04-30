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

#include <QtGui>
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
#include <QtWidgets>
#endif
#include <vector>
#include <queue>
#include <iostream>
#include <QtAlgorithms>

#include "globalconstants.h"
#include "waveplot.h"
#include "intanui.h"
#include "rhd2000datablock.h"
#include "signalsources.h"
#include "signalprocessor.h"

#include "modules/traceplot/traceplotproxy.h"

// The WavePlot widget displays multiple waveform plots in the Main Window.
// Five types of waveforms may be displayed: amplifier, auxiliary input, supply
// voltage, ADC input, and digital input waveforms.  Users may navigate through
// the displays using cursor keys, and may drag and drop displays with the mouse.
// Other keypresses are used to change the voltage and time scales of the plots.

// Constructor.
WavePlot::WavePlot(SignalProcessor *inSignalProcessor, SignalSources *inSignalSources,
                   IntanUI *inIntanUI, QWidget *parent)
    : QWidget(parent),
      plotProxy(nullptr)
{
    setBackgroundRole(QPalette::Window);
    setAutoFillBackground(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setFocusPolicy(Qt::StrongFocus);

    signalProcessor = inSignalProcessor;
    signalSources = inSignalSources;
    intanUI = inIntanUI;

    dragging = false;
    dragToIndex = -1;

    impedanceLabels = false;
    pointPlotMode = false;
}

// Initialize WavePlot object.
void WavePlot::initialize(int startingPort)
{
    selectedPort = startingPort;

    // This only needs to be as large as the maximum number of frames ever
    // displayed on one port, but let's make it so big we never need to worry
    // about increasing its size.
    plotDataOld.resize(2400);

    tPosition = 0.0;

    // Vectors to store the currently selected from (plot window) and the frame
    // in the top left of the screen for all six ports (SPI Ports A-D, ADC inputs,
    // and digital inputs).

    selectedFrame.resize(6);
    selectedFrame.fill(0);
    topLeftFrame.resize(6);
    topLeftFrame.fill(0);

    createAllFrames();

    // Set default number of frames per screen for each port.
    numFramesIndex.resize(6);
    for (int port = 0; port < numFramesIndex.size(); ++port) {
        numFramesIndex[port] = frameList.size() - 1;
        if (signalSources->signalPort[port].enabled) {
            while (frameList[numFramesIndex[port]].size() >
                   signalSources->signalPort[port].numChannels()) {
                numFramesIndex[port] = numFramesIndex[port] - 1;
            }
        }
    }

    setNumFrames(numFramesIndex[selectedPort]);
}

void WavePlot::createFrames(unsigned int frameIndex, unsigned int maxX, unsigned int maxY) {
    QVector<QRect>& frames = frameList[frameIndex];
    frameNumColumns[frameIndex] = maxX;

    unsigned int xSize = (width() - 10 - 6 * (maxX - 1)) / maxX;
    unsigned int xOffset = xSize + 6;

    const int textBoxHeight = fontMetrics().height();

    unsigned int ySpacing = 2 * textBoxHeight + ((maxY == 8) ? 1 : 3);
    unsigned int yOffset = (height() - 4) / maxY;
    unsigned int ySize = yOffset - ySpacing;

    frames.resize(maxY * maxX);
    unsigned int index = 0;
    for (unsigned int y = 0; y < maxY; ++y) {
        for (unsigned int x = 0; x < maxX; x++) {
            frames[index] = QRect(5 + xOffset * x, 2 + textBoxHeight + yOffset * y, xSize, ySize);
            ++index;
        }
    }
}

// Allocates memory for a 3-D array of doubles.
void WavePlot::allocateDoubleArray3D(QVector<QVector<QVector<double> > > &array3D,
                                     int xSize, int ySize, int zSize)
{
    int i, j;

    array3D.resize(xSize);
    for (i = 0; i < xSize; ++i) {
        array3D[i].resize(ySize);
        for (j = 0; j < ySize; ++j) {
            array3D[i][j].resize(zSize);
        }
    }
}

// Allocates memory for a 2-D array of doubles.
void WavePlot::allocateDoubleArray2D(QVector<QVector<double> > &array2D,
                                     int xSize, int ySize)
{
    int i;

    array2D.resize(xSize);
    for (i = 0; i < xSize; ++i) {
        array2D[i].resize(ySize);
    }
}

// Allocates memory for a 2-D array of integers.
void WavePlot::allocateIntArray2D(QVector<QVector<int> > &array2D,
                                  int xSize, int ySize)
{
    int i;

    array2D.resize(xSize);
    for (i = 0; i < xSize; ++i) {
        array2D[i].resize(ySize);
    }
}

// Change the number of waveforms visible on the screen.
// Returns the index to the new number of waveforms.
int WavePlot::setNumFrames(int index)
{
    return setNumFrames(index, selectedPort);
}

// Change the number of waveforms visible on the screen.
// Returns the index to the new number of waveforms.
int WavePlot::setNumFrames(int index, int port)
{
    int indexLargestFrameAllowed;

    if (index < 0 || index >= frameList.size()) {
        return numFramesIndex[port];
    }

    // Make sure we don't show more frames than the number of channels
    // on the port.
    indexLargestFrameAllowed = index;
    while (frameList[indexLargestFrameAllowed].size() >
           signalSources->signalPort[port].numChannels()) {
        indexLargestFrameAllowed--;
    }

    numFramesIndex[port] = indexLargestFrameAllowed;

    // We may need to adjust which frame appears in the top left corner
    // of the display once we go to a new number of frames.
    if (topLeftFrame[port] + frameList[numFramesIndex[port]].size() >
            signalSources->signalPort[port].numChannels()) {
        topLeftFrame[port] =
                signalSources->signalPort[port].numChannels() -
                frameList[numFramesIndex[port]].size();
    }

    if (selectedFrame[port] < topLeftFrame[port]) {
        selectedFrame[port] = topLeftFrame[port];
    } else {
        while (selectedFrame[port] >=
               topLeftFrame[port] +frameList[numFramesIndex[port]].size()) {
            topLeftFrame[port] += frameNumColumns[numFramesIndex[port]];
        }
    }

    dragToIndex = -1;
    refreshScreen();
    intanUI->setNumWaveformsComboBox(index);

    return indexLargestFrameAllowed;
}

// Select the frame that appears in the top left corner of the display.
// Returns the new top left frame index.
int WavePlot::setTopLeftFrame(int newTopLeftFrame, int port)
{
    if (newTopLeftFrame >=
            signalSources->signalPort[port].numChannels() -
            frameList[numFramesIndex[port]].size()) {
        topLeftFrame[port] =
                signalSources->signalPort[port].numChannels() -
                frameList[numFramesIndex[port]].size();
    } else {
        topLeftFrame[port] = newTopLeftFrame;
    }
    refreshPixmap();
    highlightFrame(topLeftFrame[port], false);

    return topLeftFrame[port];
}

int WavePlot::getTopLeftFrame(int port)
{
    return topLeftFrame[port];
}

int WavePlot::getNumFramesIndex(int port)
{
    return numFramesIndex[port];
}

void WavePlot::setYScale(int newYScale)
{
    yScale = newYScale;
    refreshScreen();
}

// Expand voltage axis on amplifier plots.
void WavePlot::expandYScale()
{
    int index = intanUI->yScaleComboBox->currentIndex();
    if (index > 0) {
        intanUI->yScaleComboBox->setCurrentIndex(index - 1);
        setYScale(intanUI->yScaleList[index - 1]);
    }
}

// Contract voltage axis on amplifier plots.
void WavePlot::contractYScale()
{
    int index = intanUI->yScaleComboBox->currentIndex();
    if (index < intanUI->yScaleComboBox->count() - 1) {
        intanUI->yScaleComboBox->setCurrentIndex(index + 1);
        setYScale(intanUI->yScaleList[index + 1]);
    }
}

void WavePlot::setTScale(int newTScale)
{
    tScale = newTScale;
    refreshScreen();
}

// Expand time scale on all plots.
void WavePlot::expandTScale()
{
    int index = intanUI->tScaleComboBox->currentIndex();
    if (index < intanUI->tScaleComboBox->count() - 1) {
        intanUI->tScaleComboBox->setCurrentIndex(index + 1);
        setTScale(intanUI->tScaleList[index + 1]);
    }
}

// Contract time scale on all plots.
void WavePlot::contractTScale()
{
    int index = intanUI->tScaleComboBox->currentIndex();
    if (index > 0) {
        intanUI->tScaleComboBox->setCurrentIndex(index - 1);
        setTScale(intanUI->tScaleList[index - 1]);
    }
}

// Set sampleRate variable.  (Does not change amplifier sample rate.)
void WavePlot::setSampleRate(double newSampleRate)
{
    sampleRate = newSampleRate;
}

QSize WavePlot::minimumSizeHint() const
{
    return QSize(860, 690);
}

QSize WavePlot::sizeHint() const
{
    return QSize(860, 690);
}

void WavePlot::setPlotProxy(TracePlotProxy *pp)
{
    plotProxy = pp;
}

int WavePlot::getChannelCount(int port) const
{
    return frameList[numFramesIndex[port]].size();
}

void WavePlot::paintEvent(QPaintEvent * /* event */)
{
    QStylePainter stylePainter(this);
    stylePainter.drawPixmap(0, 0, pixmap);
}

// Returns the index of the closest waveform frame to a point on the screen.
// (Used for mouse selections.)
int WavePlot::findClosestFrame(QPoint p)
{
    int distance2 = 0;
    int smallestDistance2 = 0;
    int closestFrameIndex = -1;
    for (int i = 0; i < frameList[numFramesIndex[selectedPort]].size(); ++i) {
        distance2 = distanceSquared(p, frameList[numFramesIndex[selectedPort]][i].center());
        if ((distance2 < smallestDistance2) || (i == 0)) {
            smallestDistance2 = distance2;
            closestFrameIndex = i;
        }
    }
    return closestFrameIndex;
}

// Select a frame when the left mouse button is clicked.
void WavePlot::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        highlightFrame(findClosestFrame(event->pos()) + topLeftFrame[selectedPort], true);
    } else {
        QWidget::mousePressEvent(event);
    }
}

// If we are dragging a frame, release it in the appropriate place,
// reordering the channels on the currently selected port.
void WavePlot::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (dragging) {
            int i;
            dragging = false;
            if (dragToIndex >= 0) {
                drawDragIndicator(dragToIndex, true);
            }
            // Move selected frame
            if (dragToIndex + topLeftFrame[selectedPort] == selectedFrame[selectedPort]) {
                return;
            } else {
                if (dragToIndex + topLeftFrame[selectedPort] > selectedFrame[selectedPort]) {
                    // Move selected frame forward
                    signalSources->signalPort[selectedPort].channelByIndex(selectedFrame[selectedPort])->userOrder = -10000;
                    for (i = selectedFrame[selectedPort] + 1; i < dragToIndex + topLeftFrame[selectedPort] + 1; ++i) {
                        signalSources->signalPort[selectedPort].channelByIndex(i)->userOrder = i - 1;
                    }
                    signalSources->signalPort[selectedPort].channelByIndex(-10000)->userOrder = i - 1;
                } else {
                    // Move selected frame backwards
                    signalSources->signalPort[selectedPort].channelByIndex(selectedFrame[selectedPort])->userOrder = -10000;
                    for (i = selectedFrame[selectedPort] - 1; i >= dragToIndex + topLeftFrame[selectedPort]; --i) {
                        signalSources->signalPort[selectedPort].channelByIndex(i)->userOrder = i + 1;
                    }
                    signalSources->signalPort[selectedPort].channelByIndex(-10000)->userOrder = i + 1;
                }
                changeSelectedFrame(dragToIndex + topLeftFrame[selectedPort], false);
                refreshScreen();
            }
        }
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

// Drag a selected frame when the mouse is moved.
void WavePlot::mouseMoveEvent(QMouseEvent *event)
{
    if ((event->buttons() & Qt::LeftButton)) {
        dragging = true;
        int frameIndex;
        frameIndex = findClosestFrame(event->pos());
        if (frameIndex != dragToIndex) {
            if (dragToIndex >= 0) {
                // Erase old drag target indicator
                drawDragIndicator(dragToIndex, true);
            }
            // Draw new drag target indicator
            drawDragIndicator(frameIndex, false);
            dragToIndex = frameIndex;
        }
    }
}

// Draw vertical line to indicate mouse drag location.
void WavePlot::drawDragIndicator(int frameIndex, bool erase)
{
    QPainter painter(&pixmap);
    painter.initFrom(this);
    QRect frame = frameList[numFramesIndex[selectedPort]][frameIndex];
    if (erase) {
        painter.setPen(palette().window().color());
    } else {
        painter.setPen(Qt::darkRed);
    }
    painter.drawLine(frame.center().x() - (frame.width() / 2 + 3) + 1, frame.top() - 5,
                      frame.center().x() - (frame.width() / 2 + 3) + 1, frame.bottom() + 7);
    update();
}

// If mouse wheel is turned, move selection cursor up or down on
// the screen.
void WavePlot::wheelEvent(QWheelEvent *event)
{
    int newSelectedFrame;

    if (event->delta() < 0) {
        newSelectedFrame = selectedFrame[selectedPort] +
                frameNumColumns[numFramesIndex[selectedPort]];
        changeSelectedFrame(newSelectedFrame, false);
    } else {
        newSelectedFrame = selectedFrame[selectedPort] -
                frameNumColumns[numFramesIndex[selectedPort]];
        changeSelectedFrame(newSelectedFrame, false);
    }
}

// Parse keypress commands.
void WavePlot::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Up:
        changeSelectedFrame(selectedFrame[selectedPort] -
                            frameNumColumns[numFramesIndex[selectedPort]], false);
        break;
    case Qt::Key_Down:
        changeSelectedFrame(selectedFrame[selectedPort] +
                            frameNumColumns[numFramesIndex[selectedPort]], false);
        break;
    case Qt::Key_Left:
        changeSelectedFrame(selectedFrame[selectedPort] - 1, false);
        break;
    case Qt::Key_Right:
        changeSelectedFrame(selectedFrame[selectedPort] + 1, false);
        break;
    case Qt::Key_PageUp:
        changeSelectedFrame(selectedFrame[selectedPort] -
                            frameList[numFramesIndex[selectedPort]].size(), true);
        break;
    case Qt::Key_PageDown:
        changeSelectedFrame(selectedFrame[selectedPort] +
                            frameList[numFramesIndex[selectedPort]].size(), true);
        break;
    case Qt::Key_BracketRight:
        setNumFrames(numFramesIndex[selectedPort] - 1);
        break;
    case Qt::Key_BracketLeft:
        setNumFrames(numFramesIndex[selectedPort] + 1);
        break;
    case Qt::Key_Comma:
    case Qt::Key_Less:
        contractTScale();
        break;
    case Qt::Key_Period:
    case Qt::Key_Greater:
        expandTScale();
        break;
    case Qt::Key_Minus:
    case Qt::Key_Underscore:
        contractYScale();
        break;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        expandYScale();
        break;
    case Qt::Key_Space:
        toggleSelectedChannelEnable();
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

void WavePlot::closeEvent(QCloseEvent *event)
{
    // Perform any clean-up here before application closes.
    event->accept();
}

// Change the selected frame in response to a mouse click, cursor
// keys, or PageUp/Down keys.
void WavePlot::changeSelectedFrame(int newSelectedFrame, bool pageUpDown)
{
    int newTopLeftFrame;
    if (newSelectedFrame >= topLeftFrame[selectedPort] &&
            newSelectedFrame <
            topLeftFrame[selectedPort] +
            frameList[numFramesIndex[selectedPort]].size()) {
        highlightFrame(newSelectedFrame, true);
    } else if (newSelectedFrame >=0 &&
               newSelectedFrame < topLeftFrame[selectedPort]) {
        if (!pageUpDown) {
            newTopLeftFrame =
                    topLeftFrame[selectedPort] -
                    frameNumColumns[numFramesIndex[selectedPort]];
        } else {
            newTopLeftFrame =
                    topLeftFrame[selectedPort] -
                    frameList[numFramesIndex[selectedPort]].size();
        }
        if (newTopLeftFrame < 0) newTopLeftFrame = 0;
        topLeftFrame[selectedPort] = newTopLeftFrame;
        refreshPixmap();
        highlightFrame(newSelectedFrame, false);
    } else if (newSelectedFrame < signalSources->signalPort[selectedPort].numChannels() &&
               newSelectedFrame >= topLeftFrame[selectedPort]){
        if (!pageUpDown) {
            newTopLeftFrame =
                    topLeftFrame[selectedPort] +
                    frameNumColumns[numFramesIndex[selectedPort]];
        } else {
            newTopLeftFrame =
                    topLeftFrame[selectedPort] +
                    frameList[numFramesIndex[selectedPort]].size();
        }
        if (newTopLeftFrame >=
                signalSources->signalPort[selectedPort].numChannels() -
                frameList[numFramesIndex[selectedPort]].size()) {
            newTopLeftFrame =
                    signalSources->signalPort[selectedPort].numChannels() -
                    frameList[numFramesIndex[selectedPort]].size();
        }
        topLeftFrame[selectedPort] = newTopLeftFrame;
        refreshPixmap();
        highlightFrame(newSelectedFrame, false);
    } else {
        if (pageUpDown) {
            if (newSelectedFrame >= signalSources->signalPort[selectedPort].numChannels()) {
                newTopLeftFrame =
                        signalSources->signalPort[selectedPort].numChannels() -
                        frameList[numFramesIndex[selectedPort]].size();
                topLeftFrame[selectedPort] = newTopLeftFrame;
                while (newSelectedFrame >= signalSources->signalPort[selectedPort].numChannels()) {
                    newSelectedFrame -= frameNumColumns[numFramesIndex[selectedPort]];
                }
                refreshPixmap();
                highlightFrame(newSelectedFrame, false);
            } else if (newSelectedFrame < 0) {
                newTopLeftFrame = 0;
                topLeftFrame[selectedPort] = newTopLeftFrame;
                while (newSelectedFrame < 0) {
                    newSelectedFrame += frameNumColumns[numFramesIndex[selectedPort]];
                }
                refreshPixmap();
                highlightFrame(newSelectedFrame, false);
            }
        }
    }
}

// Calculate square of distance between two points.  (Since will only use this
// function to find a minimum distance, we don't need to waste time calculating
// the square root.)
int WavePlot::distanceSquared(QPoint a, QPoint b)
{
    return (a.x() - b.x())*(a.x() - b.x()) + (a.y() - b.y())*(a.y() - b.y());
}

// Highlight the selected frame and (optionally) clear the highlight
// around a previously highlighted frame.  Then emit a signal indicating
// that the selected channel channed, and update the list of channels that
// are currently visible on the screen.
void WavePlot::highlightFrame(int frameIndex, bool eraseOldFrame)
{
    QRect frame;
    QPainter painter(&pixmap);
    painter.initFrom(this);

    painter.setPen(Qt::darkGray);

    if (eraseOldFrame) {
        frame = frameList[numFramesIndex[selectedPort]][selectedFrame[selectedPort] -
                topLeftFrame[selectedPort]];
        painter.drawRect(frame);
        painter.setPen(palette().window().color());
        frame.adjust(-1, -1, 1, 1);
        painter.drawRect(frame);
    }

    selectedFrame[selectedPort] = frameIndex;

    painter.setPen(Qt::darkRed);
    frame = frameList[numFramesIndex[selectedPort]][selectedFrame[selectedPort] -
            topLeftFrame[selectedPort]];
    painter.drawRect(frame);
    frame.adjust(-1, -1, 1, 1);
    painter.drawRect(frame);

    update();

    // Emit signal.
    emit selectedChannelChanged(selectedChannel());

    // Update list of visible channels.
    for (int i = 0; i < 8; ++i) {
        intanUI->channelVisible[i].fill(false);
    }
    for (int i = topLeftFrame[selectedPort];
         i < topLeftFrame[selectedPort] + frameList[numFramesIndex[selectedPort]].size();
         ++i) {
        intanUI->channelVisible[selectedChannel(i)->boardStream][selectedChannel(i)->chipChannel] = true;
    }
}

// Refresh pixel map used in double buffered graphics.
void WavePlot::refreshPixmap()
{
    // Pixel map used for double buffering.
    pixmap = QPixmap(size());
    pixmap.fill();

    QPainter painter(&pixmap);
    painter.initFrom(this);

    // Clear old display.
    painter.eraseRect(rect());

    // Draw box around entire display.
    painter.setPen(Qt::darkGray);
    QRect r(rect());
    r.adjust(0, 0, -1, -1);
    painter.drawRect(r);

    // Plot all frames.
    for (int i = 0; i < frameList[numFramesIndex[selectedPort]].size(); i++) {
        drawAxes(painter, i);
    }

    tPosition = 0;
    update();
}

void WavePlot::createAllFrames() {
    // Create lists of frame (plot window) dimensions for different numbers
    // of frames per screen.
    frameList.resize(6);
    frameNumColumns.resize(6);
    createFrames(0, 1, 1);
    createFrames(1, 1, 2);
    createFrames(2, 1, 4);
    createFrames(3, 2, 4);
    createFrames(4, 4, 4);
    createFrames(5, 4, 8);
}

void WavePlot::resizeEvent(QResizeEvent*) {
    createAllFrames();

    refreshPixmap();
}

// Plot a particular frame.
void WavePlot::drawAxes(QPainter &painter, int frameNumber)
{
    QRect frame = frameList[numFramesIndex[selectedPort]][frameNumber];

    painter.setPen(Qt::darkGray);

    painter.drawRect(frame);
    drawAxisLines(painter, frameNumber);
    drawAxisText(painter, frameNumber);
}

// Return a pointer to the current selected channel.
SignalChannel* WavePlot::selectedChannel()
{
    return selectedChannel(selectedFrame[selectedPort]);
}

// Return a pointer to a particular channel on the currently selected port.
SignalChannel* WavePlot::selectedChannel(int index)
{
    return signalSources->signalPort[selectedPort].channelByIndex(index);
}

// Draw axis lines inside a frame.
void WavePlot::drawAxisLines(QPainter &painter, int frameNumber)
{
    QRect frame = frameList[numFramesIndex[selectedPort]][frameNumber];
    painter.setPen(Qt::darkGray);

    SignalType type = selectedChannel(frameNumber + topLeftFrame[selectedPort])->signalType;
    if (selectedChannel(frameNumber + topLeftFrame[selectedPort])->enabled) {
        if (type == AmplifierSignal) {
            // Draw V = 0V axis line.
            painter.drawLine(frame.left(), frame.center().y(), frame.right(), frame.center().y());
        } else if (type == SupplyVoltageSignal) {
            // Draw V = 3.6V axis line.
            painter.drawLine(frame.left(), frame.top() - 0.266667 * (frame.top() - frame.bottom()) + 1,
                             frame.right(), frame.top() - 0.266667 * (frame.top() - frame.bottom()) + 1);
            // Draw V = 3.2V axis line.
            painter.drawLine(frame.left(), frame.top() - 0.533333 * (frame.top() - frame.bottom()) + 1,
                             frame.right(), frame.top() - 0.533333 * (frame.top() - frame.bottom()) + 1);
            // Draw V = 2.9V axis line.
            painter.drawLine(frame.left(), frame.top() - 0.733333 * (frame.top() - frame.bottom()) + 1,
                             frame.right(), frame.top() - 0.733333 * (frame.top() - frame.bottom()) + 1);
        }
    } else {
        // Draw X showing channel is disabled.
        painter.drawLine(frame.left(), frame.top(), frame.right(), frame.bottom());
        painter.drawLine(frame.left(), frame.bottom(), frame.right(), frame.top());
    }
}

// Draw text labels around axes of a frame.
void WavePlot::drawAxisText(QPainter &painter, int frameNumber)
{
    const int textBoxWidth = 180;
    const int textBoxHeight = painter.fontMetrics().height();

    QRect frame = frameList[numFramesIndex[selectedPort]][frameNumber];

    QString channel =
            selectedChannel(frameNumber + topLeftFrame[selectedPort])->nativeChannelName;
    QString name =
            selectedChannel(frameNumber + topLeftFrame[selectedPort])->customChannelName;
    SignalType type =
            selectedChannel(frameNumber + topLeftFrame[selectedPort])->signalType;
    bool enabled =
            selectedChannel(frameNumber + topLeftFrame[selectedPort])->enabled;
    double electrodeImpedanceMagnitude =
            selectedChannel(frameNumber + topLeftFrame[selectedPort])->electrodeImpedanceMagnitude;
    double electrodeImpedancePhase =
            selectedChannel(frameNumber + topLeftFrame[selectedPort])->electrodeImpedancePhase;

    painter.setPen(Qt::darkGray);

    // Draw vertical axis scale label.
    if (type == AmplifierSignal) {
        painter.drawText(frame.left() + 3, frame.top() - textBoxHeight - 1,
                          textBoxWidth, textBoxHeight, Qt::AlignLeft | Qt::AlignBottom,
                          QString::number(yScale) + " " + QSTRING_MU_SYMBOL + "V");
    } else if (type == AuxInputSignal) {
        painter.drawText(frame.left() + 3, frame.top() - textBoxHeight - 1,
                          textBoxWidth, textBoxHeight, Qt::AlignLeft | Qt::AlignBottom,
                          "+2.5V");
    } else if (type == SupplyVoltageSignal) {
        painter.drawText(frame.left() + 3, frame.top() - textBoxHeight - 1,
                          textBoxWidth, textBoxHeight, Qt::AlignLeft | Qt::AlignBottom,
                          "SUPPLY");
    } else if (type == BoardAdcSignal) {
        if (intanUI->getEvalBoardMode() == 1) {
            painter.drawText(frame.left() + 3, frame.top() - textBoxHeight - 1,
                              textBoxWidth, textBoxHeight, Qt::AlignLeft | Qt::AlignBottom,
                              QSTRING_PLUSMINUS_SYMBOL + "5.0V");
        } else {
            painter.drawText(frame.left() + 3, frame.top() - textBoxHeight - 1,
                              textBoxWidth, textBoxHeight, Qt::AlignLeft | Qt::AlignBottom,
                              "+3.3V");
        }
    } else if (type == BoardDigInSignal) {
        painter.drawText(frame.left() + 3, frame.top() - textBoxHeight - 1,
                          textBoxWidth, textBoxHeight, Qt::AlignLeft | Qt::AlignBottom,
                          "LOGIC");
    }

    // Draw channel name and number.
    painter.drawText(frame.right() - textBoxWidth - 2, frame.top() - textBoxHeight - 1,
                      textBoxWidth, textBoxHeight,
                      Qt::AlignRight | Qt::AlignBottom, name);
    painter.drawText(frame.right() - textBoxWidth - 2, frame.bottom() + 1,
                      textBoxWidth, textBoxHeight,
                      Qt::AlignRight | Qt::AlignTop, channel);

    // Draw time axis label.
    if (enabled) {
        painter.drawText(frame.left() + 3, frame.bottom() + 1,
                          textBoxWidth, textBoxHeight, Qt::AlignLeft | Qt::AlignTop,
                          QString::number(tScale) + " ms");
    } else {
        painter.drawText(frame.left() + 3, frame.bottom() + 1,
                          textBoxWidth, textBoxHeight, Qt::AlignLeft | Qt::AlignTop,
                          "DISABLED");
    }

    // Draw electrode impedance label (magnitude and phase).
    if (type == AmplifierSignal && impedanceLabels) {
        QString unitPrefix;
        int precision;
        double scale;
        if (electrodeImpedanceMagnitude >= 1.0e6) {
            scale = 1.0e6;
            unitPrefix = "M";
        } else {
            scale = 1.0e3;
            unitPrefix = "k";
        }

        if (electrodeImpedanceMagnitude >= 100.0e6) {
            precision = 0;
        } else if (electrodeImpedanceMagnitude >= 10.0e6) {
            precision = 1;
        } else if (electrodeImpedanceMagnitude >= 1.0e6) {
            precision = 2;
        } else if (electrodeImpedanceMagnitude >= 100.0e3) {
            precision = 0;
        } else if (electrodeImpedanceMagnitude >= 10.0e3) {
            precision = 1;
        } else {
            precision = 2;
        }

        painter.drawText(frame.center().x() - textBoxWidth / 2, frame.bottom() + 1,
                         textBoxWidth, textBoxHeight, Qt::AlignHCenter | Qt::AlignTop,
                         QString::number(electrodeImpedanceMagnitude / scale, 'f', precision) +
                         " " + unitPrefix + QSTRING_OMEGA_SYMBOL +
                         "  " + QSTRING_ANGLE_SYMBOL +
                         QString::number(electrodeImpedancePhase, 'f', 0) +
                         QSTRING_DEGREE_SYMBOL);
    }
}

void WavePlot::setNumUsbBlocksToPlot(int numBlocks)
{
    numUsbBlocksToPlot = numBlocks;
}

bool WavePlot::isPortEnabled(int port) const
{
    return signalSources->signalPort[port].enabled;
}

// Plot waveforms on screen.
void WavePlot::drawWaveforms()
{
    int i, j, xOffset, yOffset, stream, channel;
    double yAxisLength, tAxisLength;
    QRect adjustedFrame, eraseBlock;
    SignalType type;
    double tStepMsec, xScaleFactor, yScaleFactor;
    QPainter painter(&pixmap);
    painter.initFrom(this);

    int length = Rhd2000DataBlock::getSamplesPerDataBlock() * numUsbBlocksToPlot;

    QPointF *polyline = new QPointF[length + 1];

    // Assume all frames are the same size.
    yAxisLength = (frameList[numFramesIndex[selectedPort]][0].height() - 2) / 2.0;
    tAxisLength = frameList[numFramesIndex[selectedPort]][0].width() - 1;

    if (plotProxy != nullptr) {
        auto dispChanSize = plotProxy->channels().size();
        for (int k = 0; k < dispChanSize; k++) {
            auto chan = plotProxy->channels()[k];
            auto portId = chan->portChan.first;
            auto chanId = chan->portChan.second;

            stream = signalSources->signalPort[portId].channelByIndex(chanId)->boardStream;
            channel = signalSources->signalPort[portId].channelByIndex(chanId)->chipChannel;
            type = signalSources->signalPort[portId].channelByIndex(chanId)->signalType;

            if (type == AmplifierSignal) {
                for (i = 0; i < length; ++i) {
                    chan->addNewYValue(signalProcessor->amplifierPostFilter.at(stream).at(channel).at(i));
                }
            } else if (type == BoardDigInSignal) {
                for (i = 0; i < length; ++i) {
                    chan->addNewYValue(signalProcessor->boardDigIn.at(channel).at(i));
                }
            }
        }

        if (dispChanSize > 0) {
            plotProxy->updatePlot();
            plotProxy->adjustView();
        }
    }

    for (j = 0; j < frameList[numFramesIndex[selectedPort]].size(); ++j) {
        stream = selectedChannel(j + topLeftFrame[selectedPort])->boardStream;
        channel = selectedChannel(j + topLeftFrame[selectedPort])->chipChannel;
        type = selectedChannel(j + topLeftFrame[selectedPort])->signalType;

        if (selectedChannel(j + topLeftFrame[selectedPort])->enabled) {
            xOffset = frameList[numFramesIndex[selectedPort]][j].left() + 1;
            xOffset += tPosition * tAxisLength / tScale;

            // Set clipping region
            adjustedFrame = frameList[numFramesIndex[selectedPort]][j];
            adjustedFrame.adjust(0, 1, 0, 0);
            painter.setClipRect(adjustedFrame);

            // Erase segment of old wavefrom
            eraseBlock = adjustedFrame;
            eraseBlock.setLeft(xOffset);
            eraseBlock.setRight((tAxisLength * (1000.0 / sampleRate) / tScale) * (length - 1) + xOffset);
            painter.eraseRect(eraseBlock);

            // Redraw y = 0 axis
            drawAxisLines(painter, j);

            if (type == AmplifierSignal) {
                // Plot RHD2000 amplifier waveform

                tStepMsec = 1000.0 / sampleRate;
                xScaleFactor = tAxisLength * tStepMsec / tScale;
                yScaleFactor = -yAxisLength / yScale;
                yOffset = frameList[numFramesIndex[selectedPort]][j].center().y();

                // build waveform
                for (i = 0; i < length; ++i) {
                    polyline[i+1] =
                            QPointF(xScaleFactor * i + xOffset,
                                    yScaleFactor * signalProcessor->amplifierPostFilter.at(stream).at(channel).at(i) + yOffset);
                }

                // join to old waveform
                if (tPosition == 0.0) {
                    polyline[0] = polyline[1];
                } else {
                    polyline[0] =
                            QPointF(xScaleFactor * -1 + xOffset,
                                    yScaleFactor * plotDataOld.at(j + topLeftFrame[selectedPort]) + yOffset);
                }

                // save last point in waveform to join to next segment
                plotDataOld[j + topLeftFrame[selectedPort]] =
                        signalProcessor->amplifierPostFilter.at(stream).at(channel).at(length - 1);

                // draw waveform
                painter.setPen(Qt::blue);
                if (pointPlotMode) {
                    painter.drawPoints(polyline, length + 1);
                } else {
                    painter.drawPolyline(polyline, length + 1);
                }

            } else if (type == AuxInputSignal) {
                // Plot RHD2000 auxiliary input signal

                tStepMsec = 1000.0 / (sampleRate / 4);
                xScaleFactor = tAxisLength * tStepMsec / tScale;
                yScaleFactor = -(2.0 * yAxisLength) / 2.5;
                yOffset = frameList[numFramesIndex[selectedPort]][j].bottom();

                // build waveform
                for (i = 0; i < (length / 4); ++i) {
                    polyline[i+1] =
                            QPointF(xScaleFactor * i + xOffset,
                                    yScaleFactor * signalProcessor->auxChannel.at(stream).at(channel).at(i) + yOffset);
                }

                // join to old waveform
                if (tPosition == 0.0) {
                    polyline[0] = polyline[1];
                } else {
                    polyline[0] =
                            QPointF(xScaleFactor * -1 + xOffset,
                                    yScaleFactor * plotDataOld.at(j + topLeftFrame[selectedPort]) + yOffset);
                }

                // save last point in waveform to join to next segment
                plotDataOld[j + topLeftFrame[selectedPort]] =
                        signalProcessor->auxChannel.at(stream).at(channel).at((length / 4) - 1);

                // draw waveform
                QPen pen;
                pen.setColor(QColor(200, 50, 50));
                painter.setPen(pen);
                if (pointPlotMode) {
                    painter.drawPoints(polyline, (length / 4) + 1);
                } else {
                    painter.drawPolyline(polyline, (length / 4) + 1);
                }

            } else if (type == SupplyVoltageSignal) {
                // Plot RHD2000 supply voltage signal

                tStepMsec = 1000.0 / (sampleRate / 60.0);
                xScaleFactor = tAxisLength * tStepMsec / tScale;
                yScaleFactor = -(2.0 * yAxisLength) / 1.5;
                yOffset = frameList[numFramesIndex[selectedPort]][j].bottom();

                bool voltageLow = false;
                bool voltageOutOfRange = false;
                double voltage;

                // build waveform
                for (i = 0; i < (length / 60); ++i) {
                    voltage = signalProcessor->supplyVoltage.at(stream).at(i);
                    polyline[i+1] =
                            QPointF(xScaleFactor * i + xOffset,
                                    yScaleFactor * (voltage - 2.5) + yOffset);
                    if (voltage < 2.9 || voltage > 3.6) {
                        voltageOutOfRange = true;
                    } else if (voltage < 3.2) {
                        voltageLow = true;
                    }
                }

                // join to old waveform
                if (tPosition == 0.0) {
                    polyline[0] = polyline[1];
                } else {
                    polyline[0] =
                            QPointF(xScaleFactor * -1 + xOffset,
                                    yScaleFactor * (plotDataOld.at(j + topLeftFrame[selectedPort]) - 2.5) + yOffset);
                }

                // save last point in waveform to join to next segment
                plotDataOld[j + topLeftFrame[selectedPort]] =
                        signalProcessor->supplyVoltage.at(stream).at((length / 60) - 1);

                // draw waveform
                painter.setPen(Qt::green);
                if (voltageLow) painter.setPen(Qt::yellow);
                if (voltageOutOfRange) painter.setPen(Qt::red);
                if (pointPlotMode) {
                    painter.drawPoints(polyline, (length / 60) + 1);
                } else {
                    painter.drawPolyline(polyline, (length / 60) + 1);
                }

            } else if (type == BoardAdcSignal) {
                // Plot USB interface board ADC input signal

                tStepMsec = 1000.0 / sampleRate;
                xScaleFactor = tAxisLength * tStepMsec / tScale;
                yScaleFactor = -(2.0 * yAxisLength) / 3.3;
                yOffset = frameList[numFramesIndex[selectedPort]][j].bottom();

                // build waveform
                for (i = 0; i < length; ++i) {
                    polyline[i+1] =
                            QPointF(xScaleFactor * i + xOffset,
                                    yScaleFactor * signalProcessor->boardAdc.at(channel).at(i) + yOffset);
                }

                // join to old waveform
                if (tPosition == 0.0) {
                    polyline[0] = polyline[1];
                } else {
                    polyline[0] =
                            QPointF(xScaleFactor * -1 + xOffset,
                                    yScaleFactor * plotDataOld.at(j + topLeftFrame[selectedPort]) + yOffset);
                }

                // save last point in waveform to join to next segment
                plotDataOld[j + topLeftFrame[selectedPort]] =
                        signalProcessor->boardAdc.at(channel).at(length - 1);

                // draw waveform
                painter.setPen(Qt::darkGreen);
                if (pointPlotMode) {
                    painter.drawPoints(polyline, length + 1);
                } else {
                    painter.drawPolyline(polyline, length + 1);
                }

            } else if (type == BoardDigInSignal) {
                // Plot USB interface board digital input signal

                tStepMsec = 1000.0 / sampleRate;
                xScaleFactor = tAxisLength * tStepMsec / tScale;
                yScaleFactor = -(2.0 * yAxisLength) / 2.0;
                yOffset = (frameList[numFramesIndex[selectedPort]][j].bottom() +
                           frameList[numFramesIndex[selectedPort]][j].center().y()) / 2.0;

                // build waveform
                for (i = 0; i < length; ++i) {
                    polyline[i+1] =
                            QPointF(xScaleFactor * i + xOffset,
                                    yScaleFactor * signalProcessor->boardDigIn.at(channel).at(i) + yOffset);
                }

                // join to old waveform
                if (tPosition == 0.0) {
                    polyline[0] = polyline[1];
                } else {
                    polyline[0] =
                            QPointF(xScaleFactor * -1 + xOffset,
                                    yScaleFactor * plotDataOld.at(j + topLeftFrame[selectedPort]) + yOffset);
                }

                // save last point in waveform to join to next segment
                plotDataOld[j + topLeftFrame[selectedPort]] =
                        signalProcessor->boardDigIn.at(channel).at(length - 1);

                // draw waveform
                QPen pen;
                pen.setColor(QColor(200, 50, 200));
                painter.setPen(pen);
                if (pointPlotMode) {
                    painter.drawPoints(polyline, length + 1);
                } else {
                    painter.drawPolyline(polyline, length + 1);
                }
            }
            painter.setClipping(false);
        }
    }

    tStepMsec = 1000.0 / sampleRate;
    tPosition += length * tStepMsec;
    if (tPosition >= tScale) {
        tPosition = 0.0;
    }

    delete [] polyline;
}

void WavePlot::refreshScreen()
{
    refreshPixmap();
    highlightFrame(selectedFrame[selectedPort], false);
}

// Switch to new port.
int WavePlot::setPort(int port)
{
    selectedPort = port;
    refreshScreen();
    intanUI->setNumWaveformsComboBox(numFramesIndex[port]);

    return numFramesIndex[selectedPort];
}

// Return custom (user-selected) name of selected channel.
QString WavePlot::getChannelName()
{
    return selectedChannel(selectedFrame[selectedPort])->customChannelName;
}

// Return custom (user-selected) name of specified channel.
QString WavePlot::getChannelName(int port, int index)
{
    return signalSources->signalPort[port].channelByIndex(index)->customChannelName;
}

// Return native name (e.g., "A-05") of selected channel.
QString WavePlot::getNativeChannelName()
{
    return selectedChannel(selectedFrame[selectedPort])->nativeChannelName;
}

// Return native name (e.g., "A-05") of specified channel.
QString WavePlot::getNativeChannelName(int port, int index)
{
    return signalSources->signalPort[port].channelByIndex(index)->nativeChannelName;
}

// Rename selected channel.
void WavePlot::setChannelName(QString name)
{
    signalSources->signalPort[selectedPort].channelByIndex(selectedFrame[selectedPort])->customChannelName = name;
    signalSources->signalPort[selectedPort].updateAlphabeticalOrder();
}

// Rename specified channel.
void WavePlot::setChannelName(QString name, int port, int index)
{
    signalSources->signalPort[port].channelByIndex(index)->customChannelName = name;
    signalSources->signalPort[selectedPort].updateAlphabeticalOrder();
}

void WavePlot::sortChannelsByName()
{
    signalSources->signalPort[selectedPort].setAlphabeticalChannelOrder();
}

void WavePlot::sortChannelsByNumber()
{
    signalSources->signalPort[selectedPort].setOriginalChannelOrder();
}

bool WavePlot::isSelectedChannelEnabled()
{
    return selectedChannel(selectedFrame[selectedPort])->enabled;
}

// Enable or disable selected channel.
void WavePlot::setSelectedChannelEnable(bool enabled)
{
    signalSources->signalPort[selectedPort].channelByIndex(selectedFrame[selectedPort])->enabled = enabled;
    refreshScreen();
}

// Toggle enable status of selected channel.
void WavePlot::toggleSelectedChannelEnable()
{
    if (!(intanUI->isRecording())) {
        setSelectedChannelEnable(!isSelectedChannelEnabled());
    }
}

// Enable all channels on currently selected port.
void WavePlot::enableAllChannels()
{
    for (int i = 0; i < signalSources->signalPort[selectedPort].numChannels(); ++i) {
        signalSources->signalPort[selectedPort].channelByNativeOrder(i)->enabled = true;
    }
    refreshScreen();
}

// Disable all channels on currently selected port.
void WavePlot::disableAllChannels()
{
    for (int i = 0; i < signalSources->signalPort[selectedPort].numChannels(); ++i) {
        signalSources->signalPort[selectedPort].channelByNativeOrder(i)->enabled = false;
    }
    refreshScreen();
}

// Update display when new data is available.
void WavePlot::passFilteredData()
{
    drawWaveforms();
    update();
}

// Enable or disable electrode impedance labels on display.
void WavePlot::setImpedanceLabels(bool enabled)
{
    impedanceLabels = enabled;
    refreshScreen();
}

// Enable or disable point plotting mode (to reduce CPU load).
void WavePlot::setPointPlotMode(bool enabled)
{
    pointPlotMode = enabled;
}

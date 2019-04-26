//  ------------------------------------------------------------------------
//
//  This file is part of the Intan Technologies RHD2000 Interface
//  Version 1.3
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

#include <QtGui>
#include <qmath.h>
#include <iostream>

#include "qtincludes.h"

#include "globalconstants.h"
#include "signalprocessor.h"
#include "signalchannel.h"
#include "spikescopedialog.h"
#include "spikeplot.h"

// The SpikePlot widget displays a triggered neural spike plot in the
// Spike Scope dialog.  Multiple spikes are plotted on top of one another
// so users may compare their shapes.  The RMS value of the waveform is
// displayed in the plot.  Users may select a new threshold value by clicking
// on the plot.  Keypresses are used to change the voltage scale of the plot.

SpikePlot::SpikePlot(SignalProcessor *inSignalProcessor, SignalChannel *initialChannel,
                     SpikeScopeDialog *inSpikeScopeDialog, QWidget *parent) :
    QWidget(parent)
{
    signalProcessor = inSignalProcessor;
    spikeScopeDialog = inSpikeScopeDialog;

    selectedChannel = initialChannel;
    startingNewChannel = true;
    rmsDisplayPeriod = 0;
    savedRms = 0.0;

    spikeWaveformIndex = 0;
    numSpikeWaveforms = 0;
    maxNumSpikeWaveforms = 20;
    voltageTriggerMode = true;
    voltageThreshold = 0;
    digitalTriggerChannel = 0;
    digitalEdgePolarity = true;

    setBackgroundRole(QPalette::Window);
    setAutoFillBackground(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setFocusPolicy(Qt::StrongFocus);

    int i;

    // We can plot up to 30 superimposed spike waveforms on the scope.
    spikeWaveform.resize(30);
    for (i = 0; i < spikeWaveform.size(); ++i) {
        // Each waveform is 3 ms in duration.  We need 91 time steps for a 3 ms
        // waveform with the sample rate is set to its maximum value of 30 kS/s.
        spikeWaveform[i].resize(91);
        spikeWaveform[i].fill(0.0);
    }

    // Buffers to hold recent history of spike waveform and digital input,
    // used to find trigger events.
    spikeWaveformBuffer.resize(10000);
    spikeWaveformBuffer.fill(0.0);
    digitalInputBuffer.resize(10000);
    digitalInputBuffer.fill(0);

    // Set up vectors of varying plot colors so that older waveforms
    // are plotted in low-contrast gray and new waveforms are plotted
    // in high-contrast blue.  Older signals fade away, like phosphor
    // traces on old-school CRT oscilloscopes.
    scopeColors.resize(3);
    scopeColors[0].resize(10);
    scopeColors[1].resize(20);
    scopeColors[2].resize(30);

    for (i = 6; i < 10; ++i) scopeColors[0][i] = Qt::blue;
    for (i = 3; i < 6; ++i) scopeColors[0][i] = Qt::darkGray;
    for (i = 0; i < 3; ++i) scopeColors[0][i] = Qt::lightGray;

    for (i = 12; i < 20; ++i) scopeColors[1][i] = Qt::blue;
    for (i = 6; i < 12; ++i) scopeColors[1][i] = Qt::darkGray;
    for (i = 0; i < 6; ++i) scopeColors[1][i] = Qt::lightGray;

    for (i = 18; i < 30; ++i) scopeColors[2][i] = Qt::blue;
    for (i = 9; i < 18; ++i) scopeColors[2][i] = Qt::darkGray;
    for (i = 0; i < 9; ++i) scopeColors[2][i] = Qt::lightGray;

    // Default values that may be overwritten.
    yScale = 5000;
    setSampleRate(30000.0);
}

// Set voltage scale.
void SpikePlot::setYScale(int newYScale)
{
    yScale = newYScale;
    initializeDisplay();
}

// Set waveform sample rate.
void SpikePlot::setSampleRate(double newSampleRate)
{
    // Calculate time step, in msec.
    tStepMsec = 1000.0 / newSampleRate;

    // Calculate number of time steps in 3 msec sample.
    totalTSteps = qCeil(3.0 / tStepMsec) + 1;

    // Calculate number of time steps in the 1 msec pre-trigger
    // display interval.
    preTriggerTSteps = qCeil(1.0 / tStepMsec);

    // Clear old waveforms since the sample rate has changed.
    numSpikeWaveforms = 0;
    startingNewChannel = true;
}

// Draw axis lines on display.
void SpikePlot::drawAxisLines()
{
    QPainter painter(&pixmap);
    painter.initFrom(this);

    painter.eraseRect(frame);

    painter.setPen(Qt::darkGray);

    // Draw box outline.
    painter.drawRect(frame);

    // Draw horizonal zero voltage line.
    painter.drawLine(frame.left(), frame.center().y(), frame.right(), frame.center().y());

    // Draw vertical lines at 0 ms and 1 ms.
    painter.drawLine(frame.left() + (1.0/3.0) * (frame.right() - frame.left()) + 1, frame.top(),
                      frame.left() + (1.0/3.0) * (frame.right() - frame.left()) + 1, frame.bottom());
    painter.drawLine(frame.left() + (2.0/3.0) * (frame.right() - frame.left()) + 1, frame.top(),
                      frame.left() + (2.0/3.0) * (frame.right() - frame.left()) + 1, frame.bottom());

    update();
}

// Draw text around axes.
void SpikePlot::drawAxisText()
{
    QPainter painter(&pixmap);
    painter.initFrom(this);
    const int textBoxWidth = painter.fontMetrics().width("+" + QString::number(yScale) + " " + QSTRING_MU_SYMBOL + "V");
    const int textBoxHeight = painter.fontMetrics().height();

    // Clear entire Widget display area.
    painter.eraseRect(rect());

    // Draw border around Widget display area.
    painter.setPen(Qt::darkGray);
    QRect rect(0, 0, width() - 1, height() - 1);
    painter.drawRect(rect);

    // If the selected channel is an amplifier channel, then write the channel name and number,
    // otherwise remind the user than non-amplifier channels cannot be displayed in Spike Scope.
    if (selectedChannel) {
        if (selectedChannel->signalType == AmplifierSignal) {
            painter.drawText(frame.right() - textBoxWidth - 1, frame.top() - textBoxHeight - 1,
                              textBoxWidth, textBoxHeight,
                              Qt::AlignRight | Qt::AlignBottom, selectedChannel->nativeChannelName);
            painter.drawText(frame.left() + 3, frame.top() - textBoxHeight - 1,
                              textBoxWidth, textBoxHeight,
                              Qt::AlignLeft | Qt::AlignBottom, selectedChannel->customChannelName);
        } else {
            painter.drawText(frame.right() - 2 * textBoxWidth - 1, frame.top() - textBoxHeight - 1,
                              2 * textBoxWidth, textBoxHeight,
                              Qt::AlignRight | Qt::AlignBottom, tr("ONLY AMPLIFIER CHANNELS CAN BE DISPLAYED"));
        }
    }

    // Label the voltage axis.
    painter.drawText(frame.left() - textBoxWidth - 2, frame.top() - 1,
                      textBoxWidth, textBoxHeight,
                      Qt::AlignRight | Qt::AlignTop,
                      "+" + QString::number(yScale) + " " + QSTRING_MU_SYMBOL + "V");
    painter.drawText(frame.left() - textBoxWidth - 2, frame.center().y() - textBoxHeight / 2,
                      textBoxWidth, textBoxHeight,
                      Qt::AlignRight | Qt::AlignVCenter, "0");
    painter.drawText(frame.left() - textBoxWidth - 2, frame.bottom() - textBoxHeight + 1,
                      textBoxWidth, textBoxHeight,
                      Qt::AlignRight | Qt::AlignBottom,
                      "-" + QString::number(yScale) + " " + QSTRING_MU_SYMBOL + "V");

    // Label the time axis.
    painter.drawText(frame.left() - textBoxWidth / 2, frame.bottom() + 1,
                      textBoxWidth, textBoxHeight,
                      Qt::AlignHCenter | Qt::AlignTop, "-1");
    painter.drawText(frame.left() + (1.0/3.0) * (frame.right() - frame.left()) + 1 - textBoxWidth / 2, frame.bottom() + 1,
                      textBoxWidth, textBoxHeight,
                      Qt::AlignHCenter | Qt::AlignTop, "0");
    painter.drawText(frame.left() + (2.0/3.0) * (frame.right() - frame.left()) + 1 - textBoxWidth / 2, frame.bottom() + 1,
                      textBoxWidth, textBoxHeight,
                      Qt::AlignHCenter | Qt::AlignTop, "1");
    painter.drawText(frame.right() - textBoxWidth + 1, frame.bottom() + 1,
                      textBoxWidth, textBoxHeight,
                      Qt::AlignRight | Qt::AlignTop, "2 ms");

    update();
}

// This function loads waveform data for the selected channel from the signal processor object,
// looks for trigger events, captures 3-ms snippets of the waveform after trigger events,
// measures the rms level of the waveform, and updates the display.
void SpikePlot::updateWaveform(int numBlocks)
{
    int i, index, index2;
    bool triggered;
    double rms;

    // Make sure the selected channel is a valid amplifier channel
    if (!selectedChannel) return;
    if (selectedChannel->signalType != AmplifierSignal) return;

    int stream = selectedChannel->boardStream;
    int channel = selectedChannel->chipChannel;

    // Load recent waveform data and digital input data into our buffers.  Also, calculate
    // waveform RMS value.
    rms = 0.0;
    for (i = 0; i < SAMPLES_PER_DATA_BLOCK * numBlocks; ++i) {
        spikeWaveformBuffer[i + totalTSteps - 1] = signalProcessor->amplifierPostFilter.at(stream).at(channel).at(i);
        rms += (signalProcessor->amplifierPostFilter.at(stream).at(channel).at(i) *
                signalProcessor->amplifierPostFilter.at(stream).at(channel).at(i));
        digitalInputBuffer[i + totalTSteps - 1] =  signalProcessor->boardDigIn.at(digitalTriggerChannel).at(i);
    }
    rms = qSqrt(rms / (SAMPLES_PER_DATA_BLOCK * numBlocks));

    // Find trigger events, and then copy waveform snippets to spikeWaveform vector.
    index = startingNewChannel ? (preTriggerTSteps + totalTSteps) : preTriggerTSteps;
    while (index <= SAMPLES_PER_DATA_BLOCK * numBlocks + totalTSteps - 1 - (totalTSteps - preTriggerTSteps)) {
        triggered = false;
        if (voltageTriggerMode) {
            if (voltageThreshold >= 0) {
                // Positive voltage threshold trigger
                if (spikeWaveformBuffer.at(index - 1) < voltageThreshold &&
                        spikeWaveformBuffer.at(index) >= voltageThreshold) {
                    triggered = true;
                }
            } else {
                // Negative voltage threshold trigger
                if (spikeWaveformBuffer.at(index - 1) > voltageThreshold &&
                        spikeWaveformBuffer.at(index) <= voltageThreshold) {
                    triggered = true;
                }
            }
        } else {
            if (digitalEdgePolarity) {
                // Digital rising edge trigger
                if (digitalInputBuffer.at(index - 1) == 0 &&
                        digitalInputBuffer.at(index) == 1) {
                    triggered = true;
                }
            } else {
                // Digital falling edge trigger
                if (digitalInputBuffer.at(index - 1) == 1 &&
                        digitalInputBuffer.at(index) == 0) {
                    triggered = true;
                }
            }
        }
        // If we found a trigger event, grab waveform snippet.
        if (triggered) {
            index2 = 0;
            for (i = index - preTriggerTSteps;
                 i < index + totalTSteps - preTriggerTSteps; ++i) {
                spikeWaveform[spikeWaveformIndex][index2++] = spikeWaveformBuffer.at(i);
            }
            if (++spikeWaveformIndex == spikeWaveform.size()) {
                spikeWaveformIndex = 0;
            }
            if (++numSpikeWaveforms > maxNumSpikeWaveforms) {
                numSpikeWaveforms = maxNumSpikeWaveforms;
            }
            index += totalTSteps - preTriggerTSteps;
        } else {
            ++index;
        }
    }

    // Copy tail end of waveform to beginning of spike waveform buffer, in case there is a spike
    // at the seam between two data blocks.
    index = 0;
    for (i = SAMPLES_PER_DATA_BLOCK * numBlocks - totalTSteps + 1;
         i < SAMPLES_PER_DATA_BLOCK * numBlocks; ++i) {
        spikeWaveformBuffer[index++] = signalProcessor->amplifierPostFilter.at(stream).at(channel).at(i);
    }

    if (startingNewChannel) startingNewChannel = false;

    // Update plot.
    updateSpikePlot(rms);
}

// Plots spike waveforms and writes RMS value to display.
void SpikePlot::updateSpikePlot(double rms)
{
    int i, j, xOffset, yOffset, index;
    double yAxisLength, tAxisLength;
    QRect adjustedFrame;
    double xScaleFactor, yScaleFactor;
    const double tScale = 3.0;  // time scale = 3.0 ms

    int colorIndex = 2;
    switch (maxNumSpikeWaveforms) {
    case 10: colorIndex = 0; break;
    case 20: colorIndex = 1; break;
    case 30: colorIndex = 2; break;
    }

    drawAxisLines();

    QPainter painter(&pixmap);
    painter.initFrom(this);

    // Vector for waveform plot points
    QPointF *polyline = new QPointF[totalTSteps];

    yAxisLength = (frame.height() - 2) / 2.0;
    tAxisLength = frame.width() - 1;

    xOffset = frame.left() + 1;

    // Set clipping region for plotting.
    adjustedFrame = frame;
    adjustedFrame.adjust(0, 1, 0, 0);
    painter.setClipRect(adjustedFrame);

    xScaleFactor = tAxisLength * tStepMsec / tScale;
    yScaleFactor = -yAxisLength / yScale;
    yOffset = frame.center().y();

    index = maxNumSpikeWaveforms - numSpikeWaveforms;
    for (j = spikeWaveformIndex - numSpikeWaveforms; j < spikeWaveformIndex; ++j) {
        // Build waveform
        for (i = 0; i < totalTSteps; ++i) {
            polyline[i] = QPointF(xScaleFactor * i + xOffset, yScaleFactor * spikeWaveform.at((j + 30) % spikeWaveform.size()).at(i) + yOffset);
        }

        // Draw waveform
        painter.setPen(scopeColors.at(colorIndex).at(index++));
        painter.drawPolyline(polyline, totalTSteps);
    }

    // If using a voltage threshold trigger, plot a line at the threshold level.
    if (voltageTriggerMode) {
        painter.setPen(Qt::red);
        painter.drawLine(xOffset, yScaleFactor * voltageThreshold + yOffset,
                          xScaleFactor * (totalTSteps - 1) +  xOffset, yScaleFactor * voltageThreshold + yOffset);
    }

    painter.setClipping(false);

    // Don't update the RMS value display every time, or it will change so fast that it
    // will be hard to read.  Only update once every few times we execute this function.
    if (rmsDisplayPeriod == 0) {
        rmsDisplayPeriod = 5;
        savedRms = rms;
    } else {
        --rmsDisplayPeriod;
    }

    // Write RMS value to display.
    const int textBoxWidth = 180;
    const int textBoxHeight = painter.fontMetrics().height();
    painter.setPen(Qt::darkGreen);
    painter.drawText(frame.left() + 6, frame.top() + 5,
                      textBoxWidth, textBoxHeight,
                      Qt::AlignLeft | Qt::AlignTop,
                      "RMS:" + QString::number(savedRms, 'f', (savedRms < 10.0) ? 1 : 0) +
                      " " + QSTRING_MU_SYMBOL + "V");

    delete [] polyline;
    update();
}

// If user clicks inside display, set voltage threshold to that level.
void SpikePlot::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (frame.contains(event->pos())) {
            int yMouse = event->pos().y();
            double newThreshold = yScale * (frame.center().y() - yMouse) / (frame.height() / 2);
            setVoltageThreshold(newThreshold);
            spikeScopeDialog->setVoltageThresholdDisplay(newThreshold);
            updateSpikePlot(0.0);
        }
    } else {
        QWidget::mousePressEvent(event);
    }
}

// If user spins mouse wheel, change voltage scale.
void SpikePlot::wheelEvent(QWheelEvent *event)
{
    if (event->delta() > 0) {
        spikeScopeDialog->contractYScale();
    } else {
        spikeScopeDialog->expandYScale();
    }
}

// Keypresses to change voltage scale.
void SpikePlot::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Minus:
    case Qt::Key_Underscore:
        spikeScopeDialog->contractYScale();
        break;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        spikeScopeDialog->expandYScale();
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

QSize SpikePlot::minimumSizeHint() const
{
    return QSize(SPIKEPLOT_X_SIZE, SPIKEPLOT_Y_SIZE);
}

QSize SpikePlot::sizeHint() const
{
    return QSize(SPIKEPLOT_X_SIZE, SPIKEPLOT_Y_SIZE);
}

void SpikePlot::paintEvent(QPaintEvent * /* event */)
{
    QStylePainter stylePainter(this);
    stylePainter.drawPixmap(0, 0, pixmap);
}

void SpikePlot::closeEvent(QCloseEvent *event)
{
    // Perform any clean-up here before application closes.
    event->accept();
}

// Set the number of spikes that are plotted, superimposed, on the
// display.
void SpikePlot::setMaxNumSpikeWaveforms(int num)
{
    maxNumSpikeWaveforms = num;
    numSpikeWaveforms = 0;
}

// Clear spike display.
void SpikePlot::clearScope()
{
    numSpikeWaveforms = 0;
    drawAxisLines();
}

// Select voltage threshold trigger mode if voltageMode == true, otherwise
// select digital input trigger mode.
void SpikePlot::setVoltageTriggerMode(bool voltageMode)
{
    voltageTriggerMode = voltageMode;
    if (selectedChannel->signalType == AmplifierSignal) {
        selectedChannel->voltageTriggerMode = voltageMode;
    }
    updateSpikePlot(0.0);
}

// Set voltage threshold trigger level.  We use integer threshold
// levels (in microvolts) since there is no point going to fractional
// microvolt accuracy.
void SpikePlot::setVoltageThreshold(int threshold)
{
    voltageThreshold = threshold;
    if (selectedChannel->signalType == AmplifierSignal) {
        selectedChannel->voltageThreshold = threshold;
    }
}

// Select digital input channel for digital input trigger.
void SpikePlot::setDigitalTriggerChannel(int channel)
{
    digitalTriggerChannel = channel;
    if (selectedChannel->signalType == AmplifierSignal) {
        selectedChannel->digitalTriggerChannel = channel;
    }
}

// Set digitial trigger edge polarity to rising or falling edge.
void SpikePlot::setDigitalEdgePolarity(bool risingEdge)
{
    digitalEdgePolarity = risingEdge;
    if (selectedChannel->signalType == AmplifierSignal) {
        selectedChannel->digitalEdgePolarity = risingEdge;
    }
}


// Change to a new signal channel.
void SpikePlot::setNewChannel(SignalChannel* newChannel)
{
    selectedChannel = newChannel;
    numSpikeWaveforms = 0;
    startingNewChannel = true;
    rmsDisplayPeriod = 0;

    voltageTriggerMode = selectedChannel->voltageTriggerMode;
    voltageThreshold = selectedChannel->voltageThreshold;
    digitalTriggerChannel = selectedChannel->digitalTriggerChannel;
    digitalEdgePolarity = selectedChannel->digitalEdgePolarity;

    initializeDisplay();
}

void SpikePlot::resizeEvent(QResizeEvent*) {
    // Pixel map used for double buffering.
    pixmap = QPixmap(size());
    pixmap.fill(this, 0, 0);
    initializeDisplay();
}

void SpikePlot::initializeDisplay() {
    const int textBoxWidth = fontMetrics().width("+" + QString::number(yScale) + " " + QSTRING_MU_SYMBOL + "V");
    const int textBoxHeight = fontMetrics().height();
    frame = rect();
    frame.adjust(textBoxWidth + 5, textBoxHeight + 10, -8, -textBoxHeight - 10);

    // Initialize display.
    drawAxisText();
    drawAxisLines();
}

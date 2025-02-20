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

#include <cmath>
#include "abstractrhxcontroller.h"
#include "rhxglobals.h"
#include "spikeplot.h"

SpikePlot::SpikePlot(SystemState* state_, QWidget *parent) :
    QWidget(parent),
    state(state_),
    channel(nullptr),
    history(nullptr)
{
    connect(state, SIGNAL(stateChanged()), this, SLOT(updateFromState()));

    setBackgroundRole(QPalette::Window);
    setAutoFillBackground(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setFocusPolicy(Qt::StrongFocus);

    samplesPreDetect = 300;
    samplesPostDetect = 600;

    latestRmsCalculation = 0.0;
    latestSpikeRateCalculation = 0;

    tStepMsec = 1000.0 / state->sampleRate->getNumericValue();

    image = QImage(size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
}

SpikePlot::~SpikePlot()
{
    if (!spikeHistoryMap.empty()) {
        std::map<std::string, SpikePlotHistory*>::const_iterator it = spikeHistoryMap.begin();
        while (it != spikeHistoryMap.end()) {
            delete it->second;
            ++it;
        }
    }
}

void SpikePlot::setWaveform(const std::string& waveName)
{
    channel = state->signalSources->channelByName(waveName);
    if (channel) {
        state->spikeScopeChannel->setValue(QString::fromStdString(waveName));
    } else {
        state->spikeScopeChannel->setValue("N/A");
    }

    std::map<std::string, SpikePlotHistory*>::const_iterator it = spikeHistoryMap.find(waveName);
    if (it == spikeHistoryMap.end()) {  // If data structure for this waveform does not already exist...
        spikeHistoryMap[waveName] = new SpikePlotHistory;  // ...add new spike history data structure.
        it = spikeHistoryMap.find(waveName);
    }
    history = it->second;
}

QString SpikePlot::getWaveform()
{
    if (channel) {
        return channel->getNativeName();
    } else {
        return "";
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

void SpikePlot::updateFromState()
{
    update();
}

void SpikePlot::paintEvent(QPaintEvent * /* event */)
{
    QPainter painter(&image);

    // Clear old display.
    QRect imageFrame(rect());
    painter.fillRect(imageFrame, QBrush(Qt::black));

    // Draw border around Widget display area.
    painter.setPen(Qt::darkGray);
    painter.drawRect(QRect(0, 0, width() - 1, height() - 1));

    const int TextBoxWidth = fontMetrics().horizontalAdvance("+5000 " + MicroVoltsSymbol);
    const int TextBoxHeight = fontMetrics().height();
    scopeFrame = imageFrame.adjusted(TextBoxWidth + 5, TextBoxHeight + 10, -8, -TextBoxHeight - 10);
    updateCoordinateTranslator();
    painter.drawRect(ct.borderRect());    // Draw borders of scope.
//    painter.setClipRect(ct.frame()); // Allow plotting on top of borders.
    painter.setClipRect(ct.clippingRect()); // Keep plotting just inside borders.

    PlotDecorator plotDecorator(painter);

    // Vector for waveform plot points
    int snippetLength = samplesPreDetect + samplesPostDetect;
    QPointF *polyline = new QPointF[snippetLength];

    double tScale = state->tScaleSpikeScope->getNumericValue();

    if (history) {
        bool showArtifacts = state->artifactsShown->getValue();

        // Draw snapshot waveforms, if any exist.
        painter.setPen(SnapshotColor);
        int length = (int) history->snapshotSnippets.size();
        for (int i = 0; i < length; ++i) {
            double time = -samplesPreDetect * tStepMsec;
            for (int t = 0; t < snippetLength; ++t) {
                polyline[t] = QPointF(ct.screenXFromRealX(time), ct.screenYFromRealY(history->snapshotSnippets[i][t]));
                time += tStepMsec;
            }
            if ((history->snapshotSpikeIds[i] != (int) SpikeIdLikelyArtifact) || showArtifacts) {
                painter.drawPolyline(polyline, snippetLength);
            }
        }

        // Draw current spike waveforms.
        length = (int) history->snippets.size();
        for (int i = 0; i < length; ++i) {
            double time = -samplesPreDetect * tStepMsec;
            for (int t = 0; t < snippetLength; ++t) {
                polyline[t] = QPointF(ct.screenXFromRealX(time), ct.screenYFromRealY(history->snippets[i][t]));
                time += tStepMsec;
            }
            int value = 255 - (int)(200.0 * ((double)(length - i - 1)) / ((double)length));
            if (history->spikeIds[i] == (int) SpikeIdUnclassifiedSpike) {
                painter.setPen(QColor(value, 0, 0));
                painter.drawPolyline(polyline, snippetLength);
            } else if (history->spikeIds[i] == (int) SpikeIdLikelyArtifact) {
                painter.setPen(QColor(0, 0, value));
                if (showArtifacts) {
                    painter.drawPolyline(polyline, snippetLength);
                }
            } else {
                painter.setPen(QColor(value, value, value));
                painter.drawPolyline(polyline, snippetLength);
            }
        }
    }

    delete [] polyline;

    painter.setClipping(false);

    bool showArtifactThreshold = state->suppressionEnabled->getValue();
    if (showArtifactThreshold) {
        int artifactThreshold = state->suppressionThreshold->getValue();
        if (artifactThreshold <= ct.yMaxReal()) {
            painter.setPen(Qt::blue);
            plotDecorator.drawHorizontalAxisLine(ct, artifactThreshold);
            plotDecorator.drawHorizontalAxisLine(ct, -artifactThreshold);
            plotDecorator.drawLabeledTickMarkLeft(artifactThreshold, ct, artifactThreshold, 0);
            plotDecorator.drawLabeledTickMarkLeft(-artifactThreshold, ct, -artifactThreshold, 0);
        }
    }

    // Write waveform name.
    if (channel) {
        painter.setPen(Qt::white);
        plotDecorator.writeLabel(channel->getNativeAndCustomNames(), ct.xLeft() + 2, ct.yTop() - 1,
                                 Qt::AlignLeft | Qt::AlignBottom);
    }

    // Draw horizonal axis lines.
    painter.setPen(Qt::white);
    plotDecorator.drawHorizontalAxisLine(ct, 0.0);

    // Write voltage axis labels.
    painter.setPen(Qt::white);
    plotDecorator.drawLabeledTickMarkLeft(0, ct, 0.0, 0);
    plotDecorator.drawLabeledTickMarkLeft("+" + QString::number(ct.yMaxReal()) + " " + MicroVoltsSymbol, ct, ct.yMaxReal(), 0);
    plotDecorator.drawLabeledTickMarkLeft(QString::number(ct.yMinReal()) + " " + MicroVoltsSymbol, ct, ct.yMinReal(), 0);

    // Draw vertical axis lines.
    int tMax = qFloor(ct.xMaxReal());
    for (int t = qCeil(ct.xMinReal()); t <= tMax; ++t) {
        (t == 0) ? painter.setPen(Qt::white) : painter.setPen(Qt::darkGray);

        // For tScale of 2, 4, or 6 ms, draw a vertical line every 1 ms.
        // For tScale of 10 ms, draw a vertical line every 2 ms.
        // For tScale of 16 or 20 ms, draw a vertical line every 4 ms.
        int divisor = 1;
        switch ((int) tScale) {
        case 2:
        case 4:
        case 6:
            divisor = 1;
            break;

        case 10:
            divisor = 2;
            break;

        case 16:
        case 20:
            divisor = 4;
            break;
        }

        // Only draw a vertical line at the far-left border t, the far-right border t, or a t divisible by divisor.
        // For example, don't draw t = 3 ms with tScale of 20 ms (divisor = 4), but do draw t = 4 ms.
        if (t != ct.xMinReal() && t != ct.xMaxReal() && t % divisor != 0)
            continue;  // This t line is not important enough to draw, so move on to next iteration of for loop.

        plotDecorator.drawVerticalAxisLine(ct, t);

        // Write time axis labels.
        painter.setPen(Qt::white);
        plotDecorator.drawLabeledTickMarkBottom(QString::number(t) + ((t == tMax) ? " ms" : ""), ct, t, 0, t == tMax);
    }

    // Draw horizonal threshold line.
    double vThreshold = 0.0;
    if (channel) {
        vThreshold = channel->getSpikeThreshold();
    }
    painter.setPen(Qt::red);
    plotDecorator.drawHorizontalAxisLine(ct, vThreshold);
    plotDecorator.drawLabeledTickMarkLeft(vThreshold, ct, vThreshold, 0);

    // Write RMS value to display.
    const int textBoxWidth = 180;
    const int textBoxHeight = painter.fontMetrics().height();
    QString rmsText = "RMS: " + QString::number(latestRmsCalculation, 'f', (latestRmsCalculation < 9.95) ? 1 : 0) +
            " " + MicroVoltsSymbol;
    if (latestSpikeRateCalculation > 0) {
        rmsText += "  " + QString::number(latestSpikeRateCalculation);
        if (latestSpikeRateCalculation == 1) {
            rmsText += " spike/s";
        } else {
            rmsText += " spikes/s";
        }
    }
    painter.setPen(Qt::darkGreen);
    painter.drawText(scopeFrame.left() + 6, scopeFrame.top() + 5, textBoxWidth, textBoxHeight,
                     Qt::AlignLeft | Qt::AlignTop, rmsText);

    QStylePainter stylePainter(this);
    stylePainter.drawImage(0, 0, image);
}

void SpikePlot::closeEvent(QCloseEvent *event)
{
    event->accept();
}

void SpikePlot::mousePressEvent(QMouseEvent* event)  // Note: Must setMouseTracking(true) for this to work.
{
    if (event->button() == Qt::LeftButton) {
        if (scopeFrame.contains(event->pos())) {
            updateCoordinateTranslator();
            int newThreshold = qRound(ct.realYFromScreenY(event->pos().y()));
            if (channel) {
                channel->setSpikeThreshold(newThreshold);
            }
            update();
        } else {
            QWidget::mousePressEvent(event);
        }
    } else {
        QWidget::mousePressEvent(event);
    }
}

void SpikePlot::wheelEvent(QWheelEvent* event)
{
    bool shiftHeld = event->modifiers() & Qt::ShiftModifier;
    bool controlHeld = event->modifiers() & Qt::ControlModifier;
    int delta = event->angleDelta().y();

    if (!shiftHeld && !controlHeld) {
        if (delta > 0) {
            state->yScaleSpikeScope->decrementIndex();
        } else if (delta < 0) {
            state->yScaleSpikeScope->incrementIndex();
        }
    } else if (shiftHeld && !controlHeld) {
        if (delta > 0) {
            state->tScaleSpikeScope->decrementIndex();
        } else if (delta < 0) {
            state->tScaleSpikeScope->incrementIndex();
        }
    } else if (!shiftHeld && controlHeld) {
        int threshold = 0;
        if (channel) {
            threshold = channel->getSpikeThreshold();
        }
        if (delta > 0) {
            if (channel) {
                channel->setSpikeThreshold(threshold + 5);
            }
        } else if (delta < 0) {
            if (channel) {
                channel->setSpikeThreshold(threshold - 5);
            }
        }
    }
}

void SpikePlot::keyPressEvent(QKeyEvent* event)
{
    if (!(event->isAutoRepeat())) { // Ignore additional 'keypresses' from auto-repeat if key is held down.
        switch (event->key()) {

        case Qt::Key_Comma:
        case Qt::Key_Less:
            state->tScaleSpikeScope->decrementIndex();
            break;

        case Qt::Key_Period:
        case Qt::Key_Greater:
            state->tScaleSpikeScope->incrementIndex();
            break;

        case Qt::Key_Minus:
        case Qt::Key_Underscore:
            state->yScaleSpikeScope->incrementIndex();
            break;

        case Qt::Key_Plus:
        case Qt::Key_Equal:
            state->yScaleSpikeScope->decrementIndex();
            break;

        case Qt::Key_Delete:
            clearSpikes();
            break;
        }
    }
}

void SpikePlot::resizeEvent(QResizeEvent*) {
    image = QImage(size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    update();
}

bool SpikePlot::updateWaveforms(WaveformFifo* waveformFifo, int numSamples)
{
    if (!channel || !history)
        return false;

    if (!waveformFifo->gpuWaveformPresent(channel->getNativeNameString() + "|HIGH")) return false;

    uint16_t* spikeRaster = waveformFifo->getDigitalWaveformPointer(channel->getNativeNameString() + "|SPK");
    GpuWaveformAddress waveformAddress = waveformFifo->getGpuWaveformAddress(channel->getNativeNameString() + "|HIGH");

    if (!spikeRaster || waveformAddress.waveformIndex < 0) return false;

    int offset = samplesPostDetect - 1;
    int tStart = -offset;
    int numWordsInMemory = waveformFifo->numWordsInMemory(WaveformFifo::ReaderDisplay);
    if (offset > numWordsInMemory) {
        tStart = 0;
    }
    bool showArtifacts = state->artifactsShown->getValue();
    int numSpikesDisplayed = (int) state->numSpikesDisplayed->getNumericValue();
    int spikeId;
    for (int t = tStart; t < numSamples - offset; ++t) {
        spikeId = (int) waveformFifo->getDigitalData(WaveformFifo::ReaderDisplay, spikeRaster, t);
        if (spikeId != SpikeIdNoSpike && (t - samplesPreDetect >= -numWordsInMemory)) {
            if (showArtifacts || spikeId != SpikeIdLikelyArtifact) {
                std::vector<float> newSnippet(samplesPreDetect + samplesPostDetect);
                int index = 0;
                for (int i = t - samplesPreDetect; i < t + samplesPostDetect; ++i) {
                    newSnippet[index++] = waveformFifo->getGpuAmplifierData(WaveformFifo::ReaderDisplay, waveformAddress, i);
                }
                history->snippets.push_back(newSnippet);
                history->spikeIds.push_back(spikeId);
                while ((int) history->snippets.size() > numSpikesDisplayed) {
                    history->snippets.pop_front();
                    history->spikeIds.pop_front();
                }
            }
        }
    }

    // Calculate RMS level from recent waveform data.
    latestRmsCalculation = 0.0;
    latestSpikeRateCalculation = 0;
    if (numWordsInMemory > 0) {
        int numWordsForRms = std::min(numWordsInMemory, (int)ceil(state->sampleRate->getNumericValue()));  // Last one second of data.
        int numSamples = 0;
        int numSpikes = 0;
        double sumOfSquares = 0.0;
        for (int t = -numWordsForRms; t < 0; ++t) {
            float sample = waveformFifo->getGpuAmplifierData(WaveformFifo::ReaderDisplay, waveformAddress, t);
            sumOfSquares += sample * sample;
            int spikeId = waveformFifo->getDigitalData(WaveformFifo::ReaderDisplay, spikeRaster, t);
            if (spikeId != SpikeIdNoSpike && spikeId != SpikeIdLikelyArtifact) {
                ++numSpikes;
            }
            ++numSamples;
        }
        latestRmsCalculation = sqrt(sumOfSquares / (double)numSamples);
        latestSpikeRateCalculation = numSpikes;
    }

    update();
    return true;
}

void SpikePlot::clearSpikes()
{
    if (!history) return;
    history->snippets.clear();
    history->spikeIds.clear();
    update();
}

void SpikePlot::takeSnapshot()
{
    if (!history) return;
    history->snapshotSnippets = history->snippets;
    history->snapshotSpikeIds = history->spikeIds;
    update();
}

void SpikePlot::clearSnapshot()
{
    if (!history) return;
    history->snapshotSnippets.clear();
    history->snapshotSpikeIds.clear();
    update();
}

void SpikePlot::updateCoordinateTranslator()
{
    double tMax = state->tScaleSpikeScope->getNumericValue();
    double tMin = -tMax / 2.0;
    double vScale = state->yScaleSpikeScope->getNumericValue();
    ct.set(scopeFrame, tMin, tMax, -vScale, vScale);
}

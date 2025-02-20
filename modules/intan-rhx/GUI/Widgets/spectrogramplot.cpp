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

#include "matfilewriter.h"
#include "spectrogramplot.h"

SpectrogramPlot::SpectrogramPlot(SystemState* state_, QWidget * parent) :
    QWidget(parent),
    state(state_)
{
    setBackgroundRole(QPalette::Window);
    setAutoFillBackground(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);  // If we don't do this, mouseMoveEvent() is active only when mouse button is pressed.

    psdScaleMin = -1.7;
    psdScaleMax = 3.5;
    colorScale = new ColorScale(psdScaleMin, psdScaleMax);
    psdUnitsMicro = " " + MicroVoltsSymbol + "/" + SqrtSymbol + "Hz";
    lastMouseWasInFrame = false;

    int maxFftSize = (int) state->fftSizeSpectrogram->getNumericValue(state->fftSizeSpectrogram->numberOfItems() - 1);
    fftInputBuffer = new float [maxFftSize];

    fftEngine = new FastFourierTransform(state->sampleRate->getNumericValue());
    setNewFftSize((int) state->fftSizeSpectrogram->getNumericValue());
    setNewTimeScale(state->tScaleSpectrogram->getNumericValue());
    resetSpectrogram();

    image = QImage(size(), QImage::Format_ARGB32_Premultiplied);
}

SpectrogramPlot::~SpectrogramPlot()
{
    delete [] fftInputBuffer;
    delete fftEngine;
    delete colorScale;
}

void SpectrogramPlot::setWaveform(const std::string& waveName_)
{
    if (waveName == waveName_) return;

    waveName = waveName_;
    resetSpectrogram();
    update();
}

QString SpectrogramPlot::getWaveform() const
{
    return QString::fromStdString(waveName);
}

QSize SpectrogramPlot::minimumSizeHint() const
{
    return QSize(SPECPLOT_X_SIZE, SPECPLOT_Y_SIZE);
}

QSize SpectrogramPlot::sizeHint() const
{
    return QSize(SPECPLOT_X_SIZE, SPECPLOT_Y_SIZE);
}

void SpectrogramPlot::updateFromState()
{
    bool spectrogramSizeChanged = false;

    if (fftSize != (int) state->fftSizeSpectrogram->getNumericValue()) {
        setNewFftSize((int) state->fftSizeSpectrogram->getNumericValue());
        setNewTimeScale(state->tScaleSpectrogram->getNumericValue());
        spectrogramSizeChanged = true;
    } else if (tScale != state->tScaleSpectrogram->getNumericValue()) {
        setNewTimeScale(state->tScaleSpectrogram->getNumericValue());
        spectrogramSizeChanged = true;
    }

    if (fMinIndex != state->fMinSpectrogram->getValue() || fMaxIndex != state->fMaxSpectrogram->getValue()) {
        updateFMinMaxIndex();
    }

    if (spectrogramSizeChanged) {
        resetSpectrogram();
    }

    update();
}

void SpectrogramPlot::setNewFftSize(int fftSize_)
{
    fftSize = fftSize_;
    fftEngine->setLength(fftSize);
    int fSize = fftSize/2 + 1;
    fMinIndex = 0;
    fMaxIndex = fSize - 1;
    frequencyScale.resize(fSize);
    for (int i = 0; i < fSize; ++i) {
        frequencyScale[i] = fftEngine->getFrequency(i);
    }
    updateFMinMaxIndex();
}

void SpectrogramPlot::updateFMinMaxIndex()
{
    float fMin = (float) state->fMinSpectrogram->getValue();
    float fMax = (float) state->fMaxSpectrogram->getValue();

    int fSize = (int) frequencyScale.size();
    fMinIndex = 0;
    fMaxIndex = fSize - 1;
    for (int i = 0; i < fSize; ++i) {
        if (frequencyScale[i] <= fMin) fMinIndex = i;
    }
    for (int i = fSize - 1; i >= 0; --i) {
        if (frequencyScale[i] >= fMax) fMaxIndex = i;
    }
}

void SpectrogramPlot::setNewTimeScale(double tScale_)
{
    tScale = tScale_;
    tSize = qCeil(tScale * state->sampleRate->getNumericValue() / (double)(fftSize / 2));
    tStep = (double)(fftSize / 2) / state->sampleRate->getNumericValue();
    timeScale.resize(tSize);
    for (int i = 0; i < tSize; ++i) {
        timeScale[i] = i * tStep;
    }
}

void SpectrogramPlot::resetSpectrogram()
{
    for (int i = 0; i < (int) psdSpectrogram.size(); ++i) {
        psdSpectrogram[i].clear();
    }
    psdSpectrogram.resize(tSize);
    int fSize = (int) frequencyScale.size();
    for (int i = 0; i < tSize; ++i) {
        psdSpectrogram[i].resize(fSize);
        fill(psdSpectrogram[i].begin(), psdSpectrogram[i].end(), -10.0F);
    }
    psdSpectrum.clear();
    psdSpectrum.resize(fSize);
    fill(psdSpectrum.begin(), psdSpectrum.end(), -10.0F);

    psdRawImage = QImage(QSize(tSize, fSize), QImage::Format_ARGB32_Premultiplied);
    psdRawImage.fill(Qt::darkGray);

    tIndex = 0;
    numValidTStepsInSpectrogram = 0;
    spectrogramFull = false;

    amplifierWaveformQueue.clear();
    amplifierWaveformRecordQueue.clear();
    digitalWaveformQueue.clear();
    waveformTimeStampQueue.clear();
}

void SpectrogramPlot::keyPressEvent(QKeyEvent* event)
{
    if (!(event->isAutoRepeat())) { // Ignore additional 'keypresses' from auto-repeat if key is held down.
        switch (event->key()) {

        case Qt::Key_Comma:
        case Qt::Key_Less:
            state->tScaleSpectrogram->decrementIndex();
            break;

        case Qt::Key_Period:
        case Qt::Key_Greater:
            state->tScaleSpectrogram->incrementIndex();
            break;

        case Qt::Key_Minus:
        case Qt::Key_Underscore:
            state->fftSizeSpectrogram->decrementIndex();
            break;

        case Qt::Key_Plus:
        case Qt::Key_Equal:
            state->fftSizeSpectrogram->incrementIndex();
            break;

        case Qt::Key_Delete:
            resetSpectrogram();
            break;
        }
    }
}

bool SpectrogramPlot::updateWaveforms(WaveformFifo* waveformFifo, int numSamples)
{
    if (!waveformFifo->gpuWaveformPresent(waveName + "|WIDE")) return false;
    GpuWaveformAddress waveformAddress = waveformFifo->getGpuWaveformAddress(waveName + "|WIDE");
    if (waveformAddress.waveformIndex < 0) return false;

    QString digitalChannelName = state->digitalDisplaySpectrogram->getValueString();
    bool useAnalogAsDigital = digitalChannelName.left(1).toUpper() == "A";

    if (!useAnalogAsDigital) {  // Get digital signal
        uint16_t* digitalInWaveform = waveformFifo->getDigitalWaveformPointer("DIGITAL-IN-WORD");
        for (int t = 0; t < numSamples; ++t) {
            digitalWaveformQueue.push_back(waveformFifo->getDigitalData(WaveformFifo::ReaderDisplay, digitalInWaveform, t));
        }
    } else {  // Get thresholded analog signal as digital signal
        float* analogInWaveform = waveformFifo->getAnalogWaveformPointer(digitalChannelName.toStdString());
        float logicThreshold = (float)state->triggerAnalogVoltageThreshold->getValue();
        for (int t = 0; t < numSamples; ++t) {
            digitalWaveformQueue.push_back(waveformFifo->getAnalogDataAsDigital(WaveformFifo::ReaderDisplay,
                                                                                analogInWaveform, t, logicThreshold));
        }
    }

    for (int t = 0; t < numSamples; ++t) {
        amplifierWaveformQueue.push_back(waveformFifo->getGpuAmplifierData(WaveformFifo::ReaderDisplay, waveformAddress, t));
        amplifierWaveformRecordQueue.push_back(waveformFifo->getGpuAmplifierData(WaveformFifo::ReaderDisplay, waveformAddress, t));
        waveformTimeStampQueue.push_back(waveformFifo->getTimeStamp(WaveformFifo::ReaderDisplay, t));
    }

    float* fftOut;
    while ((int) amplifierWaveformQueue.size() >= fftSize) {
        for (int tIndex = 0; tIndex < fftSize; ++tIndex) {
            fftInputBuffer[tIndex] = amplifierWaveformQueue[tIndex];  // Copy N samples for FFT.
        }

        for (int i = 0; i < fftSize / 2; ++i) {    // Advance window by N/2 samples.
            amplifierWaveformQueue.pop_front();
        }
        if (spectrogramFull) {
            for (int i = 0; i < fftSize / 2; ++i) {
                amplifierWaveformRecordQueue.pop_front();
                waveformTimeStampQueue.pop_front();
                digitalWaveformQueue.pop_front();
            }
        }

        fftOut = fftEngine->logSqrtPowerSpectralDensity(fftInputBuffer);  // Calculate FFT and PSD.

        int fSize = (int) frequencyScale.size();
        for (int fIndex = 0; fIndex < fSize; ++fIndex) {
            psdSpectrum[fIndex] = fftOut[fIndex];
            psdSpectrogram[tIndex][fIndex] = fftOut[fIndex];   // Read out results of power spectral density (PSD).
        }

        QPainter psdPainter(&psdRawImage);
        psdPainter.drawImage(QRect(0, 0, tSize - 1, fSize), psdRawImage, QRect(1, 0, tSize - 1, fSize));  // shift existing PSD
        for (int fIndex = 0; fIndex < fSize; ++fIndex) {
            // Draw new PSD column.
            psdRawImage.setPixelColor(tSize - 1, fSize - fIndex - 1, colorScale->getColor(psdSpectrogram[tIndex][fIndex]));
        }

        if (++tIndex == tSize) {
            tIndex = 0;
            spectrogramFull = true;
        }
        if (++numValidTStepsInSpectrogram > tSize) numValidTStepsInSpectrogram = tSize;
    }

    update();
    return true;
}

void SpectrogramPlot::resizeEvent(QResizeEvent* /* event */) {
    image = QImage(size(), QImage::Format_ARGB32_Premultiplied);
    update();
}

void SpectrogramPlot::mouseMoveEvent(QMouseEvent* event)  // Note: Must setMouseTracking(true) for this to work.
{
    bool mouseIsInFrame = scopeFrame.contains(event->pos());
    if (mouseIsInFrame || lastMouseWasInFrame) update();
    lastMouseWasInFrame = mouseIsInFrame;
}

void SpectrogramPlot::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(&image);
    QRect imageFrame(rect());
    painter.fillRect(imageFrame, QBrush(Qt::black));
    QPoint cursor = mapFromGlobal(QCursor::pos());  // Get current cursor position.

    PlotDecorator plotDecorator(painter);

    const int TickMarkLength = 5;
    const int TickMarkMinorLength = 3;

    double fMin = state->fMinSpectrogram->getValue();
    double fMax = state->fMaxSpectrogram->getValue();

    if (state->displayModeSpectrogram->getValue() == "Spectrogram") {
        // Plot spectrogram (frequency vs. time with color-coded amplitude).

        int digitalFrameHeight = 0;
        int digitalFrameTopMargin = 0;
        int leftMargin = fontMetrics().horizontalAdvance("10000 Hz") + TickMarkLength + 10;
        int rightMargin = fontMetrics().horizontalAdvance("1000" + psdUnitsMicro) + TickMarkLength + 10;
        int bottomMargin = fontMetrics().height() + TickMarkLength + 10;
        int topMargin = fontMetrics().height() + 10;
        int colorBarWidth = 12;
        int colorBarSpacing = 12;
        bool drawDigital = state->digitalDisplaySpectrogram->getValue().toLower() != "none";
        if (drawDigital) {
            digitalFrameHeight = 32;
            digitalFrameTopMargin = fontMetrics().height() + 8;
            topMargin += digitalFrameTopMargin + digitalFrameHeight;
        }

        scopeFrame = QRect(imageFrame.adjusted(leftMargin, topMargin, -(rightMargin + colorBarSpacing + colorBarWidth), -bottomMargin));
        QRect digitalFrame(scopeFrame.left(), digitalFrameTopMargin, scopeFrame.width(), digitalFrameHeight);
        CoordinateTranslator ct(scopeFrame, 0.0, tScale, frequencyScale[fMinIndex], frequencyScale[fMaxIndex]);
        CoordinateTranslator ctDigital(digitalFrame, 0.0, tScale, -0.5, 1.5);

        if (drawDigital) {
            // Draw digital waveform frame.
            painter.setPen(Qt::white);
            painter.drawRect(digitalFrame);
            plotDecorator.writeLabel(state->digitalDisplaySpectrogram->getDisplayValueString(),
                                     ctDigital.xLeft() + 2, ctDigital.yTop() - 1, Qt::AlignLeft | Qt::AlignBottom);
            // Draw digital waveform.
            if (numValidTStepsInSpectrogram != 0) {
                const int YDigitalZero = ctDigital.screenYFromRealY(0);
                const int YDigitalOne = ctDigital.screenYFromRealY(1);
                const int DigitalPlotSize = digitalFrame.width() - 2;

                int numHalfFftWindows = numValidTStepsInSpectrogram;
                int numDigitalSamples = numHalfFftWindows * (fftSize / 2);
                int maxNumHalfFfts = tSize;
                int endX = digitalFrame.left() + 1 + DigitalPlotSize;
                int startX = endX - DigitalPlotSize * ((double)numHalfFftWindows / (double)maxNumHalfFfts);
                CoordinateTranslator1D ctDigWave(startX, endX + 1, fftSize / 4, fftSize / 4 + numDigitalSamples);

                painter.setPen(Qt::green);

                QString digitalChannelName = state->digitalDisplaySpectrogram->getValueString();
                bool useAnalogAsDigital = digitalChannelName.left(1).toUpper() == "A";
                uint16_t digChannelMask = 0x01u;
                if (!useAnalogAsDigital) digChannelMask = 0x01u << (int)state->digitalDisplaySpectrogram->getNumericValue();

                bool edgeDetected = false;
                bool lastSampleWasOne = false;
                int jBegin;
                int jEnd = qRound(ctDigWave.realFromScreen(startX));
                for (int i = startX; i < endX + 1; ++i) {
                    jBegin = jEnd;
                    jEnd = qRound(ctDigWave.realFromScreen(i + 1));
                    if (digitalWaveformQueue[jBegin] & digChannelMask) {
                        // This segment of digital waveform starts with a one.
                        edgeDetected = false;
                        if (i > startX && lastSampleWasOne == false) {
                            edgeDetected = true;
                        } else {
                            for (int j = jBegin + 1; j < jEnd; ++j) {
                                if (!(digitalWaveformQueue[j] & digChannelMask)) {
                                    edgeDetected = true;
                                    break;
                                }
                            }
                        }
                        if (edgeDetected) {
                            painter.drawLine(i, YDigitalZero, i, YDigitalOne);
                        } else {
                            painter.drawPoint(i, YDigitalOne);
                        }
                    } else {
                        // This segment of digital waveform starts with a zero.
                        edgeDetected = false;
                        if (i > startX && lastSampleWasOne == true) {
                            edgeDetected = true;
                        } else {
                            for (int j = jBegin + 1; j < jEnd; ++j) {
                                if (digitalWaveformQueue[j] & digChannelMask) {
                                    edgeDetected = true;
                                    break;
                                }
                            }
                        }
                        if (edgeDetected) {
                            painter.drawLine(i, YDigitalZero, i, YDigitalOne);
                        } else {
                            painter.drawPoint(i, YDigitalZero);
                        }
                    }
                    lastSampleWasOne = digitalWaveformQueue[jEnd - 1] & digChannelMask;
                }
            }
        }

        painter.setPen(Qt::white);
        QString fullName = state->signalSources->getNativeAndCustomNames(waveName);
        plotDecorator.writeLabel(fullName, ct.xLeft() + 2, ct.yTop() - 1, Qt::AlignLeft | Qt::AlignBottom);

        QRect colorScaleFrame = QRect(scopeFrame.right() + colorBarSpacing, scopeFrame.top(), colorBarWidth, scopeFrame.height());
        colorScale->drawColorScale(painter, colorScaleFrame);
        CoordinateTranslator ctColorScale(colorScaleFrame, 0.0, 1.0, psdScaleMin, psdScaleMax);
        painter.setPen(Qt::white);
        plotDecorator.drawLabeledTickMarkRight("0.1" + psdUnitsMicro, ctColorScale, -1.0, TickMarkLength);
        plotDecorator.drawLabeledTickMarkRight("1" + psdUnitsMicro, ctColorScale, 0.0, TickMarkLength);
        plotDecorator.drawLabeledTickMarkRight("10" + psdUnitsMicro, ctColorScale, 1.0, TickMarkLength);
        plotDecorator.drawLabeledTickMarkRight("100" + psdUnitsMicro, ctColorScale, 2.0, TickMarkLength);
        plotDecorator.drawLabeledTickMarkRight("1000" + psdUnitsMicro, ctColorScale, 3.0, TickMarkLength);

        painter.setPen(Qt::lightGray);
        for (int i = -2; i < 3; ++i) {
            plotDecorator.drawTickMarkRight(ctColorScale, ((double) i) + Log10_2, TickMarkMinorLength);
            plotDecorator.drawTickMarkRight(ctColorScale, ((double) i) + Log10_3, TickMarkMinorLength);
            plotDecorator.drawTickMarkRight(ctColorScale, ((double) i) + Log10_4, TickMarkMinorLength);
            plotDecorator.drawTickMarkRight(ctColorScale, ((double) i) + Log10_5, TickMarkMinorLength);
            plotDecorator.drawTickMarkRight(ctColorScale, ((double) i) + Log10_6, TickMarkMinorLength);
            plotDecorator.drawTickMarkRight(ctColorScale, ((double) i) + Log10_7, TickMarkMinorLength);
            plotDecorator.drawTickMarkRight(ctColorScale, ((double) i) + Log10_8, TickMarkMinorLength);
            plotDecorator.drawTickMarkRight(ctColorScale, ((double) i) + Log10_9, TickMarkMinorLength);
        }

        plotDecorator.drawTickMarkRight(ctColorScale, 3.0 + Log10_2, TickMarkMinorLength);
        plotDecorator.drawTickMarkRight(ctColorScale, 3.0 + Log10_3, TickMarkMinorLength);

        // Scale and insert power spectral density (PSD) image.
        painter.drawImage(scopeFrame, psdRawImage,
                          QRect(0, psdRawImage.height() - fMaxIndex - 1, psdRawImage.width(), fMaxIndex - fMinIndex + 1));

        // Label frequency axis.
        painter.setPen(Qt::white);
        plotDecorator.drawLabeledTickMarkLeft(QString::number(fMin) + " Hz", ct, fMin, TickMarkLength);
        plotDecorator.drawLabeledTickMarkLeft(QString::number(fMax) + " Hz", ct, fMax, TickMarkLength);

        // Label time axis.
        for (int t = 0; t < tScale; ++t) {
            plotDecorator.drawLabeledTickMarkBottom(t, ct, t, TickMarkLength);
        }
        plotDecorator.drawLabeledTickMarkBottom(QString::number(tScale) + " s", ct, tScale, TickMarkLength);

        if (state->showFMarkerSpectrogram->getValue()) {
            painter.setPen(Qt::red);
            double fMarker = state->fMarkerSpectrogram->getValue();
            plotDecorator.drawLabeledTickMarkLeft(QString::number(qRound(fMarker)) + " Hz", ct, fMarker, TickMarkLength);
            painter.setPen(Qt::black);
            plotDecorator.drawHorizontalAxisLine(ct, fMarker);
            for (int h = 0; h < state->numHarmonicsSpectrogram->getValue(); ++h) {
                double fHarmonic = (h + 2) * fMarker;
                if (fHarmonic >= fMin && fHarmonic <= fMax) {
                    painter.setPen(Qt::darkRed);
                    plotDecorator.drawTickMarkLeft(ct, fHarmonic, TickMarkLength);
                    painter.setPen(Qt::black);
                    plotDecorator.drawHorizontalAxisLine(ct, fHarmonic);
                }
            }
        }

        if (scopeFrame.contains(cursor)) {
            double fCursor = ct.realYFromScreenY(cursor.y());
            painter.setPen(Qt::yellow);
            plotDecorator.drawLabeledTickMarkLeft(QString::number(qRound(fCursor)) + " Hz", ct, fCursor, TickMarkLength);
            painter.setPen(Qt::black);
            plotDecorator.drawHorizontalAxisLine(ct, fCursor);
        }
    } else {
        // Plot spectrum (amplitude vs. time).

        const int LeftMargin = fontMetrics().horizontalAdvance("1000" + psdUnitsMicro) + TickMarkLength + 10;
        const int RightMargin = 25;
        const int BottomMargin = fontMetrics().height() + TickMarkLength + 10;
        const int TopMargin = fontMetrics().height() + 10;

        scopeFrame = QRect(imageFrame.adjusted(LeftMargin, TopMargin, -RightMargin, -BottomMargin));
        CoordinateTranslator ct(scopeFrame, 0.0, tScale, frequencyScale[fMinIndex], frequencyScale[fMaxIndex]);

        ct.set(scopeFrame, frequencyScale[fMinIndex], frequencyScale[fMaxIndex], psdScaleMin, psdScaleMax);
        painter.setPen(Qt::white);
        painter.drawRect(ct.borderRect());    // Draw borders of scope.
        QString fullName = state->signalSources->getNativeAndCustomNames(waveName);
        plotDecorator.writeLabel(fullName, ct.xLeft() + 2, ct.yTop() - 1, Qt::AlignLeft | Qt::AlignBottom);

        plotDecorator.drawLabeledTickMarkBottom(QString::number(fMin) + " Hz", ct, fMin, TickMarkLength);
        plotDecorator.drawLabeledTickMarkBottom(QString::number(fMax) + " Hz", ct, fMax, TickMarkLength);

        plotDecorator.drawLabeledTickMarkLeft("0.1" + psdUnitsMicro, ct, -1.0, TickMarkLength);
        plotDecorator.drawLabeledTickMarkLeft("1" + psdUnitsMicro, ct, 0.0, TickMarkLength);
        plotDecorator.drawLabeledTickMarkLeft("10" + psdUnitsMicro, ct, 1.0, TickMarkLength);
        plotDecorator.drawLabeledTickMarkLeft("100" + psdUnitsMicro, ct, 2.0, TickMarkLength);
        plotDecorator.drawLabeledTickMarkLeft("1000" + psdUnitsMicro, ct, 3.0, TickMarkLength);

        painter.setPen(Qt::lightGray);
        for (int i = -2; i < 3; ++i) {
            plotDecorator.drawTickMarkLeft(ct, ((double) i) + Log10_2, TickMarkMinorLength);
            plotDecorator.drawTickMarkLeft(ct, ((double) i) + Log10_3, TickMarkMinorLength);
            plotDecorator.drawTickMarkLeft(ct, ((double) i) + Log10_4, TickMarkMinorLength);
            plotDecorator.drawTickMarkLeft(ct, ((double) i) + Log10_5, TickMarkMinorLength);
            plotDecorator.drawTickMarkLeft(ct, ((double) i) + Log10_6, TickMarkMinorLength);
            plotDecorator.drawTickMarkLeft(ct, ((double) i) + Log10_7, TickMarkMinorLength);
            plotDecorator.drawTickMarkLeft(ct, ((double) i) + Log10_8, TickMarkMinorLength);
            plotDecorator.drawTickMarkLeft(ct, ((double) i) + Log10_9, TickMarkMinorLength);
        }
        plotDecorator.drawTickMarkLeft(ct, 3.0 + Log10_2, TickMarkMinorLength);
        plotDecorator.drawTickMarkLeft(ct, 3.0 + Log10_3, TickMarkMinorLength);

        painter.setPen(QColor(72, 72, 72));
        for (int y = -1; y < 4; ++y) {
            plotDecorator.drawHorizontalAxisLine(ct, y, 1);
        }

        if (state->showFMarkerSpectrogram->getValue()) {
            painter.setPen(Qt::red);
            double fMarker = state->fMarkerSpectrogram->getValue();
            plotDecorator.drawVerticalAxisLine(ct, fMarker);
            plotDecorator.drawLabeledTickMarkBottom(QString::number(qRound(fMarker)) + " Hz", ct, fMarker, TickMarkLength);
            painter.setPen(Qt::darkRed);
            for (int h = 0; h < state->numHarmonicsSpectrogram->getValue(); ++h) {
                double fHarmonic = (h + 2) * fMarker;
                if (fHarmonic >= fMin && fHarmonic <= fMax) {
                    plotDecorator.drawVerticalAxisLine(ct, fHarmonic);
                    plotDecorator.drawTickMarkBottom(ct, fHarmonic, TickMarkLength);
                }
            }
        }

        if (scopeFrame.contains(cursor)) {
            painter.setPen(Qt::yellow);
            double fCursor = ct.realXFromScreenX(cursor.x());
            plotDecorator.drawVerticalAxisLine(ct, fCursor);
            plotDecorator.drawLabeledTickMarkBottom(QString::number(qRound(fCursor)) + " Hz", ct, fCursor, TickMarkLength);
        }

        painter.setClipRect(ct.clippingRect());

        int polylineLength = fMaxIndex - fMinIndex + 1;
        QPointF *polyline = new QPointF[polylineLength];
        int i = 0;
        for (int fIndex = fMinIndex; fIndex <= fMaxIndex; ++fIndex) {
            polyline[i++] = QPointF(ct.screenXFromRealX(frequencyScale[fIndex]), ct.screenYFromRealY(psdSpectrum[fIndex]));
        }
        painter.setPen(Qt::green);
        painter.drawPolyline(polyline, polylineLength);
        delete [] polyline;
        painter.setClipping(false);
    }

    QStylePainter stylePainter(this);
    stylePainter.drawImage(0, 0, image);
}

bool SpectrogramPlot::saveMatFile(const QString& fileName) const
{
    if (numValidTStepsInSpectrogram == 0) return false;

    bool spectrogramMode = state->displayModeSpectrogram->getValue() == "Spectrogram";

    MatFileWriter matFileWriter;

    QString fullName = state->signalSources->getNativeAndCustomNames(waveName);
    matFileWriter.addString("waveform_name", fullName);
    matFileWriter.addRealScalar("nfft", state->fftSizeSpectrogram->getNumericValue());
    matFileWriter.addRealScalar("sample_rate", state->sampleRate->getNumericValue());

    int fRange = fMaxIndex - fMinIndex + 1;
    std::vector<float> frequencyVector(fRange);
    for (int i = fMinIndex; i <= fMaxIndex; ++i) {
        frequencyVector[i - fMinIndex] = frequencyScale[i];
    }
    matFileWriter.addRealVector("f", frequencyVector);
    if (state->showFMarkerSpectrogram->getValue()) {
        matFileWriter.addRealScalar("f_marker", state->fMarkerSpectrogram->getValue());
    }

    int numSamples = spectrogramMode ? ((numValidTStepsInSpectrogram + 1) * (fftSize / 2)) : fftSize;
    std::vector<float> timeWaveform(numSamples);
    std::vector<float> amplifierWaveform(numSamples);
    int64_t timeStampTimeZero = (int64_t) waveformTimeStampQueue[fftSize / 2];
    float waveformTimeStep = 1.0 / (double)state->sampleRate->getNumericValue();
    for (int i = 0; i < numSamples; ++i) {
        timeWaveform[i] = ((int64_t)waveformTimeStampQueue[i] - timeStampTimeZero) * waveformTimeStep;
        amplifierWaveform[i] = amplifierWaveformRecordQueue[i];
    }
    matFileWriter.addRealVector("t_waveform", timeWaveform);
    matFileWriter.addRealVector("amplifier_waveform", amplifierWaveform);
    matFileWriter.addString("HOW_TO_PLOT_AMPLIFIER_WAVEFORM",
                            "plot(t_waveform,amplifier_waveform);");
    matFileWriter.addIntegerScalar("timestamp_at_time_0", timeStampTimeZero, MatlabDataTypeUInt32);

    if (spectrogramMode) {
        // Spectrogram display.
        std::vector<float> timeVector(numValidTStepsInSpectrogram);
        for (int i = 0; i < numValidTStepsInSpectrogram; ++i) {
            timeVector[i] = i * (float) tStep;
        }

        std::vector<std::vector<float> > psdArray(numValidTStepsInSpectrogram);
        int index = spectrogramFull ? tIndex : 0;
        for (int i = 0; i < numValidTStepsInSpectrogram; ++i) {
            psdArray[i].resize(fRange);
            for (int j = fMinIndex; j <= fMaxIndex; ++j) {
                psdArray[i][j - fMinIndex] = psdSpectrogram[index][j];
            }
            if (++index == tSize) index = 0;
        }

        std::vector<std::vector<float> > colorMap;
        colorScale->copyColorMapToArray(colorMap);

        std::vector<float> colorLimits(2);
        colorLimits[0] = psdScaleMin;
        colorLimits[1] = psdScaleMax;

        matFileWriter.addRealVector("t", timeVector);
        matFileWriter.addRealArray("specgram", psdArray);
        matFileWriter.transposeLastElement();
        matFileWriter.addRealArray("color_map", colorMap);
        matFileWriter.addRealVector("clim", colorLimits);
        matFileWriter.transposeLastElement();
        matFileWriter.addString("HOW_TO_PLOT_SPECTROGRAM",
                                "colormap(color_map); imagesc(t,f,specgram,clim); axis xy; colorbar;");

        if (state->digitalDisplaySpectrogram->getValue() != "None") {
            std::vector<uint16_t> digitalWaveform(numSamples);
            for (int i = 0; i < numSamples; ++i) {
                digitalWaveform[i] = digitalWaveformQueue[i];
            }
            matFileWriter.addUInt16Vector("digital_inputs", digitalWaveform);
            matFileWriter.addString("HOW_TO_PLOT_DIGITAL_INPUT",
                                    "dig_in_channel=0; plot(t_waveform,bitand(digital_inputs,2^dig_in_channel) ~= 0);");
        }
    } else {
        // Spectrum display.
        std::vector<float> spectrum(fRange);
        for (int i = fMinIndex; i <= fMaxIndex; ++i) {
            spectrum[i - fMinIndex] = psdSpectrum[i];
        }
        matFileWriter.addRealVector("spectrum", spectrum);
        matFileWriter.addString("HOW_TO_PLOT_SPECTRUM",
                                "plot(f,spectrum);");
    }

    return matFileWriter.writeFile(fileName);
}

bool SpectrogramPlot::saveCsvFile(QString fileName) const
{
    if (numValidTStepsInSpectrogram == 0) return false;

    if (fileName.right(4).toLower() != ".csv") fileName.append(".csv");
    QFile csvFile(fileName);
    if (!csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "SpectrogramPlot::saveCsvFile: Could not open CSV save file " << fileName;
        return false;
    }
    QTextStream outStream(&csvFile);

    if (state->displayModeSpectrogram->getValue() == "Spectrogram") {
        // Spectrogram display.
        outStream << "Time (s) / Frequency (Hz),";
        for (int j = fMinIndex; j <= fMaxIndex; ++j) {
            outStream << frequencyScale[j] << ",";
        }
        outStream << EndOfLine;

        int index = spectrogramFull ? tIndex : 0;
        for (int i = 0; i < numValidTStepsInSpectrogram; ++i) {
            outStream << i * tStep << ",";
            for (int j = fMinIndex; j <= fMaxIndex; ++j) {
                outStream << psdSpectrogram[index][j] << ",";
            }
            if (++index == tSize) index = 0;
            outStream << EndOfLine;
        }

    } else {
        // Spectrum display.
        outStream << "Frequency (Hz),Log spectrum" << EndOfLine;
        for (int j = fMinIndex; j <= fMaxIndex; ++j) {
            outStream << frequencyScale[j] << "," << psdSpectrum[j] << EndOfLine;
        }
    }
    csvFile.close();
    return true;
}

bool SpectrogramPlot::savePngFile(const QString& fileName) const
{
    return image.save(fileName, "PNG", 100);
}

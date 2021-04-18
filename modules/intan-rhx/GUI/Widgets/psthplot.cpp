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

#include "matfilewriter.h"
#include "psthplot.h"

PSTHPlot::PSTHPlot(SystemState* state_, QWidget *parent) :
    QWidget(parent),
    state(state_)
{
    setBackgroundRole(QPalette::Window);
    setAutoFillBackground(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);  // If we don't do this, mouseMoveEvent() is active only when mouse button is pressed.

    lastMouseWasInFrame = false;
    preTriggerTimeSpan = 1;
    postTriggerTimeSpan = 1;
    binSize = 1;
    maxNumTrials = 1;
    numTrials = 0;
    maxValueHistogram = 0.0;
    rasterPlotWidth = 1;

    image = QImage(size(), QImage::Format_ARGB32_Premultiplied);

    updateFromState();
}

void PSTHPlot::setWaveform(const string& waveName_)
{
    if (waveName == waveName_) return;

    waveName = waveName_;
    resetPSTH();
    update();
}

void PSTHPlot::updateFromState()
{
    bool psthSizeChanged = false;

    if (preTriggerTimeSpan != (int) state->tSpanPreTriggerPSTH->getNumericValue()) {
        preTriggerTimeSpan = (int) state->tSpanPreTriggerPSTH->getNumericValue();
        psthSizeChanged = true;
    }
    if (postTriggerTimeSpan != (int) state->tSpanPostTriggerPSTH->getNumericValue()) {
        postTriggerTimeSpan = (int) state->tSpanPostTriggerPSTH->getNumericValue();
        psthSizeChanged = true;
    }
    if (binSize != (int) state->binSizePSTH->getNumericValue()) {
        binSize = (int) state->binSizePSTH->getNumericValue();
        calculateHistogram();
    }
    if (maxNumTrials != (int) state->maxNumTrialsPSTH->getNumericValue()) {
        maxNumTrials = (int) state->maxNumTrialsPSTH->getNumericValue();
        resizeRasters();
    }

    if (psthSizeChanged) {
        resetPSTH();
    }

    update();
}

void PSTHPlot::resetPSTH()
{
    numTrials = 0;

    for (int trial = 0; trial < (int) rasters.size(); trial++) {
        rasters[trial].clear();
    }
    rasters.clear();

    rasters.resize(maxNumTrials);
    for (int trial = 0; trial < (int) rasters.size(); trial++) {
        rasters[trial].resize(numTStepsTotal());
        fill(rasters[trial].begin(), rasters[trial].end(), 0u);
    }
    triggerWaveform.resize(numTStepsTotal());

    spikeTrainQueue.clear();
    digitalWaveformQueue.clear();

    calculateHistogram();

    rasterImage = QImage(QSize(rasterPlotWidth, maxNumTrials), QImage::Format_ARGB32_Premultiplied);
    rasterImage.fill(Qt::black);
}

void PSTHPlot::adjustRasterPlotWidth(int width)
{
    rasterPlotWidth = width;

    rasterImage = QImage(QSize(rasterPlotWidth, maxNumTrials), QImage::Format_ARGB32_Premultiplied);
    rasterImage.fill(Qt::black);

    redrawRasterImage();
}

void PSTHPlot::resizeRasters()
{
    int oldMaxNumTrials = (int) rasters.size();
    if (maxNumTrials == oldMaxNumTrials) return;

    if (maxNumTrials > oldMaxNumTrials) {
        // Grow raster array.
        rasters.resize(maxNumTrials);
        for (int i = oldMaxNumTrials; i < maxNumTrials; ++i) {
            rasters[i].resize(numTStepsTotal());
            fill(rasters[i].begin(), rasters[i].end(), 0u);
        }
    } else {
        // Shrink raster array.
        int start = numTrials - maxNumTrials;
        if (start < 0) start = 0;

        if (start > 0) {
            for (int i = 0; i < maxNumTrials; ++i) {
                for (int j = 0; j < numTStepsTotal(); ++j) {
                    rasters[i][j] = rasters[i + start][j];
                }
            }
        }
        for (int i = maxNumTrials; i < oldMaxNumTrials; ++i) {
            rasters[i].clear();
        }
        rasters.resize(maxNumTrials);
    }

    if (numTrials > maxNumTrials) numTrials = maxNumTrials;
    adjustRasterPlotWidth(rasterPlotWidth);
    calculateHistogram();
}

void PSTHPlot::calculateHistogram()
{
    // Clear histogram
    histogram.resize(numBins());
    fill(histogram.begin(), histogram.end(), 0.0F);

    histogramTScale.resize(numBins());
    for (int i = 0; i < (int) histogramTScale.size(); ++i) {
        histogramTScale[i] = -preTriggerTimeSpan + i * binSize;
    }
    maxValueHistogram = 0.0;
    if (numTrials == 0) return;

    // Calculate new histogram
    for (int trial = 0; trial < numTrials; ++trial) {
        vector<uint8_t>& currentRaster = rasters[trial];
        int histIndex = 0;
        for (int t = 0; t < numTStepsTotal(); t += numTStepsPerBin()) {
            for (int tt = 0; tt < numTStepsPerBin(); ++tt) {
                if (currentRaster[t + tt] != 0) ++histogram[histIndex];
            }
            ++histIndex;
        }
    }

    // Divide spike count by bin size and number of trials to get average spikes/s
    double scaleFactor = 1000.0 / ((double) (binSize * numTrials));
    for (int i = 0; i < (int) histogram.size(); ++i) {
        histogram[i] *= scaleFactor;
    }

    // Find maximum value of histogram
    for (int i = 0; i < (int) histogram.size(); ++i) {
        if (histogram[i] > maxValueHistogram) maxValueHistogram = histogram[i];
    }
}

void PSTHPlot::keyPressEvent(QKeyEvent* event)
{
    if (!(event->isAutoRepeat())) { // Ignore additional 'keypresses' from auto-repeat if key is held down.
        switch (event->key()) {

        case Qt::Key_Comma:
        case Qt::Key_Less:
            state->binSizePSTH->decrementIndex();
            break;

        case Qt::Key_Period:
        case Qt::Key_Greater:
            state->binSizePSTH->incrementIndex();
            break;

        case Qt::Key_Delete:
            deleteLastRaster();
            break;
        }
    }
}

bool PSTHPlot::updateWaveforms(WaveformFifo* waveformFifo, int numSamples)
{
    if (!waveformFifo->gpuWaveformPresent(waveName + "|SPK")) return false;
    uint16_t* spikeTrain = waveformFifo->getDigitalWaveformPointer(waveName + "|SPK");
    if (!spikeTrain) return false;

    QString triggerChannelName = state->digitalTriggerPSTH->getValueString();
    bool useAnalogTrigger = triggerChannelName.left(1).toUpper() == "A";
    uint16_t triggerMask = 0x01u;

    if (!useAnalogTrigger) {  // Use digital trigger
        uint16_t* digitalInWaveform = waveformFifo->getDigitalWaveformPointer("DIGITAL-IN-WORD");
        for (int t = 0; t < numSamples; ++t) {
            digitalWaveformQueue.push_back(waveformFifo->getDigitalData(WaveformFifo::ReaderDisplay, digitalInWaveform, t));
        }
        triggerMask = 0x01u << (int)state->digitalTriggerPSTH->getNumericValue();
    } else {  // Use analog trigger
        float* analogInWaveform = waveformFifo->getAnalogWaveformPointer(triggerChannelName.toStdString());
        float logicThreshold = (float)state->triggerAnalogVoltageThreshold->getValue();
        for (int t = 0; t < numSamples; ++t) {
            digitalWaveformQueue.push_back(waveformFifo->getAnalogDataAsDigital(WaveformFifo::ReaderDisplay,
                                                                                analogInWaveform, t, logicThreshold));
        }
    }

    for (int t = 0; t < numSamples; ++t) {
        spikeTrainQueue.push_back(waveformFifo->getDigitalData(WaveformFifo::ReaderDisplay, spikeTrain, t));
    }

    bool risingEdge = state->triggerPolarityPSTH->getValue() == "Rising";
    int timeZeroIndex = numTStepsPreTrigger();
    while ((int) spikeTrainQueue.size() >= numTStepsTotal()) {
        bool triggerFound = false;
        bool lastTriggerValueWasHigh = digitalWaveformQueue[timeZeroIndex - 1] & triggerMask;
        bool currentTriggerValueIsHigh = digitalWaveformQueue[timeZeroIndex] & triggerMask;
        if (currentTriggerValueIsHigh != lastTriggerValueWasHigh) { // trigger edge found
            if (risingEdge && currentTriggerValueIsHigh) triggerFound = true;
            else if (!risingEdge && !currentTriggerValueIsHigh) triggerFound = true;
        }
        if (triggerFound) {
            addNewRaster(timeZeroIndex);
            for (int t = 0; t < numTStepsPostTrigger(); ++t) {
                spikeTrainQueue.pop_front();
                digitalWaveformQueue.pop_front();
            }
        } else {
            spikeTrainQueue.pop_front();
            digitalWaveformQueue.pop_front();
        }
    }

    update();
    return true;
}

void PSTHPlot::addNewRaster(int timeIndex)
{
    if (numTrials == maxNumTrials) {
        // If raster record is full, shift it back by one.
        for (int trial = 0; trial < maxNumTrials - 1; ++trial) {
            vector<uint8_t>& currentRaster = rasters[trial];
            vector<uint8_t>& nextRaster = rasters[trial + 1];
            for (int t = 0; t < numTStepsTotal(); ++t) {
                currentRaster[t] = nextRaster[t];
            }
        }
        // Also shift raster image back by one.
        QPainter rasterPainter(&rasterImage);
        rasterPainter.drawImage(QRect(0, 0, rasterPlotWidth, maxNumTrials - 1), rasterImage,
                                QRect(0, 1, rasterPlotWidth, maxNumTrials - 1));
        rasterPainter.setPen(Qt::black);
        rasterPainter.drawLine(0, maxNumTrials - 1, rasterPlotWidth - 1, maxNumTrials - 1);

        numTrials = maxNumTrials - 1;
    }
    // Add new raster at the end of the raster record.
    timeIndex -= numTStepsPreTrigger();
    vector<uint8_t>& currentRaster = rasters[numTrials];
    for (int t = 0; t < numTStepsTotal(); ++t) {
        currentRaster[t] = (spikeTrainQueue[timeIndex + t]) ? 1u : 0u;
        triggerWaveform[t] = digitalWaveformQueue[t];
    }

    // Also add new raster to raster image.
    CoordinateTranslator1D ct(0, rasterPlotWidth, 0, numTStepsTotal());
    for (int t = 0; t < numTStepsTotal(); ++t) {
        if (currentRaster[t]) {
            rasterImage.setPixelColor(ct.screenFromReal(t), numTrials, Qt::white);
        }
    }

    ++numTrials;

    // Update histogram.
    calculateHistogram();
}

void PSTHPlot::deleteLastRaster()
{
    if (numTrials == 0) return;
    --numTrials;
    fill(rasters[numTrials].begin(), rasters[numTrials].end(), 0u);
    for (int x = 0; x < rasterImage.width(); ++x) {
        rasterImage.setPixelColor(x, numTrials, Qt::black);
    }

    // Update histogram.
    calculateHistogram();
}

void PSTHPlot::redrawRasterImage()
{
    for (int trial = 0; trial < numTrials; ++trial) {
        vector<uint8_t>& currentRaster = rasters[trial];
        CoordinateTranslator1D ct(0, rasterPlotWidth, 0, numTStepsTotal());
        for (int t = 0; t < numTStepsTotal(); ++t) {
            if (currentRaster[t]) {
                rasterImage.setPixelColor(ct.screenFromReal(t), trial, Qt::white);
            }
        }
    }
}

void PSTHPlot::resizeEvent(QResizeEvent* /* event */) {
    image = QImage(size(), QImage::Format_ARGB32_Premultiplied);
    update();
}

void PSTHPlot::mouseMoveEvent(QMouseEvent* event)  // Note: Must setMouseTracking(true) for this to work.
{
    bool mouseIsInFrame = digitalFrame.contains(event->pos()) ||
                          rasterFrame.contains(event->pos()) ||
                          histogramFrame.contains(event->pos());
    if (mouseIsInFrame || lastMouseWasInFrame) update();
    lastMouseWasInFrame = mouseIsInFrame;
}

void PSTHPlot::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(&image);
    QRect imageFrame(rect());
    painter.fillRect(imageFrame, QBrush(Qt::black));
    QPoint cursor = mapFromGlobal(QCursor::pos());  // get current cursor position

    PlotDecorator plotDecorator(painter);

    const int TickMarkLength = 5;
    int leftMargin = fontMetrics().horizontalAdvance("1000") + TickMarkLength + fontMetrics().ascent() + 10;
    int leftAxisLabelSpacing = leftMargin - (fontMetrics().ascent() + 5);
    int rightMargin = 10;

    int bottomMarginHistogram = fontMetrics().height() + TickMarkLength + 10;
    int histogramHeight = 170;
    int digitalFrameHeight = 32;
    int digitalFrameTopMargin = fontMetrics().height() + 8;
    int topMarginRaster = fontMetrics().height() + 10 + digitalFrameTopMargin + digitalFrameHeight;
    int bottomMarginRaster = bottomMarginHistogram + histogramHeight + 10;

    rasterFrame = QRect(imageFrame.adjusted(leftMargin, topMarginRaster, -rightMargin, -bottomMarginRaster));
    if (rasterFrame.width() != rasterPlotWidth) {
        adjustRasterPlotWidth(rasterFrame.width());
    }
    int topMarginHistogram = rasterFrame.bottom() + 18;
    histogramFrame = QRect(imageFrame.adjusted(leftMargin, topMarginHistogram, -rightMargin, -bottomMarginHistogram));
    digitalFrame = QRect(rasterFrame.left(), digitalFrameTopMargin, rasterFrame.width(), digitalFrameHeight);

    // Draw digital waveform frame
    painter.setPen(Qt::white);
    painter.drawRect(digitalFrame);
    plotDecorator.writeLabel(state->digitalTriggerPSTH->getDisplayValueString(),
                             digitalFrame.left() + 2, digitalFrame.top() - 1, Qt::AlignLeft | Qt::AlignBottom);

    int startX = digitalFrame.left() + 1;
    int endX = digitalFrame.right() + 1;
    CoordinateTranslator1D ctTime(startX, endX, 0, numTStepsTotal());
    int zeroX = ctTime.screenFromReal(numTStepsPreTrigger());
    painter.setPen(Qt::darkGray);
    painter.drawLine(zeroX, digitalFrame.top() + 1, zeroX, digitalFrame.bottom());

    // Draw digital waveform
    if (numTrials > 0) {
        painter.setPen(Qt::green);

        CoordinateTranslator1D ctDigitalY(digitalFrame.bottom(), digitalFrame.top(), -0.5, 1.5);
        const int YDigitalZero = ctDigitalY.screenFromReal(0);
        const int YDigitalOne = ctDigitalY.screenFromReal(1);

        QString triggerChannelName = state->digitalTriggerPSTH->getValueString();
        bool useAnalogTrigger = triggerChannelName.left(1).toUpper() == "A";
        uint16_t triggerMask = 0x01u;
        if (!useAnalogTrigger) triggerMask = 0x01u << (int)state->digitalTriggerPSTH->getNumericValue();

        bool edgeDetected = false;
        bool lastSampleWasOne = false;
        int jBegin;
        int jEnd = qRound(ctTime.realFromScreen(startX));
        for (int i = startX; i < endX; ++i) {
            jBegin = jEnd;
            jEnd = qRound(ctTime.realFromScreen(i + 1));
            if (triggerWaveform[jBegin] & triggerMask) {
                // This segment of digital waveform starts with a one.
                edgeDetected = false;
                if (i > startX && lastSampleWasOne == false) {
                    edgeDetected = true;
                } else {
                    for (int j = jBegin + 1; j < jEnd; ++j) {
                        if (!(triggerWaveform[j] & triggerMask)) {
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
                        if (triggerWaveform[j] & triggerMask) {
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
            lastSampleWasOne = triggerWaveform[jEnd - 1] & triggerMask;
        }
    }

    // Scale and insert raster image.
    painter.drawImage(rasterFrame.adjusted(1, 1, 0, 0), rasterImage);

    // Draw raster frame
    painter.setPen(Qt::white);
    painter.drawRect(rasterFrame);
    QString fullName = state->signalSources->getNativeAndCustomNames(waveName);
    plotDecorator.writeLabel(fullName, rasterFrame.left() + 2, rasterFrame.top() - 1,
                             Qt::AlignLeft | Qt::AlignBottom);
    plotDecorator.writeLabel(QString::number(numTrials) + ((numTrials == 1) ? " trial" : " trials"),
                             rasterFrame.right() - 2, rasterFrame.top() - 1, Qt::AlignRight | Qt::AlignBottom);
    CoordinateTranslator ctRaster(rasterFrame.adjusted(1, 1, 0, 0),
                                -preTriggerTimeSpan, postTriggerTimeSpan, maxNumTrials, 1);
    plotDecorator.writeYAxisLabel("trials", ctRaster, leftAxisLabelSpacing);

    int trialTick = 5;
    if (maxNumTrials > 200) trialTick = 100;
    else if (maxNumTrials > 100) trialTick = 50;
    else if (maxNumTrials > 50) trialTick = 20;
    else if (maxNumTrials > 20) trialTick = 10;
    for (int i = trialTick; i <= maxNumTrials; i += trialTick) {
        plotDecorator.drawLabeledTickMarkLeft(i, ctRaster, i, TickMarkLength);
    }

    painter.setPen(Qt::darkGray);
    painter.drawLine(zeroX, rasterFrame.top() + 1, zeroX, rasterFrame.bottom());

    // Draw histogram image
    vector<double> yAxisTicks;
    vector<QString> yAxisLabels;
    double yMax = plotDecorator.autoCalculateYAxis(maxValueHistogram, yAxisTicks, yAxisLabels);
    CoordinateTranslator ctHist(histogramFrame.adjusted(1, 1, 0, 0),
                                -preTriggerTimeSpan, postTriggerTimeSpan, 0, yMax);

    histogramImage = QImage(QSize((int) histogram.size(), histogramFrame.height()), QImage::Format_ARGB32_Premultiplied);
    histogramImage.fill(Qt::black);

    if (maxValueHistogram > 0.0) {
        QPainter histPainter(&histogramImage);
        CoordinateTranslator1D ctHistImage(histogramFrame.height(), 0, 0, yMax);
        histPainter.setPen(QColor(100, 149, 237));
        for (int i = 0; i < (int) histogram.size(); ++i) {
            histPainter.drawLine(i, histogramFrame.height(), i, ctHistImage.screenFromReal(histogram[i]));
        }
    }

    // Scale and insert histogram image.
    painter.drawImage(histogramFrame.adjusted(1, 1, 0, 0), histogramImage);

    // Draw histogram frame
    painter.setPen(Qt::white);
    painter.drawRect(histogramFrame);
    plotDecorator.writeYAxisLabel("spikes/s", ctHist, leftAxisLabelSpacing);

    for (int i = 0; i < (int) yAxisTicks.size(); ++i) {
        plotDecorator.drawLabeledTickMarkLeft(yAxisLabels[i], ctHist, yAxisTicks[i], TickMarkLength);
    }

    int tTick = 100;
    bool showSeconds = false;
    int totalTimeSpan = preTriggerTimeSpan + postTriggerTimeSpan;
    if (totalTimeSpan > 12000) {
        tTick = 2000;
        showSeconds = true;
    } else if (totalTimeSpan > 3000) {
        tTick = 1000;
        showSeconds = true;
    } else if (totalTimeSpan > 1000) {
        tTick = 500;
    }

    QString timeUnitString = " ms";
    if (showSeconds) timeUnitString = " s";

    int scaleFactor = 1;
    if (showSeconds) scaleFactor = 1000;

    for (int t = 0; t <= postTriggerTimeSpan; t += tTick) {
        if (!showSeconds || t == 0 || t >= scaleFactor) {
            if (t != postTriggerTimeSpan) {
                plotDecorator.drawLabeledTickMarkBottom(t / scaleFactor, ctHist, t, TickMarkLength);
            } else {
                plotDecorator.drawLabeledTickMarkBottom(QString::number(t / scaleFactor) + timeUnitString, ctHist, t,
                                                        TickMarkLength, true);
            }
        }
    }
    if (tTick > postTriggerTimeSpan) {
        if (!showSeconds || postTriggerTimeSpan >= scaleFactor) {
            plotDecorator.drawLabeledTickMarkBottom(QString::number(postTriggerTimeSpan / scaleFactor) + timeUnitString,
                                                    ctHist, postTriggerTimeSpan, TickMarkLength, true);
        }
    }
    for (int t = -tTick; t >= -preTriggerTimeSpan; t -= tTick) {
        if (!showSeconds || t <= -scaleFactor) {
            plotDecorator.drawLabeledTickMarkBottom(t / scaleFactor, ctHist, t, TickMarkLength);
        }
    }
    if (tTick > preTriggerTimeSpan) {
        if (!showSeconds || preTriggerTimeSpan >= scaleFactor) {
            plotDecorator.drawLabeledTickMarkBottom(-preTriggerTimeSpan / scaleFactor, ctHist, -preTriggerTimeSpan,
                                                    TickMarkLength);
        }
    }

    painter.setPen(Qt::darkGray);
    painter.drawLine(zeroX, histogramFrame.top() + 1, zeroX, histogramFrame.bottom());

    painter.setPen(Qt::yellow);
    if (histogramFrame.contains(cursor) || rasterFrame.contains(cursor) || digitalFrame.contains(cursor)) {
        painter.drawLine(cursor.x(), histogramFrame.top() + 1, cursor.x(), histogramFrame.bottom());
        painter.drawLine(cursor.x(), rasterFrame.top() + 1, cursor.x(), rasterFrame.bottom());
        painter.drawLine(cursor.x(), digitalFrame.top() + 1, cursor.x(), digitalFrame.bottom());
        if (histogramFrame.contains(cursor) || rasterFrame.contains(cursor)) {
            painter.drawLine(startX, cursor.y(), endX - 1, cursor.y());
        }
    }

    QStylePainter stylePainter(this);
    stylePainter.drawImage(0, 0, image);
}

bool PSTHPlot::saveMatFile(const QString& fileName) const
{
    MatFileWriter matFileWriter;

    QString fullName = state->signalSources->getNativeAndCustomNames(waveName);
    matFileWriter.addString("waveform_name", fullName);
    matFileWriter.addRealScalar("sample_rate", state->sampleRate->getNumericValue());
    matFileWriter.addRealScalar("bin_size", binSize);
    matFileWriter.addRealScalar("num_trials", numTrials);
    matFileWriter.addRealVector("avg_firing_rate", histogram);
    matFileWriter.addRealVector("t_bins", histogramTScale);
    matFileWriter.addString("HOW_TO_PLOT_HISTOGRAM",
                            "bar(t_bins,avg_firing_rate,1);");

    QString rastersName = "rasters";
    if (numTrials == maxNumTrials) {
        matFileWriter.addUInt8SparseArray(rastersName, rasters);
    } else {
        vector<vector<uint8_t> > rastersSave;
        rastersSave.resize(numTrials);
        for (int trial = 0; trial < numTrials; ++trial) {
            rastersSave[trial].resize(numTStepsTotal());
            for (int t = 0; t < numTStepsTotal(); ++t) {
                rastersSave[trial][t] = rasters[trial][t];
            }
        }
        matFileWriter.addUInt8SparseArray(rastersName, rastersSave);
    }
    matFileWriter.transposeLastElement();

    vector<float> trials(numTrials);
    for (int i = 0; i < numTrials; ++i) {
        trials[i] = i + 1;
    }
    matFileWriter.addRealVector("trials", trials);

    float tStepSec = 1.0F / state->sampleRate->getNumericValue();
    float tOffsetSec = -0.001F * (float)preTriggerTimeSpan;
    vector<float> tRasters(numTStepsTotal());
    for (int i = 0; i < numTStepsTotal(); ++i) {
        tRasters[i] = i * tStepSec + tOffsetSec;
    }
    matFileWriter.addRealVector("t_rasters", tRasters);

    return matFileWriter.writeFile(fileName);
}

bool PSTHPlot::saveCsvFile(QString fileName) const
{
    if (fileName.right(4).toLower() != ".csv") fileName.append(".csv");
    QFile csvFile(fileName);
    if (!csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "PSTHPlot::saveCsvFile: Could not open CSV save file " << fileName;
        return false;
    }
    QTextStream outStream(&csvFile);

    outStream << "Peri-stimulus time (msec),Average firing rate (spikes/s)" << EndOfLine;
    for (int i = 0; i < (int) histogram.size(); ++i) {
        outStream << histogramTScale[i] << "," << histogram[i] << EndOfLine;
    }

    csvFile.close();
    return true;
}

bool PSTHPlot::savePngFile(const QString& fileName) const
{
    return image.save(fileName, "PNG", 100);
}

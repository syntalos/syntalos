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

#include <cmath>
#include "matfilewriter.h"
#include "isiplot.h"

ISIPlot::ISIPlot(SystemState* state_, QWidget *parent) :
    QWidget(parent),
    state(state_)
{
    setBackgroundRole(QPalette::Window);
    setAutoFillBackground(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);  // If we don't do this, mouseMoveEvent() is active only when mouse button is pressed.

    lastMouseWasInFrame = false;
    timeSpan = 1;
    binSize = 1;
    maxValueHistogram = 0.0;
    minNonZeroValueHistogram = 1.0;
    numISIsRecorded = 0;
    lastTimeStamp = 0u;
    isiMean = 0.0;
    isiStdDev = 0.0;

    const int maxNumSecondsToRecordISI = 1;
    isiCount.resize(qCeil(maxNumSecondsToRecordISI * state->sampleRate->getNumericValue()), 0);
    largestISIrecorded = 0;
    timeScaleISI.resize(isiCount.size());
    for (int t = 0; t < (int) isiCount.size(); ++t) {
        timeScaleISI[t] = 1000.0F * (float) t / (float) state->sampleRate->getNumericValue();
    }

    image = QImage(size(), QImage::Format_ARGB32_Premultiplied);

    updateFromState();
}

void ISIPlot::setWaveform(const string& waveName_)
{
    if (waveName == waveName_) return;

    waveName = waveName_;
    resetISI();
    update();
}

void ISIPlot::updateFromState()
{
    if (timeSpan != (int) state->tSpanISI->getNumericValue()) {
        timeSpan = (int) state->tSpanISI->getNumericValue();
        calculateHistogram();
    }
    if (binSize != (int) state->binSizeISI->getNumericValue()) {
        binSize = (int) state->binSizeISI->getNumericValue();
        calculateHistogram();
    }
    update();
}

void ISIPlot::resetISI()
{
    fill(isiCount.begin(), isiCount.end(), 0);
    largestISIrecorded = 0;
    numISIsRecorded = 0;
    lastTimeStamp = 0u;
    isiMean = 0.0;
    isiStdDev = 0.0;

    calculateHistogram();
    calculateISIStatistics();
}

void ISIPlot::calculateHistogram()
{
    // Clear histogram
    histogram.resize(numBins());
    fill(histogram.begin(), histogram.end(), 0.0F);

    histogramTScale.resize(numBins());
    for (int i = 0; i < (int) histogram.size(); ++i) {
        histogramTScale[i] = i * binSize;
    }
    maxValueHistogram = 0.0;
    minNonZeroValueHistogram = 1.0;
    if (largestISIrecorded == 0) return;

    // Calculate new histogram
    int t = 0;
    for (int i = 0; i < (int) histogram.size(); ++i) {
        for (int tt = 0; tt < numTStepsPerBin(); ++tt) {
            histogram[i] += (float) isiCount[t + tt];
        }
        t += numTStepsPerBin();
    }

    // Divide ISI count in each bin by total number of ISIs to get probability
    for (int i = 0; i < (int) histogram.size(); ++i) {
        histogram[i] /= (float) numISIsRecorded;
    }

    // Find maximum value of histogram
    for (int i = 0; i < (int) histogram.size(); ++i) {
        if (histogram[i] > maxValueHistogram) maxValueHistogram = histogram[i];
        if (histogram[i] > 0 && histogram[i] < minNonZeroValueHistogram) {
            minNonZeroValueHistogram = histogram[i];
        }
    }
}

void ISIPlot::calculateISIStatistics()
{
    if (numISIsRecorded == 0) {
        isiMean = 0.0;
        isiStdDev = 0.0;
        return;
    }

    double isiSum = 0.0;
    for (int i = 0; i <= largestISIrecorded; ++i) {
        if (isiCount[i] != 0) {
            isiSum += isiCount[i] * timeScaleISI[i];
        }
    }
    isiMean = isiSum / (double) numISIsRecorded;

    if (numISIsRecorded == 1) {
        isiStdDev = 0.0;
        return;
    }

    double sumSqauredDiff = 0.0;
    for (int i = 0; i <= largestISIrecorded; ++i) {
        if (isiCount[i] != 0) {
            double diff = timeScaleISI[i] - isiMean;
            sumSqauredDiff += isiCount[i] * diff * diff;
        }
    }
    isiStdDev = sqrt(sumSqauredDiff / (double) (numISIsRecorded - 1));
}

void ISIPlot::keyPressEvent(QKeyEvent* event)
{
    if (!(event->isAutoRepeat())) { // Ignore additional 'keypresses' from auto-repeat if key is held down.
        switch (event->key()) {

        case Qt::Key_Comma:
        case Qt::Key_Less:
            state->binSizeISI->decrementIndex();
            break;

        case Qt::Key_Period:
        case Qt::Key_Greater:
            state->binSizeISI->incrementIndex();
            break;
        }
    }
}

bool ISIPlot::updateWaveforms(WaveformFifo *waveformFifo, int numSamples)
{
    if (!waveformFifo->gpuWaveformPresent(waveName + "|SPK")) return false;
    uint16_t* spikeTrain = waveformFifo->getDigitalWaveformPointer(waveName + "|SPK");
    if (!spikeTrain) return false;

    bool foundNewSpikes = false;
    for (int t = 0; t < numSamples; ++t) {
        if (waveformFifo->getDigitalData(WaveformFifo::ReaderDisplay, spikeTrain, t) != 0) {
            foundNewSpikes = true;
            uint32_t newTimeStamp = waveformFifo->getTimeStamp(WaveformFifo::ReaderDisplay, t);
            if (lastTimeStamp != 0u) {
                int newISI = (int)((int64_t)newTimeStamp - (int64_t)lastTimeStamp);
                if ((newISI < (int) isiCount.size()) && (newISI > 0)) {
                    ++isiCount[newISI];
                    ++numISIsRecorded;
                    if (newISI > largestISIrecorded) largestISIrecorded = newISI;
                }
            }
            lastTimeStamp = newTimeStamp;
        }
    }
    if (foundNewSpikes) {
        calculateHistogram();
        calculateISIStatistics();
        update();
    }

    return true;
}

void ISIPlot::resizeEvent(QResizeEvent* /* event */) {
    image = QImage(size(), QImage::Format_ARGB32_Premultiplied);
    update();
}

void ISIPlot::mouseMoveEvent(QMouseEvent* event)  // Note: Must setMouseTracking(true) for this to work.
{
    bool mouseIsInFrame = histogramFrame.contains(event->pos());
    if (mouseIsInFrame || lastMouseWasInFrame) repaint();
    lastMouseWasInFrame = mouseIsInFrame;
}

void ISIPlot::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(&image);
    QRect imageFrame(rect());
    painter.fillRect(imageFrame, QBrush(Qt::black));
    QPoint cursor = mapFromGlobal(QCursor::pos());  // get current cursor position

    PlotDecorator plotDecorator(painter);

    const int TickMarkLength = 5;
    const int TickMarkMinorLength = 3;
    int leftMargin = fontMetrics().horizontalAdvance("0.000001") + TickMarkLength + fontMetrics().ascent() + 10;
    int leftAxisLabelSpacing = leftMargin - (fontMetrics().ascent() + 5);
    int rightMargin = 10;
    int bottomMargin = fontMetrics().height() + TickMarkLength + 10;
    int topMargin = 4 * fontMetrics().height() + 8;

    histogramFrame = QRect(imageFrame.adjusted(leftMargin, topMargin, -rightMargin, -bottomMargin));

    // Draw histogram image
    bool yAxisLog = state->yAxisLogISI->getValue();
    vector<double> yAxisTicks;
    vector<QString> yAxisLabels;
    double yMin = 0.0;
    double yMax = 0.0;
    CoordinateTranslator ctHist;
    if (!yAxisLog) {
        yMax = plotDecorator.autoCalculateYAxis(maxValueHistogram, yAxisTicks, yAxisLabels);
        ctHist.set(histogramFrame.adjusted(1, 1, 0, 0), 0.0, timeSpan, 0, yMax);
    } else {
        MinMaxPair minMax = plotDecorator.autoCalculateLogYAxis(minNonZeroValueHistogram, maxValueHistogram,
                                                                yAxisTicks, yAxisLabels);
        yMin = minMax.min;
        yMax = minMax.max;
        ctHist.set(histogramFrame.adjusted(1, 1, 0, 0), 0.0, timeSpan, yMin, yMax);
    }

    histogramImage = QImage(QSize((int) histogram.size(), histogramFrame.height()), QImage::Format_ARGB32_Premultiplied);
    histogramImage.fill(Qt::black);

    if (maxValueHistogram > 0.0) {
        QPainter histPainter(&histogramImage);
        histPainter.setPen(QColor(100, 149, 237));
        if (!yAxisLog) {
            CoordinateTranslator1D ctHistImage(histogramFrame.height(), 0, 0, yMax);
            for (int i = 0; i < (int) histogram.size(); ++i) {
                histPainter.drawLine(i, histogramFrame.height(), i, ctHistImage.screenFromReal(histogram[i]));
            }
        } else {
            CoordinateTranslator1D ctHistImage(histogramFrame.height(), 0, yMin, yMax);
            for (int i = 0; i < (int) histogram.size(); ++i) {
                if (histogram[i] > 0) {
                    histPainter.drawLine(i, histogramFrame.height(), i, ctHistImage.screenFromReal(log10(histogram[i])));
                }
            }
        }
    }

    // Scale and insert histogram image.
    painter.drawImage(histogramFrame.adjusted(1, 1, 0, 0), histogramImage);

    // Draw histogram frame
    painter.setPen(Qt::white);
    painter.drawRect(histogramFrame);
    QString fullName = state->signalSources->getNativeAndCustomNames(waveName);
    plotDecorator.writeLabel(fullName, histogramFrame.left() + 2, histogramFrame.top() - 1,
                             Qt::AlignLeft | Qt::AlignBottom);

    int x1 = histogramFrame.right() - 2;
    int y1 = histogramFrame.top() - 1;
    int dy = fontMetrics().height();
    QString isiFreq = (isiMean == 0.0) ? EnDashSymbol : QString::number(1000.0 / isiMean, 'f', 1);

    plotDecorator.writeLabel("mean ISI: " + QString::number(isiMean, 'f', 1) + " ms",
                             x1, y1 - 3 * dy, Qt::AlignRight | Qt::AlignBottom);
    plotDecorator.writeLabel("1/mean ISI: " + isiFreq + " Hz",
                             x1, y1 - 2 * dy, Qt::AlignRight | Qt::AlignBottom);
    plotDecorator.writeLabel("std dev: " + QString::number(isiStdDev, 'f', 1) + " ms",
                             x1, y1 - dy, Qt::AlignRight | Qt::AlignBottom);
    plotDecorator.writeLabel(QString::number(numISIsRecorded) + ((numISIsRecorded == 1) ? " ISIs recorded" : " ISIs recorded"),
                             x1, y1, Qt::AlignRight | Qt::AlignBottom);

    plotDecorator.writeYAxisLabel("probability", ctHist, leftAxisLabelSpacing);

    for (int i = 0; i < (int) yAxisTicks.size(); ++i) {
        plotDecorator.drawLabeledTickMarkLeft(yAxisLabels[i], ctHist, yAxisTicks[i], TickMarkLength);
    }
    if (yAxisLog) {
        painter.setPen(Qt::lightGray);
        for (int i = 0; i < (int) yAxisTicks.size() - 1; ++i) {
            double y = yAxisTicks[i];
            plotDecorator.drawTickMarkLeft(ctHist, y + Log10_2, TickMarkMinorLength);
            plotDecorator.drawTickMarkLeft(ctHist, y + Log10_3, TickMarkMinorLength);
            plotDecorator.drawTickMarkLeft(ctHist, y + Log10_4, TickMarkMinorLength);
            plotDecorator.drawTickMarkLeft(ctHist, y + Log10_5, TickMarkMinorLength);
            plotDecorator.drawTickMarkLeft(ctHist, y + Log10_6, TickMarkMinorLength);
            plotDecorator.drawTickMarkLeft(ctHist, y + Log10_7, TickMarkMinorLength);
            plotDecorator.drawTickMarkLeft(ctHist, y + Log10_8, TickMarkMinorLength);
            plotDecorator.drawTickMarkLeft(ctHist, y + Log10_9, TickMarkMinorLength);
        }
        painter.setPen(Qt::white);
    }

    int tTick = 10;
    if (timeSpan > 500) tTick = 100;
    else if (timeSpan > 100) tTick = 50;
    for (int t = 0; t <= timeSpan; t += tTick) {
        if (t != timeSpan) {
            plotDecorator.drawLabeledTickMarkBottom(t, ctHist, t, TickMarkLength);
        } else {
            plotDecorator.drawLabeledTickMarkBottom(QString::number(t) + " ms", ctHist, t, TickMarkLength, true);
        }
    }
    if (tTick > timeSpan) {
        plotDecorator.drawLabeledTickMarkBottom(QString::number(timeSpan) + " ms",
                                                ctHist, timeSpan, TickMarkLength, true);
    }

    painter.setPen(Qt::yellow);
    if (histogramFrame.contains(cursor)) {
        painter.drawLine(cursor.x(), histogramFrame.top() + 1, cursor.x(), histogramFrame.bottom());
        painter.drawLine(histogramFrame.left() + 1, cursor.y(), histogramFrame.right(), cursor.y());
        plotDecorator.drawLabeledTickMarkBottom(ctHist.realXFromScreenX(cursor.x()),
                                                ctHist, ctHist.realXFromScreenX(cursor.x()), TickMarkLength);
        if (!yAxisLog) {
            plotDecorator.drawLabeledTickMarkLeft(QString::number(ctHist.realYFromScreenY(cursor.y()), 'f', 2),
                                                  ctHist, ctHist.realYFromScreenY(cursor.y()), TickMarkLength);
        } else {
            double yValue = pow(10.0, ctHist.realYFromScreenY(cursor.y()));
            int decPoints = -floor(ctHist.realYFromScreenY(cursor.y())) + 1;
            plotDecorator.drawLabeledTickMarkLeft(QString::number(yValue, 'f', decPoints),
                                                  ctHist, ctHist.realYFromScreenY(cursor.y()), TickMarkLength);
        }
    }

    QStylePainter stylePainter(this);
    stylePainter.drawImage(0, 0, image);
}

bool ISIPlot::saveMatFile(const QString& fileName) const
{
    MatFileWriter matFileWriter;

    QString fullName = state->signalSources->getNativeAndCustomNames(waveName);
    matFileWriter.addString("waveform_name", fullName);
    matFileWriter.addRealScalar("sample_rate", state->sampleRate->getNumericValue());
    matFileWriter.addRealScalar("bin_size", binSize);
    matFileWriter.addRealScalar("num_ISIs_recorded", numISIsRecorded);
    matFileWriter.addRealScalar("mean_ISI", isiMean);
    matFileWriter.addRealScalar("std_dev_ISI", isiStdDev);
    matFileWriter.addRealVector("probability", histogram);
    matFileWriter.addRealVector("t_bins", histogramTScale);
    matFileWriter.addString("HOW_TO_PLOT_HISTOGRAM",
                            "bar(t_bins,probability,1);");

    return matFileWriter.writeFile(fileName);
}

bool ISIPlot::saveCsvFile(QString fileName) const
{
    if (fileName.right(4).toLower() != ".csv") fileName.append(".csv");
    QFile csvFile(fileName);
    if (!csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "ISIPlot::saveCsvFile: Could not open CSV save file " << fileName;
        return false;
    }
    QTextStream outStream(&csvFile);

    outStream << "Interspike interval (msec),probability" << EndOfLine;
    for (int i = 0; i < (int) histogram.size(); ++i) {
        outStream << histogramTScale[i] << "," << histogram[i] << EndOfLine;
    }

    csvFile.close();
    return true;
}

bool ISIPlot::savePngFile(const QString& fileName) const
{
    return image.save(fileName, "PNG", 100);
}

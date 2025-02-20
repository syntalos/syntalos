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

#include "filterplot.h"

FilterPlot::FilterPlot(SystemState* state_, QWidget *parent) :
    QWidget(parent),
    state(state_),
    logarithmic(true)
{
    connect(state, SIGNAL(stateChanged()), this, SLOT(updateFromState()));
    wideColor.setRgb(191, 128, 64);
    highColor.setRgb(0, 156, 0);
    lowColor.setRgb(0, 0, 255);

    setBackgroundRole(QPalette::Window);
    setAutoFillBackground(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setFocusPolicy(Qt::StrongFocus);

    qreal dpr = devicePixelRatioF();
    pixmap = QPixmap(width() * dpr, height() * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    beadsEnabled = false;
    currentFrequency = 100;

    wideNormalizedGain = 0.5;
    lowNormalizedGain = 0.5;
    highNormalizedGain = 0.5;

    setMouseTracking(true);
}

void FilterPlot::updateFromState()
{
    update();
}

// Toggle between logarithmic and linear frequency scaling.
void FilterPlot::toggleLogarithmic(bool log)
{
    logarithmic = log;
    update();
}

// Initialize display for every paint event.
void FilterPlot::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter;
    painter.begin(&pixmap);

    updateLinearMaxFreq();
    drawBackground(painter);
    drawPlottingRect(painter);
    labelAxis(painter);
    drawHashmarks(painter);
    drawLegend(painter);

    drawLowFilter(painter);
    drawHighFilter(painter);
    drawWideFilter(painter);

    if (beadsEnabled) {
        drawLowBead(painter);
        drawHighBead(painter);
        drawWideBead(painter);
    }

    QPainter sPainter(this);
    sPainter.setRenderHint(QPainter::Antialiasing);
    sPainter.drawPixmap(0, 0, pixmap);
}

// When FilterPlot is resized, recalculate what needs to be painted and repaint.
void FilterPlot::resizeEvent(QResizeEvent* /* event */)
{
    // Pixel map used for double buffering
    pixmap = QPixmap(size());
    pixmap.fill();
    adjustSize();
//    pixmap.fill(this, 0, 0);
    update();
}

// Handle a movement of the mouse.
void FilterPlot::mouseMoveEvent(QMouseEvent *event)
{
    // Make sure mouse's new position is within plot.
    if (event->pos().x() >= actualTopLeft.x() && event->pos().x() <= bottomRight.x() && event->pos().y() >= actualTopLeft.y() && event->pos().y() <= bottomRight.y()) {
        // Convert mouse position to ratio, then frequency.
        double ratio = (double) (event->pos().x() - actualTopLeft.x()) / (double) domain;
        currentFrequency = ratioToFrequency(ratio);
        wideNormalizedGain = getWideAttenuation(currentFrequency);
        lowNormalizedGain = getLowAttenuation(currentFrequency);
        highNormalizedGain = getHighAttenuation(currentFrequency);
        beadsEnabled = true;
        emit mouseMovedInPlot(currentFrequency, wideNormalizedGain, lowNormalizedGain, highNormalizedGain);
    } else {
        beadsEnabled = false;
        emit mouseLeftPlot();
    }
    update();
}

// Track when mouse leaves FilterPlot.
void FilterPlot::leaveEvent(QEvent* /* event */)
{
    beadsEnabled = false;
    emit mouseLeftPlot();
    update();
}

// Minimum size of the plot
QSize FilterPlot::minimumSizeHint() const
{
    return QSize(FilterPlotXSize / 10, FilterPlotYSize / 10);
}

// Default suggested size of the plot
QSize FilterPlot::sizeHint() const
{
    return QSize(FilterPlotXSize, FilterPlotYSize);
}

// Clear the plot, draw a small border, and fill in the plot's background.
void FilterPlot::drawBackground(QPainter &painter)
{
    // Clear entire Widget display area.
    painter.eraseRect(rect());

    // Draw border around Widget display area.
    painter.setPen(Qt::darkGray);
    QRect rect(0, 0, width() - 1, height() - 1);

    painter.fillRect(rect, Qt::white);
    painter.drawRect(rect);
}

// Draw the white rectangle which contains the actual plot of the transfer function of gain vs. frequency.
void FilterPlot::drawPlottingRect(QPainter &painter)
{
    // Draw border around plottingRect display area.
    painter.setPen(Qt::darkGray);
    visibleTopLeft.setX(0.04 * width());
    actualTopLeft.setX(0.04 * width());
    visibleTopLeft.setY(0.2 * height());
    actualTopLeft.setY(visibleTopLeft.y());
    bottomRight.setX(0.96 * width());
    bottomRight.setY(0.8 * height());
    domain = bottomRight.x() - actualTopLeft.x() + 1;
    range = bottomRight.y() - actualTopLeft.y() + 1;
    QRect plottingRect(visibleTopLeft, bottomRight);

    painter.fillRect(plottingRect, Qt::white);
    painter.drawRect(plottingRect);
}

// Label frequency axis.
void FilterPlot::labelAxis(QPainter &painter)
{
    painter.setPen(Qt::black);

    // Label x axis.
    QPoint xLabelTopLeft(0.4 * width(), 0.85 * height());
    QPoint xLabelBottomRight(0.6 * width(), 0.95 * height());
    QRect xLabelRect(xLabelTopLeft, xLabelBottomRight);

    painter.drawText(xLabelRect, Qt::AlignCenter, "Frequency (Hz)");
}

// Draw hashmarks along the frequency axis.
void FilterPlot::drawHashmarks(QPainter &painter)
{
    if (logarithmic) {
        // Draw hashmarks at 0.03 Hz, 0.3 Hz, 3 Hz, 30 Hz, 300 Hz, 3 kHz, and 30 kHz.
        hashmark(0.03, painter);
        hashmark(0.3, painter);
        hashmark(3, painter);
        hashmark(30, painter);
        hashmark(300, painter);
        hashmark(3000, painter);
        hashmark(30000, painter);

        // Draw hashmarks at 0.1 Hz, 1 Hz, 10 Hz, 100 Hz, 1 kHz, and 10 kHz.
        hashmark(0.1, painter);
        hashmark(1, painter);
        hashmark(10, painter);
        hashmark(100, painter);
        hashmark(1000, painter);
        hashmark(10000, painter);

        // Draw vertical gray lines along logarithmic scale.
        logLines(0.04, 0.1, 0.01, painter);
        logLines(0.1, 1, 0.1, painter);
        logLines(1, 10, 1, painter);
        logLines(10, 100, 10, painter);
        logLines(100, 1000, 100, painter);
        logLines(1000, 10000, 1000, painter);
        logLines(10000, 20000, 10000, painter);
    } else {
        // Draw 9 hashmarks (0 - 8).
        for (int i = 0; i < 9; ++i) {
            hashmark(i * linearMaxFreq / 8.0, painter);
        }

        // Draw vertical gray lines for each non-border hashmark.
        line(1.0 * linearMaxFreq / 8.0, painter);
        line(2.0 * linearMaxFreq / 8.0, painter);
        line(3.0 * linearMaxFreq / 8.0, painter);
        line(4.0 * linearMaxFreq / 8.0, painter);
        line(5.0 * linearMaxFreq / 8.0, painter);
        line(6.0 * linearMaxFreq / 8.0, painter);
        line(7.0 * linearMaxFreq / 8.0, painter);
    }

    // Draw horizontal gray line at 0.707 (cutoff frequency).
    painter.setPen(Qt::DotLine);
    painter.drawLine(actualTopLeft.x(), bottomRight.y() - (range * 0.707), bottomRight.x(), bottomRight.y() - (range * 0.707));
}

// Draw vertical gray lines on a logarithmic scale.
void FilterPlot::logLines(double start, double stop, double increment, QPainter &painter)
{
    painter.setPen(Qt::lightGray);

    // Convert start, stop, and increment to hundredths for the for-loop, to avoid iterating with a double variable
    for (int currentXHundredth = start * 100; currentXHundredth <= stop * 100; currentXHundredth += increment * 100) {
        int xpos = frequencyToRatio(((double) currentXHundredth) / 100.0) * domain + actualTopLeft.x();
        painter.drawLine(xpos, actualTopLeft.y() + 1, xpos, bottomRight.y());
    }
}

// Draw a vertical gray line (for linear scale).
void FilterPlot::line(double frequency, QPainter &painter)
{
    painter.setPen(Qt::lightGray);

    int xpos = frequencyToRatio(frequency) * domain + actualTopLeft.x();
    painter.drawLine(xpos, actualTopLeft.y() + 1, xpos, bottomRight.y());
}

// Draw a single hashmark at the given frequency.
void FilterPlot::hashmark(double frequency, QPainter &painter)
{
    painter.setPen(Qt::black);

    double ratio = frequencyToRatio(frequency);
    QString label = QString::number(frequency);
    int xpos = ratio * domain + actualTopLeft.x();

    // Draw the actual hashmark.
    painter.drawLine(xpos, bottomRight.y(), xpos, bottomRight.y() - 5);
    QPoint hashmarkLabelTopLeft(xpos - QFontMetrics(painter.font()).horizontalAdvance(label)*0.5, bottomRight.y());
    QPoint hashmarkLabelBottomRight(xpos + QFontMetrics(painter.font()).horizontalAdvance(label)*0.5, bottomRight.y() + QFontMetrics(painter.font()).height());
    QRect hashmarkLabelRect(hashmarkLabelTopLeft, hashmarkLabelBottomRight);

    painter.drawText(hashmarkLabelRect, Qt::AlignCenter, label);
}

void FilterPlot::drawWideFilter(QPainter &painter)
{
    painter.setPen(wideColor);

    int previousX = actualTopLeft.x();
    int previousY = (1 - getWideAttenuation(0.03)) * range + actualTopLeft.y();

    bool minReached = false;

    for (int pixel = 1; pixel < domain; pixel++) {
        double thisPixelRatio = ((double) pixel) / ((double) domain);
        double thisF = ratioToFrequency(thisPixelRatio);

        // Draw a line to this point.
        int xpos = pixel + actualTopLeft.x();
        int ypos = (1 - getWideAttenuation(thisF)) * range + actualTopLeft.y();

        checkForNotchMinimum(logarithmic, thisF, previousY, ypos, minReached);

        painter.drawLine(previousX, previousY, xpos, ypos);
        previousX = xpos;
        previousY = ypos;
    }
}

void FilterPlot::drawLowFilter(QPainter &painter)
{
    painter.setPen(lowColor);

    int previousX = actualTopLeft.x();
    int previousY = (1 - getLowAttenuation(0.03)) * range + actualTopLeft.y();

    bool minReached = false;

    for (int pixel = 1; pixel < domain; pixel++) {
        double thisPixelRatio = ((double) pixel) / ((double) domain);
        double thisF = ratioToFrequency(thisPixelRatio);

        // Draw line to this point.
        int xpos = pixel + actualTopLeft.x();
        int ypos = (1 - getLowAttenuation(thisF)) * range + actualTopLeft.y();

        checkForNotchMinimum(logarithmic, thisF, previousY, ypos, minReached);

        painter.drawLine(previousX, previousY, xpos, ypos);
        previousX = xpos;
        previousY = ypos;

    }
}

void FilterPlot::drawHighFilter(QPainter &painter)
{
    painter.setPen(highColor);

    int previousX = actualTopLeft.x();
    int previousY = (1 - getHighAttenuation(0.03)) * range + actualTopLeft.y();

    bool minReached = false;

    for (int pixel = 1; pixel < domain; pixel++) {
        double thisPixelRatio = ((double) pixel) / ((double) domain);
        double thisF = ratioToFrequency(thisPixelRatio);

        // Draw line to this point.
        int xpos = pixel + actualTopLeft.x();
        int ypos = (1 - getHighAttenuation(thisF)) * range + actualTopLeft.y();

        checkForNotchMinimum(logarithmic, thisF, previousY, ypos, minReached);

        painter.drawLine(previousX, previousY, xpos, ypos);
        previousX = xpos;
        previousY = ypos;
    }
}

void FilterPlot::drawWideBead(QPainter &painter)
{
    painter.setPen(Qt::black);
    painter.setBrush(wideColor);
    double pixelRatio = frequencyToRatio(currentFrequency);
    int xPixel = pixelRatio * domain + actualTopLeft.x();
    int yPixel = (1 - wideNormalizedGain) * range + actualTopLeft.y();
    painter.drawEllipse(QPoint(xPixel, yPixel), 3, 3);
}

void FilterPlot::drawLowBead(QPainter &painter)
{
    painter.setPen(Qt::black);
    painter.setBrush(lowColor);
    double pixelRatio = frequencyToRatio(currentFrequency);
    int xPixel = pixelRatio * domain + actualTopLeft.x();
    int yPixel = (1 - lowNormalizedGain) * range + actualTopLeft.y();
    painter.drawEllipse(QPoint(xPixel, yPixel), 3, 3);
}

void FilterPlot::drawHighBead(QPainter &painter)
{
    painter.setPen(Qt::black);
    painter.setBrush(highColor);
    double pixelRatio = frequencyToRatio(currentFrequency);
    int xPixel = pixelRatio * domain + actualTopLeft.x();
    int yPixel = (1 - highNormalizedGain) * range + actualTopLeft.y();
    painter.drawEllipse(QPoint(xPixel, yPixel), 3, 3);
}

// Check to see if this is the minimum point from the notch filter. If it is, set the gain to 0 for that one point.
void FilterPlot::checkForNotchMinimum(bool logarithmic, double thisF, int previousY, int &ypos, bool &minReached)
{
    if (state->notchFreq->getNumericValue() == 50.0) {
        if (logarithmic) {
            if ((thisF > 45) && (thisF < 55)) {
                if (ypos < previousY && !minReached) {
                    minReached = true;
                    ypos = bottomRight.y();
                }
            }
        } else {
            if ((thisF > 50 - 0.01*linearMaxFreq) && (thisF < 50 + 0.01*linearMaxFreq)) {
                if ((ypos < previousY) && (!minReached) && (thisF > 25)) {
                    minReached = true;
                    ypos = bottomRight.y();
                }
            }
        }
    } else if (state->notchFreq->getNumericValue() == 60.0) {
        if (logarithmic) {
            if ((thisF > 55) && (thisF < 65)) {
                if (ypos < previousY && !minReached) {
                    minReached = true;
                    ypos = bottomRight.y();
                }
            }
        } else {
            if ((thisF > 60 - 0.01*linearMaxFreq) && (thisF < 60 + 0.01*linearMaxFreq)) {
                if ((ypos < previousY) && (!minReached) && (thisF > 25)) {
                    minReached = true;
                    ypos = bottomRight.y();
                }
            }
        }
    }
}

// Draw a legend distinguishing hardware and composite functions.
void FilterPlot::drawLegend(QPainter &painter)
{
    painter.setPen(Qt::black);

    QString lowFilterString = tr("  LOW Pass Response");
    QString wideFilterString = tr("  WIDE Band Response");
    QString highFilterString = tr("  HIGH Pass Response");

    int lowWidth = painter.fontMetrics().horizontalAdvance(lowFilterString);
    int wideWidth = painter.fontMetrics().horizontalAdvance(wideFilterString);
    int highWidth = painter.fontMetrics().horizontalAdvance(highFilterString);
    int textHeight = height() * 0.1 + painter.fontMetrics().height()/2;

    int thirdWidth = domain / 3;

    int firstThirdCenter = thirdWidth / 2;
    int secondThirdCenter = firstThirdCenter + thirdWidth;
    int thirdThirdCenter = secondThirdCenter + thirdWidth;

    QPoint lowPoint(visibleTopLeft.x() + firstThirdCenter - lowWidth / 2, textHeight);
    QPoint widePoint(visibleTopLeft.x() + secondThirdCenter - wideWidth / 2, textHeight);
    QPoint highPoint(visibleTopLeft.x() + thirdThirdCenter - highWidth / 2, textHeight);

    painter.drawText(lowPoint, lowFilterString);
    painter.drawText(widePoint, wideFilterString);
    painter.drawText(highPoint, highFilterString);

    lowPoint.setY(lowPoint.y() - painter.fontMetrics().ascent()/2);
    widePoint.setY(widePoint.y() - painter.fontMetrics().ascent()/2);
    highPoint.setY(highPoint.y() - painter.fontMetrics().ascent()/2);

    const int LineLength = 20;
    painter.setPen(lowColor);
    painter.drawLine(lowPoint, QPoint(lowPoint.x() - LineLength, lowPoint.y()));

    painter.setPen(wideColor);
    painter.drawLine(widePoint, QPoint(widePoint.x() - LineLength, widePoint.y()));

    painter.setPen(highColor);
    painter.drawLine(highPoint, QPoint(highPoint.x() - LineLength, highPoint.y()));
}


// Return the normalized gain of an nth-order Bessel filter at a particular frequency normalized to the cutoff frequency.
double FilterPlot::bessel(double omega, int filterOrder)
{
    double numerator, denominator;

    if (filterOrder == 1) {
        numerator = 1;
        double w3db = omega;
        denominator = sqrt(pow(w3db, 2) + 1);
    } else if (filterOrder == 2) {
        numerator = 3;
        double w3db = omega * 1.361654129;
        denominator = sqrt(pow(w3db, 4) + 3 * pow(w3db, 2) + 9);
    } else if (filterOrder == 3) {
        numerator = 15;
        double w3db = omega * 1.755672389;
        denominator = sqrt(pow(w3db, 6) + 6 * pow(w3db, 4) + 45 * pow(w3db, 2) + 225);
    } else if (filterOrder == 4) {
        numerator = 105;
        double w3db = omega * 2.113917675;
        denominator = sqrt(pow(w3db, 8) + 10 * pow(w3db, 6) + 135 * pow(w3db, 4) + 1575 * pow(w3db, 2) + 11025);
    } else if (filterOrder == 5) {
        numerator = 945;
        double w3db = omega * 2.427410702;
        denominator = sqrt(pow(w3db, 10) + 15 * pow(w3db, 8) + 315 * pow(w3db, 6) + 6300 * pow(w3db, 4) + 99225 * pow(w3db, 2) + 893025);
    } else if (filterOrder == 6) {
        numerator = 10395;
        double w3db = omega * 2.703395061;
        denominator = sqrt(pow(w3db, 12) + 21 * pow(w3db, 10) + 630 * pow(w3db, 8) + 18900 * pow(w3db, 6) + 496125 * pow(w3db, 4) + 9823275 * pow(w3db, 2) + 108056025);
    } else if (filterOrder == 7) {
        numerator = 135135;
        double w3db = omega * 2.951722147;
        denominator = sqrt(pow(w3db, 14) + 28 * pow(w3db, 12) + 1134 * pow(w3db, 10) + 47250 * pow(w3db, 8) + 1819125 * pow(w3db, 6) + 58939650 * pow(w3db, 4) + 1404728325 * pow(w3db, 2) + 18261468225);
    } else {
        numerator = 2027025;
        double w3db = omega * 3.17962;
        denominator = sqrt(pow(w3db, 16) + 36 * pow(w3db, 14) + 1890 * pow(w3db, 12) + 103950 * pow(w3db, 10) + 5457375 * pow(w3db, 8) + 255405150 * pow(w3db, 6) + 9833098275 * pow(w3db, 4) + 273922072750 * pow(w3db, 2) + 4108830350625);
    }

    return numerator / denominator;
}

// Return the normalized gain of an nth-order Butterworth filter at a particular frequency normalized to the cutoff frequency.
double FilterPlot::butterworth(double omega, int filterOrder)
{
    return (1.0 / sqrt(1 + pow(omega, 2 * filterOrder)));
}

// Return the normalized gain of a Notch filter at a particular frequency given the notch frequency.
double FilterPlot::notch(double frequency, double notchFreq)
{
    // If frequency and notchFreq are the same, just return 0 to avoid divide-by-zero exception.
    if (frequency == notchFreq)
        return 0.0;

    double denominator = sqrt(1 + pow(((NotchBandwidth * frequency) / (pow(notchFreq, 2) - pow(frequency, 2))), 2));
    return (1.0 / denominator);
}

double FilterPlot::getWideAttenuation(double frequency)
{
    // Apply 3rd-order Butterworth LPF at upper bandwidth frequency.
    double denominator = sqrt(1 + pow(frequency / state->actualUpperBandwidth->getValue(), 2 * 3));
    double attenuation = 1.0 / denominator;

    // Aply 1st-order HP at lower bandwidth frequency.
    denominator = sqrt(1 + pow(state->actualLowerBandwidth->getValue() / frequency, 2 * 1));
    attenuation *= 1.0 / denominator;

    // If dsp is enabled, apply 1st-order HPF at DSP cutoff frequency.
    if (state->dspEnabled->getValue()) {
        denominator = sqrt(1 + pow(state->actualDspCutoffFreq->getValue() / frequency, 2 * 1));
        attenuation *= 1.0 / denominator;
    }

    if (state->notchFreq->getNumericValue() == 50.0) {
        attenuation *= notch(frequency, 50);
    } else if (state->notchFreq->getNumericValue() == 60.0) {
        attenuation *= notch(frequency, 60);
    }

    return attenuation;
}

double FilterPlot::getLowAttenuation(double frequency)
{
    double attenuation = getWideAttenuation(frequency);

    // Low-Pass
    if (state->lowType->getValueString().toLower() == "bessel") {
        attenuation *= bessel(frequency / state->lowSWCutoffFreq->getValue(), state->lowOrder->getValue());
    } else if (state->lowType->getValueString().toLower() == "butterworth") {
        attenuation *= butterworth(frequency / state->lowSWCutoffFreq->getValue(), state->lowOrder->getValue());
    }

    return attenuation;
}

double FilterPlot::getHighAttenuation(double frequency)
{
    double attenuation = getWideAttenuation(frequency);

    // High-Pass
    if (state->highType->getValueString().toLower() == "bessel") {
        attenuation *= bessel(state->highSWCutoffFreq->getValue() / frequency, state->highOrder->getValue());
    } else if (state->highType->getValueString().toLower() == "butterworth") {
        attenuation *= butterworth(state->highSWCutoffFreq->getValue() / frequency, state->highOrder->getValue());
    }

    return attenuation;
}

void FilterPlot::updateLinearMaxFreq()
{
    highestCutoff = state->actualUpperBandwidth->getValue();
    highestCutoff = std::max(highestCutoff, state->lowSWCutoffFreq->getValue());
    highestCutoff = std::max(highestCutoff, state->highSWCutoffFreq->getValue());
    highestCutoff = roundToAcceptableMidpoint(1.2 * highestCutoff);
    linearMaxFreq = highestCutoff;
}

// Round the given number to the nearest acceptable midpoint (easily divisible by 4 for reasonable scaling in linear view).
double FilterPlot::roundToAcceptableMidpoint(double number)
{
    double scaledNum = number;
    int scaledExp = 0;

    // Bring scaledNum down to the range 0.1 to 1.
    while (scaledNum > 1) {
        scaledNum = scaledNum / 10.0;
        scaledExp = scaledExp + 1;
    }

    // Bring scaledNum up to the range 0.1 to 1.
    while (scaledNum <= 0.1) {
        scaledNum = scaledNum * 10.0;
        scaledExp = scaledExp - 1;
    }

    QVector<double> acceptableMidpoints;
    acceptableMidpoints.append(0.1);
    acceptableMidpoints.append(0.2);
    acceptableMidpoints.append(0.4);
    acceptableMidpoints.append(0.8);

    double closestDiff = 1.0;
    int closestIndex = 0;

    // Scan through all acceptable midpoints, finding the one closest to scaledNum.
    for (int i = 0; i < acceptableMidpoints.size(); ++i) {
        double diff = abs(scaledNum - acceptableMidpoints[i]);
        if (diff < closestDiff) {
            closestDiff = diff;
            closestIndex = i;
        }
    }
    scaledNum = acceptableMidpoints[closestIndex];
    return scaledNum * pow(10.0, scaledExp);
}

// Convert the given frequency to a ratio between 0 and 1 representing its position on the frequency axis.
double FilterPlot::frequencyToRatio(double frequency)
{
    if (logarithmic) {
        // Return the given frequency as a decimal between 0 and 1 over a logarithmic range from 0.03 Hz to 30 kHz.
        // Numbers outside this range will be brought to 0 or 1.
        if (frequency <= 0.03) return 0.0;
        if (frequency >= 30000.0) return 1.0;
        return ((log10(frequency / 0.03)) / 6.0);
    } else {
        // Return the given frequency as a decimal between 0 and 1 over a linear range from 0 Hz to the maximum freq.
        // Numbers outside this range will be brought to 0 or 1.
        if (frequency <= 0.0) return 0.0;
        if (frequency >= linearMaxFreq)return 1.0;
        return (frequency / linearMaxFreq);
    }
}

// Convert the given ratio between 0 and 1 representing a posiion on the frequency axis to its frequency.
double FilterPlot::ratioToFrequency(double ratio)
{
    if (logarithmic) {
        // Return the given decimal between 0 and 1 as a frequency over a logarithmic range from 0.03 Hz to 30 kHz.
        // Numbers outside this range will be brought to 0.03 Hz or 30 kHz.
        if (ratio < 0.0) return 0.03;
        if (ratio > 1.0)return 30000.0;
        return (0.03 * pow(10.0, 6.0 * ratio));
    } else {
        // Return the given decimal between 0 and 1 as a frequency over a linear range from 0 Hz to the maximum freq.
        // Numbers outside this range will be brought to 0 Hz or the maximum freq.
        if (ratio < 0.0) return 0.0;
        if (ratio > 1.0) return linearMaxFreq;
        return (ratio * linearMaxFreq);
    }
}

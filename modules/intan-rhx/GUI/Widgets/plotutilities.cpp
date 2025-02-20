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
#include <iostream>
#include "plotutilities.h"

CoordinateTranslator::CoordinateTranslator()
{
    set(QRect(0, 0, 2, 2), 0.0, 1.0, 0.0, 1.0);   // dummy default values to avoid divide-by zero errors
}

CoordinateTranslator::CoordinateTranslator(QRect frame__, double xMinReal__, double xMaxReal__, double yMinReal__, double yMaxReal__)
{
    set(frame__, xMinReal__, xMaxReal__, yMinReal__, yMaxReal__);
}

void CoordinateTranslator::set(QRect frame__, double xMinReal__, double xMaxReal__, double yMinReal__, double yMaxReal__)
{
    frame_ = frame__;
    xMinReal_ = xMinReal__;
    xMaxReal_ = xMaxReal__;
    yMinReal_ = yMinReal__;
    yMaxReal_ = yMaxReal__;
    double xSpanReal = xMaxReal_ - xMinReal_;
    double ySpanReal = yMaxReal_ - yMinReal_;
    double widthScreen = (double)(frame_.width() - 1);
    double heightScreen = (double)(frame_.height() - 1);

    if (xSpanReal != 0.0) xScale = widthScreen / xSpanReal;
    else xScale = std::numeric_limits<double>::infinity();  // Prevent divide-by-zero error.

    if (ySpanReal != 0.0) yScale = heightScreen / ySpanReal;
    else yScale = std::numeric_limits<double>::infinity();  // Prevent divide-by-zero error.
}

int CoordinateTranslator::screenXFromRealX(double x) const
{
    return frame_.left() + (int) (xScale * (x - xMinReal_));
}

int CoordinateTranslator::screenYFromRealY(double y) const
{
    return frame_.top() + (int) (yScale * (yMaxReal_ - y));
}

double CoordinateTranslator::realXFromScreenX(int x) const
{
    return xMinReal_ + ((double) (x - frame_.left())) / xScale;
}

double CoordinateTranslator::realYFromScreenY(int y) const
{
    return yMaxReal_ - ((double) (y - frame_.top())) / yScale;
}


CoordinateTranslator1D::CoordinateTranslator1D()
{
    set(0, 1, 0.0, 1.0);   // dummy default values to avoid divide-by zero errors
}

CoordinateTranslator1D::CoordinateTranslator1D(int minScreen__, int maxScreen__, double minReal__, double maxReal__)
{
    set(minScreen__, maxScreen__, minReal__, maxReal__);
}

void CoordinateTranslator1D::set(int minScreen__, int maxScreen__, double minReal__, double maxReal__)
{
    minScreen_ = minScreen__;
    maxScreen_ = maxScreen__;
    minReal_ = minReal__;
    maxReal_ = maxReal__;
    double spanReal = maxReal_ - minReal_;
    double spanScreen = (double)(maxScreen_ - minScreen_);

    if (spanReal != 0.0) scale = spanScreen / spanReal;
    else scale = std::numeric_limits<double>::infinity();  // Prevent divide-by-zero error.
}

int CoordinateTranslator1D::screenFromReal(double real) const
{
    return minScreen_ + (int) (scale * (real - minReal_));
}

double CoordinateTranslator1D::realFromScreen(int screen) const
{
    return minReal_ + ((double) (screen - minScreen_)) / scale;
}


PlotDecorator::PlotDecorator(QPainter &painter_) :
    painter(painter_)
{
}

void PlotDecorator::drawVerticalAxisLine(CoordinateTranslator& ct, double xReal, int shrink) const
{
    painter.drawLine(QPoint(ct.screenXFromRealX(xReal), ct.yTop() + shrink),
                     QPoint(ct.screenXFromRealX(xReal), ct.yBottom() - shrink));
}

void PlotDecorator::drawHorizontalAxisLine(CoordinateTranslator& ct, double yReal, int shrink) const
{
    painter.drawLine(QPoint(ct.xLeft() + shrink, ct.screenYFromRealY(yReal)),
                     QPoint(ct.xRight() - shrink, ct.screenYFromRealY(yReal)));
}

void PlotDecorator::writeLabel(const QString& text, int x, int y, int flags) const
{
    const int TextBoxWidth = painter.fontMetrics().horizontalAdvance(text);
    const int TextBoxHeight = painter.fontMetrics().height();
    const int TextBoxHalfWidth = qCeil((double)TextBoxWidth / 2.0);
    const int TextBoxHalfHeight = qCeil((double)TextBoxHeight / 2.0);

    int xPos = x;
    int yPos = y;

    if (flags & Qt::AlignHCenter) {
        xPos -= TextBoxHalfWidth;
    } else if (flags & Qt::AlignRight) {
        xPos -= TextBoxWidth;
    }

    if (flags & Qt::AlignVCenter) {
        yPos -= TextBoxHalfHeight;
    } else if (flags & Qt::AlignBottom) {
        yPos -= TextBoxHeight;
    }

    painter.drawText(xPos, yPos, TextBoxWidth, TextBoxHeight, flags, text);
}

void PlotDecorator::writeLabel(int number, int x, int y, int flags) const
{
    writeLabel(QString::number(number), x, y, flags);
}

void PlotDecorator::writeYAxisLabel(const QString& text, CoordinateTranslator& ct, int xOffset) const
{
    painter.save();
    painter.translate(ct.xLeft() - xOffset, (ct.yBottom() + ct.yTop()) / 2);
    painter.rotate(-90);
    // writeLabel(text, ct.xLeft() - xOffset, (ct.yBottom() + ct.yTop()) / 2, Qt::AlignHCenter | Qt::AlignBottom);
    writeLabel(text, 0, 0, Qt::AlignHCenter | Qt::AlignBottom);
    painter.restore();
}

void PlotDecorator::drawTickMarkLeft(CoordinateTranslator& ct, double yReal, int length) const
{
    painter.drawLine(QPoint(ct.xLeft() - 1, ct.screenYFromRealY(yReal)),
                     QPoint(ct.xLeft() - 1 - length, ct.screenYFromRealY(yReal)));
}

void PlotDecorator::drawTickMarkRight(CoordinateTranslator& ct, double yReal, int length) const
{
    painter.drawLine(QPoint(ct.xRight() + 1, ct.screenYFromRealY(yReal)),
                     QPoint(ct.xRight() + 1 + length, ct.screenYFromRealY(yReal)));
}

void PlotDecorator::drawTickMarkBottom(CoordinateTranslator& ct, double xReal, int length) const
{
    painter.drawLine(QPoint(ct.screenXFromRealX(xReal), ct.yBottom() + 1),
                     QPoint(ct.screenXFromRealX(xReal), ct.yBottom() + 1 + length));
}

void PlotDecorator::drawTickMarkTop(CoordinateTranslator& ct, double xReal, int length) const
{
    painter.drawLine(QPoint(ct.screenXFromRealX(xReal), ct.yTop() - 1),
                     QPoint(ct.screenXFromRealX(xReal), ct.yTop() - 1 - length));
}

void PlotDecorator::drawLabeledTickMarkLeft(const QString& text, CoordinateTranslator& ct, double yReal, int length) const
{
    if (length != 0) drawTickMarkLeft(ct, yReal, length);
    writeLabel(text, ct.xLeft() - length - 3, ct.screenYFromRealY(yReal), Qt::AlignRight | Qt::AlignVCenter);
}

void PlotDecorator::drawLabeledTickMarkLeft(int number, CoordinateTranslator& ct, double yReal, int length) const
{
    drawLabeledTickMarkLeft(QString::number(number), ct, yReal, length);
}

void PlotDecorator::drawLabeledTickMarkRight(const QString& text, CoordinateTranslator& ct, double yReal, int length) const
{
    if (length != 0) drawTickMarkRight(ct, yReal, length);
    writeLabel(text, ct.xRight() + length + 4, ct.screenYFromRealY(yReal), Qt::AlignLeft | Qt::AlignVCenter);
}

void PlotDecorator::drawLabeledTickMarkRight(int number, CoordinateTranslator& ct, double yReal, int length) const
{
    drawLabeledTickMarkRight(QString::number(number), ct, yReal, length);
}

void PlotDecorator::drawLabeledTickMarkBottom(const QString& text, CoordinateTranslator& ct, double xReal, int length, bool rightEnd) const
{
    if (length != 0) drawTickMarkBottom(ct, xReal, length);
    writeLabel(text, ct.screenXFromRealX(xReal), ct.yBottom() + length + 2,
               rightEnd ? (Qt::AlignRight | Qt::AlignTop) : (Qt::AlignHCenter | Qt::AlignTop));
}

void PlotDecorator::drawLabeledTickMarkBottom(int number, CoordinateTranslator& ct, double xReal, int length, bool rightEnd) const
{
    drawLabeledTickMarkBottom(QString::number(number), ct, xReal, length, rightEnd);
}

void PlotDecorator::drawLabeledTickMarkTop(const QString& text, CoordinateTranslator& ct, double xReal, int length, bool rightEnd) const
{
    if (length != 0) drawTickMarkTop(ct, xReal, length);
    writeLabel(text, ct.screenXFromRealX(xReal), ct.yTop() - length - 2,
               rightEnd ? (Qt::AlignRight | Qt::AlignBottom) : (Qt::AlignHCenter | Qt::AlignBottom));
}

void PlotDecorator::drawLabeledTickMarkTop(int number, CoordinateTranslator& ct, double xReal, int length, bool rightEnd) const
{
    drawLabeledTickMarkTop(QString::number(number), ct, xReal, length, rightEnd);
}

// Given the maximum y value in a data series (maxYValue), this function generates a appropriate y scale ranging from zero
// to a 'round' number slightly higher than maxYValue; this value is returned.  The function also returns two vectors: a
// list of values for evenly-spaced tick marks on this axis and a list of QString labels for these tick marks.
double PlotDecorator::autoCalculateYAxis(double maxY, std::vector<double>& yAxisTicks, std::vector<QString>& yAxisLabels) const
{
    yAxisTicks.clear();
    yAxisLabels.clear();
    yAxisTicks.push_back(0.0);
    yAxisLabels.push_back("0");

    if (maxY <= 0.0) return 0.0;

    double magnitude = pow(10.0, floor(log10(maxY)));
    double significand = maxY / magnitude;

    if (significand > 8.0) {
        yAxisTicks.push_back(2.0 * magnitude);
        yAxisTicks.push_back(4.0 * magnitude);
        yAxisTicks.push_back(6.0 * magnitude);
        yAxisTicks.push_back(8.0 * magnitude);
        yAxisTicks.push_back(10.0 * magnitude);
    } else if (significand > 5.0) {
        yAxisTicks.push_back(2.0 * magnitude);
        yAxisTicks.push_back(4.0 * magnitude);
        yAxisTicks.push_back(6.0 * magnitude);
        yAxisTicks.push_back(8.0 * magnitude);
    } else if (significand > 4.0) {
        yAxisTicks.push_back(1.0 * magnitude);
        yAxisTicks.push_back(2.0 * magnitude);
        yAxisTicks.push_back(3.0 * magnitude);
        yAxisTicks.push_back(4.0 * magnitude);
        yAxisTicks.push_back(5.0 * magnitude);
    } else if (significand > 2.5) {
        yAxisTicks.push_back(1.0 * magnitude);
        yAxisTicks.push_back(2.0 * magnitude);
        yAxisTicks.push_back(3.0 * magnitude);
        yAxisTicks.push_back(4.0 * magnitude);
    } else if (significand > 2.0) {
        yAxisTicks.push_back(0.5 * magnitude);
        yAxisTicks.push_back(1.0 * magnitude);
        yAxisTicks.push_back(1.5 * magnitude);
        yAxisTicks.push_back(2.0 * magnitude);
        yAxisTicks.push_back(2.5 * magnitude);
    } else if (significand > 1.5) {
        yAxisTicks.push_back(0.5 * magnitude);
        yAxisTicks.push_back(1.0 * magnitude);
        yAxisTicks.push_back(1.5 * magnitude);
        yAxisTicks.push_back(2.0 * magnitude);
    } else {
        yAxisTicks.push_back(0.2 * magnitude);
        yAxisTicks.push_back(0.4 * magnitude);
        yAxisTicks.push_back(0.6 * magnitude);
        yAxisTicks.push_back(0.8 * magnitude);
        yAxisTicks.push_back(1.0 * magnitude);
        yAxisTicks.push_back(1.2 * magnitude);
    }

    for (int i = 1; i < (int) yAxisTicks.size(); ++i) {
        yAxisLabels.push_back(QString::number(yAxisTicks[i]));
    }
    return yAxisTicks[yAxisTicks.size() - 1];
}

MinMaxPair PlotDecorator::autoCalculateLogYAxis(double minNonZeroY, double maxY, std::vector<double>& yAxisTicks,
                                                std::vector<QString>& yAxisLabels) const
{
    MinMaxPair result;
    result.min = floor(log10(minNonZeroY));
    result.max = ceil(log10(maxY));
    for (int i = round(result.min); i <= round(result.max); ++i) {
        yAxisTicks.push_back(i);
        yAxisLabels.push_back(QString::number(pow(10.0, i), 'f', (i < 0) ? -i : 0));
    }
    return result;
}


ColorScale::ColorScale(float minValue_, float maxValue_) :
    minValue(minValue_),
    maxValue(maxValue_)
{
    valueRange = maxValue - minValue;
    colorMap.resize(ColorMapSize);
    calculateColorMap();
}

void ColorScale::setRange(float minValue_, float maxValue_)
{
    minValue = minValue_;
    maxValue = maxValue_;
    valueRange = maxValue - minValue;
    calculateColorMap();
}

QColor ColorScale::getColor(double value) const
{
    int index = qRound((ColorMapSize - 1) * (value - minValue) / valueRange);
    if (index < 0) {
        index = 0;
    } else if (index >= ColorMapSize) {
        index = ColorMapSize - 1;
    }
    return colorMap[index];
}

void ColorScale::copyColorMapToArray(std::vector<std::vector<float> >& mapArray) const
{
    for (int i = 0; i < (int) mapArray.size(); ++i) {
        mapArray[i].clear();
    }
    mapArray.clear();

    mapArray.resize(ColorMapSize);
    for (int i = 0; i < ColorMapSize; ++i) {
        mapArray[i].resize(3);
        mapArray[i][0] = colorMap[i].redF();
        mapArray[i][1] = colorMap[i].greenF();
        mapArray[i][2] = colorMap[i].blueF();
    }
}

void ColorScale::drawColorScale(QPainter& painter, const QRect& r) const
{
    int yTop = r.top();
    int yBottom = r.bottom();
    int xLeft = r.left();
    int xRight = r.right();

    for (int y = yTop; y <= yBottom; ++y) {
        int index = qRound((double) ((ColorMapSize - 1) * (yBottom - y)) / (double) (yBottom - yTop));
        painter.setPen(colorMap[index]);
        painter.drawLine(QLine(xLeft, y, xRight, y));
    }
}

void ColorScale::calculateColorMap()
{
    int c1 = qRound((double)ColorMapSize * 0.20);
    int c2 = qRound((double)ColorMapSize * 0.75);
    double iNorm, hue, value, saturation;

    // Dark violet fading to bright blue
    for (int i = 0; i < c1; ++i) {
        iNorm = (double)i / (double)c1;
        hue = 290.0 - 40.0 * iNorm;
        saturation = 255.0;
        value = 255.0 * (0.7 * iNorm + 0.3);
        colorMap[i] = QColor::fromHsv(hue, saturation, value);
    }
    // Hue sweeping from blue through green, yellow, orange, to red
    for (int i = c1; i < c2; ++i) {
        iNorm = (double)(i - c1) / (double)(c2 - c1 + 1);
        hue = 250.0 - 255.0 * iNorm;
        saturation = 255.0;
        value = 255.0;
        if (hue < 0.0) hue += 360.0;
        colorMap[i] = QColor::fromHsv(hue, saturation, value);
    }
    // Red fading into white
    for (int i = c2; i < ColorMapSize; ++i) {
        iNorm = (double)(i - c2) / (double)(ColorMapSize - c2 - 1);
        saturation = 255.0 * (1.0 - iNorm);
        value = 255.0;
        hue = 355.0;
        colorMap[i] = QColor::fromHsv(hue, saturation, value);
    }
}

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

#ifndef PLOTUTILITIES_H
#define PLOTUTILITIES_H

#include <QtWidgets>
#include <vector>

class CoordinateTranslator
{
public:
    CoordinateTranslator();
    CoordinateTranslator(QRect frame__, double xMinReal__, double xMaxReal__, double yMinReal__, double yMaxReal__);
    void set(QRect frame__, double xMinReal__, double xMaxReal__, double yMinReal__, double yMaxReal__);

    QRect frame() const { return frame_; }
    QRect borderRect() const { return frame_.adjusted(0, 0, -1, -1); }
    QRect clippingRect() const { return frame_.adjusted(1, 1, -1, -1); }
    int xLeft() const { return frame_.left(); }
    int xRight() const { return frame_.right(); }
    int yTop() const { return frame_.top(); }
    int yBottom() const { return frame_.bottom(); }
    double xMinReal() const { return xMinReal_; }
    double xMaxReal() const { return xMaxReal_; }
    double yMinReal() const { return yMinReal_; }
    double yMaxReal() const { return yMaxReal_; }

    int screenXFromRealX(double x) const;
    int screenYFromRealY(double y) const;
    double realXFromScreenX(int x) const;
    double realYFromScreenY(int y) const;

private:
    QRect frame_;
    double xMinReal_;
    double xMaxReal_;
    double yMinReal_;
    double yMaxReal_;
    double xScale;
    double yScale;
};

class CoordinateTranslator1D
{
public:
    CoordinateTranslator1D();
    CoordinateTranslator1D(int minScreen__, int maxScreen__, double minReal__, double maxReal__);
    void set(int minScreen__, int maxScreen__, double minReal__, double maxReal__);

    int minScreen() const { return minScreen_; }
    int maxScreen() const { return maxScreen_; }
    double minReal() const { return minReal_; }
    double maxReal() const { return maxReal_; }

    int screenFromReal(double real) const;
    double realFromScreen(int screen) const;

private:
    int minScreen_;
    int maxScreen_;
    double minReal_;
    double maxReal_;
    double scale;
};

struct MinMaxPair
{
    double min;
    double max;
};

class PlotDecorator
{
public:
    PlotDecorator(QPainter &painter_);

    void drawVerticalAxisLine(CoordinateTranslator& ct, double xReal, int shrink = 0) const;
    void drawHorizontalAxisLine(CoordinateTranslator& ct, double yReal, int shrink = 0) const;

    void writeLabel(const QString& text, int x, int y, int flags) const;
    void writeLabel(int number, int x, int y, int flags) const;

    void writeYAxisLabel(const QString& text, CoordinateTranslator& ct, int xOffset) const;

    void drawTickMarkLeft(CoordinateTranslator& ct, double yReal, int length) const;
    void drawTickMarkRight(CoordinateTranslator& ct, double yReal, int length) const;
    void drawTickMarkBottom(CoordinateTranslator& ct, double xReal, int length) const;
    void drawTickMarkTop(CoordinateTranslator& ct, double xReal, int length) const;

    void drawLabeledTickMarkLeft(const QString& text, CoordinateTranslator& ct, double yReal, int length) const;
    void drawLabeledTickMarkLeft(int number, CoordinateTranslator& ct, double yReal, int length) const;
    void drawLabeledTickMarkRight(const QString& text, CoordinateTranslator& ct, double yReal, int length) const;
    void drawLabeledTickMarkRight(int number, CoordinateTranslator& ct, double yReal, int length) const;
    void drawLabeledTickMarkBottom(const QString& text, CoordinateTranslator& ct, double xReal, int length, bool rightEnd = false) const;
    void drawLabeledTickMarkBottom(int number, CoordinateTranslator& ct, double xReal, int length, bool rightEnd = false) const;
    void drawLabeledTickMarkTop(const QString& text, CoordinateTranslator& ct, double xReal, int length, bool rightEnd = false) const;
    void drawLabeledTickMarkTop(int number, CoordinateTranslator& ct, double xReal, int length, bool rightEnd = false) const;

    double autoCalculateYAxis(double maxY, std::vector<double>& yAxisTicks, std::vector<QString>& yAxisLabels) const;
    MinMaxPair autoCalculateLogYAxis(double minNonZeroY, double maxY, std::vector<double>& yAxisTicks,
                                     std::vector<QString>& yAxisLabels) const;

private:
    QPainter& painter;
};

class ColorScale
{
public:
    ColorScale(float minValue_ = 0.0, float maxValue_ = 1.0);
    void setRange(float minValue_, float maxValue_);
    QColor getColor(double value) const;
    void drawColorScale(QPainter& painter, const QRect& r) const;
    void copyColorMapToArray(std::vector<std::vector<float> >& mapArray) const;

private:
    float minValue;
    float maxValue;
    float valueRange;
    std::vector<QColor> colorMap;
    static const int ColorMapSize = 256;

    void calculateColorMap();
};

#endif // PLOTUTILITIES_H

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

#ifndef FILTERPLOT_H
#define FILTERPLOT_H

#include <QtWidgets>
#include "systemstate.h"


class FilterPlot : public QWidget
{
    Q_OBJECT
public:
    explicit FilterPlot(SystemState* state_, QWidget *parent = nullptr);

    // View functions
    void toggleLogarithmic(bool log); // Toggle whether or not the composite filter should be drawn.

signals:
    // The mouse moved within the plot, at the given frequency which corresponds to the given normalizedGain
    void mouseMovedInPlot(double frequency, double wideNormalizedGain, double lowNormalizedGain, double highNormalizedGain);

    // The mouse left the plot.
    void mouseLeftPlot();

protected:
    void paintEvent(QPaintEvent *event) override; // Initialize display for every paint event.
    void resizeEvent(QResizeEvent *event) override; // When FilterPlot is resized, recalculate what needs to be painted and repaint.
    void mouseMoveEvent(QMouseEvent *event) override; // Handle a movement of the mouse.
    void leaveEvent(QEvent *event) override; // Track when mouse leaves FilterPlot.

private slots:
    void updateFromState();

private:
    // Widget sizing
    QSize minimumSizeHint() const override; // Minimum size of the plot
    QSize sizeHint() const override; // Default suggested size of the plot

    // Drawing
    void drawBackground(QPainter &painter); // Clear the plot, draw a small border, and fill in the plot's background.
    void drawPlottingRect(QPainter &painter); // Draw the white rectangle which contains the actual plot of the transfer function of gain vs. frequency.
    void labelAxis(QPainter &painter); // Label frequency axis.
    void drawHashmarks(QPainter &painter); // Draw hashmarks along the frequency axis.
    void logLines(double start, double stop, double increment, QPainter &painter); // Draw vertical gray lines on a logarithmic scale.
    void line(double frequency, QPainter &painter); // Draw a vertical gray line (for linear scale).
    void hashmark(double frequency, QPainter &painter); // Draw a single hashmark at the given frequency.
    void drawWideFilter(QPainter &painter); // Draw a function representing the wide-pass filter.
    void drawLowFilter(QPainter &painter); // Draw a function representing the low-pass filter.
    void drawHighFilter(QPainter &painter); // Draw a function representing the high-pass filter.
    void drawWideBead(QPainter &painter);
    void drawLowBead(QPainter &painter);
    void drawHighBead(QPainter &painter);
    void checkForNotchMinimum(bool logarithmic, double thisF, int previousY, int &ypos, bool &minReached);
    void drawLegend(QPainter &painter); // Draw a legend distinguishing hardware and composite functions.

    // Filtering
    // For a given input of 'omega', return the output of a 'filterOrder'-order Bessel filter.
    double bessel(double omega, int filterOrder);

    // For a given input of 'omega', return the output of a 'filterOrder'-order Butterworth filter.
    double butterworth(double omega, int filterOrder);

    // For a given input of 'frequency', return the output of a 'notchFreq'-frequency notch filter.
    double notch(double frequency, double notchFreq);

    double getWideAttenuation(double frequency);
    double getLowAttenuation(double frequency);
    double getHighAttenuation(double frequency);

    // Numerical utilities
    void updateLinearMaxFreq();
    double roundToAcceptableMidpoint(double number); // Round the given number to the nearest acceptable midpoint (easily divisible by 4 for reasonable scaling in linear view).
    double frequencyToRatio(double frequency); // Convert the given frequency to a ratio between 0 and 1 representing its position on the frequency axis.
    double ratioToFrequency(double ratio); // Convert the given ratio between 0 and 1 representing a posiion on the frequency axis to its frequency.

    QPixmap pixmap;
    QPoint visibleTopLeft;
    QPoint actualTopLeft;
    QPoint bottomRight;
    int domain;
    int range;

    SystemState* state;

    QColor wideColor;
    QColor lowColor;
    QColor highColor;

    bool beadsEnabled;
    double currentFrequency;
    double lowNormalizedGain;
    double wideNormalizedGain;
    double highNormalizedGain;

    bool logarithmic;
    double highestCutoff;
    double linearMaxFreq;

    static const int FilterPlotXSize = 800;
    static const int FilterPlotYSize = 200;
};

#endif // FILTERPLOT_H

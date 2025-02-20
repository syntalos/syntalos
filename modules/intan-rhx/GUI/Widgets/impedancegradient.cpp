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

#include "impedancegradient.h"

ImpedanceGradient::ImpedanceGradient(QWidget *parent) :
    QWidget(parent)
{
    setFixedSize(GRADIENTWIDTH + 2*BUFFERWIDTH, GRADIENTHEIGHT + 2*BUFFERHEIGHT);
    qreal dpr = devicePixelRatioF();
    pixmap = QPixmap(width() * dpr, height() * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
}

// Set up a QLinearGradient representing the portion of the rainbow from purple to red that we want to represent impedances from 10K to 10M.
QLinearGradient ImpedanceGradient::setupGradient() {
    QLinearGradient rainbow(QPointF(BUFFERWIDTH, 0), QPointF(BUFFERWIDTH + GRADIENTWIDTH, 0));
    for (int i = 0; i <= 100; ++i) {
        double fractionalI = ((double) i / 100.0);
        rainbow.setColorAt(1.0 - fractionalI, QColor::fromHsv(fractionalI * 270, 255, 255));
    }
    return rainbow;
}

// Take a scaled magnitude number (from 0 to 1, 0 representing 10K and 1 representing 10M) and returns the corresponding color according to our rainbow.
QColor ImpedanceGradient::getColor(float magnitudeScaled) {
    return QColor::fromHsv((1 - magnitudeScaled) * 270, 255, 255);
}

// Initialize display for every paint event.
void ImpedanceGradient::paintEvent(QPaintEvent * /* event */)
{
    QPainter painter;
    painter.begin(&pixmap);

    // Draw entire widget rectangle as white, with a black border.
    painter.setBrush(Qt::white);
    painter.drawRect(QRect(QPoint(0, 0), QSize(width() - 1, height() - 1)));

    // Draw the hash mark lines.
    for (int hashMark = 0; hashMark < 4; hashMark++) {
        painter.drawLine(QPoint(BUFFERWIDTH + hashMark * (GRADIENTWIDTH/3), BUFFERHEIGHT - HASHMARKLENGTH), QPoint(BUFFERWIDTH + hashMark * (GRADIENTWIDTH/3), BUFFERHEIGHT));
    }

    // Draw the hash mark labels.
    QFontMetrics metrics(painter.font());

    QString labelText10k = "10 k" + OmegaSymbol;
    painter.drawText(QPoint(BUFFERWIDTH - metrics.horizontalAdvance(labelText10k)/2, BUFFERHEIGHT - 1.5 * HASHMARKLENGTH), labelText10k);

    QString labelText100k = "100 k" + OmegaSymbol;
    painter.drawText(QPoint(BUFFERWIDTH + (GRADIENTWIDTH/3) - metrics.horizontalAdvance(labelText100k)/2, BUFFERHEIGHT - 1.5 * HASHMARKLENGTH), labelText100k);

    QString labelText1M = "1 M" + OmegaSymbol;
    painter.drawText(QPoint(BUFFERWIDTH + (2*GRADIENTWIDTH/3) - metrics.horizontalAdvance(labelText1M)/2, BUFFERHEIGHT - 1.5 * HASHMARKLENGTH), labelText1M);

    QString labelText10M = "10 M" + OmegaSymbol;
    painter.drawText(QPoint(BUFFERWIDTH + (GRADIENTWIDTH) - metrics.horizontalAdvance(labelText10M)/2, BUFFERHEIGHT - 1.5 * HASHMARKLENGTH), labelText10M);

    // Draw the 'Impedance Gradient Legend' label.
    QString legEndOfLineabel(tr("Electrode Impedance"));
    painter.drawText(QPoint(BUFFERWIDTH + (GRADIENTWIDTH/2) - metrics.horizontalAdvance(legEndOfLineabel)/2, GRADIENTHEIGHT + 1.5 * BUFFERHEIGHT + metrics.height()/2), legEndOfLineabel);

    // Draw the rainbow.
    QLinearGradient rainbow = setupGradient();

    QBrush brush(rainbow);
    painter.setBrush(brush);
    painter.drawRect(QRect(QPoint(BUFFERWIDTH, BUFFERHEIGHT), QSize(GRADIENTWIDTH, GRADIENTHEIGHT)));

    QPainter sPainter(this);
    sPainter.drawPixmap(0, 0, pixmap);
}

// Perform any clean-up here before application closes.
void ImpedanceGradient::closeEvent(QCloseEvent *event)
{
    // Perform any clean-up here before application closes.
    event->accept();
}

// Minimum size of the widget
QSize ImpedanceGradient::minimumSizeHint() const
{
    return QSize(GRADIENTWIDTH + 2*BUFFERWIDTH, GRADIENTHEIGHT + 2*BUFFERHEIGHT);
}

// Default suggested size of the widget
QSize ImpedanceGradient::sizeHint() const
{
    return QSize(GRADIENTWIDTH + 2*BUFFERWIDTH, GRADIENTHEIGHT + 2*BUFFERHEIGHT);
}

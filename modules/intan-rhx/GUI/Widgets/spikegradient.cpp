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

#include "spikegradient.h"
#include "systemstate.h"

SpikeGradient::SpikeGradient(SystemState* state_, QWidget *parent) :
    QWidget(parent),
    state(state_)
{
    setFixedSize(GRADIENTWIDTH + 2*BUFFERWIDTH, GRADIENTHEIGHT + 2*BUFFERHEIGHT);
    qreal dpr = devicePixelRatioF();
    pixmap = QPixmap(width() * dpr, height() * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
}

QLinearGradient SpikeGradient::setupGradient()
{
    QLinearGradient gradient(QPointF(BUFFERWIDTH, 0), QPointF(BUFFERWIDTH + GRADIENTWIDTH, 0));
    for (int i = 0; i <= 100; ++i) {
        double fractionalI = ((double) i / 100.0);
        gradient.setColorAt(1.0 - fractionalI, QColor(fractionalI * 255.0, fractionalI * 255.0, fractionalI * 255.0));
    }
    return gradient;
}

QColor SpikeGradient::getColor(float seconds, float decayTime)
{
    if (seconds <= 0) {
        return QColor(255, 255, 255);
    }

    if (seconds >= decayTime) {
        return QColor(0, 0, 0);
    }

    int brightness = round(255.0 * (1 - seconds / decayTime));

    return QColor(brightness, brightness, brightness);
}

// Initialize display for every paint event.
void SpikeGradient::paintEvent(QPaintEvent * /* event */)
{
    QPainter painter;
    painter.begin(&pixmap);

    // Draw entire widget rectangle as white, with a black border.
    painter.setBrush(Qt::white);
    painter.drawRect(QRect(QPoint(0, 0), QSize(width() - 1, height() - 1)));

    // Draw the hash mark lines.
    for (int hashMark = 0; hashMark < 5; hashMark++) {
        painter.drawLine(QPoint(BUFFERWIDTH + hashMark * (GRADIENTWIDTH/4), BUFFERHEIGHT - HASHMARKLENGTH), QPoint(BUFFERWIDTH + hashMark * (GRADIENTWIDTH/4), BUFFERHEIGHT));
    }

    // Draw the hash mark labels.
    QFontMetrics metrics(painter.font());

    QString labelText0 = "0 s";
    painter.drawText(QPoint(BUFFERWIDTH - metrics.horizontalAdvance(labelText0)/2, BUFFERHEIGHT - 1.5 * HASHMARKLENGTH), labelText0);

    QString labelText1 = QString::number(state->getDecayTime() / 4.0) + " s";
    painter.drawText(QPoint(BUFFERWIDTH + (GRADIENTWIDTH/4) - metrics.horizontalAdvance(labelText1)/2, BUFFERHEIGHT - 1.5 * HASHMARKLENGTH), labelText1);

    QString labelText2 = QString::number(state->getDecayTime() / 2.0) + " s";
    painter.drawText(QPoint(BUFFERWIDTH + (GRADIENTWIDTH/2) - metrics.horizontalAdvance(labelText2)/2, BUFFERHEIGHT - 1.5 * HASHMARKLENGTH), labelText2);

    QString labelText3 = QString::number(3.0 * state->getDecayTime() / 4.0) + " s";
    painter.drawText(QPoint(BUFFERWIDTH + (3 * GRADIENTWIDTH/4) - metrics.horizontalAdvance(labelText3)/2, BUFFERHEIGHT - 1.5 * HASHMARKLENGTH), labelText3);

    QString labelText4 = QString::number(state->getDecayTime()) + " s";
    painter.drawText(QPoint(BUFFERWIDTH + GRADIENTWIDTH - metrics.horizontalAdvance(labelText4)/2, BUFFERHEIGHT - 1.5 * HASHMARKLENGTH), labelText4);

    // Draw the 'Spike Gradient Legend' label.
    QString legEndOfLineabel(tr("Time Since Last Spike"));
    painter.drawText(QPoint(BUFFERWIDTH + (GRADIENTWIDTH/2) - metrics.horizontalAdvance(legEndOfLineabel)/2, GRADIENTHEIGHT + 1.5 * BUFFERHEIGHT + metrics.height()/2), legEndOfLineabel);

    // Draw the gradient.
    QLinearGradient gradient = setupGradient();

    QBrush brush(gradient);
    painter.setBrush(brush);
    painter.drawRect(QRect(QPoint(BUFFERWIDTH, BUFFERHEIGHT), QSize(GRADIENTWIDTH, GRADIENTHEIGHT)));

    QPainter sPainter(this);
    sPainter.drawPixmap(0, 0, pixmap);
}

void SpikeGradient::closeEvent(QCloseEvent *event)
{
    // Perform any clean-up here before application closes.
    event->accept();
}

QSize SpikeGradient::minimumSizeHint() const
{
    return QSize(GRADIENTWIDTH + 2*BUFFERWIDTH, GRADIENTHEIGHT + 2*BUFFERHEIGHT);
}

QSize SpikeGradient::sizeHint() const
{
    return QSize(GRADIENTWIDTH + 2*BUFFERWIDTH, GRADIENTHEIGHT + 2*BUFFERHEIGHT);
}

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

#include <QtWidgets>
#include <qmath.h>
#include <iostream>
#include "stimparameters.h"
#include "abstractfigure.h"
#include "digfigure.h"

DigFigure::DigFigure(StimParameters *stimParameters, QWidget* parent) :
    AbstractFigure()
{
    generalSetup(stimParameters, parent);

    // Establish that the amplitude vertical arrow should not be displayed.
    // (With a digital pulse, user has no control over amplitude.)
    noAmplitude = true;

    localStimPolarity = PositiveFirst;
    localPulseOrTrain = (PulseOrTrain) parameters->pulseOrTrain->getIndex();
    localStimShape = Monophasic;
}

void DigFigure::uniqueRedraw(QPainter &painter)
{
    y0 = stimFrame.bottom();
    painter.drawLine(x0, y0, xStimBegin, y0);

    // Draw digital pulse.
    int xStimEnd = x0 + 0.3 * xLength;
    int yMaxAmplitude = 0.5 * stimFrame.height();
    int xStimBegin2 = 0;
    int xStimEnd2 = 0;
    drawHorizontalArrow(painter, x0, xStimBegin, y0 - yMaxAmplitude - ArrowSize - 3,
                        highlitPostTriggerDelay ? durationLabelColorHL : durationLabelColor, "DELAY", true);

    drawStimPulse(painter, xStimBegin, xStimEnd, y0, yMaxAmplitude);

    if (localPulseOrTrain == SinglePulse) {
        painter.drawLine(xStimEnd, y0, xEnd, y0);
    } else {
        painter.drawLine(xStimEnd, y0, x0 + 0.35 * xLength, y0);
        drawDashedHorizontalLine(painter, x0 + 0.35 * xLength, x0 + 0.45 * xLength, y0, (highlitStimTrace ? currentColorHL : currentColor), 6);
        xStimBegin2 = x0 + 0.5 * xLength;
        xStimEnd2 = x0 + 0.7 * xLength;
        painter.drawLine(x0 + 0.45 * xLength, y0, xStimBegin2, y0);
        drawStimPulse(painter, xStimBegin2, xStimEnd2, y0, yMaxAmplitude);
        painter.drawLine(xStimEnd2, y0, xEnd, y0);
        drawHorizontalArrow(painter, xStimBegin, xStimBegin2, y0 + ArrowSize + 3,
                            highlitPulseTrainPeriod ? durationLabelColorHL : durationLabelColor, "PULSE PERIOD", false);
    }

    int xStimEndFinal = (localPulseOrTrain == SinglePulse) ? xStimEnd : xStimEnd2;
    drawHorizontalArrow(painter, xStimEndFinal, xEnd, y0 - yMaxAmplitude - ArrowSize - 3,
                        highlitRefractoryPeriod ? durationLabelColorHL : durationLabelColor, "REFRACTORY", true);

    // Draw trigger marker.
    painter.setPen(Qt::darkBlue);
    painter.drawText(stimFrame.left(), fontMetrics().height() + 1, triggerLabel);
    painter.drawLine(x0, stimFrame.top(), x0, frame.bottom() - 5);

    // Draw end marker.
    painter.drawText(stimFrame.right() - EndOfLineabelWidth, fontMetrics().height() + 1, EndOfLineabel);
    painter.drawLine(xEnd, stimFrame.top(), xEnd, frame.bottom() - 5);
}

QSize DigFigure::minimumSizeHint() const
{
    return QSize(figureXSize, figureYSize/2);
}

QSize DigFigure::sizeHint() const
{
    return QSize(figureXSize, figureYSize/2);
}

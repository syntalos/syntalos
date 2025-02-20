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
#include "stimfigure.h"

StimFigure::StimFigure(StimParameters *stimParameters, QWidget *parent) :
    AbstractFigure()
{
    generalSetup(stimParameters, parent);

    localStimShape = (StimShape) parameters->stimShape->getIndex();
    localStimPolarity = (StimPolarity) parameters->stimPolarity->getIndex();
    localPulseOrTrain = (PulseOrTrain) parameters->pulseOrTrain->getIndex();
    localEnableAmpSettle = parameters->enableAmpSettle->getValue();
    localMaintainAmpSettle = parameters->maintainAmpSettle->getValue();
    localEnableChargeRecovery = parameters->enableChargeRecovery->getValue();
}

void StimFigure::uniqueRedraw(QPainter &painter)
{
    y0 = stimFrame.center().y();
    painter.drawLine(x0, y0, xStimBegin, y0);

    painter.drawText(x0 + 3, y0 + fontMetrics().height(), "STIM CURRENT");

    // Draw stimulation pulse.
    int xStimEnd = x0 + 0.3 * xLength;
    int yMaxAmplitude = 0.3 * stimFrame.height();
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
        drawHorizontalArrow(painter, xStimBegin, xStimBegin2, y0 + yMaxAmplitude + ArrowSize + 3,
                            highlitPulseTrainPeriod ? durationLabelColorHL : durationLabelColor, "PULSE PERIOD", false);
    }
    int xStimEndFinal = (localPulseOrTrain == SinglePulse) ? xStimEnd : xStimEnd2;
    drawHorizontalArrow(painter, xStimEndFinal, xEnd, y0 - yMaxAmplitude - ArrowSize - 3,
                        highlitRefractoryPeriod ? durationLabelColorHL : durationLabelColor, "REFRACTORY", true);

    // Draw amplifier settle signal.
    int yAmpSettle = frame.top() + 0.8 * frame.height();
    int barThickness = 5;

    painter.setPen(highlitStimTrace ? ampSettleColorHL : ampSettleColor);
    painter.drawLine(x0, yAmpSettle, xEnd, yAmpSettle);
    painter.drawText(x0 + 3, yAmpSettle + barThickness + fontMetrics().height(), "AMPLIFIER SETTLE");

    QString ampSettlePre = "pre";
    QString ampSettlePost = "post";

    int xAmpSettleBegin = (x0 + xStimBegin) / 2;
    int xAmpSettleEnd;
    if (localEnableAmpSettle) {
        if (localPulseOrTrain == SinglePulse || !(localMaintainAmpSettle)) {
            xAmpSettleEnd = xStimEnd + 0.05 * xLength;
            QRect rect1(xAmpSettleBegin, yAmpSettle - barThickness, xAmpSettleEnd - xAmpSettleBegin, 2 * barThickness);
            painter.fillRect(rect1, (highlitStimTrace ? ampSettleColorHL : ampSettleColor));
            drawHorizontalArrow(painter, xAmpSettleBegin, xStimBegin, yAmpSettle - barThickness - ArrowSize - 3,
                                highlitPreStimAmpSettle ? durationLabelColorHL : durationLabelColor, ampSettlePre, true);
            drawHorizontalArrow(painter, xStimEnd, xAmpSettleEnd, yAmpSettle - barThickness - ArrowSize - 3,
                                highlitPostStimAmpSettle ? durationLabelColorHL : durationLabelColor, ampSettlePost, true);
        }
        if (localPulseOrTrain == PulseTrain){
            if (localMaintainAmpSettle) {
                xAmpSettleEnd = xStimEnd2 + 0.05 * xLength;
                QRect rect1(xAmpSettleBegin, yAmpSettle - barThickness, xAmpSettleEnd - xAmpSettleBegin, 2 * barThickness);
                painter.fillRect(rect1, (highlitStimTrace ? ampSettleColorHL : ampSettleColor));
                drawHorizontalArrow(painter, xAmpSettleBegin, xStimBegin, yAmpSettle - barThickness - ArrowSize - 3,
                                    highlitPreStimAmpSettle ? durationLabelColorHL : durationLabelColor, ampSettlePre, true);
                drawHorizontalArrow(painter, xStimEnd2, xAmpSettleEnd, yAmpSettle - barThickness - ArrowSize - 3,
                                    highlitPostStimAmpSettle ? durationLabelColorHL : durationLabelColor, ampSettlePost, true);
            } else {
                xAmpSettleBegin = xStimBegin2 - 0.05 * xLength;
                xAmpSettleEnd = xStimEnd2 + 0.05 * xLength;
                QRect rect1(xAmpSettleBegin, yAmpSettle - barThickness, xAmpSettleEnd - xAmpSettleBegin, 2 * barThickness);
                painter.fillRect(rect1, (highlitStimTrace ? ampSettleColorHL : ampSettleColor));
                drawHorizontalArrow(painter, xAmpSettleBegin, xStimBegin2, yAmpSettle - barThickness - ArrowSize - 3,
                                    highlitPreStimAmpSettle ? durationLabelColorHL : durationLabelColor, ampSettlePre, true);
                drawHorizontalArrow(painter, xStimEnd2, xAmpSettleEnd, yAmpSettle - barThickness - ArrowSize - 3,
                                    highlitPostStimAmpSettle ? durationLabelColorHL : durationLabelColor, ampSettlePost, true);
            }
        }
    }

    // Draw charge recovery signal.
    int yChargeRecov = frame.top() + 0.9 * frame.height();

    painter.setPen(highlitStimTrace ? chargeRecovColorHL : chargeRecovColor);
    painter.drawLine(x0, yChargeRecov, xEnd, yChargeRecov);
    painter.drawText(x0 + 3, yChargeRecov + fontMetrics().height(), "CHARGE RECOVERY");

    int xChargeRecovBegin = xStimEndFinal + 0.15 * xLength;
    int xChargeRecovWidth = 0.1 * xLength;
    if (localEnableChargeRecovery) {
        QRect rect1(xChargeRecovBegin, yChargeRecov - barThickness, xChargeRecovWidth, 2 * barThickness);
        painter.fillRect(rect1, (highlitStimTrace ? chargeRecovColorHL : chargeRecovColor));
        drawHorizontalArrow(painter, xStimEndFinal, xChargeRecovBegin,
                            yChargeRecov - barThickness - ArrowSize - 3,
                            highlitPostStimChargeRecovOn ? durationLabelColorHL : durationLabelColor, "start", true);
        drawHorizontalArrow(painter, xStimEndFinal, xChargeRecovBegin + xChargeRecovWidth,
                            yChargeRecov + barThickness + ArrowSize + 3,
                            highlitPostStimChargeRecovOff ? durationLabelColorHL : durationLabelColor, "stop", false);
    }

    // Draw trigger marker.
    painter.setPen(Qt::darkBlue);
    painter.drawText(stimFrame.left(), fontMetrics().height() + 1, triggerLabel);
    painter.drawLine(x0, stimFrame.top(), x0, frame.bottom() - 5);

    // Draw end marker.
    painter.drawText(stimFrame.right() - EndOfLineabelWidth, fontMetrics().height() + 1, EndOfLineabel);
    painter.drawLine(xEnd, stimFrame.top(), xEnd, frame.bottom() - 5);
}

QSize StimFigure::minimumSizeHint() const
{
    return QSize(figureXSize, figureYSize);
}

QSize StimFigure::sizeHint() const
{
    return QSize(figureXSize, figureYSize);
}

void StimFigure::updateEnableAmpSettle(bool enabled)
{
    localEnableAmpSettle = enabled;
    generalRedraw();
}

void StimFigure::updateMaintainAmpSettle(bool maintain)
{
    localMaintainAmpSettle = maintain;
    generalRedraw();
}

void StimFigure::updateEnableChargeRecovery(bool enabled)
{
    localEnableChargeRecovery = enabled;
    generalRedraw();
}

void StimFigure::highlightSecondPhaseDuration(bool highlight)
{
    highlitSecondPhaseDuration = highlight;
    generalRedraw();
}

void StimFigure::highlightInterphaseDelay(bool highlight)
{
    highlitInterphaseDelay = highlight;
    generalRedraw();
}

void StimFigure::highlightFirstPhaseAmplitude(bool highlight)
{
    highlitFirstPhaseAmplitude = highlight;
    generalRedraw();
}

void StimFigure::highlightSecondPhaseAmplitude(bool highlight)
{
    highlitSecondPhaseAmplitude = highlight;
    generalRedraw();
}

void StimFigure::highlightPreStimAmpSettle(bool highlight)
{
    highlitPreStimAmpSettle = highlight;
    generalRedraw();
}

void StimFigure::highlightPostStimAmpSettle(bool highlight)
{
    highlitPostStimAmpSettle = highlight;
    generalRedraw();
}

void StimFigure::highlightPostStimChargeRecovOn(bool highlight)
{
    highlitPostStimChargeRecovOn = highlight;
    generalRedraw();
}

void StimFigure::highlightPostStimChargeRecovOff(bool highlight)
{
    highlitPostStimChargeRecovOff = highlight;
    generalRedraw();
}

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

#include <limits>
#include "controllerinterface.h"
#include "waveformfifo.h"
#include "waveformselectdialog.h"
#include "waveformdisplaycolumn.h"
#include "multiwaveformplot.h"

MultiWaveformPlot::MultiWaveformPlot(int columnIndex_, WaveformDisplayManager* waveformManager_,
                                     ControllerInterface *controllerInterface_, SystemState *state_,
                                     WaveformDisplayColumn* parent_) :
    QWidget(parent_),
    columnIndex(columnIndex_),
    waveformManager(waveformManager_),
    controllerInterface(controllerInterface_),
    state(state_),
    parent(parent_)
{
    setBackgroundRole(QPalette::Window);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(MINIMUM_X_SIZE_MULTIWAVEFORM_PLOT, MINIMUM_Y_SIZE_MULTIWAVEFORM_PLOT);
    setMouseTracking(true);  // If we don't do this, mouseMoveEvent() is active only when mouse button is pressed.
    if (!state->testMode->getValue()) {
        setFocusPolicy(Qt::StrongFocus);
        QApplication::focusWidget();
    }

    waveformSelectDialog = nullptr;

    showPinned = true;

    hoverWaveIndex.index = -1;
    hoverWaveIndex.inPinned = false;

    dragState.dragging = false;
    dragState.fromPinned = false;

    mouseButtonDown = false;
    outOfBoundsClick = false;

    numFiltersDisplayed = 0;
    arrangeByFilter = false;

    listManager = new DisplayListManager(displayList, pinnedList, state);

    int fontSize = 8;
    if (state->highDPIScaleFactor > 1) fontSize = 12;
    labelFont = new QFont("Courier New", fontSize);
    labelFontMetrics = new QFontMetrics(*labelFont);
    labelHeight = labelFontMetrics->ascent() + 1;

    labelWidthIndexOld = -1;
    int numLabelWidths = state->labelWidth->numberOfItems();

    labelWidth.resize(numLabelWidths);
    labelWidthFilter.resize(numLabelWidths);

    labelWidth[0] = 9;
    labelWidthFilter[0] = 0;
    labelWidth[1] = labelFontMetrics->horizontalAdvance("XA-008") + 12;
    labelWidthFilter[1] = labelFontMetrics->horizontalAdvance("W");
    labelWidth[2] = labelFontMetrics->horizontalAdvance("XXXXDIGITAL-OUT-08") + 10;
    labelWidthFilter[2] = labelFontMetrics->horizontalAdvance("WIDE");

    // TEMP:
//    cout << "labelHeight = " << labelHeight << EndOfLine;
//    cout << "labelWidth[1] = " << labelWidth[1] << EndOfLine;
//    cout << "labelWidth[2] = " << labelWidth[2] << EndOfLine;
//    cout << "labelWidthFilter[1] = " << labelWidthFilter[1] << EndOfLine;
//    cout << "labelWidthFilter[2] = " << labelWidthFilter[2] << EndOfLine;

    regionWaveforms.resize(numLabelWidths);
    regionLabels.resize(numLabelWidths);
    regionTimeAxis.resize(numLabelWidths);

    calculateScreenRegions();
    updateFromState();

    scrollBar = new ScrollBar(regionScrollBar, parent->numPorts(), this);

    saveBadge = QImage(":/images/save_badge.png");
    stimBadge = QImage(":/images/stim_enabled_badge.png");
    tcpBadge = QImage(":/images/tcp_badge.png");
    saveSelectedBadge = QImage(":/images/save_badge_selected.png");
    stimSelectedBadge = QImage(":/images/stim_enabled_badge_selected.png");
    tcpSelectedBadge = QImage(":/images/tcp_badge_selected.png");
    unpinBadge = QImage(":/images/unpin_badge_angle.png");
    image = QImage(size(), QImage::Format_ARGB32_Premultiplied);
    corePixmap = QPixmap(size());
    fullPixmap = QPixmap(size());

//    contextMenu = nullptr;
//    enableAction = new QAction(tr("Toggle enable"), this);
//    connect(enableAction, SIGNAL(triggered()), this, SLOT(enableSlot()));

    connect(state, SIGNAL(stateChanged()), this, SLOT(updateFromState()));
}

MultiWaveformPlot::~MultiWaveformPlot()
{
    disconnect(state, nullptr, this, nullptr);
    disconnect(this, nullptr, nullptr, nullptr);
    delete listManager;
    delete scrollBar;
    delete labelFontMetrics;
    delete labelFont;
}

void MultiWaveformPlot::calculateScreenRegions()
{
    int xSize = width();
    int ySize = height();

    int xScrollBarLeft = xSize - 18;
    int yWaveformTop = labelHeight + 10;
    int yWaveformBottom = ySize - 8;

    for (int i = 0; i < state->labelWidth->numberOfItems(); ++i) {
        regionWaveforms[i] = QRect(labelWidth[i], yWaveformTop, xScrollBarLeft - labelWidth[i] - 1,
                                   yWaveformBottom - yWaveformTop);
        regionLabels[i] = QRect(0, yWaveformTop, labelWidth[i], yWaveformBottom - yWaveformTop);
        regionTimeAxis[i] = QRect(labelWidth[i], 0, xScrollBarLeft - labelWidth[i] - 1, yWaveformTop + 3);
    }
    regionAboveLabels = QRect(0, 0, xSize, yWaveformTop);
    regionBelowLabels = QRect(0, yWaveformBottom, xSize, ySize - yWaveformBottom);
    regionScrollBar = QRect(xScrollBarLeft, yWaveformTop + 4, 14, yWaveformBottom - yWaveformTop - 5);
    regionUnpinSymbols = QRect(xScrollBarLeft, yWaveformTop, 15, 1);

    parent->setWaveformWidth(regionWaveforms[state->labelWidth->getIndex()].width());
}

QSize MultiWaveformPlot::minimumSizeHint() const
{
    return QSize(MINIMUM_X_SIZE_MULTIWAVEFORM_PLOT, MINIMUM_Y_SIZE_MULTIWAVEFORM_PLOT);
}

QSize MultiWaveformPlot::sizeHint() const
{
    return QSize(MINIMUM_X_SIZE_MULTIWAVEFORM_PLOT, MINIMUM_Y_SIZE_MULTIWAVEFORM_PLOT);
}

void MultiWaveformPlot::updateFromState()
{
    int labelWidthIndex = state->labelWidth->getIndex();
    if (labelWidthIndex != labelWidthIndexOld) {
        labelWidthIndexOld = labelWidthIndex;
        parent->setWaveformWidth(regionWaveforms[labelWidthIndex].width());
    }


    QString selectedPort = parent->getSelectedPort();
    QStringList displayWaveforms = state->signalSources->getDisplayListAmplifiers(selectedPort);
    QStringList auxInputWaveforms = state->signalSources->getDisplayListAuxInputs(selectedPort);
    QStringList supplyVoltageWaveforms = state->signalSources->getDisplayListSupplyVoltages(selectedPort);
    QStringList baseWaveforms = state->signalSources->getDisplayListBaseGroup(selectedPort);

    listManager->populateDisplayList(displayWaveforms, auxInputWaveforms, supplyVoltageWaveforms, baseWaveforms);

    numFiltersDisplayed = 0;
    if (state->filterDisplay1->getValue() != "None") ++numFiltersDisplayed;
    if (state->filterDisplay2->getValue() != "None") ++numFiltersDisplayed;
    if (state->filterDisplay3->getValue() != "None") ++numFiltersDisplayed;
    if (state->filterDisplay4->getValue() != "None") ++numFiltersDisplayed;
    arrangeByFilter = (state->arrangeBy->getValue() == "Filter");

    if (state->plottingMode->getValue() == "High Efficiency") {
        // Determine if any time or voltage scales changed (which should trigger a reset of the plot)
        bool scalesChanged = false; // Consult previous and current time, wide, low, high, DC , aux, and analog scales
        current_tScale = state->tScale->getIndex();
        current_yScaleWide = state->yScaleWide->getIndex();
        current_yScaleLow = state->yScaleLow->getIndex();
        current_yScaleHigh = state->yScaleHigh->getIndex();
        current_yScaleDC = state->yScaleDC->getIndex();
        current_yScaleAux = state->yScaleAux->getIndex();
        current_yScaleAnalog = state->yScaleAnalog->getIndex();

        if (current_tScale != prev_tScale) {
            scalesChanged = true;
            prev_tScale = current_tScale;
        }
        if (current_yScaleWide != prev_yScaleWide) {
            scalesChanged = true;
            prev_yScaleWide = current_yScaleWide;
        }
        if (current_yScaleLow != prev_yScaleLow) {
            scalesChanged = true;
            prev_yScaleLow = current_yScaleLow;
        }
        if (current_yScaleHigh != prev_yScaleHigh) {
            scalesChanged = true;
            prev_yScaleHigh = current_yScaleHigh;
        }
        if (current_yScaleDC != prev_yScaleDC) {
            scalesChanged = true;
            prev_yScaleDC = current_yScaleDC;
        }
        if (current_yScaleAux != prev_yScaleAux) {
            scalesChanged = true;
            prev_yScaleAux = current_yScaleAux;
        }
        if (current_yScaleAnalog != prev_yScaleAnalog) {
            scalesChanged = true;
            prev_yScaleAnalog = current_yScaleAnalog;
        }

        if (scalesChanged) {
            requestReset();
        }

        if (state->running) {
            requestRedraw();
        }
    }

    update();
}

void MultiWaveformPlot::openWaveformSelectDialog()
{
    if (!waveformSelectDialog) {
        waveformSelectDialog = new WaveformSelectDialog(state->signalSources, this);
    }
    waveformSelectDialog->show();
    waveformSelectDialog->raise();
    waveformSelectDialog->activateWindow();
}

bool MultiWaveformPlot::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip) {  // Implement custom tool tips.
        QHelpEvent* helpEvent = (QHelpEvent*) event;

        QPoint cursor = mapFromGlobal(QCursor::pos());
        if (regionLabels[state->labelWidth->getIndex()].contains(cursor)) {
            calculateDisplayYCoords();
            hoverWaveIndex = findSelectedWaveform(cursor.y());
            if (hoverWaveIndex.index >= 0) {
                Channel* channel = listManager->displayedWaveform(hoverWaveIndex)->channel;
                if (!channel) {
                    // Escape without attempting to use channel if it's a null pointer.
                    return true;
                }
                QString toolTip = channel->getCustomName();
                if (channel->getCustomName() != channel->getNativeName()) toolTip += " (" + channel->getNativeName() + ")";
                if (channel->getSignalType() == AmplifierSignal) {
                    if (channel->isImpedanceValid()) {
                        toolTip += "\n" + tr("Z = ") + channel->getImpedanceMagnitudeString() + " " +
                                channel->getImpedancePhaseString();
                    }
                    if (channel->getReference().toLower() != "hardware") {
                        toolTip += "\n" + tr("REF: ") + channel->getReference();
                    }
                }
                if (channel->isStimEnabled()) {
                    toolTip += "\n" + tr("Stim Trigger: ") + channel->getStimTrigger();
                }
                QToolTip::showText(helpEvent->globalPos(), toolTip);
            } else {
                QToolTip::hideText();
            }
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return QWidget::event(event);
}

//void MultiWaveformPlot::contextMenuEvent(QContextMenuEvent* event)
//{
//    if (!contextMenu) contextMenu = new QMenu(this);
//    contextMenu->addAction(enableAction);
//    contextMenu->popup(event->globalPos());  // Use popup() for asynchronous execution; exec() for synchronous execution.
//}

void MultiWaveformPlot::paintEvent(QPaintEvent* /* event */)
{
    if (state->plottingMode->getValue() == "Original" || state->rollMode->getValue()) {
        // ORIGINAL
            //QElapsedTimer timer;
            //timer.start();

        QPainter painter(&image);
        bool showDisabledChannels = state->showDisabledChannels->getValue();
        bool clipWaveforms = state->clipWaveforms->getValue();

        int labelWidthIndex = state->labelWidth->getIndex();
        QRect regionWaveforms1 = regionWaveforms[labelWidthIndex];
        QRect regionLabels1 = regionLabels[labelWidthIndex];
        QRect regionTimeAxis1 = regionTimeAxis[labelWidthIndex];

        // Clear old display.
        QRect imageFrame(rect());
        const QColor BackgroundColor = QColor(state->backgroundColor->getValueString());
        painter.fillRect(imageFrame, QBrush(BackgroundColor));

        calculateDisplayYCoords();
        QPoint cursor = mapFromGlobal(QCursor::pos());
        hoverWaveIndex = findSelectedWaveform(cursor.y());
        bool cursorInWaveformArea = regionLabels1.contains(cursor) || regionWaveforms1.contains(cursor);

        // Draw vertical time lines.
        if (regionTimeAxis1.contains(cursor)) {
            painter.setClipRegion(imageFrame);
            drawVerticalTimeLines(painter, regionTimeAxis1.left(), cursor.x());
        }

        // Draw display trigger line.
        drawTriggerLine(painter, regionTimeAxis1.left());

        QRect plotClipRegion = regionWaveforms1.united(regionLabels1).united(regionScrollBar);
        painter.setClipRegion(plotClipRegion);
        int clipX = plotClipRegion.left();
        int clipWidth = plotClipRegion.width();

        int yPosition;

        // Draw y = 0 baseline.
        if (hoverWaveIndex.index >= 0 && cursorInWaveformArea && !dragState.dragging) {
            yPosition = listManager->displayedWaveform(hoverWaveIndex)->yCoord;
            painter.setPen(QColor(72, 72, 72));
            painter.drawLine(QPoint(regionWaveforms1.left(), yPosition), QPoint(regionWaveforms1.right() - 1, yPosition));
        }

        if (showPinned) {
            // Draw pinned/unpinned waveform divider.
            if (!pinnedList.isEmpty()) {
                waveformManager->drawDivider(painter, pinnedYDivider, regionLabels1.left() + 2, regionWaveforms1.right());
            }

            // Draw pinned waveforms.
            for (int i = 0; i < pinnedList.size(); ++i) {
                yPosition = pinnedList.at(i).yCoord;
                // If dragging, spread out waveforms around potential drop spot.
                if (dragState.dragging && dragState.fromPinned && hoverWaveIndex.inPinned) {
                    yPosition = spreadAroundDraggingTarget(yPosition, hoverWaveIndex.index, i);
                }
                QString waveName = pinnedList.at(i).waveName;
                bool enabled = pinnedList.at(i).isEnabled();
                if (enabled || showDisabledChannels) {
                    if (enabled) {
                        bool hoverHighlight = hoverWaveIndex.inPinned && i == hoverWaveIndex.index && cursorInWaveformArea;
                        QColor waveColor = adjustedColor(pinnedList.at(i), hoverHighlight);
                        if (clipWaveforms) {
                            QRect individualClipRegion = QRect(clipX, pinnedList.at(i).yTopLimit, clipWidth,
                                                               pinnedList.at(i).yBottomLimit - pinnedList.at(i).yTopLimit);
                            painter.setClipRegion(individualClipRegion);
                        }
                        waveformManager->draw(painter, waveName, QPoint(regionWaveforms1.left(), yPosition), waveColor);
                        if (clipWaveforms) {
                            painter.setClipRegion(plotClipRegion);
                        }
                    }
                    QColor waveColor = adjustedColor(pinnedList.at(i));
                    Channel* channel = pinnedList.at(i).channel;
                    drawWaveformLabel(painter, waveName, channel, QPoint(regionLabels1.right(), yPosition), waveColor,
                                      pinnedList.at(i).isSelected() ? Qt::black : Qt::white);
                    painter.drawImage(QPoint(regionScrollBar.left() + 1, yPosition - 5), unpinBadge);
                    pinnedList[i].isCurrentlyVisible = enabled;
                } else {
                    pinnedList[i].isCurrentlyVisible = false;
                }
            }
        }

        // Draw grouping markers.
        int i = 0;
        while (i < displayList.size()) {
            if (displayList.at(i).getGroupID() == 0) {
                ++i;
                continue;
            }
            // Beginning of group found; now find end.
            int start = i;
            int end = 0;
            int groupID = displayList.at(start).getGroupID();
            QColor color = adjustedColor(displayList.at(start));
            ++i;
            while (i < displayList.size()) {
                if (displayList.at(i).getGroupID() != groupID) {
                    end = i - 1;
                    break;
                }
                ++i;
            }
            // Draw group marker.
            int y1 = displayList.at(start).yTop;
            int y2 = displayList.at(end).yBottom;
            painter.fillRect(1, y1, 2, y2 - y1 + 2, color);
            painter.fillRect(1, y1 - 2, 7, 2, color);
            painter.fillRect(1, y2 + 2, 7, 2, color);
        }

        // Draw waveforms.
        for (int i = 0; i < displayList.size(); ++i) {
            yPosition = displayList.at(i).yCoord;
            // If dragging, spread out waveforms around potential drop spot.
            if (dragState.dragging && !dragState.fromPinned && !hoverWaveIndex.inPinned &&
                    listManager->isValidDragTarget(hoverWaveIndex, numFiltersDisplayed, arrangeByFilter)) {
                yPosition = spreadAroundDraggingTarget(yPosition, hoverWaveIndex.index, i);
            }
            if (yPosition >= pinnedYDivider + YExtra && yPosition <= regionWaveforms1.bottom() - YExtra) {
                bool enabled = displayList.at(i).isEnabled();
                if (!displayList.at(i).isDivider()) {
                    if (enabled || showDisabledChannels) {
                        QString waveName = displayList.at(i).waveName;
                        if (enabled) {
                            bool hoverHighlight = hoverWaveIndex.inPinned && i == hoverWaveIndex.index && cursorInWaveformArea;
                            QColor waveColor = adjustedColor(displayList.at(i), hoverHighlight);
                            if (clipWaveforms) {
                                QRect individualClipRegion = QRect(clipX, displayList.at(i).yTopLimit, clipWidth,
                                                                   displayList.at(i).yBottomLimit - displayList.at(i).yTopLimit);
                                painter.setClipRegion(individualClipRegion);
                            }
                            waveformManager->draw(painter, waveName, QPoint(regionWaveforms1.left(), yPosition), waveColor);
                            if (clipWaveforms) {
                                painter.setClipRegion(plotClipRegion);
                            }
                        }
                        QColor waveColor = adjustedColor(displayList.at(i));
                        Channel* channel = displayList.at(i).channel;
                        drawWaveformLabel(painter, waveName, channel, QPoint(regionLabels1.right(), yPosition), waveColor,
                                          displayList.at(i).isSelected() ? Qt::black : Qt::white);
                    }
                } else {
                    waveformManager->drawDivider(painter, yPosition, regionLabels1.left() + 2, regionWaveforms1.right());
                }
                displayList[i].isCurrentlyVisible = enabled;
            } else {
                displayList[i].isCurrentlyVisible = false;
            }
        }

        // Draw 'drag target' marker between waveforms.
        drawBetweenWaveformMarker(painter, regionLabels1.right());

        // Draw vertical scale bar.
        if (hoverWaveIndex.index >= 0 && !dragState.dragging) {
            int maxHeight;
            const DisplayedWaveform* wave = listManager->displayedWaveform(hoverWaveIndex);
            if (wave->isEnabled()) {
                int yTop = hoverWaveIndex.inPinned ? regionWaveforms1.top() : pinnedYDivider;
                if (hoverWaveIndex.index > 0) {
                    const DisplayedWaveform *prevWave = listManager->displayedWaveform(hoverWaveIndex.index - 1, hoverWaveIndex.inPinned);
                    maxHeight = (int)(0.67 * (double)(wave->yCoord - prevWave->yCoord));
                } else {
                    const DisplayedWaveform *topWave = listManager->displayedWaveform(0, hoverWaveIndex.inPinned);
                    maxHeight = (int)(0.67 * (double)(topWave->yCoord - yTop));
                }
                maxHeight = qMax(maxHeight, 12);

                drawYScaleBar(painter, cursor, wave->yCoord, maxHeight, wave);
            }
        }

        // Draw vertical scan line.
        if (!state->sweeping && !state->rollMode->getValue() && state->tScale->getNumericValue() >= 1000) {
            if ((!pinnedList.isEmpty() && showPinned) || !displayList.isEmpty()) {
                int x = waveformManager->getRefreshXPosition();
                painter.setPen(QColor(97, 69, 157));
                painter.drawLine(QLineF(x + regionWaveforms1.left(), regionWaveforms1.top(),
                                        x + regionWaveforms1.left(), regionWaveforms1.bottom()));
            }
        }

        painter.setClipRegion(imageFrame);

        // Draw time axis and scroll bar.
        drawTimeAxis(painter, (int) state->tScale->getNumericValue(), regionTimeAxis1.bottomLeft());
        scrollBar->draw(painter);

        // Draw dragged waveform labels.
        if (dragState.dragging) {
            int yPosition;
            for (int i = 0; i < listManager->numDisplayedWaveforms(dragState.fromPinned); ++i) {
                DisplayedWaveform* wave = listManager->displayedWaveform(i, dragState.fromPinned);
                if (wave->isSelected()) {
                    yPosition = wave->yCoord + dragDelta.y();
                    if (yPosition > -labelHeight && yPosition < rect().bottom() + labelHeight) {
                        QColor waveColor = adjustedColor(*wave, true);
                        drawWaveformLabel(painter, wave->waveName, wave->channel,
                                          QPoint(regionLabels1.right() + dragDelta.x(), yPosition), waveColor, Qt::black);
                    }
                }
            }
        }

        QStylePainter stylePainter(this);
        stylePainter.drawImage(0, 0, image);

//    cout << "plot time (ms): " << timer.nsecsElapsed() / 1.0e6 << EndOfLine;
//        qDebug() << "plot time (ms): " << timer.nsecsElapsed() / 1.0e6;
    } else {

        // EXPERIMENTAL
        //QElapsedTimer timer;
        //timer.start();
        QPainter painter(&corePixmap);
        bool showDisabledChannels = state->showDisabledChannels->getValue();
        bool clipWaveforms = state->clipWaveforms->getValue();

        int labelWidthIndex = state->labelWidth->getIndex();
        QRect regionWaveforms1 = regionWaveforms[labelWidthIndex];
        QRect regionLabels1 = regionLabels[labelWidthIndex];
        QRect regionTimeAxis1 = regionTimeAxis[labelWidthIndex];

        QRect imageFrame(rect());

        calculateDisplayYCoords();
        QPoint cursor = mapFromGlobal(QCursor::pos());
        hoverWaveIndex = findSelectedWaveform(cursor.y());
        bool cursorInWaveformArea = regionLabels1.contains(cursor) || regionWaveforms1.contains(cursor);

        int yPosition;
        QRect plotClipRegion = regionWaveforms1.united(regionLabels1).united(regionScrollBar);
        int clipX = plotClipRegion.left();
        int clipWidth = plotClipRegion.width();

        // CORE PIXMAP CHANGES
        // These are the changes that should persist to all future paint events, permanently changing corePixmap.

        if (waveformManager->needsFullRedraw) {

            // Future improvements to high-efficiency plotting:
            // At this point, it's possible that after a scroll event new waveforms will be visible,
            // but they won't have previous data loaded - this happens ultimately in controllerinterface
            // depending on when data comes in. If this redraw occurs before that data load (more than likely),
            // the newly displayed channels won't display any data before when the scroll occurred.
            // This could likely be fixed by either a) somehow forcing the data to load at this point in the redraw or
            // b) triggering another full redraw as soon as more data comes in from controllerinterface.

            // Clear old display.
            const QColor BackgroundColor = QColor(state->backgroundColor->getValueString());
            painter.fillRect(imageFrame, QBrush(BackgroundColor));

            // For full reset, reset waveformManager.
            if (waveformManager->needsFullReset) {
                waveformManager->resetAll();
                waveformManager->singlePlotFullResetFinished();
            }

            // Draw time axis and scroll bar.
            painter.setClipRegion(imageFrame);
            drawTimeAxis(painter, (int) state->tScale->getNumericValue(), regionTimeAxis1.bottomLeft());
            scrollBar->draw(painter);
        }

        // Clear currently active zone of plot.
        painter.setClipRegion(plotClipRegion);
        if (state->running) {
            waveformManager->clearActiveSectionOfRect(painter, regionWaveforms1);
        }

        // Draw waveforms.
        for (int i = 0; i < displayList.size(); ++i) {
            yPosition = displayList.at(i).yCoord;
            // If dragging, spread out waveforms around potential drop spot.
            if (dragState.dragging && !dragState.fromPinned && !hoverWaveIndex.inPinned &&
                    listManager->isValidDragTarget(hoverWaveIndex, numFiltersDisplayed, arrangeByFilter)) {
                yPosition = spreadAroundDraggingTarget(yPosition, hoverWaveIndex.index, i);
            }

            if (yPosition >= pinnedYDivider + YExtra && yPosition <= regionWaveforms1.bottom() - YExtra) {
                bool enabled = displayList.at(i).isEnabled();
                if (!displayList.at(i).isDivider()) {
                    if (enabled || showDisabledChannels) {
                        QString waveName = displayList.at(i).waveName;
                        if (enabled) {
                            //bool hoverHighlight = hoverWaveIndex.inPinned && i == hoverWaveIndex.index && cursorInWaveformArea;
                            // for high-efficiency mode to reduce redraws, do not highlight waveforms upon hovering.
                            bool hoverHighlight = false;
                            QColor waveColor = adjustedColor(displayList.at(i), hoverHighlight);
                            if (clipWaveforms) {
                                QRect individualClipRegion = QRect(clipX, displayList.at(i).yTopLimit, clipWidth,
                                                                   displayList.at(i).yBottomLimit - displayList.at(i).yTopLimit);
                                painter.setClipRegion(individualClipRegion);
                            }
                            waveformManager->draw(painter, waveName, QPoint(regionWaveforms1.left(), yPosition), waveColor);
                            if (clipWaveforms) {
                                painter.setClipRegion(plotClipRegion);
                            }
                        }
                    }
                } else {
                    waveformManager->drawDivider(painter, yPosition, regionLabels1.left() + 2, regionWaveforms1.right());
                }
                displayList[i].isCurrentlyVisible = enabled;
            } else {
                displayList[i].isCurrentlyVisible = false;
            }
        }

        if (showPinned) {
            // Draw pinned/unpinned waveform divider.
            if (!pinnedList.isEmpty()) {
                waveformManager->drawDivider(painter, pinnedYDivider, regionLabels1.left() + 2, regionWaveforms1.right());
            }

            // Draw pinned waveforms.
            for (int i = 0; i < pinnedList.size(); ++i) {
                yPosition = pinnedList.at(i).yCoord;
                // If dragging, spread out waveforms around potential drop spot.
                if (dragState.dragging && dragState.fromPinned && hoverWaveIndex.inPinned) {
                    yPosition = spreadAroundDraggingTarget(yPosition, hoverWaveIndex.index, i);
                }
                QString waveName = pinnedList.at(i).waveName;
                bool enabled = pinnedList.at(i).isEnabled();
                if (enabled || showDisabledChannels) {
                    if (enabled) {
                        //bool hoverHighlight = hoverWaveIndex.inPinned && i == hoverWaveIndex.index && cursorInWaveformArea;
                        // for high-efficiency mode to reduce redraws, do not highlight waveforms upon hovering.
                        bool hoverHighlight = false;
                        QColor waveColor = adjustedColor(pinnedList.at(i), hoverHighlight);
                        if (clipWaveforms) {
                            QRect individualClipRegion = QRect(clipX, pinnedList.at(i).yTopLimit, clipWidth,
                                                               pinnedList.at(i).yBottomLimit - pinnedList.at(i).yTopLimit);
                            painter.setClipRegion(individualClipRegion);
                        }
                        waveformManager->draw(painter, waveName, QPoint(regionWaveforms1.left(), yPosition), waveColor);
                        if (clipWaveforms) {
                            painter.setClipRegion(plotClipRegion);
                        }
                    }
                    pinnedList[i].isCurrentlyVisible = enabled;
                } else {
                    pinnedList[i].isCurrentlyVisible = false;
                }
            }
        }


        // FULL PIXMAP CHANGES
        // These are the changes that are only applicable for the current paint event, and only change fullPixmap, not
        // the underlying corePixmap.
        // This is for the type of transient changes that depend on things like mousePosition that should not be retained.
        painter.end();
        fullPixmap = corePixmap.copy();
        painter.begin(&fullPixmap);

        // Draw waveform labels
        for (int i = 0; i < displayList.size(); ++i) {
            yPosition = displayList.at(i).yCoord;
            // If dragging, spread out waveforms around potential drop spot.
            if (dragState.dragging && !dragState.fromPinned && !hoverWaveIndex.inPinned &&
                    listManager->isValidDragTarget(hoverWaveIndex, numFiltersDisplayed, arrangeByFilter)) {
                yPosition = spreadAroundDraggingTarget(yPosition, hoverWaveIndex.index, i);
            }

            if (yPosition >= pinnedYDivider + YExtra && yPosition <= regionWaveforms1.bottom() - YExtra) {
                bool enabled = displayList.at(i).isEnabled();
                if (!displayList.at(i).isDivider()) {
                    if (enabled || showDisabledChannels) {
                        QString waveName = displayList.at(i).waveName;
                        QColor waveColor = adjustedColor(displayList.at(i));
                        Channel* channel = displayList.at(i).channel;
                        drawWaveformLabel(painter, waveName, channel, QPoint(regionLabels1.right(), yPosition), waveColor,
                                          displayList.at(i).isSelected() ? Qt::black : Qt::white);
                    }
                }
            }
        }

        if (showPinned) {
            for (int i = 0; i < pinnedList.size(); ++i) {
                yPosition = pinnedList.at(i).yCoord;
                // If dragging, spread out waveforms around potential drop spot.
                if (dragState.dragging && !dragState.fromPinned && hoverWaveIndex.inPinned) {
                    yPosition = spreadAroundDraggingTarget(yPosition, hoverWaveIndex.index, i);
                }

                QString waveName = pinnedList.at(i).waveName;
                bool enabled = pinnedList.at(i).isEnabled();
                if (enabled || showDisabledChannels) {
                    QColor waveColor = adjustedColor(pinnedList.at(i));
                    Channel* channel = pinnedList.at(i).channel;
                    drawWaveformLabel(painter, waveName, channel, QPoint(regionLabels1.right(), yPosition), waveColor,
                                      pinnedList.at(i).isSelected() ? Qt::black : Qt::white);
                    painter.drawImage(QPoint(regionScrollBar.left() + 1, yPosition - 5), unpinBadge);
                }

            }
        }

        // Draw vertical time lines.
        if (regionTimeAxis1.contains(cursor)) {
            painter.setClipRegion(imageFrame);
            drawVerticalTimeLines(painter, regionTimeAxis1.left(), cursor.x());
        }

        painter.setClipRegion(plotClipRegion);

        // Draw display trigger line.
        drawTriggerLine(painter, regionTimeAxis1.left());

        // Draw y = 0 baseline.
        if (hoverWaveIndex.index >= 0 && cursorInWaveformArea && !dragState.dragging) {
            yPosition = listManager->displayedWaveform(hoverWaveIndex)->yCoord;
            painter.setPen(QColor(72, 72, 72));
            painter.drawLine(QPoint(regionWaveforms1.left(), yPosition), QPoint(regionWaveforms1.right() - 1, yPosition));
        }

        // Draw grouping markers.
        int i = 0;
        while (i < displayList.size()) {
            if (displayList.at(i).getGroupID() == 0) {
                ++i;
                continue;
            }
            // Beginning of group found; now find end.
            int start = i;
            int end = 0;
            int groupID = displayList.at(start).getGroupID();
            QColor color = adjustedColor(displayList.at(start));
            ++i;
            while (i < displayList.size()) {
                if (displayList.at(i).getGroupID() != groupID) {
                    end = i - 1;
                    break;
                }
                ++i;
            }
            // Draw group marker.
            int y1 = displayList.at(start).yTop;
            int y2 = displayList.at(end).yBottom;
            painter.fillRect(1, y1, 2, y2 - y1 + 2, color);
            painter.fillRect(1, y1 - 2, 7, 2, color);
            painter.fillRect(1, y2 + 2, 7, 2, color);
        }

        // Draw 'drag target' marker between waveforms.
        drawBetweenWaveformMarker(painter, regionLabels1.right());

        // Draw vertical scale bar.
        if (hoverWaveIndex.index >= 0 && !dragState.dragging) {
            int maxHeight;
            const DisplayedWaveform* wave = listManager->displayedWaveform(hoverWaveIndex);
            if (wave->isEnabled()) {
                int yTop = hoverWaveIndex.inPinned ? regionWaveforms1.top() : pinnedYDivider;
                if (hoverWaveIndex.index > 0) {
                    const DisplayedWaveform *prevWave = listManager->displayedWaveform(hoverWaveIndex.index - 1, hoverWaveIndex.inPinned);
                    maxHeight = (int)(0.67 * (double)(wave->yCoord - prevWave->yCoord));
                } else {
                    const DisplayedWaveform *topWave = listManager->displayedWaveform(0, hoverWaveIndex.inPinned);
                    maxHeight = (int)(0.67 * (double)(topWave->yCoord - yTop));
                }
                maxHeight = qMax(maxHeight, 12);

                drawYScaleBar(painter, cursor, wave->yCoord, maxHeight, wave);
            }
        }

        // Draw vertical scan line.
        if (!state->sweeping && !state->rollMode->getValue() && state->tScale->getNumericValue() >= 1000) {
            if ((!pinnedList.isEmpty() && showPinned) || !displayList.isEmpty()) {
                int x = waveformManager->getRefreshXPosition();
                painter.setPen(QColor(97, 69, 157));
                painter.drawLine(QLineF(x + regionWaveforms1.left(), regionWaveforms1.top(),
                                        x + regionWaveforms1.left(), regionWaveforms1.bottom()));
            }
        }

        // Draw dragged waveform labels.
        painter.setClipRegion(imageFrame);
        if (dragState.dragging) {
            int yPosition;
            for (int i = 0; i < listManager->numDisplayedWaveforms(dragState.fromPinned); ++i) {
                DisplayedWaveform* wave = listManager->displayedWaveform(i, dragState.fromPinned);
                if (wave->isSelected()) {
                    yPosition = wave->yCoord + dragDelta.y();
                    if (yPosition > -labelHeight && yPosition < rect().bottom() + labelHeight) {
                        QColor waveColor = adjustedColor(*wave, true);
                        drawWaveformLabel(painter, wave->waveName, wave->channel,
                                          QPoint(regionLabels1.right() + dragDelta.x(), yPosition), waveColor, Qt::black);
                    }
                }
            }
        }

        if (waveformManager->needsFullRedraw) {
            waveformManager->singlePlotFullRedrawFinished();
        }

        QStylePainter stylePainter(this);
        stylePainter.drawPixmap(0, 0, fullPixmap);

        //qDebug() << "plot time (ms): " << timer.nsecsElapsed() / 1.0e6;
        //cout << "plot time (ms): " << timer.nsecsElapsed() / 1.0e6 << EndOfLine;
    }
}

QColor MultiWaveformPlot::adjustedColor(const DisplayedWaveform& waveform, bool hoverSelect) const
{
    bool enabled = waveform.isEnabled();
    bool selected = waveform.isSelected() || hoverSelect;
    QColor color = waveform.getColor();

    if (enabled && !selected) return color;
    if (!enabled) color = DisabledColor;
    if (selected) color = color.lighter(170);
    return color;
}

int MultiWaveformPlot::spreadAroundDraggingTarget(int y, int hoverIndex, int index) const
{
    int yPosition = y;
    int n;
    if (hoverIndex < 0) n = -hoverIndex - 2;
    else n = hoverIndex;
    if (n == index + 1) yPosition = y - 2;
    if (n == index    ) yPosition = y - 4;
    if (n == index - 1) yPosition = y + 4;
    if (n == index - 2) yPosition = y + 2;
    return yPosition;
}

void MultiWaveformPlot::calculateDisplayYCoords()
{
    int topBottomMargin = labelHeight;
    double zoomFactor = scrollBar->getZoomFactor();
    int minSpacing;
    int labelWidthIndex = state->labelWidth->getIndex();
    QRect regionWaveforms1 = regionWaveforms[labelWidthIndex];
    pinnedYDivider = regionWaveforms1.top();

    int y = topBottomMargin;

    if (!pinnedList.isEmpty() && showPinned) {
        y = listManager->calculateYCoords(pinnedList, y, labelHeight, zoomFactor, minSpacing);
        listManager->addYOffset(pinnedList, regionWaveforms1.top());
        pinnedYDivider += y;
        y += topBottomMargin;
    }

    regionScrollBar.setTop(pinnedYDivider);
    regionUnpinSymbols.setBottom(pinnedYDivider);
    scrollBar->resize(regionScrollBar);

    y = listManager->calculateYCoords(displayList, y, labelHeight, zoomFactor, minSpacing);
    listManager->addYOffset(displayList, regionWaveforms1.top() - scrollBar->getTopPosition());

    scrollBar->setRange(y);
    scrollBar->setPageSize(regionWaveforms1.height());
    scrollBar->setStepSize(minSpacing);
}

WaveIndex MultiWaveformPlot::findSelectedWaveform(int y) const
{
    WaveIndex w;
    if (y < pinnedYDivider) {
        w = { listManager->findSelectedWaveform(pinnedList, y), true };
    } else {
        w = { listManager->findSelectedWaveform(displayList, y), false };
    }
    return w;
}

void MultiWaveformPlot::mouseMoveEvent(QMouseEvent* event)
{
    if (event->pos() == lastMousePos) return;  // Only deal with actual mouse moves.  (Apparently mouseMoveEvent is also
    else lastMousePos = event->pos();          // triggered by mouse button presses, which we handle separately.)

    hoverWaveIndex = findSelectedWaveform(event->pos().y());

    scrollBar->handleMouseMove(event->pos());

    if (dragState.dragging && !dragState.fromPinned) {
        // Scroll up or down if pointer is above or below labels.
        if (regionBelowLabels.contains(event->pos())) {
            scrollBar->scroll(scrollBar->getStepSize());
        } else if (regionAboveLabels.contains(event->pos()) || hoverWaveIndex.inPinned) {
            scrollBar->scroll(-scrollBar->getStepSize());
        }
    } else if (!dragState.dragging && event->buttons() & Qt::LeftButton) {
        if (regionLabels[state->labelWidth->getIndex()].contains(event->pos()) && !outOfBoundsClick) {
            // Start dragging.
            WaveIndex clickedWaveform = findSelectedWaveform(event->pos().y());
            dragState.dragging = true;
            dragState.fromPinned = clickedWaveform.inPinned;
            if (clickedWaveform.index >= 0) {
                if (!(listManager->displayedWaveform(clickedWaveform)->isSelected())) {
                    if (!(event->modifiers() & Qt::ShiftModifier) && !(event->modifiers() & Qt::ControlModifier)) {
                        listManager->selectSingleWaveform(clickedWaveform);
                        state->forceUpdate();
                    }
                } else {
                    dragState.dragging = false;
                }
            }
        }
    }
    if (dragState.dragging) {
        dragDelta = event->pos() - clickPoint;
        if (!dragState.fromPinned) {
            dragDelta += QPoint(0, scrollBar->getTopPosition());
        }
    }
    update();
}

void MultiWaveformPlot::mousePressEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton)) {  // Ignore center and right button releases.
        QWidget::mousePressEvent(event);
        return;
    }

    bool needToUpdate = false;
    bool needToUpdateState = false;
    mouseButtonDown = true;
    clickPoint = event->pos();
    if (event->pos().y() >= pinnedYDivider) {
        clickPoint += QPoint(0, scrollBar->getTopPosition());
    }

    // Handle mouse clicks on scroll bar.
    if (scrollBar->handleMousePress(event->pos())) needToUpdate = true;

    // Handle mouse clicks on waveforms.
    if (regionLabels[state->labelWidth->getIndex()].contains(event->pos()) ||
            regionWaveforms[state->labelWidth->getIndex()].contains(event->pos())) {
        WaveIndex clickedWaveform = findSelectedWaveform(event->pos().y());
        if (clickedWaveform.index >= 0) {
            DisplayedWaveform* clickedWave = listManager->displayedWaveform(clickedWaveform);
            if (!(event->modifiers() & Qt::ShiftModifier) && (event->modifiers() & Qt::ControlModifier)) {
                // Control-click to select or deselect multiple waveforms.
                if (!clickedWaveform.inPinned) {  // Don't allow multiple waveform selection in pinned list; too many problems.
                    listManager->selectNonAdjacentWaveforms(clickedWaveform, numFiltersDisplayed, arrangeByFilter);
                    needToUpdateState = true;
                }
            } else if ((event->modifiers() & Qt::ShiftModifier) && !(event->modifiers() & Qt::ControlModifier)) {
                // Shift-click to select multiple adjacent waveforms.
                if (!clickedWaveform.inPinned) {  // Don't allow multiple waveform selection in pinned list; too many problems.
                    if (arrangeByFilter && numFiltersDisplayed > 1 && clickedWave->sectionID <= numFiltersDisplayed &&
                            listManager->sectionIDOfSelectedWaveforms() <= numFiltersDisplayed &&
                            listManager->selectedWaveformsAreAmplifiers()) {
                        // Add new selections if all are amp waveforms of different filters.
                        int waveformsPerSection = listManager->numWaveformsInFirstSection(displayList);
                        listManager->selectAdjacentWaveforms(clickedWaveform, waveformsPerSection);
                        needToUpdateState = true;

                    } else if (clickedWave->sectionID == listManager->sectionIDOfSelectedWaveforms()) {
                        // Add new selections if all are same type as old.
                        listManager->selectAdjacentWaveforms(clickedWaveform);
                        needToUpdateState = true;
                    }
                }
            }

            if (clickedWave->isDivider()) outOfBoundsClick = true;
        }
    } else {
        outOfBoundsClick = true;
    }

    if (needToUpdateState) {
        state->forceUpdate();
    } else if (needToUpdate) {
        update();
    }
}

void MultiWaveformPlot::mouseReleaseEvent(QMouseEvent* event)
{
    bool needToUpdate = false;
    bool needToUpdateState = false;
    mouseButtonDown = false;
    outOfBoundsClick = false;
    QPoint dropPoint = event->pos();

    // Release scroll bar, if held.
    if (scrollBar->handleMouseRelease()) needToUpdate = true;

    // Move waveforms, if selected.
    if (dragState.dragging) {
        dragState.dragging = false;
        if (!(event->modifiers() & Qt::ShiftModifier) && !(event->modifiers() & Qt::ControlModifier)) {
            WaveIndex dropPosition = findSelectedWaveform(dropPoint.y());
            if (listManager->isValidDragTarget(dropPosition, numFiltersDisplayed, arrangeByFilter)) {
                bool realMove = false;
                state->signalSources->undoManager->pushStateToUndoStack();
                if (dragState.fromPinned && dropPosition.inPinned) {
                    // Move pinned waveforms.
                    if (dropPosition.index < -1) {  // Drop between waveforms.
                        realMove = listManager->moveSelectedWaveforms(pinnedList, -(dropPosition.index + 2));
                    } else {                        // Drop after selected waveform.
                        realMove = listManager->moveSelectedWaveforms(pinnedList, dropPosition.index);
                    }
                } else if (!dragState.fromPinned && !dropPosition.inPinned) {
                    // Move unpinned waveforms.
                    if (arrangeByFilter && numFiltersDisplayed > 1 && listManager->selectedWaveformsAreAmplifiers()) {
                        if (dropPosition.index < -1) {  // Drop between waveforms.
                            realMove = listManager->moveSelectedWaveformsArrangedByFilter(displayList,
                                                                                                   -(dropPosition.index + 2),
                                                                                                   numFiltersDisplayed);
                        } else {                        // Drop after selected waveform.
                            realMove = listManager->moveSelectedWaveformsArrangedByFilter(displayList,
                                                                                                   dropPosition.index,
                                                                                                   numFiltersDisplayed);
                        }
                    } else {
                        if (dropPosition.index < -1) {  // Drop between waveforms.
                            realMove = listManager->moveSelectedWaveforms(displayList, -(dropPosition.index + 2));
                        } else {                        // Drop after selected waveform.
                            realMove = listManager->moveSelectedWaveforms(displayList, dropPosition.index);
                        }
                    }
                }
                if (realMove) {
                    needToUpdateState = true;
                } else {
                    needToUpdate = true;
                    state->signalSources->undoManager->retractLastPush();
                }
            }
            if (needToUpdateState) {
                listManager->updateOrderInState(parent->getSelectedPort(), numFiltersDisplayed, arrangeByFilter);
            }
        }
    } else if (regionLabels[state->labelWidth->getIndex()].contains(dropPoint) ||
               regionWaveforms[state->labelWidth->getIndex()].contains(dropPoint)) {
        if (!(event->modifiers() & Qt::ShiftModifier) && !(event->modifiers() & Qt::ControlModifier)) {
            // Click and release to select a single waveform.
            WaveIndex clickedWaveform = findSelectedWaveform(dropPoint.y());

            std::vector<bool> before = listManager->selectionRecord();
            listManager->selectSingleWaveform(clickedWaveform);
            std::vector<bool> after = listManager->selectionRecord();
            needToUpdateState = !listManager->selectionRecordsAreEqual(before, after);
        }
    } else if (regionUnpinSymbols.contains(dropPoint)) {
        if (!(event->modifiers() & Qt::ShiftModifier) && !(event->modifiers() & Qt::ControlModifier)) {
            // Click and release on unpin symbol to remove a single pinned waveform.
            WaveIndex clickedWaveform = findSelectedWaveform(dropPoint.y());
            if (clickedWaveform.inPinned) {
                state->signalSources->undoManager->pushStateToUndoStack();
                pinnedList.removeAt(clickedWaveform.index);
                needToUpdate = true;
            }
        }
    }

    if (needToUpdateState) {
        state->forceUpdate();
    } else if (needToUpdate) {
        update();
    }
}

void MultiWaveformPlot::wheelEvent(QWheelEvent* event)
{
    bool needToUpdate = false;
    int topPositionOld = scrollBar->getTopPosition();
    bool shiftHeld = event->modifiers() & Qt::ShiftModifier;
    bool controlHeld = event->modifiers() & Qt::ControlModifier;
    int delta = event->angleDelta().y();

    if (scrollBar->handleWheelEvent(delta, shiftHeld, controlHeld)) needToUpdate = true;

    if (shiftHeld && !controlHeld) {
        if (delta > 0) {
            state->tScale->decrementIndex();
        } else if (delta < 0) {
            state->tScale->incrementIndex();
        }
    }

    if (dragState.dragging && !dragState.fromPinned) {
        dragDelta.setY(dragDelta.y() - (topPositionOld - scrollBar->getTopPosition()));
        needToUpdate = true;
    }
    if (needToUpdate) update();
}

void MultiWaveformPlot::keyPressEvent(QKeyEvent* event)
{
    bool needToUpdate = false;
    bool needToUpdateState = false;
    bool ctrlPressed = event->modifiers() & Qt::ControlModifier;

    int topPositionOld = scrollBar->getTopPosition();

    if (scrollBar->handleKeyPressEvent(event->key())) {
        needToUpdate = true;
    } else if (event->matches(QKeySequence::MoveToNextPage)) {  // Check for alternate (Mac) key sequences.
        if (scrollBar->handleKeyPressEvent(Qt::Key_PageDown)) needToUpdate = true;
    } else if (event->matches(QKeySequence::MoveToPreviousPage)) {
        if (scrollBar->handleKeyPressEvent(Qt::Key_PageUp)) needToUpdate = true;
    } else if (event->matches(QKeySequence::MoveToStartOfLine)) {
        if (scrollBar->handleKeyPressEvent(Qt::Key_Home)) needToUpdate = true;
    } else if (event->matches(QKeySequence::MoveToEndOfLine)) {
        if (scrollBar->handleKeyPressEvent(Qt::Key_End)) needToUpdate = true;
    } else if (event->key() == Qt::Key_Down || event->matches(QKeySequence::MoveToNextLine)) {
        needToUpdate = true;
        needToUpdateState = true;
        if (listManager->selectNextWaveform()) scrollBar->handleKeyPressEvent(Qt::Key_PageDown);
    } else if (event->key() == Qt::Key_Up || event->matches(QKeySequence::MoveToPreviousLine)) {
        needToUpdate = true;
        needToUpdateState = true;
        if (listManager->selectPreviousWaveform()) scrollBar->handleKeyPressEvent(Qt::Key_PageUp);
    }

    if (!(event->isAutoRepeat())) { // Ignore additional 'keypresses' from auto-repeat if key is held down.
        switch (event->key()) {

        case Qt::Key_Space:  // Space bar: Toggle waveform enable.
            if (!state->testMode->getValue()) {
                state->signalSources->undoManager->pushStateToUndoStack();
                listManager->toggleSelectedWaveforms();
                needToUpdateState = true;
            } else {
                QWidget::keyPressEvent(event);
            }
            break;

        case Qt::Key_P:
            if (ctrlPressed) {  // Ctrl+P: Pin selected waveforms.
                int numSelected = listManager->numSelectedWaveforms();
                if (numSelected > 0 && numSelected <= 12) {  // Limit max number to prevent accidentally pinning everything.
                    state->signalSources->undoManager->pushStateToUndoStack();
                    listManager->pinSelectedWaveforms();
                    needToUpdate = true;
                }
            }
            break;

        case Qt::Key_U:
            if (ctrlPressed) {  // Ctrl+U: Unpin selected waveforms.
                state->signalSources->undoManager->pushStateToUndoStack();
                listManager->unpinSelectedWaveforms();
                needToUpdate = true;
            }
            break;

        default:
            QWidget::keyPressEvent(event);
        }
    }

    if (dragState.dragging) {
        dragDelta.setY(dragDelta.y() - (topPositionOld - scrollBar->getTopPosition()));
        needToUpdate = true;
    }
    if (needToUpdateState) {
        state->forceUpdate();
    } else if (needToUpdate) {
        update();
    }
}

void MultiWaveformPlot::group()
{
    state->signalSources->undoManager->pushStateToUndoStack();
    if (listManager->groupSelectedWaveforms(true, numFiltersDisplayed, arrangeByFilter)) {
        listManager->updateOrderInState(parent->getSelectedPort(), numFiltersDisplayed, arrangeByFilter);
    }
    state->forceUpdate();
}

void MultiWaveformPlot::ungroup()
{
    state->signalSources->undoManager->pushStateToUndoStack();
    listManager->groupSelectedWaveforms(false, numFiltersDisplayed, arrangeByFilter);
    state->forceUpdate();
}

int MultiWaveformPlot::currentPageIndex() const
{
    return parent->currentPortIndex();
}

/* Request that a full redraw is triggered on ALL plots */
void MultiWaveformPlot::requestRedraw() {
    waveformManager->needsFullRedraw = true;
}

/* Request that a full reset and redraw is triggered on ALL plots */
void MultiWaveformPlot::requestReset() {
    waveformManager->needsFullReset = true;
    requestRedraw();
}

void MultiWaveformPlot::resizeEvent(QResizeEvent*) {
    calculateScreenRegions();
    scrollBar->resize(regionScrollBar);
    image = QImage(size(), QImage::Format_ARGB32_Premultiplied);
    corePixmap = QPixmap(size());
    fullPixmap = QPixmap(size());
    update();

    requestReset();
}

void MultiWaveformPlot::drawVerticalTimeLines(QPainter &painter, int xPosition, int xCursor)
{
    float xSize = (float) waveformManager->getWidthInPixels() / 10.0F;
    float yTop = (float) regionWaveforms[state->labelWidth->getIndex()].top();
    float yBottom = (float) regionWaveforms[state->labelWidth->getIndex()].bottom();
    painter.setPen(QColor(72, 72, 72));
    float x = (float) xPosition;
    int step = timeAxisStep();
    for (int i = 0; i <= 10; i += step) {
        painter.drawLine(QPointF(x, yTop), QPointF(x, yBottom));
        x += xSize * step;
    }
    if (xCursor >= xPosition && xCursor <= x - xSize) {
        painter.setPen(Qt::yellow);
        painter.drawLine(QPoint(xCursor, yTop), QPoint(xCursor, yBottom));
    }
}

void MultiWaveformPlot::drawTriggerLine(QPainter &painter, int xPosition)
{
    if (!state->triggerModeDisplay->getValue()) return;

    float triggerPos = (float) state->triggerPositionDisplay->getNumericValue();
    float x = (float) xPosition + (float) waveformManager->getWidthInPixels() * triggerPos;
    painter.setPen(Qt::blue);
    float yTop = (float) regionWaveforms[state->labelWidth->getIndex()].top();
    float yBottom = (float) regionWaveforms[state->labelWidth->getIndex()].bottom();
    painter.drawLine(QPoint(x, yTop), QPoint(x, yBottom));

    // Plot 'T'
    painter.drawLine(QPoint(x, yTop - 5), QPoint(x, yTop - 10));
    painter.drawLine(QPoint(x - 3, yTop - 10), QPoint(x + 3, yTop - 10));
}

int MultiWaveformPlot::timeAxisStep() const
{
    int xSize = waveformManager->getWidthInPixels();
    int step = 1;
    if (xSize < 650) step = 2;
    if (xSize < 325) step = 5;
    return step;
}

void MultiWaveformPlot::drawTimeAxis(QPainter &painter, int tScaleInMsec, QPoint position)
{
    QString label;
    int pixelsWide;
    int xShift;

    const QColor BackgroundColor = QColor(state->backgroundColor->getValueString());
    if (BackgroundColor.value() < 128) {  // Ensure time axis is visible against background.
        painter.setPen(Qt::white);
    } else {
        painter.setPen(Qt::black);
    }

    int xSize = waveformManager->getWidthInPixels();
    int ySize = 8;
    float x = (float) position.x();
    float y = (float) position.y();
    float y1 = y - (float) ySize;

    int step = timeAxisStep();
    float xIncrement = ((float) (xSize * step)) / 10.0F;

    for (int i = 0; i <= 10; i += step) {
        painter.drawLine(QPointF(x, y), QPointF(x, y1));
        label = QString::number(i * tScaleInMsec / 10);
        pixelsWide = labelFontMetrics->horizontalAdvance(label);
        if (i < 10) {
            xShift = pixelsWide / 2;
        } else {
            label = label + QString(tr(" ms"));
            pixelsWide = labelFontMetrics->horizontalAdvance(label);
            xShift = pixelsWide - 10;
        }
        painter.drawText(x - xShift, y - labelHeight, label);
        x += xIncrement;
    }

    int yLine = position.y() - ySize / 2;
    painter.drawLine(QPointF(position.x(), yLine), QPointF(position.x() + xSize, yLine));
}

void MultiWaveformPlot::drawBetweenWaveformMarker(QPainter &painter, int xPosition)
{
    if (!dragState.dragging) return;
    if (dragState.fromPinned && !hoverWaveIndex.inPinned) return;
    if (!dragState.fromPinned && hoverWaveIndex.inPinned) return;
    if (!(listManager->isValidDragTarget(hoverWaveIndex, numFiltersDisplayed, arrangeByFilter))) return;

    int y;
    int ySpace;
    int length = listManager->numDisplayedWaveforms(hoverWaveIndex.inPinned);
    if (hoverWaveIndex.index == -1) {
        y = listManager->displayedWaveform(0, hoverWaveIndex.inPinned)->yTop - 5;
    } else if (hoverWaveIndex.index == -(length + 1) || hoverWaveIndex.index == length - 1) {
        y = listManager->displayedWaveform(length - 1, hoverWaveIndex.inPinned)->yBottom + 6;
    } else if (hoverWaveIndex.index >= 0) {
        const DisplayedWaveform* wave1 = listManager->displayedWaveform(hoverWaveIndex);
        const DisplayedWaveform* wave2 = listManager->displayedWaveform(hoverWaveIndex.index + 1, hoverWaveIndex.inPinned);
        y = (wave1->yBottom + wave2->yTop) / 2;
        ySpace = wave2->yTop - wave1->yBottom;
    } else {
        const DisplayedWaveform* wave1 = listManager->displayedWaveform(-hoverWaveIndex.index - 2, hoverWaveIndex.inPinned);
        const DisplayedWaveform* wave2 = listManager->displayedWaveform(-hoverWaveIndex.index - 1, hoverWaveIndex.inPinned);
        y = (wave1->yBottom + wave2->yTop) / 2;
        ySpace = wave2->yTop - wave1->yBottom;
    }
    ySpace = qBound(18, ySpace, labelHeight + 8);
    painter.setPen(Qt::white);
    painter.drawRect(QRect(xPosition - labelWidth[state->labelWidth->getIndex()] + 5, y - ySpace / 2 + 5,
                     labelWidth[state->labelWidth->getIndex()] - 7, ySpace - 8));
}

void MultiWaveformPlot::drawWaveformLabel(QPainter &painter, const QString& name, const Channel* channel, QPoint position,
                                          QColor color, QColor textColor)
{
    int labelWidthIndex = state->labelWidth->getIndex();
    int labelWidth1 = labelWidth[labelWidthIndex];
    int labelWidthFilter1 = labelWidthFilter[labelWidthIndex];

    QString mainText;
    QString filterText;
    bool drawFilterText = false;
    bool isAmpSignal = channel->getSignalType() == AmplifierSignal;
    if (isAmpSignal) {
        filterText = name.section('|', 1, 1);
        if (labelWidthIndex == 1) filterText = filterText.left(1);
    }

    QString labelSelection = state->displayLabelText->getValue();
    if (!isAmpSignal) {
        if (labelSelection == "Native Name") mainText = channel->getNativeName();
        else mainText = channel->getCustomName();
        if (labelWidthIndex == 1 && mainText.length() > 10) {
            if (mainText.length() == 11 && mainText.left(10) == "ANALOG-IN-") mainText = "AI-" + mainText.right(1);
            else if (mainText.length() == 12 && mainText.left(11) == "ANALOG-OUT-") mainText = "AO-" + mainText.right(1);
            else if (mainText.length() == 13 && mainText.left(11) == "DIGITAL-IN-") mainText = "DI-" + mainText.right(2);
            else if (mainText.length() == 14 && mainText.left(12) == "DIGITAL-OUT-") mainText = "DO-" + mainText.right(2);
        }
    } else if (labelSelection == "CustomName") {
        mainText = channel->getCustomName();
        drawFilterText = labelWidthIndex != 0;
    } else if (labelSelection == "NativeName") {
        mainText = channel->getNativeName();
        drawFilterText = labelWidthIndex != 0;
    } else if (labelSelection == "ImpedanceMagnitude") {
        if (!channel->isImpedanceValid()) {
            mainText = tr("n/a");
        } else {
            mainText = channel->getImpedanceMagnitudeString();
        }
        mainText = channel->getCustomName() + " " + mainText;
    } else if (labelSelection == "ImpedancePhase") {
        if (!channel->isImpedanceValid()) {
            mainText = tr("n/a");
        } else {
            mainText = channel->getImpedancePhaseString();
        }
        mainText = channel->getCustomName() + " " + mainText;
    } else if (labelSelection == "Reference") {
        if (labelWidthIndex == 2) {
            mainText = tr("REF: ") + channel->getReference();
        } else if (labelWidthIndex == 1) {
            mainText = tr("R:") + channel->getReference();
        }
    }
    if (labelWidthIndex == 1) {
        if (isAmpSignal && mainText.length() > 5) {
            mainText = EllipsisSymbol + mainText.right(4);
        } else if (!isAmpSignal && mainText.length() > 6) {
            mainText = EllipsisSymbol + mainText.right(5);
        }
    } else if (labelWidthIndex == 2) {
        if (isAmpSignal && mainText.length() > 13) {
            mainText = EllipsisSymbol + mainText.right(12);
        }
    }

    painter.setFont(*labelFont);

    int halfHeight = (labelHeight - 1) / 2;
    int y1 = position.y() - halfHeight;
    int y2 = position.y() + halfHeight;

    int x = position.x() + 3;
    int x1 = x - labelWidth1 + 2;
    int xFilterText = x - labelWidthFilter1 - 5;
    int xText;
    if (drawFilterText) {
        painter.fillRect(x1, y1, xFilterText - x1 - 3, labelHeight, color);
        painter.fillRect(xFilterText - 1, y1, labelWidthFilter1 + 2, labelHeight, color);
        xText = xFilterText - 4 - labelFontMetrics->horizontalAdvance(mainText);
    } else {
        painter.fillRect(x1, y1, labelWidth1 - 6, labelHeight, color);
        xText = x - 5 - labelFontMetrics->horizontalAdvance(mainText);
    }
    painter.setPen(textColor);
    if (drawFilterText) painter.drawText(xFilterText, y2, filterText);
    if (labelWidthIndex != 0) painter.drawText(xText, y2, mainText);

    if (labelWidthIndex == 2) {
        bool darkText = textColor == Qt::black;
        int xOffset = 0;
        if (channel->isEnabled()) {
            bool oldSaveFile = (state->fileFormat->getValue().toLower() == "traditional");  // Old .rhd/.rhs file format does not support LOW, HIGH, SPK.
            if (((state->saveWidebandAmplifierWaveforms->getValue() || oldSaveFile) && filterText == "WIDE") ||
                (state->saveLowpassAmplifierWaveforms->getValue() && filterText == "LOW" && !oldSaveFile) ||
                (state->saveHighpassAmplifierWaveforms->getValue() && filterText == "HIGH" && !oldSaveFile) ||
                (state->saveSpikeData->getValue() && filterText == "SPK" && !oldSaveFile) ||
                (state->saveDCAmplifierWaveforms->getValue() && filterText == "DC") ||
                (!isAmpSignal)) {
                painter.fillRect(x1, y1, 13, labelHeight, color);
                painter.drawImage(x1 + 2, y1, darkText ? saveSelectedBadge : saveBadge);
                xOffset += 11;
            }
        }

        if (channel->getOutputToTcp()) {
            painter.fillRect(x1 + xOffset, y1, 15, labelHeight, color);
            painter.drawImage(x1 + xOffset + 2, y1, darkText ? tcpSelectedBadge : tcpBadge);
            xOffset += 13;
        }

        if (channel->isStimEnabled()) {
            painter.fillRect(x1 + xOffset, y1, 10, labelHeight, color);
            painter.drawImage(x1 + xOffset + 1, y1, darkText ? stimSelectedBadge : stimBadge);
        }
    }
}

void MultiWaveformPlot::drawYScaleBar(QPainter &painter, QPoint cursor, int yPosition, int maxHeight,
                                      const DisplayedWaveform* waveform)
{    
    if (waveform->waveformType == RasterWaveform || waveform->waveformType == SupplyVoltageWaveform ||
        waveform->waveformType == WaveformDivider || waveform->waveformType == UnknownWaveform) return;

    int height = 0;
    QString label;
    QString filterText;

    switch (waveform->waveformType) {
    case AmplifierWaveform:
        filterText = waveform->waveName.section('|', 1, 1);
        if (filterText == "WIDE") {
            height = getYScaleHeightAndText(waveform, state->yScaleWide, maxHeight, label);
        } else if (filterText == "LOW") {
            height = getYScaleHeightAndText(waveform, state->yScaleLow, maxHeight, label);
        } else if (filterText == "HIGH") {
            height = getYScaleHeightAndText(waveform, state->yScaleHigh, maxHeight, label);
        } else if (filterText == "DC") {
            height = getYScaleHeightAndText(waveform, state->yScaleDC, maxHeight, label);
        }
        break;
    case AuxInputWaveform:
        height = getYScaleHeightAndText(waveform, state->yScaleAux, maxHeight, label);
        break;
    case BoardAdcWaveform:
    case BoardDacWaveform:
        height = getYScaleHeightAndText(waveform, state->yScaleAnalog, maxHeight, label);
        break;
    case BoardDigInWaveform:
    case BoardDigOutWaveform:
        height = getYScaleHeightAndText(waveform, nullptr, maxHeight, label);
        break;
    case SupplyVoltageWaveform:
    case RasterWaveform:
    case UnknownWaveform:
    case WaveformDivider:
        // None of these enum values should be reached.
        std::cerr << "Error: Unexpected WaveformType switch case reached.";
        return;
    }

    // Negative scale bar in some cases.
    if (cursor.y() > yPosition && !DisplayedWaveform::positiveOnlyType(waveform->waveformType)) {
        yPosition += height;
    }

    if (waveform->waveformType == BoardDigInWaveform || waveform->waveformType == BoardDigOutWaveform) {
        yPosition += height / 2;
    }

    const int HalfWidth = 2;
    const int XOffset = 12;
    int xPosition = cursor.x();
    int xLeft = regionWaveforms[state->labelWidth->getIndex()].left();
    int xRight = regionWaveforms[state->labelWidth->getIndex()].right();
    if (xPosition < xLeft + XOffset + 2 * HalfWidth || xPosition > xRight) return;

    int xValidData = waveformManager->getValidDataXPosition();
    if (state->rollMode->getValue()) {
        if (xPosition < xLeft + xValidData) return;
    } else {
        if (xPosition > xLeft + xValidData) return;
    }
    xPosition -= XOffset + HalfWidth;

    painter.fillRect(xPosition - 1, yPosition - height - 1, 3, height + 2, Qt::black);
    painter.fillRect(xPosition - HalfWidth - 1, yPosition - 1, 2 * HalfWidth + 3, 3, Qt::black);
    painter.fillRect(xPosition - HalfWidth - 1, yPosition - height - 1, 2 * HalfWidth + 3, 3, Qt::black);
    painter.setPen(Qt::white);
    painter.drawLine(QPoint(xPosition, yPosition), QPoint(xPosition, yPosition - height));
    painter.drawLine(QPoint(xPosition - HalfWidth, yPosition), QPoint(xPosition + HalfWidth, yPosition));
    painter.drawLine(QPoint(xPosition - HalfWidth, yPosition - height), QPoint(xPosition + HalfWidth, yPosition - height));
    const int XTextMax = 200;
    int yText = qMin(yPosition - (labelHeight + height) / 2, yPosition - labelHeight - 2);
    QRect boundingBox(xPosition - XTextMax, yText, XTextMax - 5, labelHeight + 2);
    painter.setPen(Qt::black);
    painter.drawText(boundingBox.adjusted(-1, -1, -1, -1), Qt::AlignRight, label);
    painter.drawText(boundingBox.adjusted( 1, -1,  1, -1), Qt::AlignRight, label);
    painter.drawText(boundingBox.adjusted(-1,  1, -1,  1), Qt::AlignRight, label);
    painter.drawText(boundingBox.adjusted( 1,  1,  1,  1), Qt::AlignRight, label);
    painter.setPen(Qt::white);
    painter.drawText(boundingBox, Qt::AlignRight, label);
}

int MultiWaveformPlot::getYScaleHeightAndText(const DisplayedWaveform* waveform, const DiscreteItemList* yScaleList,
                                              int maxHeight, QString& label)
{
    float yScaleFactor = waveformManager->getYScaleFactor(waveform->waveName);

    if (waveform->waveformType == BoardDigInWaveform || waveform->waveformType == BoardDigOutWaveform) {
        label = QString("1");
        return round(yScaleFactor);
    }

    double yValue = (double) maxHeight / yScaleFactor;
    int height = round(yScaleList->getNumericValue(0) * yScaleFactor);
    label = yScaleList->getDisplayValueString(0);

    // Adjust scale bar height to largest 'round number' below maxHeight.
    for (int i = yScaleList->numberOfItems() - 1; i >= 0; --i) {
        if (yScaleList->getNumericValue(i) <= yValue) {
            yValue = yScaleList->getNumericValue(i);
            height = round(yValue * yScaleFactor);
            label = yScaleList->getDisplayValueString(i);
            break;
        }
    }
    return height;
}

void MultiWaveformPlot::loadWaveformData(WaveformFifo* waveformFifo)
{
    for (int i = 0; i < pinnedList.size(); ++i) {
        if (pinnedList.at(i).isCurrentlyVisible) {
            waveformManager->loadNewData(waveformFifo, pinnedList.at(i).waveName);
        }
    }
    bool loadAllFilters = true;
    bool dcWaveformsPreset = state->getControllerTypeEnum() == ControllerStimRecord;
    for (int i = 0; i < displayList.size(); ++i) {
        if (displayList.at(i).isCurrentlyVisible && !displayList.at(i).isDivider()) {
        // Of interest for future improvements to high-efficiency data plotting:
        // inefficient way to make sure all data is plotted immediately after scrolling
        //if (!displayList.at(i).isDivider()) {
            QString waveName = displayList.at(i).waveName;
            if (!loadAllFilters || !waveName.contains('|')) {
                waveformManager->loadNewData(waveformFifo, waveName);
            } else {
                QString baseName = waveName.section('|', 0, 0);
                waveformManager->loadNewData(waveformFifo, baseName + "|WIDE");
                waveformManager->loadNewData(waveformFifo, baseName + "|LOW");
                waveformManager->loadNewData(waveformFifo, baseName + "|HIGH");
                waveformManager->loadNewData(waveformFifo, baseName + "|SPK");
                if (dcWaveformsPreset) {
                    waveformManager->loadNewData(waveformFifo, baseName + "|DC");
                }
            }
        }
    }
    // Note: repaint() seems to give slightly smoother animation than update(), but may cause "QWidget::repaint.
    // Recursive repaint detected" crash when columns are added.
//    repaint();
    update();
}

void MultiWaveformPlot::loadWaveformDataFromMemory(WaveformFifo* waveformFifo, int startTime, bool loadAll)
{
    for (int i = 0; i < pinnedList.size(); ++i) {
        if (pinnedList.at(i).isCurrentlyVisible || loadAll) {
            waveformManager->loadOldData(waveformFifo, pinnedList.at(i).waveName, startTime);
        }
    }
    for (int i = 0; i < displayList.size(); ++i) {
        if ((displayList.at(i).isCurrentlyVisible || loadAll) && !displayList.at(i).isDivider()) {
            waveformManager->loadOldData(waveformFifo, displayList.at(i).waveName, startTime);
        }
    }
    // Note: repaint() seems to give slightly smoother animation than update(), but may cause "QWidget::repaint.
    // Recursive repaint detected" crash when columns are added.
//    repaint();
    update();
}

void MultiWaveformPlot::loadWaveformDataDirect(QVector<QVector<QVector<double>>> &ampData, QVector<QVector<QString>> &ampChannelNames, QVector<QVector<double>> &auxInData)
{
    for (int i = 0; i < pinnedList.size(); ++i) {
        if (pinnedList.at(i).isCurrentlyVisible) {

            // Get this channel's name
            QString name = pinnedList.at(i).waveName;

            // Find this channel in ampDataNames to get its tream and channel index
            for (int stream = 0; stream < ampData.size(); ++stream) {
                for (int channel = 0; channel < ampData[stream].size(); ++channel) {
                    if (name == ampChannelNames[stream][channel]) {
                        waveformManager->loadDataDirect(ampData[stream][channel], ampChannelNames[stream][channel]); // Pass the 1-D vector of ampData specifically for this channel
                    }
                }
            }
        }
    }

    for (int i = 0; i < displayList.size(); ++i) {
        if (displayList.at(i).isCurrentlyVisible && !displayList.at(i).isDivider()) {

            // Get this channel's name
            QString name = displayList.at(i).waveName;

            // Find this channel in ampDataNames to get its stream and channel index
            for (int stream = 0; stream < ampData.size(); ++stream) {
                for (int channel = 0; channel < ampData[stream].size(); ++channel) {
                    if (name == ampChannelNames[stream][channel]) {
                        waveformManager->loadDataDirect(ampData[stream][channel], ampChannelNames[stream][channel]); // Pass the 1-D vector of ampData specifically for this channel
                    }
                }
            }
        }
    }

    // If testing aux ins, plot them as well
    if (state->testAuxIns->getValue()) {
        QVector<double> stretchedAuxData;
        for (int channel = 0; channel < auxInData.size(); channel++) {
            stretchedAuxData.resize(auxInData[channel].size() * 4);
            for (int sample = 0; sample < auxInData[channel].size(); sample++) {
                stretchedAuxData[4 * sample + 0] = auxInData[channel][sample];
                stretchedAuxData[4 * sample + 1] = auxInData[channel][sample];
                stretchedAuxData[4 * sample + 2] = auxInData[channel][sample];
                stretchedAuxData[4 * sample + 3] = auxInData[channel][sample];
            }
            waveformManager->loadDataDirect(stretchedAuxData, state->testingPort->getValueString() + "-AUX" + QString::number(channel + 1));
        }
    }
    update();
}

QStringList MultiWaveformPlot::getPinnedWaveNames() const
{
    QStringList pinnedWaveNames;
    for (int i = 0; i < pinnedList.size(); ++i) {
        pinnedWaveNames.append(pinnedList.at(i).waveName);
    }
    return pinnedWaveNames;
}

void MultiWaveformPlot::setPinnedWaveforms(const QStringList& pinnedWaveNames)
{
    pinnedList.clear();
    for (int i = 0; i < pinnedWaveNames.size(); ++i) {
        listManager->addPinnedWaveform(pinnedWaveNames.at(i));
    }
}

bool MultiWaveformPlot::groupIsPossible() const
{
    return listManager->selectedWaveformsCanBeGrouped(numFiltersDisplayed, arrangeByFilter);
}

bool MultiWaveformPlot::ungroupIsPossible() const
{
    return listManager->selectedWaveformsCanBeUngrouped();
}

void MultiWaveformPlot::enableSlot()
{
    state->signalSources->undoManager->pushStateToUndoStack();
    listManager->toggleSelectedWaveforms();
    state->forceUpdate();
}

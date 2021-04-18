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

#include "multiwaveformplot.h"
#include "scrollbar.h"

ScrollBar::ScrollBar(QRect position, int numPages, MultiWaveformPlot* parent_) :
    parent(parent_),
    upButtonActive(false),
    downButtonActive(false),
    barActive(false),
    barGrabPoint(0),
    barGrabPosition(0)
{
    scrollState.range = 100;
    scrollState.topPosition.resize(numPages, 0);
    scrollState.pageSize = 10;
    scrollState.stepSize = 1;
    scrollState.zoomFactor = 1.0;

    upArrow = QImage(":/images/scroll_up_badge.png");
    upArrowActive = QImage(":/images/scroll_up_active_badge.png");
    downArrow = QImage(":/images/scroll_down_badge.png");
    downArrowActive = QImage(":/images/scroll_down_active_badge.png");

    resize(position);
}

void ScrollBar::resize(QRect position)
{
    scrollFrame = position.adjusted(0, position.width() + 4, 0, -position.width() - 5);
    upButton = QRect(position.x(), position.y(), position.width(), position.width());
    downButton = QRect(position.x(), position.bottom() - position.width(), position.width(), position.width());
    scrollBar = scrollFrame.adjusted(2, 2, -1, 1);

    scrollBarBounds = scrollFrame;
    scrollBarBounds = scrollBarBounds.united(upButton);
    scrollBarBounds = scrollBarBounds.united(downButton);
    scrollBarBounds = scrollBarBounds.united(scrollBar);
}

int ScrollBar::getTopPosition() const
{
    return scrollState.topPosition[parent->currentPageIndex()];
}

void ScrollBar::setRange(int range_)
{
    int pageIndex = parent->currentPageIndex();
    scrollState.range = range_;
    if (scrollState.pageSize > scrollState.range) {
        scrollState.pageSize = scrollState.range;
    }
    setTopPosition(scrollState.topPosition[pageIndex], pageIndex);
}

void ScrollBar::setTopPosition(int topPosition_, int page)
{
    if (topPosition_ < 0) {
        scrollState.topPosition[page] = 0;
    } else if (topPosition_ > scrollState.range - scrollState.pageSize) {
        scrollState.topPosition[page] = scrollState.range - scrollState.pageSize;
    } else {
        scrollState.topPosition[page] = topPosition_;
    }
}

void ScrollBar::setPageSize(int pageSize_)
{
    int pageIndex = parent->currentPageIndex();
    if (pageSize_ < 1) {
        scrollState.pageSize = 1;
    } else if (pageSize_ <= scrollState.range) {
        scrollState.pageSize = pageSize_;
    } else {
        scrollState.pageSize = scrollState.range;
    }
    setTopPosition(scrollState.topPosition[pageIndex], pageIndex);

}

void ScrollBar::scroll(int delta)
{
    int pageIndex = parent->currentPageIndex();
    setTopPosition(scrollState.topPosition[pageIndex] + delta, pageIndex);
}

ScrollBarState ScrollBar::getState() const
{
    ScrollBarState savedState;
    savedState.range = scrollState.range;
    savedState.topPosition.resize(scrollState.topPosition.size());
    for (int i = 0; i < (int) savedState.topPosition.size(); ++i) {
        savedState.topPosition[i] = scrollState.topPosition[i];
    }
    savedState.pageSize = scrollState.pageSize;
    savedState.stepSize = scrollState.stepSize;
    savedState.zoomFactor = scrollState.zoomFactor;
    return savedState;
}

void ScrollBar::restoreState(const ScrollBarState& savedState)
{
    scrollState.range = scrollState.range;
    for (int i = 0; i < (int) savedState.topPosition.size(); ++i) {
        if (i < (int) scrollState.topPosition.size()) {
            scrollState.topPosition[i] = savedState.topPosition[i];
        }
    }
    scrollState.pageSize = savedState.pageSize;
    scrollState.stepSize = savedState.stepSize;
    scrollState.zoomFactor = savedState.zoomFactor;
}

// Format: Numbers separated by commas in the following order:
// "range,pageSize,stepSize,zoomFactor,topPosition[0],topPosition[1],topPosition[2],..."
QString ScrollBar::scrollBarStateToString(const ScrollBarState& state)
{
    QString stateString;
    const QString Separator = ",";
    stateString += QString::number(state.range);
    stateString += Separator;
    stateString += QString::number(state.pageSize);
    stateString += Separator;
    stateString += QString::number(state.stepSize);
    stateString += Separator;
    stateString += QString::number(state.zoomFactor);
    stateString += Separator;
    for (int i = 0; i < (int) state.topPosition.size(); ++i) {
        stateString += QString::number(state.topPosition[i]);
        if (i < (int) state.topPosition.size() - 1) {
            stateString += Separator;
        }
    }
    return stateString;
}

ScrollBarState ScrollBar::scrollBarStateFromString(const QString& stateString)
{
    ScrollBarState state;
    const QString Separator = ",";
    int numPages = stateString.count(Separator) - 3;
    state.topPosition.resize(numPages);
    state.range = stateString.section(Separator, 0, 0).toInt();
    state.pageSize = stateString.section(Separator, 1, 1).toInt();
    state.stepSize = stateString.section(Separator, 2, 2).toInt();
    state.zoomFactor = stateString.section(Separator, 3, 3).toDouble();
    for (int i = 0; i < numPages; ++i) {
        state.topPosition[i] = stateString.section(Separator, i + 4, i + 4).toInt();
    }
    return state;
}

void ScrollBar::adjustToNewNumberOfPages(int numPages)
{
    scrollState.topPosition.clear();
    scrollState.topPosition.resize(numPages, 0);
}

bool ScrollBar::handleMousePress(QPoint position)
{
    if (!scrollBarBounds.contains(position)) return false;
    int pageIndex = parent->currentPageIndex();

    int topPositionOld = scrollState.topPosition[pageIndex];
    if (scrollFrame.contains(position)) {
        if (scrollBar.contains(position)) {
            barActive = true;
            barGrabPoint = position.y();
            barGrabPosition = scrollState.topPosition[pageIndex];
        } else if (position.y() < scrollBar.top()) {
            scroll(-scrollState.pageSize);
        } else if (position.y() > scrollBar.bottom()) {
            scroll(scrollState.pageSize);
        }
    } else if (upButton.contains(position)) {
        upButtonActive = true;
        scroll(-scrollState.stepSize);
    } else if (downButton.contains(position)) {
        downButtonActive = true;
        scroll(scrollState.stepSize);
    }
    return (scrollState.topPosition[pageIndex] != topPositionOld);
}

bool ScrollBar::handleMouseRelease()
{
    bool barActiveOld = barActive;
    bool upButtonActiveOld = upButtonActive;
    bool downButtonActiveOld = downButtonActive;
    if (barActive) {
        barActive = false;
    } else {
        upButtonActive = false;
        downButtonActive = false;
    }
    return (barActive != barActiveOld) ||
            (upButtonActive != upButtonActiveOld) ||
            (downButtonActive != downButtonActiveOld);
}

bool ScrollBar::handleMouseMove(QPoint position)
{
    int pageIndex = parent->currentPageIndex();
    int topPositionOld = scrollState.topPosition[pageIndex];
    if (barActive) {
        double deltaY = position.y() - barGrabPoint;
        setTopPosition(barGrabPosition + scrollState.range * (deltaY / (double)scrollFrame.height()), pageIndex);
    } else if (upButtonActive) {
        if (!upButton.contains(position)) {
            upButtonActive = false;
        }
    } else if (downButtonActive) {
        if (!downButton.contains(position)) {
            downButtonActive = false;
        }
    }
    return (scrollState.topPosition[pageIndex] != topPositionOld);
}

bool ScrollBar::handleWheelEvent(int delta, bool shiftHeld, bool controlHeld)
{
    int pageIndex = parent->currentPageIndex();
    int topPositionOld = scrollState.topPosition[pageIndex];
    double zoomFactorOld = scrollState.zoomFactor;
    if (!shiftHeld && !controlHeld) {
        if (delta > 0) scroll(-ceil((double)scrollState.pageSize / 8.0));
        else if (delta < 0) scroll(ceil((double)scrollState.pageSize / 8.0));
    } else if (controlHeld & !shiftHeld) {
        if (delta > 0) zoomIn();
        else if (delta < 0) zoomOut();
    }
    return (scrollState.topPosition[pageIndex] != topPositionOld || scrollState.zoomFactor != zoomFactorOld);
}

bool ScrollBar::handleKeyPressEvent(int key)
{
    int pageIndex = parent->currentPageIndex();
    int topPositionOld = scrollState.topPosition[pageIndex];
    int pageSizeOld = scrollState.pageSize;

    switch (key) {
    case Qt::Key_PageUp:
        scroll(-scrollState.pageSize);
        break;
    case Qt::Key_PageDown:
        scroll(scrollState.pageSize);
        break;
    case Qt::Key_Home:
        setTopPosition(0, pageIndex);
        break;
    case Qt::Key_End:
        setTopPosition(scrollState.range - scrollState.pageSize, pageIndex);
        break;
    }
    return (scrollState.topPosition[pageIndex] != topPositionOld || scrollState.pageSize != pageSizeOld);
}

void ScrollBar::draw(QPainter &painter)
{
    int pageIndex = parent->currentPageIndex();
    painter.setPen(scrollBarColor);

    if (upButtonActive) {
        painter.drawImage(upButton.topLeft(), upArrowActive);
    } else {
        painter.drawImage(upButton.topLeft(), upArrow);
    }

    if (downButtonActive) {
        painter.drawImage(downButton.topLeft(), downArrowActive);
    } else {
        painter.drawImage(downButton.topLeft(), downArrow);
    }

    painter.setPen(scrollBarColor);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(scrollFrame);

    scrollBar = scrollFrame.adjusted(2, 2, -1, -1);
    scrollBar.moveTop(scrollBar.top() +
                      ((double)scrollState.topPosition[pageIndex] / (double)scrollState.range) * (scrollBar.height() + 1));
    scrollBar.setHeight(((double)scrollState.pageSize / (double)scrollState.range) * scrollBar.height());
    if (scrollState.topPosition[pageIndex] == scrollState.range - scrollState.pageSize) {
        scrollBar.moveTop(scrollFrame.bottom() - scrollBar.height());
    }
    painter.fillRect(scrollBar, barActive ? scrollBarActiveColor : scrollBarColor);
}

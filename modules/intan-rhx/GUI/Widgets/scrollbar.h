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

#ifndef SCROLLBAR_H
#define SCROLLBAR_H

#include <vector>
#include <QtWidgets>
#include "systemstate.h"

using namespace std;

class MultiWaveformPlot;

struct ScrollBarState
{
    int range;                // Scroll bar operates from 0 to this value.
    vector<int> topPosition;  // Current position of scroll bar on each page; always between 0 and (range - pageSize)
    int pageSize;             // Size of one page.  Must be <= range.  Sets scroll bar size.  Increment of movement for page up/down.
    int stepSize;             // Increment of movement from up/down button or cursor up/down
    double zoomFactor;        // Number from 1.0 to ZoomLimit used to expand display
};

class ScrollBar
{
public:
    ScrollBar(QRect position, int numPages, MultiWaveformPlot* parent_);

    void resize(QRect position);

    int getTopPosition() const;
    inline int getStepSize() const { return scrollState.stepSize; }
    inline double getZoomFactor() const { return scrollState.zoomFactor; }

    ScrollBarState getState() const;
    void restoreState(const ScrollBarState& savedState);
    QString getStateString() const { return scrollBarStateToString(getState()); }
    void restoreStateFromString(const QString& stateString) { restoreState(scrollBarStateFromString(stateString)); }
    void adjustToNewNumberOfPages(int numPages);

    void setRange(int range_);
    void setPageSize(int pageSize_);
    inline void setStepSize(int stepSize_) { scrollState.stepSize = stepSize_; }
    inline void zoomIn() { scrollState.zoomFactor = qMin(scrollState.zoomFactor * ZoomStepMultiple, ZoomLimit); }
    inline void zoomOut() { scrollState.zoomFactor = qMax(scrollState.zoomFactor / ZoomStepMultiple, 1.0); }

    void scroll(int delta);

    // Handle mouse and keypress events, and return true if display needs to be updated.
    bool handleMousePress(QPoint position);
    bool handleMouseRelease();
    bool handleMouseMove(QPoint position);
    bool handleWheelEvent(int delta, bool shiftHeld, bool controlHeld);
    bool handleKeyPressEvent(int key);

    const QColor scrollBarColor = Qt::gray;
    const QColor scrollBarActiveColor = Qt::white;

    void draw(QPainter &painter);

private:
    MultiWaveformPlot* parent;

    QImage upArrow;
    QImage upArrowActive;
    QImage downArrow;
    QImage downArrowActive;

    // Long-term state
    ScrollBarState scrollState;

    // Short-term state
    bool upButtonActive;
    bool downButtonActive;
    bool barActive;
    int barGrabPoint;
    int barGrabPosition;

    // Clickable regions
    QRect scrollFrame;
    QRect scrollBar;
    QRect upButton;
    QRect downButton;
    QRect scrollBarBounds;

    const double ZoomLimit = 50.0;
    const double ZoomStepMultiple = 1.5;

    void setTopPosition(int topPosition_, int page);
    static QString scrollBarStateToString(const ScrollBarState& state);
    static ScrollBarState scrollBarStateFromString(const QString& stateString);
};

#endif // SCROLLBAR_H

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

#ifndef MULTIWAVEFORMPLOT_H
#define MULTIWAVEFORMPLOT_H

//#define MINIMUM_X_SIZE_MULTIWAVEFORM_PLOT 180
// Changed to avoid crashing on various smaller laptops
//#define MINIMUM_X_SIZE_MULTIWAVEFORM_PLOT 250
// Changed again to avoid crashing on another small laptop
#define MINIMUM_X_SIZE_MULTIWAVEFORM_PLOT 350
#define MINIMUM_Y_SIZE_MULTIWAVEFORM_PLOT 400

#include <QtWidgets>
#include "waveformdisplaymanager.h"
#include "displaylistmanager.h"
#include "scrollbar.h"
#include "systemstate.h"
#include "displayedwaveform.h"

const QColor DisabledColor = QColor(80, 80, 80);

struct DragStateData {
    bool dragging;
    bool fromPinned;
};

class WaveformFifo;
class ControllerInterface;
class WaveformDisplayColumn;
class WaveformSelectDialog;

class MultiWaveformPlot : public QWidget
{
    Q_OBJECT
public:
    explicit MultiWaveformPlot(int columnIndex_, WaveformDisplayManager* waveformManager_,
                               ControllerInterface* controllerInterface_, SystemState *state_,
                               WaveformDisplayColumn* parent_);
    ~MultiWaveformPlot();

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

    void loadWaveformData(WaveformFifo* waveformFifo);
    void loadWaveformDataFromMemory(WaveformFifo* waveformFifo, int startTime, bool loadAll = false);
    void loadWaveformDataDirect(QVector<QVector<QVector<double>>> &ampData, QVector<QVector<QString>> &ampChannelNames, QVector<QVector<double>> &auxInData);
    inline void updateNow() { update(); }

    void enableSelectedWaveforms(bool enable) { listManager->enableSelectedWaveforms(enable); }

    inline void setIndex(int columnIndex_) { columnIndex = columnIndex_; }
    inline void showPinnedWaveforms(bool enable) { showPinned = enable; update(); }

    QStringList getPinnedWaveNames() const;
    void setPinnedWaveforms(const QStringList& pinnedWaveNames);
    void unpinAllWaveforms() { listManager->unpinAllWaveforms(); }

    bool groupIsPossible() const;
    bool ungroupIsPossible() const;
    void group();
    void ungroup();

    inline ScrollBarState getScrollBarState() const { return scrollBar->getState(); }
    inline void restoreScrollBarState(const ScrollBarState& state) { scrollBar->restoreState(state); }
    inline QString getScrollBarStateString() const { return scrollBar->getStateString(); }
    inline void restoreScrollBarStateFromString(const QString& stateString) { scrollBar->restoreStateFromString(stateString); }
    int currentPageIndex() const;
    void adjustToNewNumberOfPorts(int numPorts) { scrollBar->adjustToNewNumberOfPages(numPorts); }

    void requestRedraw();
    void requestReset();

public slots:
    void updateFromState();
    void openWaveformSelectDialog();
    void addPinnedWaveform(const QString& waveName) { state->signalSources->undoManager->pushStateToUndoStack();
                                                      listManager->addPinnedWaveform(waveName); }
    void increaseSpacing() { scrollBar->zoomIn(); update(); }
    void decreaseSpacing() { scrollBar->zoomOut(); update(); }

private slots:
    void enableSlot();

protected:
    bool event(QEvent* event) override;  // Used for implementing custom tool tips.
//    void contextMenuEvent(QContextMenuEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    MultiWaveformPlot(const MultiWaveformPlot&);  // Copying not allowed.
    MultiWaveformPlot& operator=(const MultiWaveformPlot&);  // Copying not allowed.

    int columnIndex;
    WaveformDisplayManager* waveformManager;
    ControllerInterface* controllerInterface;
    SystemState* state;
    WaveformDisplayColumn* parent;

    ScrollBar* scrollBar;

    QImage image; // Paint target for traditional plotting
    QPixmap corePixmap; // Paint target for experimental plotting - CORE includes only changes that should be retained across multiple paintEvents
    QPixmap fullPixmap; // Paint target for experimental plotting - FULL includes CORE, plus transient changes that should only be shown currently, not retained in the future

    QImage saveBadge;
    QImage stimBadge;
    QImage tcpBadge;
    QImage saveSelectedBadge;
    QImage stimSelectedBadge;
    QImage tcpSelectedBadge;

    QImage unpinBadge;

    QList<DisplayedWaveform> displayList;
    QList<DisplayedWaveform> pinnedList;
    int pinnedYDivider;
    WaveIndex hoverWaveIndex;
    bool showPinned;
    DisplayListManager* listManager;

    int numFiltersDisplayed;
    bool arrangeByFilter;

    WaveformSelectDialog* waveformSelectDialog;

    // Context menu
//    QMenu* contextMenu;
//    QAction* enableAction;

    // GUI short-term state
    DragStateData dragState;
    bool mouseButtonDown;
    bool outOfBoundsClick;
    QPoint lastMousePos;
    QPoint clickPoint;
    QPoint dragDelta;

    // Label text parameters
    QFont* labelFont;
    QFontMetrics* labelFontMetrics;
    int labelHeight;
    std::vector<int> labelWidth;
    std::vector<int> labelWidthFilter;
    int labelWidthIndexOld;

    // Screen regions used for plotting and mouse calculations
    std::vector<QRect> regionWaveforms;
    std::vector<QRect> regionLabels;
    std::vector<QRect> regionTimeAxis;
    QRect regionAboveLabels;
    QRect regionBelowLabels;
    QRect regionScrollBar;
    QRect regionUnpinSymbols;

    static const int YExtra = 5;

    int timeAxisStep() const;
    void drawVerticalTimeLines(QPainter &painter, int xPosition, int xCursor = -1);
    void drawTriggerLine(QPainter &painter, int xPosition);
    void drawTimeAxis(QPainter &painter, int tScaleInMsec, QPoint position);
    void drawBetweenWaveformMarker(QPainter &painter, int xPosition);
    void drawWaveformLabel(QPainter &painter, const QString& name, const Channel* channel, QPoint position, QColor color,
                           QColor textColor);
    void drawYScaleBar(QPainter &painter, QPoint cursor, int yPosition, int maxHeight, const DisplayedWaveform* waveform);
    int getYScaleHeightAndText(const DisplayedWaveform* waveform, const DiscreteItemList* yScaleList, int maxHeight,
                               QString& label);

    void calculateScreenRegions();
    void calculateDisplayYCoords();

    WaveIndex findSelectedWaveform(int y) const;

    QColor adjustedColor(const DisplayedWaveform& waveform, bool hoverSelect = false) const;
    int spreadAroundDraggingTarget(int y, int hoverIndex, int index) const;

    int current_tScale;
    int current_yScaleWide;
    int current_yScaleLow;
    int current_yScaleHigh;
    int current_yScaleDC;
    int current_yScaleAux;
    int current_yScaleAnalog;

    int prev_tScale;
    int prev_yScaleWide;
    int prev_yScaleLow;
    int prev_yScaleHigh;
    int prev_yScaleDC;
    int prev_yScaleAux;
    int prev_yScaleAnalog;
};

#endif // MULTIWAVEFORMPLOT_H

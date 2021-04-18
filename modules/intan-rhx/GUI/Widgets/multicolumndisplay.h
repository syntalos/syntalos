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

#ifndef MULTICOLUMNDISPLAY_H
#define MULTICOLUMNDISPLAY_H

#include <QtWidgets>
#include "waveformdisplaymanager.h"
#include "waveformdisplaycolumn.h"

class MultiColumnDisplay : public QWidget
{
    Q_OBJECT
public:
    explicit MultiColumnDisplay(ControllerInterface* controllerInterface_, SystemState *state_, QWidget *parent = nullptr);
    ~MultiColumnDisplay();

    inline int numColumns() const { return (int) displayColumns.size(); }
    int numVisibleColumns() const;
    inline int maxNumColumns() const { return MaxNumColumns; }
    QString getSelectedPort(int column) const { return displayColumns[column]->getSelectedPort(); }
    void setSelectedPort(int column, const QString& portName) { displayColumns[column]->setSelectedPort(portName); }

    bool addColumn(int position);
    bool deleteColumn(int position, bool silent = false);
    bool moveColumnLeft(int position);
    bool moveColumnRight(int position);

    void updateHideAndDeleteButtons();

    void setWaveformWidth(int width);

    YScaleUsed loadWaveformData(WaveformFifo* waveformFifo);
    YScaleUsed loadWaveformDataFromMemory(WaveformFifo* waveformFifo, int startTime, bool loadAll = false);
    void reset();

    inline int getSamplesPerRefresh() const { return waveformManager->getSamplesPerRefresh(); }
    int getMaxSamplesPerRefresh() const;

    inline int getSamplesPerFullRefresh() const { return waveformManager->getSamplesPerFullRefresh(); }

    bool groupIsPossible() const;
    bool ungroupIsPossible() const;

    void enableSelectedChannels(bool enabled);
    void group();
    void ungroup();

    void updateForRun();
    void updateForLoad();
    void updateForStop();
    void updateForRescan();

    void addWaveforms();

    QStringList getPinnedWaveNames(int column) const { return displayColumns[column]->getPinnedWaveNames(); }
    void setPinnedWaveforms(int column, const QStringList& pinnedWaveNames) { displayColumns[column]->setPinnedWaveforms(pinnedWaveNames); }

    bool arePinnedShown(int column) const { return displayColumns[column]->arePinnedShown(); }
    void setShowPinned(int column, bool showPinned);

    bool isColumnVisible(int column) const { return displayColumns[column]->isColumnVisible();}
    void setColumnVisible(int column, bool visible);

    inline ScrollBarState getScrollBarState(int column) const { return displayColumns[column]->getScrollBarState(); }
    inline void restoreScrollBarState(int column, const ScrollBarState& state) { displayColumns[column]->restoreScrollBarState(state); }

    void updatePortSelectionBoxes();

    QString getDisplaySettingsString() const;
    void restoreFromDisplaySettingsString(const QString& settings);

public slots:
    void updateFromState();

private:
    ControllerInterface* controllerInterface;
    SystemState* state;

    WaveformDisplayManager* waveformManager;
    vector<int> numRefreshZones;

    static const int MaxNumColumns = 10;
    QList<WaveformDisplayColumn*> displayColumns;

    int tScaleFormerIndex;
    bool rollModeFormerValue;

    void updateColumnIndices();
    void updateLayout();
};

#endif // MULTICOLUMNDISPLAY_H

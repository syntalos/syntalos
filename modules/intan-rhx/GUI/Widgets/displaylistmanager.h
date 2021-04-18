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

#ifndef DISPLAYLISTMANAGER_H
#define DISPLAYLISTMANAGER_H

#include "systemstate.h"
#include "displayedwaveform.h"

using namespace std;

const int MaxNumWaveformsInGroup = 4;

struct WaveIndex {
    int index;
    bool inPinned;
};

class DisplayListManager
{
public:
    DisplayListManager(QList<DisplayedWaveform>& displayList_, QList<DisplayedWaveform>& pinnedList_, SystemState* state_);

    void populateDisplayList(const QStringList& displayWaveforms, const QStringList& auxInputWaveforms,
                             const QStringList& supplyVoltageWaveforms, const QStringList& baseWaveforms);
    void addPinnedWaveform(const QString& waveName);
    void pinSelectedWaveforms();
    void unpinSelectedWaveforms();
    void unpinAllWaveforms();
    bool groupSelectedWaveforms(bool group, int numFiltersDisplayed, bool arrangeByFilter);
    bool selectedWaveformsCanBeGrouped(int numFiltersDisplayed, bool arrangeByFilter) const;
    bool selectedWaveformsCanBeUngrouped() const;
    int findSelectedWaveform(const QList<DisplayedWaveform>& list, int y) const;

    DisplayedWaveform* displayedWaveform(WaveIndex waveIndex);
    DisplayedWaveform* displayedWaveform(int index, bool inPinned);

    int numDisplayedWaveforms(bool inPinned = false) const;
    int numSelectedWaveforms(bool inPinned = false) const;
    int numSelectedAmplifierChannels(int numFiltersDisplayed, bool arrangeByFilter) const;
    int numSelectedVisibleAmplifierChannels(int numFiltersDisplayed, bool arrangeByFilter) const;
    int sectionIDOfSelectedWaveforms() const;
    bool selectedWaveformsAreAmplifiers() const;

    void enableSelectedWaveforms(bool enable);
    void toggleSelectedWaveforms();

    bool isValidDragTarget(WaveIndex target, int numFiltersDisplayed, bool arrangeByFilter) const;

    bool moveSelectedWaveforms(QList<DisplayedWaveform> &list, int position);
    bool moveSelectedWaveformsArrangedByFilter(QList<DisplayedWaveform> &list, int position, int numFilters);
    int numWaveformsInFirstSection(const QList<DisplayedWaveform> &list) const;

    int calculateYCoords(QList<DisplayedWaveform>& list, int y, int labelHeight, double zoomFactor, int& minSpacing);
    void addYOffset(QList<DisplayedWaveform>& list, int yOffset);

    void selectSingleWaveform(WaveIndex waveIndex);
    void selectAdjacentWaveforms(WaveIndex waveIndex, int sectionExtent = -1);
    void selectNonAdjacentWaveforms(WaveIndex waveIndex, int numFiltersDisplayed, bool arrangeByFilter);
    bool selectNextWaveform() { return selectNextOrPreviousWaveform(true); }
    bool selectPreviousWaveform() { return selectNextOrPreviousWaveform(false); }

    vector<bool> selectionRecord() const;
    bool selectionRecordsAreEqual(const vector<bool>& a, const vector<bool>& b) const;

    // These functions update the system state intelligently, and automatically trigger a state change if anything changed.
    void updateOrderInState(const QString& portName, int numFiltersDisplayed, bool arrangeByFilter);

private:
    QList<DisplayedWaveform>& displayList;
    QList<DisplayedWaveform>& pinnedList;
    SystemState* state;

    void cleanUpDisplayList();

    DisplayedWaveform displayedWaveformFromName(const QString& waveName) const;
    DisplayedWaveform waveformDivider() const { return DisplayedWaveform("", WaveformDivider, nullptr); }

    bool selectedWaveformsAreEnabled() const;

    // Must manually trigger a state change after calling these functions.
    void deselectAllWaveforms();
    void selectWaveform(const QString& waveName, bool select = true);
    bool selectNextOrPreviousWaveform(bool selectNext);
};

#endif // DISPLAYLISTMANAGER_H

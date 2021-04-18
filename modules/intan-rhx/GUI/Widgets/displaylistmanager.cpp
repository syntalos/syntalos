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

#include <iostream>
#include <limits>
#include "displaylistmanager.h"

DisplayListManager::DisplayListManager(QList<DisplayedWaveform>& displayList_, QList<DisplayedWaveform>& pinnedList_,
                                       SystemState* state_) :
    displayList(displayList_),
    pinnedList(pinnedList_),
    state(state_)
{
}

void DisplayListManager::populateDisplayList(const QStringList& displayWaveforms, const QStringList& auxInputWaveforms,
                                             const QStringList& supplyVoltageWaveforms, const QStringList& baseWaveforms)
{
    displayList.clear();

    for (int i = 0; i < displayWaveforms.size(); ++i) {
        DisplayedWaveform wave = displayedWaveformFromName(displayWaveforms.at(i));
        displayList.append(wave);
    }

    displayList.append(waveformDivider());

    if (state->showAuxInputs->getValue()) {
        for (int i = 0; i < auxInputWaveforms.size(); ++i) {
            DisplayedWaveform wave = displayedWaveformFromName(auxInputWaveforms.at(i));
            displayList.append(wave);
        }
    }

    displayList.append(waveformDivider());

    if (state->showSupplyVoltages->getValue()) {
        for (int i = 0; i < supplyVoltageWaveforms.size(); ++i) {
            DisplayedWaveform wave = displayedWaveformFromName(supplyVoltageWaveforms.at(i));
            displayList.append(wave);
        }
    }

    displayList.append(waveformDivider());

    for (int i = 0; i < baseWaveforms.size(); ++i) {
        DisplayedWaveform wave = displayedWaveformFromName(baseWaveforms.at(i));
        displayList.append(wave);
    }

    cleanUpDisplayList();  // Remove redundant waveform dividers and calculate display regions.
}

DisplayedWaveform DisplayListManager::displayedWaveformFromName(const QString& waveName) const
{
    if (waveName == "/") return waveformDivider();

    Channel* channel = state->signalSources->channelByName(waveName);
    if (!channel) return DisplayedWaveform("", UnknownWaveform, nullptr);
    WaveformType waveType = DisplayedWaveform::translateSignalTypeToWaveformType(channel->getSignalType());
    if (waveName.right(4) == "|SPK") waveType = RasterWaveform;
    DisplayedWaveform dw(waveName, waveType, channel);
    return dw;
}

void DisplayListManager::cleanUpDisplayList()
{
    if (displayList.isEmpty()) return;

    // Remove waveform dividers at beginning or end of list.
    while (displayList.first().isDivider()) {
        displayList.removeFirst();
        if (displayList.isEmpty()) return;
    }
    while (displayList.last().isDivider()) {
        displayList.removeLast();
        if (displayList.isEmpty()) return;
    }

    // Remove duplicate waveform dividers.
    for (int i = 1; i < displayList.size() - 1; ++i) {
        if (displayList.at(i).isDivider() && displayList.at(i + 1).isDivider()) {
            displayList.removeAt(i + 1);
            --i;
        }
    }

    // Calculate display sections.
    int section = 1;
    for (int i = 0; i < displayList.size(); ++i) {
        if (displayList.at(i).isDivider()) {
            displayList[i].sectionID = DividerSectionID;
            ++section;
        } else {
            displayList[i].sectionID = section;
        }
    }
}

void DisplayListManager::addPinnedWaveform(const QString& waveName)
{
    Channel* channel = state->signalSources->channelByName(waveName);
    if (channel) {
        pinnedList.append(displayedWaveformFromName(waveName));
    }
}

void DisplayListManager::pinSelectedWaveforms()
{
    for (int i = 0; i < displayList.size(); ++i) {
        if (displayList.at(i).isSelected()) {
            // Don't pin channels if they are disabled and invisible.
            if (displayList.at(i).isEnabled() || state->showDisabledChannels->getValue()) {
                addPinnedWaveform(displayList.at(i).waveName);
            }
        }
    }
}

void DisplayListManager::unpinSelectedWaveforms()
{
    for (int i = 0; i < pinnedList.size(); ++i) {
        if (pinnedList.at(i).isSelected()) {
            pinnedList.removeAt(i);
            --i;
        }
    }
}

void DisplayListManager::unpinAllWaveforms()
{
    pinnedList.clear();
}

// This function returns true if the order of waveforms have been changed, requiring an update of userOrder in SignalSources.
bool DisplayListManager::groupSelectedWaveforms(bool group, int numFiltersDisplayed, bool arrangeByFilter)
{
    if (displayList.isEmpty()) return false;

    if (!group) {  // Assign group ID of zero to ungroup.
        for (int i = 0; i < displayList.size(); ++i) {
            if (displayList.at(i).isSelected()) {
                Channel* channel = displayList.at(i).channel;
                if (channel) channel->setGroupID(0);
            }
        }
        return false;
    }

    if (!selectedWaveformsCanBeGrouped(numFiltersDisplayed, arrangeByFilter)) return false;

    // There may be hidden disabled channels mixed in with selected waveforms.  Move them to the end of selection.
    bool reordered = false;
    if (!state->showDisabledChannels->getValue()) {
        int firstSelected = 0;
        while (!displayList.at(firstSelected).isSelected() || !displayList.at(firstSelected).isEnabled()) ++firstSelected;

        int lastSelected;
        if (displayList.at(0).isAmplifier()) {
            lastSelected = numWaveformsInFirstSection(displayList) - 1;
            if (firstSelected > lastSelected) lastSelected = displayList.size() - 1;  // Non-amp selection.
        } else {
            lastSelected = displayList.size() - 1;
        }
        while (!displayList.at(lastSelected).isSelected() || !displayList.at(lastSelected).isEnabled()) --lastSelected;

        // Move all intermediate disabled channels to end of selection.
        for (int i = lastSelected - 1; i >= firstSelected; --i) {
            if (!displayList.at(i).isEnabled()) {
                Channel* channel = displayList.at(i).channel;
                if (channel) channel->setSelected(false);  // Disable invisible channels.
                displayList.move(i, lastSelected);
                reordered = true;
            }
        }
    }

    // Assign all grouped channels the same group ID and color.
    int groupID = state->signalSources->getNextAvailableGroupID();

    QColor color;
    bool colorFound = false;
    for (int i = 0; i < displayList.size(); ++i) {
        if (displayList.at(i).isSelected() && displayList.at(i).isEnabled()) {
            if (!colorFound) {
                color = displayList.at(i).getColor();
                colorFound = true;
            }
            Channel* channel = displayList.at(i).channel;
            if (channel) {
                channel->setGroupID(groupID);
                channel->setColor(color);
            }
        }
    }

    return reordered;
}

bool DisplayListManager::selectedWaveformsCanBeGrouped(int numFiltersDisplayed, bool arrangeByFilter) const
{
    if (numSelectedAmplifierChannels(numFiltersDisplayed, arrangeByFilter) < 2) {
        return false;  // At least two waveforms must be selected to be grouped.
    }
    if (numSelectedVisibleAmplifierChannels(numFiltersDisplayed, arrangeByFilter) > MaxNumWaveformsInGroup) {
        return false;   // Cannot group together too many waveforms.
    }

    bool selectionStartReached = false;
    bool selectionEndReached = false;
    bool visibleSignalsFoundPastEnd = false;
    int numWavesInFirstSection = numWaveformsInFirstSection(displayList);
    int end = displayList.size();
    for (int i = 0; i < end; ++i) {
        if (displayList.at(i).isSelected()) {
            if (!selectionStartReached) {
                selectionStartReached = true;
                if (displayList.at(i).isAmplifier()) {
                    end = numWavesInFirstSection;
                }
            }
            if (!displayList.at(i).isEnabled() && state->showDisabledChannels->getValue()) {
//                cout << "Disabled channels cannot be grouped." << EndOfLine;
                return false;  // Disabled channels cannot be grouped.
            }
            if (selectionEndReached && visibleSignalsFoundPastEnd) {
//                cout << "Non-contiguous waveforms cannot be grouped." << EndOfLine;
                return false;  // Non-contiguous waveforms cannot be grouped.
            }
            if (!displayList.at(i).isAmplifier()) {
//                cout << "Only amplifier waveforms can be grouped. " << i << " " << displayList.at(i).waveformType << EndOfLine;
                return false;  // Only amplifier waveforms can be grouped.
            }
            if (displayList.at(i).getGroupID() != 0) {
//                cout << "Waveforms already in groups cannot be grouped." << EndOfLine;
                return false;  // Waveforms already in groups cannot be grouped.
            }
        } else {
            if (selectionStartReached) selectionEndReached = true;
            if (selectionEndReached) {
                if (displayList.at(i).isEnabled() || state->showDisabledChannels->getValue()) {
                    visibleSignalsFoundPastEnd = true;
                }
            }
        }
    }
    return true;
}

bool DisplayListManager::selectedWaveformsCanBeUngrouped() const
{
    for (int i = 0; i < displayList.size(); ++i) {
        if (displayList.at(i).isSelected()) {
            if (displayList.at(i).getGroupID() != 0) return true;  // If any selections belong to a group, ungroup is possible.
        }
    }
    return false;
}

// If cursor is near a waveform, this function returns the waveform index (an integer between 0 and N for N+1 waveforms).
// If cursor is above waveform 0, this function returns -1.
// If cursor is between waveform N and N+1, this function returns -(N+2).
// If cursor is below waveform N, this function returns -(N+2).
int DisplayListManager::findSelectedWaveform(const QList<DisplayedWaveform>& list, int y) const
{
    if (list.isEmpty()) return -1;
    int length = list.size();

    for (int i = 0; i < length; ++i) {
        if (y >= list[i].yTop && y <= list[i].yBottom) return i;
    }
    if (y < list[0].yTop) return -1;
    if (y > list[length - 1].yBottom) return -(length + 1);
    for (int i = 0; i < length - 1; ++i) {
        if (y > list[i].yBottom && y < list[i + 1].yTop) return -(i + 2);
    }
    cerr << "DisplayListManager::findSelectedWaveform: This line should never be reached." << '\n';
    return 0;  // This line should never be reached - included so the compiler doesn't complain.
}

DisplayedWaveform* DisplayListManager::displayedWaveform(WaveIndex waveIndex)
{
    if (waveIndex.inPinned) {
        return &pinnedList[waveIndex.index];
    } else {
        return &displayList[waveIndex.index];
    }
}

DisplayedWaveform* DisplayListManager::displayedWaveform(int index, bool inPinned)
{
    if (inPinned) {
        return &pinnedList[index];
    } else {
        return &displayList[index];
    }
}

int DisplayListManager::numDisplayedWaveforms(bool inPinned) const
{
    if (inPinned) {
        return pinnedList.size();
    } else {
        return displayList.size();
    }
}

int DisplayListManager::numSelectedWaveforms(bool inPinned) const
{
    int numSelected = 0;
    if (inPinned) {
        for (int i = 0; i < pinnedList.size(); ++i) {
            if (pinnedList.at(i).isSelected()) ++numSelected;
        }
    } else {
        for (int i = 0; i < displayList.size(); ++i) {
            if (displayList.at(i).isSelected()) ++numSelected;
        }
    }
    return numSelected;
}

int DisplayListManager::numSelectedAmplifierChannels(int numFiltersDisplayed, bool arrangeByFilter) const
{
    if (displayList.isEmpty()) return 0;
    if (!displayList[0].isAmplifier()) return 0;

    int numSelected = 0;
    int waveformsInFirstSection = numWaveformsInFirstSection(displayList);
    for (int i = 0; i < waveformsInFirstSection; ++i) {
        if (displayList.at(i).isSelected()) ++numSelected;
    }
    if (!arrangeByFilter) {
        numSelected /= numFiltersDisplayed;
    }
    return numSelected;
}

int DisplayListManager::numSelectedVisibleAmplifierChannels(int numFiltersDisplayed, bool arrangeByFilter) const
{
    if (displayList.isEmpty()) return 0;
    if (!displayList.at(0).isAmplifier()) return 0;

    bool showDisabled = state->showDisabledChannels->getValue();
    int numSelected = 0;
    int waveformsInFirstSection = numWaveformsInFirstSection(displayList);
    for (int i = 0; i < waveformsInFirstSection; ++i) {
        if (displayList.at(i).isSelected()) {
            if (displayList.at(i).isEnabled() || showDisabled) ++numSelected;
        }
    }
    if (!arrangeByFilter) numSelected /= numFiltersDisplayed;
    return numSelected;
}

// This function assumes that all selected waveforms are in the same section, if multiple waveforms are selected.
int DisplayListManager::sectionIDOfSelectedWaveforms() const
{
    for (int i = 0; i < displayList.size(); ++i) {
        if (displayList.at(i).isSelected()) return displayList.at(i).sectionID;
    }
    for (int i = 0; i < pinnedList.size(); ++i) {
        if (pinnedList.at(i).isSelected()) return pinnedList.at(i).sectionID;
    }
    return DividerSectionID;  // No waveforms are selected.
}

// This function assumes that all selected waveforms are the same type, if multiple waveforms are selected.
bool DisplayListManager::selectedWaveformsAreAmplifiers() const
{
    for (int i = 0; i < displayList.size(); ++i) {
        if (displayList.at(i).isSelected()) return displayList.at(i).isAmplifier();
    }
    for (int i = 0; i < pinnedList.size(); ++i) {
        if (pinnedList.at(i).isSelected()) return pinnedList.at(i).isAmplifier();
    }
    return false;
}

// This function assumes that all selected waveforms are enabled or all are disabled, if multiple waveforms are selected.
bool DisplayListManager::selectedWaveformsAreEnabled() const
{
    for (int i = 0; i < displayList.size(); ++i) {
        if (displayList.at(i).isSelected()) return displayList.at(i).isEnabled();
    }
    for (int i = 0; i < pinnedList.size(); ++i) {
        if (pinnedList.at(i).isSelected()) return pinnedList.at(i).isEnabled();
    }
    return false;  // No waveforms are selected.
}

void DisplayListManager::enableSelectedWaveforms(bool enable)
{
    for (int i = 0; i < pinnedList.size(); ++i) {
        if (pinnedList.at(i).isSelected()) {
            pinnedList[i].setEnabled(enable);
        }
    }
    for (int i = 0; i < displayList.size(); ++i) {
        if (displayList.at(i).isSelected()) {
            displayList[i].setEnabled(enable);
        }
    }
}

void DisplayListManager::toggleSelectedWaveforms()
{
    enableSelectedWaveforms(!selectedWaveformsAreEnabled());
}

// Move all selected waveforms to the spot just after position.  In a list of N waveforms indexed 0 to N-1,
// position can have a value between -1 and N-1.  Use position = -1 to insert at top of list.
bool DisplayListManager::moveSelectedWaveforms(QList<DisplayedWaveform> &list, int position)
{
    bool realMove = false;
    int target = position;
    for (int i = target; i >= 0; --i) {
        if (list[i].isSelected()) {
            if (i != target) realMove = true;
            list.move(i, target--);
        }
    }
    target = position + 1;
    for (int i = target; i < list.size(); ++i) {
        if (list[i].isSelected()) {
            if (i != target) realMove = true;
            list.move(i, target++);
        }
    }
    return realMove;
}

// Move all selected waveforms to the spot just after position, accounting for multiple sections displaying numFilters
// different filters.
// In a list of N waveforms indexed 0 to N-1, position can have a value between -1 and N-1.  Use position = -1 to insert
// at top of list.
bool DisplayListManager::moveSelectedWaveformsArrangedByFilter(QList<DisplayedWaveform> &list, int position, int numFilters)
{
    bool realMove = false;

    // First, count how many displayed waveforms are in each filter section.
    int waveformsPerSection = numWaveformsInFirstSection(list);

    while (position >= waveformsPerSection + 1) {   // + 1 to account for section divider
        position -= waveformsPerSection + 1;
    }

    int sectionStart = 0;
    for (int filter = 0; filter < numFilters; ++filter) {
        int target = sectionStart + position;
        for (int i = target; i >= sectionStart; --i) {
            if (list[i].isSelected()) {
                if (i != target) realMove = true;
                list.move(i, target--);
            }
        }
        target = sectionStart + position + 1;
        for (int i = target; i < sectionStart + waveformsPerSection; ++i) {
            if (list[i].isSelected()) {
                if (i != target) realMove = true;
                list.move(i, target++);
            }
        }
        sectionStart += waveformsPerSection + 1;  // + 1 to account for section divider
    }
    return realMove;
}

// Returns number of waveforms in first section.
int DisplayListManager::numWaveformsInFirstSection(const QList<DisplayedWaveform> &list) const
{
    if (list.isEmpty()) return 0;
    int section = list[0].sectionID;
    int waveformsPerSection = 1;
    for (int i = 1; i < list.size(); ++i) {
        if (list[i].sectionID != section) break;
        else ++waveformsPerSection;
    }
    return waveformsPerSection;
}

bool DisplayListManager::isValidDragTarget(WaveIndex target, int numFiltersDisplayed, bool arrangeByFilter) const
{
    if (target.inPinned) return true;
    int index = target.index;
    int section = sectionIDOfSelectedWaveforms();
    bool multiFilter = (selectedWaveformsAreAmplifiers()) && numFiltersDisplayed > 1 && arrangeByFilter;

    if (displayList.isEmpty()) return false;

    if (!multiFilter) {  // Simpler case: Dragging waveforms without multiple filters taken into consideration.
        if (index == -1) {  // Before first waveform.
            return (displayList.at(0).sectionID == section);
        }
        if (index < -1) {   // Drop between waveforms...
            const DisplayedWaveform* wave1 = &displayList[-(index + 2)];
            if (index == -displayList.size() - 1) {  // After last waveform.
                return (wave1->sectionID == section);
            } else {
                const DisplayedWaveform* wave2 = &displayList[-(index + 1)];
                if (wave1->sectionID == section || wave2->sectionID == section) {  // ...but only in same section...
                    if (wave1->waveNameWithoutFilter() == wave2->waveNameWithoutFilter()) {  // ...and only between channels.
                        return false;
                    } else if (wave1->getGroupID() == 0 && wave2->getGroupID() == 0) {  // Dropping between ungrouped channels is okay...
                        return true;
                    } else if (wave1->getGroupID() == wave2->getGroupID()) {  // ...but dropping in the middle of a group is not.
                        return false;
                    } else {
                        return true;
                    }
                } else {
                    return false;
                }
            }
        } else {            // Drop after selected waveform...
            const DisplayedWaveform* wave1 = &displayList[index];
            if (index == displayList.size() - 1) {  // After last waveform.
                return (wave1->sectionID == section);
            } else {
                const DisplayedWaveform* wave2 = &displayList[index + 1];
                if (wave1->sectionID == section || wave2->sectionID == section) {  // ...but only in same section...
                    if (wave1->waveNameWithoutFilter() == wave2->waveNameWithoutFilter()) {  // ...and only between channels.
                        return false;
                    } else if (wave1->getGroupID() == 0 && wave2->getGroupID() == 0) {  // Dropping between ungrouped channels is okay...
                        return true;
                    } else if (wave1->getGroupID() == wave2->getGroupID()) {  // ...but dropping in the middle of a group is not.
                        return false;
                    } else {
                        return true;
                    }
                } else {
                    return false;
                }
            }
        }
    } else {  // More complex case: Dragging amplifier waveforms when multiple filters are displayed, arranged by filter.
        // Here we assume that all amplifier waveforms are displayed in the first N sections: 1, 2,..., N,
        // where N = the total number of filters displayed.
        if (index == -1) {  // Before first waveform.
            return (displayList.at(0).sectionID <= numFiltersDisplayed);
        }
        if (index < -1) {   // Drop between waveforms...
            const DisplayedWaveform* wave1 = &displayList[-(index + 2)];
            if (index == -displayList.size() - 1) {  // After last waveform.
                return (wave1->sectionID <= numFiltersDisplayed);
            } else {
                const DisplayedWaveform* wave2 = &displayList[-(index + 1)];
                if (wave1->sectionID <= numFiltersDisplayed || wave2->sectionID <= numFiltersDisplayed) {  // ...but only in amp sections...
                    if (wave1->getGroupID() == 0 && wave2->getGroupID() == 0) {  // Dropping between ungrouped channels is okay...
                        return true;
                    } else if (wave1->getGroupID() == wave2->getGroupID()) {  // ...but dropping in the middle of a group is not.
                        return false;
                    } else {
                        return true;
                    }
                } else {
                    return false;
                }
            }
        } else {            // Drop after selected waveform...
            const DisplayedWaveform* wave1 = &displayList[index];
            if (index == displayList.size() - 1) {  // After last waveform.
                return (wave1->sectionID <= numFiltersDisplayed);
            } else {
                const DisplayedWaveform* wave2 = &displayList[index + 1];
                if (wave1->sectionID <= numFiltersDisplayed || wave2->sectionID <= numFiltersDisplayed) {  // ...but only in amp sections...
                    if (wave1->getGroupID() == 0 && wave2->getGroupID() == 0) {  // Dropping between ungrouped channels is okay...
                        return true;
                    } else if (wave1->getGroupID() == wave2->getGroupID()) {  // ...but dropping in the middle of a group is not.
                        return false;
                    } else {
                        return true;
                    }
                } else {
                    return false;
                }
            }
        }
    }
}

int DisplayListManager::calculateYCoords(QList<DisplayedWaveform>& list, int y, int labelHeight, double zoomFactor, int& minSpacing)
{
    int halfLabelHeight = ceil((double)labelHeight / 2.0) + 1;
    const int YExtraClickArea = 2;
    bool showDisabledChannels = state->showDisabledChannels->getValue();
    int topBottomMargin = labelHeight;

    minSpacing = std::numeric_limits<int>::max();
    for (int i = 0; i < list.size(); ++i) {
        int y1 = y;
        if (showDisabledChannels || list[i].isEnabled()) {
            list[i].yTopLimit = y;
            y += halfLabelHeight + zoomFactor * list[i].spacingAbove;
            list[i].yCoord = y;
            list[i].yTop = (y1 + y) / 2 - YExtraClickArea;
            y += halfLabelHeight + zoomFactor * list[i].spacingBelow;
            list[i].yBottom = (list[i].yCoord + y) / 2 + YExtraClickArea;
            list[i].yBottomLimit = y;
            int ySpacing = y - y1;
            if (ySpacing > 0 && ySpacing < minSpacing) {
                minSpacing = ySpacing;
            }
        } else {
            list[i].yCoord = y;
            list[i].yTopLimit = y;
            list[i].yTop = y;
            list[i].yBottom = y;
            list[i].yBottomLimit = y;
        }
    }
    y += topBottomMargin;
    return y;
}

void DisplayListManager::addYOffset(QList<DisplayedWaveform>& list, int yOffset)
{
    for (int i = 0; i < list.size(); ++i) {
        list[i].yCoord += yOffset;
        list[i].yTop += yOffset;
        list[i].yBottom += yOffset;
        list[i].yTopLimit += yOffset;
        list[i].yBottomLimit += yOffset;
    }
}

void DisplayListManager::selectSingleWaveform(WaveIndex waveIndex)
{
    deselectAllWaveforms();
    if (waveIndex.index >= 0) {
        if (waveIndex.inPinned) {
            selectWaveform(pinnedList.at(waveIndex.index).waveName);
        } else {
            if (!displayList.at(waveIndex.index).isDivider()) {
                selectWaveform(displayList.at(waveIndex.index).waveName);
            }
        }
    }
}

void DisplayListManager::selectAdjacentWaveforms(WaveIndex waveIndex, int sectionExtent)
{
    DisplayedWaveform* wave = displayedWaveform(waveIndex);
    if (sectionExtent == -1) sectionExtent = numDisplayedWaveforms(waveIndex.inPinned);
    selectWaveform(wave->waveName);

    int firstSelected;
    for (int i = 0; i < sectionExtent; ++i) {
        if (displayedWaveform(i, waveIndex.inPinned)->isSelected()) {
            firstSelected = i;
            break;
        }
    }
    int lastSelected = 0;
    for (int i = sectionExtent - 1; i >= 0; --i) {
        if (displayedWaveform(i, waveIndex.inPinned)->isSelected()) {
            lastSelected = i;
            break;
        }
    }
    for (int i = firstSelected; i <= lastSelected; ++i) {
        selectWaveform(displayedWaveform(i, waveIndex.inPinned)->waveName);
    }
}

void DisplayListManager::selectNonAdjacentWaveforms(WaveIndex waveIndex, int numFiltersDisplayed, bool arrangeByFilter)
{
    DisplayedWaveform* wave = displayedWaveform(waveIndex);
    bool select = !wave->isSelected() && !wave->isDivider();
    if (!select) {
        selectWaveform(wave->waveName, false);   // Always allow de-selections.
    } else if (wave->sectionID == sectionIDOfSelectedWaveforms()) {
        selectWaveform(wave->waveName);  // Add new selection if same type as old.
    } else if (arrangeByFilter && numFiltersDisplayed > 1 && wave->sectionID <= numFiltersDisplayed &&
               sectionIDOfSelectedWaveforms() <= numFiltersDisplayed && selectedWaveformsAreAmplifiers()) {
        selectWaveform(wave->waveName);  // Add new selection if all are amp waveforms of different filters.
    }
}

bool DisplayListManager::selectNextOrPreviousWaveform(bool selectNext)
{
    bool pinned = true;
    for (int i = 0; i < displayList.size(); ++i) {
        if (displayList[i].isSelected()) {
            pinned = false;
            break;
        }
    }

    int firstSelected = -1;
    if (selectNext) {
        for (int i = 0; i < numDisplayedWaveforms(pinned); ++i) {
            if (displayedWaveform(i, pinned)->isSelected()) {
                firstSelected = i;
                break;
            }
        }
    } else {
        for (int i = numDisplayedWaveforms(pinned) - 1; i >= 0; --i) {
            if (displayedWaveform(i, pinned)->isSelected()) {
                firstSelected = i;
                break;
            }
        }
    }
    if (firstSelected == -1) return false;

    int newIndex = -1;
    if (selectNext) {
        for (int i = firstSelected + 1; i < numDisplayedWaveforms(pinned); ++i) {
            if (!displayedWaveform(i, pinned)->isSelected() && !displayedWaveform(i, pinned)->isDivider()) {
                newIndex = i;
                break;
            }
        }
    } else {
        for (int i = firstSelected - 1; i >= 0; --i) {
            if (!displayedWaveform(i, pinned)->isSelected() && !displayedWaveform(i, pinned)->isDivider()) {
                newIndex = i;
                break;
            }
        }
    }
    if (newIndex == -1) return false;

    deselectAllWaveforms();
    selectWaveform(displayedWaveform(newIndex, pinned)->waveName);
    return (displayedWaveform(firstSelected, pinned)->isCurrentlyVisible &&
            !displayedWaveform(newIndex, pinned)->isCurrentlyVisible);
}

void DisplayListManager::selectWaveform(const QString& waveName, bool select)
{
    state->signalSources->setSelectedEntireGroupID(waveName, select);
}

void DisplayListManager::deselectAllWaveforms()
{
    for (int group = 0; group < state->signalSources->numGroups(); group++) {
        SignalGroup* signalGroup = state->signalSources->groupByIndex(group);
        for (int signal = 0; signal < signalGroup->numChannels(); signal++) {
            Channel* signalChannel = signalGroup->channelByIndex(signal);
            signalChannel->setSelected(false);
        }
    }
}

void DisplayListManager::updateOrderInState(const QString& portName, int numFiltersDisplayed, bool arrangeByFilter)
{
    SignalGroup* group = state->signalSources->groupByName(portName);
    if (!group) {
        cerr << "DisplayListManager::updateOrderInState: Signal group not found: " << portName.toStdString() << '\n';
        return;
    }
    int numAmplifierChannels = group->numChannels(AmplifierSignal);
    int numAuxInputChannels = group->numChannels(AuxInputSignal);

    if (!displayList.isEmpty()) {
        int index = 0;
        if (displayList[0].isAmplifier()) {
            int waveformsInFirstSection = numWaveformsInFirstSection(displayList);
            if (arrangeByFilter) {
                for (int i = 0; i < waveformsInFirstSection; ++i) {
                    Channel* channel = displayList.at(i).channel;
                    if (!channel) {
                        cerr << "DisplayListManager::updateOrderInState: Channel not found: " <<
                                displayList.at(i).waveNameWithoutFilter().toStdString() << '\n';
                        return;
                    }
                    if (channel) channel->setUserOrder(i);
                }
            } else {
                int order = 0;
                for (int i = 0; i < waveformsInFirstSection; i += numFiltersDisplayed) {
                    Channel* channel = displayList.at(i).channel;
                    if (!channel) {
                        cerr << "MultiWaveformPlot::updateOrderInState: Channel not found: " <<
                                displayList.at(i).waveNameWithoutFilter().toStdString() << '\n';
                        return;
                    }
                    if (channel) channel->setUserOrder(order++);
                }
            }
            index += numFiltersDisplayed * (waveformsInFirstSection + 1);  // + 1 to account for section divider
        }

        int numAuxInputChannelsFound = 0;
        int numSupplyVoltageChannelsFound = 0;
        int numOtherChannelsFound = 0;

        for (int i = index; i < displayList.size(); ++i) {
            WaveformType type = displayList.at(i).waveformType;
            if (type == WaveformDivider) continue;
            Channel* channel = state->signalSources->channelByName(displayList.at(i).waveName);
            if (!channel) {
                cerr << "MultiWaveformPlot::updateOrderInState: Channel not found: " <<
                        displayList.at(i).waveName.toStdString() << '\n';
                return;
            }
            if (type == AuxInputWaveform) {
                channel->setUserOrder(numAmplifierChannels + numAuxInputChannelsFound);
                ++numAuxInputChannelsFound;
            } else if (type == SupplyVoltageWaveform) {
                channel->setUserOrder(numAmplifierChannels + numAuxInputChannels + numSupplyVoltageChannelsFound);
                ++numSupplyVoltageChannelsFound;
            } else {
                channel->setUserOrder(numOtherChannelsFound);
                ++numOtherChannelsFound;
            }
        }
    }
    // Note: Any function calling this function should also call state->forceUpdate().
}

vector<bool> DisplayListManager::selectionRecord() const
{
    vector<bool> record;

    for (int i = 0; i < state->signalSources->numPortGroups(); ++i) {
        SignalGroup* group = state->signalSources->portGroupByIndex(i);
        for (int j = 0; j < group->numChannels(); ++j) {
            Channel* channel = group->channelByIndex(j);
            if (channel) {
                record.push_back(channel->isSelected());
            }
        }
    }
    for (int i = 0; i < state->signalSources->numBaseGroups(); ++i) {
        SignalGroup* group = state->signalSources->baseGroupByIndex(i);
        for (int j = 0; j < group->numChannels(); ++j) {
            Channel* channel = group->channelByIndex(j);
            if (channel) {
                record.push_back(channel->isSelected());
            }
        }
    }
    return record;
}

bool DisplayListManager::selectionRecordsAreEqual(const vector<bool>& a, const vector<bool>& b) const
{
    if (a.size() != b.size()) return false;
    for (int i = 0; i < (int) a.size(); ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

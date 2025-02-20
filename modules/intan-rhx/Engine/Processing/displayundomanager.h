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

#ifndef DISPLAYUNDOMANAGER_H
#define DISPLAYUNDOMANAGER_H

#include <vector>
#include <QString>
#include <QColor>
#include "scrollbar.h"

class SignalSources;

// ChannelState, SignalGroupState, and SignalSourcesState are used to save and restore user-configurable aspects of
// SignalSources, e.g. for saving/loading settings files, or undo/redo operations.
struct ChannelState
{
    QString nativeChannelName;
    QString customChannelName;
    QColor color;
    int userOrder;
    int groupID;
    bool enabled;
    bool selected;
    bool outputToTcp;
    QString reference;
};

struct SignalGroupState
{
    QString name;
    std::vector<ChannelState> signalChannels;
};

struct DisplayColumnState
{
    QStringList pinnedWaveNames;
    bool showPinned;
    bool columnVisible;
    QString visiblePortName;
    ScrollBarState scrollBarState;
};

struct DisplayState
{
    std::vector<DisplayColumnState> columns;
    std::vector<SignalGroupState> groups;
};


class DisplayUndoManager
{
public:
    DisplayUndoManager(SignalSources* signalSources_);
    void pushStateToUndoStack();
    void retractLastPush();
    void clearUndoStack();
    void undo();
    void redo();
    inline bool canUndo() const { return undoIndex > 0; }
    inline bool canRedo() const { return undoIndex < undoStack.size() - 1; }

private:
    SignalSources* signalSources;

    QList<DisplayState> undoStack;
    int undoIndex;
    const int MaxSizeUndoStack = 200;
};

#endif // DISPLAYUNDOMANAGER_H

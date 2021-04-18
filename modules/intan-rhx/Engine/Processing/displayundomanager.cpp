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
#include "signalsources.h"
#include "displayundomanager.h"

DisplayUndoManager::DisplayUndoManager(SignalSources* signalSources_) :
    signalSources(signalSources_),
    undoIndex(0)
{
}

void DisplayUndoManager::pushStateToUndoStack()
{
    if (canRedo()) {
        while (canRedo()) {  // If we are in the middle of the undo stack, clear all potential redos before pushing.
            undoStack.removeLast();
        }
        undoStack.removeLast();  // Also remove saved "pre-undo" state (see undo() below).
    }
    undoStack.append(signalSources->saveState());
    undoIndex = undoStack.size();

    while (undoStack.size() > MaxSizeUndoStack) {
        undoStack.removeFirst();
        undoIndex--;
    }
}

void DisplayUndoManager::retractLastPush()
{
    undoStack.removeLast();
    undoIndex = undoStack.size();
}

void DisplayUndoManager::clearUndoStack()
{
    undoStack.clear();
    undoIndex = 0;
}

void DisplayUndoManager::undo()
{
    if (!canUndo()) return;
    if (undoIndex == undoStack.size()) {
        undoStack.append(signalSources->saveState());   // Add "pre-undo" state, so we can optionally redo back to this.
    }
    DisplayState undoState = undoStack.at(undoIndex - 1);
    signalSources->restoreState(undoState);
    undoIndex--;
}

void DisplayUndoManager::redo()
{
    if (!canRedo()) return;
    DisplayState undoState = undoStack.at(undoIndex + 1);
    signalSources->restoreState(undoState);
    undoIndex++;
    if (undoIndex == undoStack.size() - 1) {  // If we redo back to the top of the stack, remove saved "pre-undo" state.
        undoStack.removeLast();
    }
}


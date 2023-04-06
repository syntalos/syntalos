//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.2.0
//
//  Copyright (c) 2020-2023 Intan Technologies
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

#include "multicolumndisplay.h"

MultiColumnDisplay::MultiColumnDisplay(ControllerInterface* controllerInterface_, SystemState *state_, QWidget *parent) :
    QWidget(parent),
    controllerInterface(controllerInterface_),
    state(state_)
{
    state->writeToLog("Beginning of MultiColumnDisplay ctor");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
//    setFocusPolicy(Qt::StrongFocus);

    QHBoxLayout* mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(mainLayout);

    numRefreshZones.resize(state->tScale->numberOfItems());
    numRefreshZones[0] = 1;
    numRefreshZones[1] = 1;
    numRefreshZones[2] = 1;
    numRefreshZones[3] = 4;
    numRefreshZones[4] = 8;
    numRefreshZones[5] = 16;
    numRefreshZones[6] = 20;
    numRefreshZones[7] = 40;
    numRefreshZones[8] = 80;    // This index should equal state->tScale->numberOfItems() - 1.

    state->writeToLog("About to create WaveformDisplayManager");
    waveformManager = new WaveformDisplayManager(state, 100, numRefreshZones[state->tScale->getIndex()]);
    state->writeToLog("Created WaveformDisplayManager");

    state->writeToLog("About to call addWaveforms()");
    addWaveforms();
    state->writeToLog("Completed addWaveforms()");

    state->writeToLog("About to call waveformManager->resetAll()");
    waveformManager->resetAll();
    state->writeToLog("Completed waveformManager->resetAll()");

    state->writeToLog("About to call addColumn(0)");
    addColumn(0);
    state->writeToLog("Completed addColumn(0)");

    state->writeToLog("About to call setDisplayForUndo()");
    state->signalSources->setDisplayForUndo(this);
    state->writeToLog("completed setDisplayForUndo()");

    tScaleFormerIndex = state->tScale->getIndex();
    rollModeFormerValue = state->rollMode->getValue();
    connect(state, SIGNAL(stateChanged()), this, SLOT(updateFromState()));
    state->writeToLog("End of MultiColumnDisplay ctor");
}

MultiColumnDisplay::~MultiColumnDisplay()
{
    delete waveformManager;
    for (int i = 0; i < displayColumns.size(); ++i) {
        if (displayColumns.at(i)) delete displayColumns.at(i);
    }
}

int MultiColumnDisplay::numVisibleColumns() const
{
    int num = 0;
    for (int i = 0; i < displayColumns.size(); ++i) {
        if (displayColumns.at(i)->isColumnVisible()) ++num;
    }
    return num;
}

bool MultiColumnDisplay::addColumn(int position)
{
    state->writeToLog("Beginning of addColumn(0)");
    if (numColumns() == MaxNumColumns) {
        state->writeToLog("numColumns == MaxNumColumns end");
        return false;
    }
    if (position < 0 || position > numColumns()) {
        state->writeToLog("position < 0 or position > numColumns end. position: " + QString::number(position));
        return false;
    }

    state->writeToLog("About to call clearUndoStack()");
    state->signalSources->undoManager->clearUndoStack();
    state->writeToLog("Completed clearUndoStack()");

    state->writeToLog("About to create WaveformDisplayColumn");
    WaveformDisplayColumn* newColumn = new WaveformDisplayColumn(position, waveformManager, controllerInterface, state, this);
    state->writeToLog("Created WaveformDisplayColumn. About to insert newColumn");
    displayColumns.insert(position, newColumn);
    state->writeToLog("Inserted newColumn");

    QList<QString> pinnedList;
    state->pinnedWaveforms.insert(position, pinnedList);

    state->writeToLog("About to call updateColumnIndices() and updateLayout()");
    updateColumnIndices();
    updateLayout();
    state->writeToLog("Completed updateColumnIndices() and updateLayout()");
    waveformManager->numColumns = displayColumns.size();
    waveformManager->needsFullRedraw = true;
    waveformManager->needsFullReset = true;
    return true;
}

bool MultiColumnDisplay::deleteColumn(int position, bool silent)
{
    if (numColumns() == 1) return false;
    if (position < 0 || position >= numColumns()) return false;

    if (!silent) {
        if (QMessageBox::question(this, tr("Delete Column"), tr("Delete this display column?")) ==
                QMessageBox::No) return false;
    }

    state->signalSources->undoManager->clearUndoStack();

    delete displayColumns.at(position);
    displayColumns.removeAt(position);

    state->pinnedWaveforms.removeAt(position);

    updateColumnIndices();
    updateLayout();
    waveformManager->numColumns = displayColumns.size();
    return true;
}

bool MultiColumnDisplay::moveColumnLeft(int position)
{
    if (position == 0) return false;

    state->signalSources->undoManager->clearUndoStack();

    displayColumns.move(position, position - 1);
    state->pinnedWaveforms.move(position, position - 1);
    updateColumnIndices();
    updateLayout();
    return true;
}

bool MultiColumnDisplay::moveColumnRight(int position)
{
    if (position == numColumns() - 1) return false;

    state->signalSources->undoManager->clearUndoStack();

    displayColumns.move(position, position + 1);
    state->pinnedWaveforms.move(position, position + 1);
    updateColumnIndices();
    updateLayout();
    return true;
}

void MultiColumnDisplay::updateColumnIndices()
{
    for (int i = 0; i < numColumns(); ++i) {
        displayColumns[i]->setIndex(i);
    }
}

void MultiColumnDisplay::updateLayout()
{
    // Remove all items from layout.
    while (layout()->count() > 0) {
        layout()->takeAt(0);
    }

    // Add columns in correct order.
    for (int i = 0; i < numColumns(); ++i) {
        layout()->addWidget(displayColumns.at(i));
        displayColumns[i]->enableHideAndDelete(true);
        displayColumns[i]->enableMoveLeft(true);
        displayColumns[i]->enableMoveRight(true);
    }

    updateHideAndDeleteButtons();

    displayColumns[0]->enableMoveLeft(false);                   // Disable 'move left' button in leftmost column.
    displayColumns[numColumns() - 1]->enableMoveRight(false);   // Disable 'move right' button in rightmost column.
}

void MultiColumnDisplay::updateHideAndDeleteButtons()
{
    // If only one column is visible, disable hide and delete buttons.
    bool enable = numVisibleColumns() > 1;
    for (int i = 0; i < numColumns(); ++i) {
        displayColumns[i]->enableHideAndDelete(enable);
    }
}

void MultiColumnDisplay::updateForRun()
{
    for (int i = 0; i < numColumns(); ++i) {
        displayColumns[i]->updateForRun();
    }
}

void MultiColumnDisplay::updateForLoad()
{
    updateForRun();
}

void MultiColumnDisplay::updateForStop()
{
    for (int i = 0; i < numColumns(); ++i) {
        displayColumns[i]->updateForStop();
    }
}

void MultiColumnDisplay::updateForRescan()
{
    waveformManager->removeAllWaveforms();
    addWaveforms();
    waveformManager->resetAll();
}

void MultiColumnDisplay::addWaveforms()
{
    state->writeToLog("Beginning of addWaveforms()");
    // Add all waveforms to waveform manager.
    bool isStim = state->getControllerTypeEnum() == ControllerStimRecord;
    QStringList groupList = state->signalSources->populatedGroupList();
    state->writeToLog("About to enter groupList for loop. groupList size: " + QString::number(groupList.size()));
    for (int i = 0; i < groupList.size(); ++i) {
        QString groupName = groupList.at(i);
        state->writeToLog("groupName: " + groupName);
        if (groupName.left(5).toLower() == "port ") {
            // Headstage port
            QStringList amplifierList = state->signalSources->getDisplayListAmplifiersNoFilters(groupName);
            for (int j = 0; j < amplifierList.size(); ++j) {
                QString amplifierName = amplifierList.at(j);
                waveformManager->addWaveform(amplifierName + "|WIDE", isStim);
                waveformManager->addWaveform(amplifierName + "|LOW", isStim);
                waveformManager->addWaveform(amplifierName + "|HIGH", isStim);
                waveformManager->addWaveform(amplifierName + "|SPK", isStim, true);
                if (isStim) {
                    waveformManager->addWaveform(amplifierName + "|DC", true);
                }
            }
            QStringList auxInputList = state->signalSources->getDisplayListAuxInputs(groupName);
            for (int j = 0; j < auxInputList.size(); ++j) {
                waveformManager->addWaveform(auxInputList.at(j));
            }
            QStringList supplyVoltageList = state->signalSources->getDisplayListSupplyVoltages(groupName);
            for (int j = 0; j < supplyVoltageList.size(); ++j) {
                waveformManager->addWaveform(supplyVoltageList.at(j));
            }
        } else {
            // Non-headstage port
            QStringList baseList = state->signalSources->getDisplayListBaseGroup(groupName);
            for (int j = 0; j < baseList.size(); ++j) {
                waveformManager->addWaveform(baseList.at(j));
            }
        }
    }
    state->writeToLog("End of addWaveforms()");
}

void MultiColumnDisplay::updateFromState()
{
    // Reset display if time scale or roll mode has changed.
    if (state->tScale->getIndex() != tScaleFormerIndex || state->rollMode->getValue() != rollModeFormerValue) {
        tScaleFormerIndex = state->tScale->getIndex();
        rollModeFormerValue = state->rollMode->getValue();
        reset();
    }
}

void MultiColumnDisplay::reset()
{
    waveformManager->setTScaleInMsec((int) state->tScale->getNumericValue(), numRefreshZones[state->tScale->getIndex()]);
    waveformManager->resetAll();
    for (int i = 0; i < numColumns(); ++i) {
        displayColumns[i]->updateNow();
    }
}

void MultiColumnDisplay::setShowPinned(int column, bool showPinned)
{
    displayColumns[column]->setShowPinnedWaveforms(showPinned);
    state->forceUpdate();
}

void MultiColumnDisplay::setColumnVisible(int column, bool visible)
{
    if (visible) {
        if (!displayColumns.at(column)->isColumnVisible()) displayColumns[column]->showColumn();
    } else {
        if (displayColumns.at(column)->isColumnVisible()) displayColumns[column]->hideColumn();
    }
    state->forceUpdate();
}

void MultiColumnDisplay::updatePortSelectionBoxes(bool switchToFirstPort)
{
    for (int i = 0; i < numColumns(); ++i) {
        displayColumns[i]->updatePortSelectionBox(switchToFirstPort);
    }
    state->forceUpdate();
}

void MultiColumnDisplay::setWaveformWidth(int width)
{
    waveformManager->setMaxWidthInPixels(width);
}

YScaleUsed MultiColumnDisplay::loadWaveformData(WaveformFifo* waveformFifo)
{
    waveformManager->prepForLoadingNewData();
    for (int i = 0; i < numColumns(); ++i) {
        displayColumns[i]->loadWaveformData(waveformFifo);
    }
    return waveformManager->finishLoading();
}

YScaleUsed MultiColumnDisplay::loadWaveformDataFromMemory(WaveformFifo* waveformFifo, int startTime, bool loadAll)
{
    waveformManager->prepForLoadingOldData(startTime);
    for (int i = 0; i < numColumns(); ++i) {
        displayColumns[i]->loadWaveformDataFromMemory(waveformFifo, startTime, loadAll);
    }
    return waveformManager->finishLoading();
}

int MultiColumnDisplay::getMaxSamplesPerRefresh() const
{
    int maxSamplesPerRefresh = 0;
    int samplesPerRefresh;
    for (int i = 0; i < state->tScale->numberOfItems(); ++i) {
        samplesPerRefresh = waveformManager->getSamplesPerRefresh((int) state->tScale->getNumericValue(), numRefreshZones[i]);
        if (samplesPerRefresh > maxSamplesPerRefresh) {
            maxSamplesPerRefresh = samplesPerRefresh;
        }
    }
    return maxSamplesPerRefresh;
}

bool MultiColumnDisplay::groupIsPossible() const
{
    bool isPossible = false;
    for (int i = 0; i < numColumns(); ++i) {
        if (displayColumns[i]->groupIsPossible()) {
            isPossible = true;
            break;
        }
    }
    return isPossible;
}

bool MultiColumnDisplay::ungroupIsPossible() const
{
    bool isPossible = false;
    for (int i = 0; i < numColumns(); ++i) {
        if (displayColumns[i]->ungroupIsPossible()) {
            isPossible = true;
            break;
        }
    }
    return isPossible;
}

void MultiColumnDisplay::enableSelectedChannels(bool enabled)
{
    state->signalSources->undoManager->pushStateToUndoStack();
    for (int i = 0; i < displayColumns.size(); ++i) {
        displayColumns[i]->enableSelectedWaveforms(enabled);
    }
    state->forceUpdate();
}

void MultiColumnDisplay::group()
{
    for (int i = 0; i < numColumns(); ++i) {
        if (displayColumns[i]->groupIsPossible()) {
            displayColumns[i]->group();
            qApp->processEvents();
        }
    }
}

void MultiColumnDisplay::ungroup()
{
    for (int i = 0; i < numColumns(); ++i) {
        if (displayColumns[i]->ungroupIsPossible()) {
            displayColumns[i]->ungroup();
            qApp->processEvents();
        }
    }
}

QString MultiColumnDisplay::getDisplaySettingsString() const
{
    QString displaySettings;
    const QString ColumnSeparator = ";";
    for (int i = 0; i < numColumns(); ++i) {
        displaySettings += displayColumns[i]->getColumnSettingsString();
        if (i < numColumns() - 1) displaySettings += ColumnSeparator;
    }
    return displaySettings;
}

void MultiColumnDisplay::restoreFromDisplaySettingsString(const QString& settings)
{
    const QString ColumnSeparator = ";";
    int numSettingsColumns = qBound(0, settings.count(ColumnSeparator) + 1, 10);
    while (numColumns() > numSettingsColumns) {
        deleteColumn(numColumns() - 1, true);
    }
    while (numColumns() < numSettingsColumns) {
        addColumn(numColumns());
    }
    for (int i = 0; i < numSettingsColumns; ++i) {
        QString columnSettings = settings.section(ColumnSeparator, i, i);
        if (!columnSettings.isEmpty()) {
            displayColumns[i]->restoreFromSettingsString(columnSettings);
        }
    }
}

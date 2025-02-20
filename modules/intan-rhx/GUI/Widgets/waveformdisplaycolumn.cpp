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

#include <iostream>
#include "multicolumndisplay.h"
#include "waveformdisplaycolumn.h"

WaveformDisplayColumn::WaveformDisplayColumn(int columnIndex_, WaveformDisplayManager* waveformManager,
                                             ControllerInterface* controllerInterface_, SystemState *state_,
                                             MultiColumnDisplay *parent_) :
    QWidget(parent_),
    parent(parent_),
    columnIndex(columnIndex_),
    controllerInterface(controllerInterface_),
    state(state_)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
//    setFocusPolicy(Qt::StrongFocus);

    visible = true;
    deleteButtonEnabled = true;
    pinnedShown = true;

    portComboBox = new QComboBox(this);
    portComboBox->addItems(state->signalSources->populatedGroupListWithChannelCounts());

    waveformPlot = new MultiWaveformPlot(columnIndex, waveformManager, controllerInterface, state, this);
    connect(portComboBox, SIGNAL(currentIndexChanged(int)), waveformPlot, SLOT(updateFromState()));
    if (state->testMode->getValue()) {
        portComboBox->setFocusProxy(waveformPlot);
    }

    hideColumnButton = new QToolButton(this);
    hideColumnButton->setIcon(QIcon(":/images/collapseicon.png"));
    hideColumnButton->setToolTip(tr("Hide Column"));
    connect(hideColumnButton, SIGNAL(clicked()), this, SLOT(hideColumnSlot()));

    showColumnButton = new QToolButton(this);
    showColumnButton->setIcon(QIcon(":/images/expandicon.png"));
    showColumnButton->setToolTip(tr("Unhide Column"));
    showColumnButton->hide();
    connect(showColumnButton, SIGNAL(clicked()), this, SLOT(showColumnSlot()));

    moveLeftButton = new QToolButton(this);
    moveLeftButton->setIcon(QIcon(":/images/leftarrow.png"));
    moveLeftButton->setToolTip(tr("Move Column Left"));
    connect(moveLeftButton, SIGNAL(clicked()), this, SLOT(moveColumnLeft()));

    moveRightButton = new QToolButton(this);
    moveRightButton->setIcon(QIcon(":/images/rightarrow.png"));
    moveRightButton->setToolTip(tr("Move Column Right"));
    connect(moveRightButton, SIGNAL(clicked()), this, SLOT(moveColumnRight()));

    addColumnButton = new QToolButton(this);
    addColumnButton->setIcon(QIcon(":/images/plusicon.png"));
    addColumnButton->setToolTip(tr("Add Column"));
    connect(addColumnButton, SIGNAL(clicked()), this, SLOT(addColumn()));

    deleteColumnButton = new QToolButton(this);
    deleteColumnButton->setIcon(QIcon(":/images/minusicon.png"));
    deleteColumnButton->setToolTip(tr("Delete Column"));
    connect(deleteColumnButton, SIGNAL(clicked()), this, SLOT(deleteColumn()));

    if (state->testMode->getValue()) {
        pinWaveformButton = nullptr;
        showPinnedCheckBox = nullptr;
    } else {
        pinWaveformButton = new QToolButton(this);
        pinWaveformButton->setIcon(QIcon(":/images/thumbtackdownicon.png"));
        pinWaveformButton->setToolTip(tr("Add Pinned Waveform"));
        connect(pinWaveformButton, SIGNAL(clicked()), waveformPlot, SLOT(openWaveformSelectDialog()));

        showPinnedCheckBox = new QCheckBox(tr("show pinned"), this);
        showPinnedCheckBox->setChecked(true);
        connect(showPinnedCheckBox, SIGNAL(clicked()), this, SLOT(showPinnedWaveformsSlot()));
    }

    lessSpacingButton = new QToolButton(this);
    lessSpacingButton->setIcon(QIcon(":/images/lessspacingicon.png"));
    lessSpacingButton->setToolTip(tr("Less Spacing"));
    connect(lessSpacingButton, SIGNAL(clicked()), waveformPlot, SLOT(decreaseSpacing()));

    moreSpacingButton = new QToolButton(this);
    moreSpacingButton->setIcon(QIcon(":/images/morespacingicon.png"));
    moreSpacingButton->setToolTip(tr("More Spacing"));
    connect(moreSpacingButton, SIGNAL(clicked()), waveformPlot, SLOT(increaseSpacing()));

    QHBoxLayout* toolRow = new QHBoxLayout;
    toolRow->addWidget(showColumnButton);
    toolRow->addWidget(hideColumnButton);
    toolRow->addWidget(portComboBox);
    toolRow->addWidget(moveLeftButton);
    toolRow->addWidget(moveRightButton);
    if (!state->testMode->getValue()) {
        toolRow->addWidget(pinWaveformButton);
        toolRow->addWidget(showPinnedCheckBox);
    }
    toolRow->addWidget(lessSpacingButton);
    toolRow->addWidget(moreSpacingButton);
    toolRow->addStretch(1);
    toolRow->addWidget(addColumnButton);
    toolRow->addWidget(deleteColumnButton);

    verticalSpacer = new QSpacerItem(1, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(waveformPlot);
    mainLayout->addSpacerItem(verticalSpacer);
    mainLayout->addLayout(toolRow);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(1);

    setLayout(mainLayout);

    if (state->running) updateForRun();
}

WaveformDisplayColumn::~WaveformDisplayColumn()
{
    disconnect(this, nullptr, waveformPlot, nullptr);
    disconnect(this, nullptr, nullptr, nullptr);
    delete waveformPlot;    // Probably not necessary since WaveformPlot is a QWidget, but just to be safe.
}

void WaveformDisplayColumn::enableHideAndDelete(bool enable)
{
    hideColumnButton->setEnabled(enable);
    deleteColumnButton->setEnabled(enable & !state->running);
    deleteButtonEnabled = enable;
}

void WaveformDisplayColumn::setWaveformWidth(int width)
{
    parent->setWaveformWidth(width);
}

void WaveformDisplayColumn::hideColumnSlot()
{
    state->signalSources->undoManager->pushStateToUndoStack();
    hideColumn();
}

void WaveformDisplayColumn::hideColumn()
{
    visible = false;

    showColumnButton->show();
    hideColumnButton->hide();

    portComboBox->hide();
    moveLeftButton->hide();
    moveRightButton->hide();
    if (!state->testMode->getValue()) {
        pinWaveformButton->hide();
        showPinnedCheckBox->hide();
    }
    lessSpacingButton->hide();
    moreSpacingButton->hide();
    addColumnButton->hide();
    deleteColumnButton->hide();
    waveformPlot->hide();

    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);

    verticalSpacer->changeSize(1, 0, QSizePolicy::Fixed, QSizePolicy::Expanding);
    layout()->invalidate();
    parent->updateHideAndDeleteButtons();
}

void WaveformDisplayColumn::showColumnSlot()
{
    state->signalSources->undoManager->pushStateToUndoStack();
    showColumn();
}

void WaveformDisplayColumn::showColumn()
{
    visible = true;

    showColumnButton->hide();
    hideColumnButton->show();

    portComboBox->show();
    moveLeftButton->show();
    moveRightButton->show();
    if (!state->testMode->getValue()) {
        pinWaveformButton->show();
        showPinnedCheckBox->show();
    }
    lessSpacingButton->show();
    moreSpacingButton->show();
    addColumnButton->show();
    deleteColumnButton->show();
    waveformPlot->show();

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    verticalSpacer->changeSize(1, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout()->invalidate();
    parent->updateHideAndDeleteButtons();
}

void WaveformDisplayColumn::setSelectedPort(const QString& portName)
{
    for (int i = 0; i < portComboBox->count(); ++i) {
        if (portComboBox->itemText(i).section(" (", 0, 0) == portName) {
            portComboBox->setCurrentIndex(i);
            break;
        }
    }
}

void WaveformDisplayColumn::updateForRun()
{
    deleteColumnButton->setEnabled(false);
}

void WaveformDisplayColumn::updateForStop()
{
    deleteColumnButton->setEnabled(deleteButtonEnabled);
}

void WaveformDisplayColumn::addColumn()
{
    parent->addColumn(columnIndex + 1);  // Add new column to the right of this column.
}

void WaveformDisplayColumn::deleteColumn()
{
    parent->deleteColumn(columnIndex);
}

void WaveformDisplayColumn::moveColumnLeft()
{
    parent->moveColumnLeft(columnIndex);
}

void WaveformDisplayColumn::moveColumnRight()
{
    parent->moveColumnRight(columnIndex);
}

void WaveformDisplayColumn::showPinnedWaveformsSlot()
{
    if (state->testMode->getValue()) return;
    state->signalSources->undoManager->pushStateToUndoStack();
    bool show = showPinnedCheckBox->checkState() != Qt::Unchecked;
    waveformPlot->showPinnedWaveforms(show);
    pinnedShown = show;
}

void WaveformDisplayColumn::setShowPinnedWaveforms(bool show)
{
    if (state->testMode->getValue()) return;
    showPinnedCheckBox->setChecked(show);
    waveformPlot->showPinnedWaveforms(show);
    pinnedShown = show;
}

void WaveformDisplayColumn::updatePortSelectionBox(bool switchToFirstPort)
{
    QString selectedItemText = portComboBox->currentText();
    int n = portComboBox->count();
    for (int i = n - 1; i >= 0; --i) {
        portComboBox->removeItem(i);
    }
    portComboBox->addItems(state->signalSources->populatedGroupListWithChannelCounts());
    portComboBox->setCurrentIndex(0);  // Default to first port on new combo box list...
    if (!switchToFirstPort) {
        portComboBox->setCurrentText(selectedItemText); // ... but if previous selection is present, use that. Don't do this if we switch to first port.
    }
    waveformPlot->adjustToNewNumberOfPorts(portComboBox->count());
    // Caller should probably force updateFromState() after calling this function.
}

QString WaveformDisplayColumn::getColumnSettingsString() const
{
    QString columnSettings;
    const QString ParameterSeparator = ":";
    const QString ListSeparator = ",";

    columnSettings += visible ? "ShowColumn" : "HideColumn";
    columnSettings += ParameterSeparator;
    columnSettings += portComboBox->currentText().section(" (", 0, 0);
    columnSettings += ParameterSeparator;
    columnSettings += waveformPlot->getScrollBarStateString();
    columnSettings += ParameterSeparator;
    columnSettings += pinnedShown ? "ShowPinned" : "HidePinned";
    columnSettings += ParameterSeparator;
    QStringList pinnedWaveNames = waveformPlot->getPinnedWaveNames();
    for (int i = 0; i < pinnedWaveNames.size(); ++i) {
        columnSettings += pinnedWaveNames[i];
        if (i < pinnedWaveNames.size() - 1) columnSettings += ListSeparator;
    }
    return columnSettings;
}

void WaveformDisplayColumn::restoreFromSettingsString(const QString& settings)
{
    const QString ParameterSeparator = ":";
    const QString ListSeparator = ",";

    bool columnVisible = !(settings.section(ParameterSeparator, 0, 0).toLower() == "hidecolumn");
    if (!columnVisible && visible) {
        hideColumn();
    } else if (columnVisible && !visible) {
        showColumn();
    }

    QString portSelection = settings.section(ParameterSeparator, 1, 1);
    setSelectedPort(portSelection);

    QString scrollBarState = settings.section(ParameterSeparator, 2, 2);
    waveformPlot->restoreScrollBarStateFromString(scrollBarState);

    bool showPinned = !(settings.section(ParameterSeparator, 3, 3).toLower() == "hidepinned");
    pinnedShown = showPinned;
    waveformPlot->showPinnedWaveforms(pinnedShown);

    waveformPlot->unpinAllWaveforms();
    QString pinnedWaveNames = settings.section(ParameterSeparator, 4, 4);
    int numPinned = 0;
    if (!pinnedWaveNames.isEmpty()) numPinned = pinnedWaveNames.count(ListSeparator) + 1;
    for (int i = 0; i < numPinned; ++i) {
        waveformPlot->addPinnedWaveform(pinnedWaveNames.section(ListSeparator, i, i));
    }
}

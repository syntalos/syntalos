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

#include <QtWidgets>

#include "keyboardshortcutdialog.h"

// Keyboard shortcut dialog - this displays a window listing keyboard shortcuts.
KeyboardShortcutDialog::KeyboardShortcutDialog(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(tr("Keyboard Shortcuts"));

    QVBoxLayout *generalLayout = new QVBoxLayout;

    generalLayout->addWidget(new QLabel(tr("<b>Ctrl+O:</b> Load settings"), this));
    generalLayout->addWidget(new QLabel(tr("<b>Ctrl+S:</b> Save settings"), this));
    generalLayout->addWidget(new QLabel(tr("<b>Ctrl+Q:</b> Exit"), this));
    generalLayout->addWidget(new QLabel(tr("<b>Ctrl+R:</b> Rename selected channel(s)"), this));
    generalLayout->addWidget(new QLabel(tr("<b>Ctrl+C:</b> Copy selected channel stimulation parameters"), this));
    generalLayout->addWidget(new QLabel(tr("<b>Ctrl+V:</b> Paste stimulation parameters to selected channel(s)"), this));
    generalLayout->addWidget(new QLabel(tr("<b>F12:</b> Open/close keyboard shortcuts dialog"), this));


    generalLayout->addStretch(1);

    QGroupBox *generalGroupBox = new QGroupBox("General", this);
    generalGroupBox->setLayout(generalLayout);

    QVBoxLayout *waveformPlotLayout = new QVBoxLayout;

    waveformPlotLayout->addWidget(new QLabel(tr("<b>/ or ? Key:</b> Toggle roll/sweep mode"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>&lt; or , Key:</b> Zoom in on time scale"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>&gt; or . Key:</b> Zoom out on time scale"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>+ or = Key:</b> Zoom in on voltage scale"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>- or _ Key:</b> Zoom out on voltage scale"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>Ctrl+P:</b> Pin selected channels"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>Ctrl+U:</b> Unpin selected channels"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>Mouse Wheel:</b> Scroll through channels"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>Ctrl+Mouse Wheel:</b> Adjust vertical spacing of channels"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>Cursor Keys:</b> Step through channels"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>Page Up/Down Keys:</b> Scroll through channels"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>Home Key:</b> Scroll to first channel"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>End Key:</b> Scroll to last channel"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>Spacebar:</b> Enable/disable channels"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>Ctrl+G:</b> Group selected channels"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>Ctrl+Shift+G:</b> Ungroup selected channels"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>Ctrl+1:</b> Display custom channel name"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>Ctrl+2:</b> Display native channel name"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>Ctrl+3:</b> Display impedance magnitude"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>Ctrl+4:</b> Display impedance phase"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>Ctrl+5:</b> Display reference"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>Ctrl+T:</b> Cycle through waveform display labels"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>Ctrl+Z:</b> Undo"), this));
    waveformPlotLayout->addWidget(new QLabel(tr("<b>Ctrl+Y:</b> Redo"), this));

    waveformPlotLayout->addStretch(1);

    QGroupBox *waveformPlotGroupBox = new QGroupBox("Waveform Plot", this);
    waveformPlotGroupBox->setLayout(waveformPlotLayout);

    QVBoxLayout *spikeSortingPlotLayout = new QVBoxLayout;

    spikeSortingPlotLayout->addWidget(new QLabel(tr("<b>&lt; or , Key:</b> Zoom in on time scale"), this));
    spikeSortingPlotLayout->addWidget(new QLabel(tr("<b>&gt; or . Key:</b> Zoom out on time scale"), this));
    spikeSortingPlotLayout->addWidget(new QLabel(tr("<b>+ or = Key:</b> Zoom in on voltage scale"), this));
    spikeSortingPlotLayout->addWidget(new QLabel(tr("<b>- or _ Key:</b> Zoom out on voltage scale"), this));
    spikeSortingPlotLayout->addWidget(new QLabel(tr("<b>Mouse Wheel:</b> Zoom through voltage scale"), this));
    spikeSortingPlotLayout->addWidget(new QLabel(tr("<b>Shift+Mouse Wheel:</b> Zoom through time scale"), this));

    spikeSortingPlotLayout->addStretch(1);

    QGroupBox *spikeSortingPlotGroupBox = new QGroupBox("Spike Scope Plot", this);
    spikeSortingPlotGroupBox->setLayout(spikeSortingPlotLayout);

    QVBoxLayout *probeMapLayout = new QVBoxLayout;

    probeMapLayout->addWidget(new QLabel(tr("<b>Mouse Wheel Up or Shift+Up or Ctrl+Up Arrow or +:</b> Zoom in"), this));
    probeMapLayout->addWidget(new QLabel(tr("<b>Mouse Wheel Down or Shift+Down or Ctrl+Down Arrow or -:</b> Zoom out"), this));
    probeMapLayout->addWidget(new QLabel(tr("<b>Shift+Mouse Wheel Up or Up Arrow or Page Up:</b> Scroll up"), this));
    probeMapLayout->addWidget(new QLabel(tr("<b>Shift+Mouse Wheel Down or Down Arrow or Page Down:</b> Scroll down"), this));
    probeMapLayout->addWidget(new QLabel(tr("<b>Ctrl+Mouse Wheel Up or Left Arrow:</b> Scroll left"), this));
    probeMapLayout->addWidget(new QLabel(tr("<b>Ctrl+Mouse Wheel Down or Right Arrow:</b> Scroll right"), this));

    probeMapLayout->addStretch(1);

    QGroupBox *probeMapGroupBox = new QGroupBox("Probe Map", this);
    probeMapGroupBox->setLayout(probeMapLayout);

    QVBoxLayout *column1 = new QVBoxLayout;
    column1->addWidget(waveformPlotGroupBox);

    QVBoxLayout *column2 = new QVBoxLayout;
    column2->addWidget(generalGroupBox);
    column2->addWidget(spikeSortingPlotGroupBox);
    column2->addWidget(probeMapGroupBox);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addLayout(column1);
    mainLayout->addLayout(column2);

    QWidget *mainWidget = new QWidget(this);
    mainWidget->setLayout(mainLayout);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(mainWidget);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QVBoxLayout *scrollLayout = new QVBoxLayout;
    scrollLayout->addWidget(scrollArea);

    // Set dialog initial size to 10% larger than scrollArea's sizeHint - should avoid scroll bars for default size.
    int initialWidth = round(mainWidget->sizeHint().width() * 1.1);
    int initialHeight = round(mainWidget->sizeHint().height() * 1.1);
    resize(initialWidth, initialHeight);

    setLayout(scrollLayout);
}

void KeyboardShortcutDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_F12) {
        close();
    }
    else {
        QDialog::keyPressEvent(event);
    }
}

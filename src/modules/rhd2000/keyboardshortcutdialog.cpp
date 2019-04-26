//  ------------------------------------------------------------------------
//
//  This file is part of the Intan Technologies RHD2000 Interface
//  Version 1.3
//  Copyright (C) 2013 Intan Technologies
//
//  ------------------------------------------------------------------------
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <QtGui>

#include "qtincludes.h"

#include "keyboardshortcutdialog.h"

// Keyboard shortcut dialog.
// Displays a window listing keyboard shortcuts.

KeyboardShortcutDialog::KeyboardShortcutDialog(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(tr("Keyboard Shortcuts"));

    QVBoxLayout *mainWindowLayout = new QVBoxLayout;
    mainWindowLayout->addWidget(new QLabel(tr("<b>&lt;/, Key:</b> Zoom in on time scale")));
    mainWindowLayout->addWidget(new QLabel(tr("<b>&gt;/. Key:</b> Zoom out on time scale")));
    mainWindowLayout->addWidget(new QLabel(tr("<b>+/= Key:</b> Zoom in on voltage scale")));
    mainWindowLayout->addWidget(new QLabel(tr("<b>-/_ Key:</b> Zoom  on voltage scale")));
    mainWindowLayout->addWidget(new QLabel(tr("<b>[ Key:</b> Increase number of waveforms on screen")));
    mainWindowLayout->addWidget(new QLabel(tr("<b>] Key:</b> Decrease number of waveforms on screen")));
    mainWindowLayout->addWidget(new QLabel(tr("<b>Space Bar:</b> Enable/disable selected channel")));
    mainWindowLayout->addWidget(new QLabel(tr("<b>Ctrl+R:</b> Rename selected channel")));
    mainWindowLayout->addWidget(new QLabel(tr("<b>Cursor Keys:</b> Navigate through channels")));
    mainWindowLayout->addWidget(new QLabel(tr("<b>Page Up/Down Keys:</b> Navigate through channel")));
    mainWindowLayout->addWidget(new QLabel(tr("<b>Mouse Click:</b> Select channel")));
    mainWindowLayout->addWidget(new QLabel(tr("<b>Mouse Drag:</b> Move channel")));
    mainWindowLayout->addWidget(new QLabel(tr("<b>Mouse Wheel:</b> Navigate through channels")));
    mainWindowLayout->addStretch(1);

    QGroupBox *mainWindowGroupBox = new QGroupBox("Main Window");
    mainWindowGroupBox->setLayout(mainWindowLayout);

    QVBoxLayout *spikeScopeLayout = new QVBoxLayout;
    spikeScopeLayout->addWidget(new QLabel(tr("<b>+/= Key:</b> Zoom in on voltage scale")));
    spikeScopeLayout->addWidget(new QLabel(tr("<b>-/_ Key:</b> Zoom out on voltage scale")));
    spikeScopeLayout->addWidget(new QLabel(tr("<b>Mouse Click:</b> Set voltage threshold level")));
    spikeScopeLayout->addWidget(new QLabel(tr("<b>Mouse Wheel:</b> Change voltage scale")));
    spikeScopeLayout->addStretch(1);

    QGroupBox *spikeScopeGroupBox = new QGroupBox("Spike Scope");
    spikeScopeGroupBox->setLayout(spikeScopeLayout);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(mainWindowGroupBox);
    mainLayout->addWidget(spikeScopeGroupBox);
    mainLayout->addStretch(1);

    setLayout(mainLayout);

}

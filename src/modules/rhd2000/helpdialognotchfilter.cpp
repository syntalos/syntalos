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

#include "helpdialognotchfilter.h"

// Software notch filter help dialog.

HelpDialogNotchFilter::HelpDialogNotchFilter(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(tr("Software Notch Filter"));

    QPixmap image;
    image.load(":/images/help_diagram_notch_filter.png", "PNG");
    QLabel *imageLabel = new QLabel();
    imageLabel->setPixmap(image);

    QLabel *label1 = new QLabel(tr("An optional 50 Hz or 60 Hz software notch filter can be enabled to help "
                                   "remove mains interference.  The notch filter is used only for displaying data; "
                                   "raw data without the notch filter applied is saved to disk.  However, each data "
                                   "file contains a parameter in its header noting the notch filter setting.  The "
                                   "MATLAB function provided by Intan Technologies reads this parameter and, if the "
                                   "notch filter was applied during recording, applies the identical notch filter "
                                   "to the data extracted in MATLAB."));
    label1->setWordWrap(true);

    QLabel *label2 = new QLabel(tr("The diagram below shows a simplified signal path from the SPI interface cable "
                                   "through the RHD2000 USB interface board to the host computer running this "
                                   "software."));
    label2->setWordWrap(true);

    QLabel *label3 = new QLabel(tr("Many users find that most 50/60 Hz interference disappears when the RHD2000 "
                                   "chip is placed in close proximity to the recording electrodes.  In many "
                                   "applications the notch filter may not be necessary."));
    label3->setWordWrap(true);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(label1);
    mainLayout->addWidget(label2);
    mainLayout->addWidget(label3);
    mainLayout->addWidget(imageLabel);

    setLayout(mainLayout);
}

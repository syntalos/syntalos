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

#include "helpdialogcomparators.h"

// Low-latency comparator help dialog.

HelpDialogComparators::HelpDialogComparators(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(tr("Low-Latency Threshold Comparators"));

    QPixmap image;
    image.load(":/images/help_diagram_comparators.png", "PNG");
    QLabel *imageLabel = new QLabel();
    imageLabel->setPixmap(image);

    QLabel *label1 = new QLabel(tr("The FPGA on the RHD2000 USB interface board implements eight low-latency "
                                   "threshold comparators that generate digital signals on Digital Output Lines "
                                   "0-7 when the amplifier channels routed to the DACs exceed user-specified "
                                   "threshold levels.  These comparators have total latencies less than 200 "
                                   "microseconds, and may be used for real-time triggering of other devices "
                                   "based on the detection of neural spikes."));
    label1->setWordWrap(true);

    QLabel *label2 = new QLabel(tr("The diagram below shows a simplified signal path from the SPI interface cable "
                                   "to the DACs and threshold comparators"));
    label2->setWordWrap(true);

    QLabel *label3 = new QLabel(tr("If spike detection is to be performed on wideband neural signals that also "
                                   "include low-frequency local field potentials (LFPs), the optional software/DAC "
                                   "high-pass filter can be enabled to pass only spikes.  Go to the <b>Bandwidth</b> "
                                   "tab to enable this filter."
                                   ));
    label3->setWordWrap(true);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(label1);
    mainLayout->addWidget(label2);
    mainLayout->addWidget(imageLabel);
    mainLayout->addWidget(label3);

    setLayout(mainLayout);
}

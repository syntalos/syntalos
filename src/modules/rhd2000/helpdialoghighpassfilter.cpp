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

#include "helpdialoghighpassfilter.h"

// Software/DAC high-pass filter help dialog.

HelpDialogHighpassFilter::HelpDialogHighpassFilter(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(tr("Software/DAC High-Pass Filter"));

    QPixmap image;
    image.load(":/images/help_diagram_software_HPFs.png", "PNG");
    QLabel *imageLabel = new QLabel();
    imageLabel->setPixmap(image);

    QLabel *label1 = new QLabel(tr("In many neural recording applications, users may wish to record wideband "
                                   "electrode waveforms (i.e., both low-frequency local field potentials and "
                                   "high-frequency spikes) but view only spikes in the GUI display.  An optional "
                                   "software-implemented high-pass filter is provided here for this purpose.  "
                                   "When enabled, a first-order high-pass filter at the user-specified cutoff "
                                   "frequency is applied to data displayed on the screen, but is not applied to "
                                   "data saved to disk."));
    label1->setWordWrap(true);

    QLabel *label2 = new QLabel(tr("The diagram below shows a simplified signal path from the SPI interface cable "
                                   "through the RHD2000 USB interface board to the host computer running this "
                                   "software."));
    label2->setWordWrap(true);

    QLabel *label3 = new QLabel(tr("When the software high-pass filters are enabled, identical high-pass filters "
                                   "implemented in the Spartan-6 FPGA on the RHD2000 USB interface board are also "
                                   "enabled.  These filters act on up to eight amplifier signals routed "
                                   "to the eight digital-to-analog converters (DACs) used for low-latency analog "
                                   "signal reconstruction."
                                   ));
    label3->setWordWrap(true);

    QLabel *label4 = new QLabel(tr("This is particularly useful when the low-latency threshold comparators (also "
                                   "implemented in the FPGA) are used to detect neural spikes in the presence of "
                                   "large low-frequency LFPs.  Click on the <b>DAC/Audio</b> tab to configure the "
                                   "DACs and comparators."));
    label4->setWordWrap(true);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(label1);
    mainLayout->addWidget(label2);
    mainLayout->addWidget(imageLabel);
    mainLayout->addWidget(label3);
    mainLayout->addWidget(label4);

    setLayout(mainLayout);
}

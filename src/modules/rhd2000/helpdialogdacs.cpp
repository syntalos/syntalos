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

#include "helpdialogdacs.h"

// Low-latency DAC help dialog.

HelpDialogDacs::HelpDialogDacs(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(tr("Low-Latency Digital-to-Analog Converters"));

    QPixmap image;
    image.load(":/images/help_diagram_DACs.png", "PNG");
    QLabel *imageLabel = new QLabel();
    imageLabel->setPixmap(image);

    QLabel *label1 = new QLabel(tr("Up to eight selected amplifier channels may be routed to the eight "
                                   "digital-to-analog converters (DACs) on the RHD2000 USB interface board.  "
                                   "This provides compatibility with legacy analog-input data acquisition "
                                   "systems.  DAC channels 1 and 2 are also connected to the left and right "
                                   "channels of the 'audio line out' jack on the USB interface board.  Any "
                                   "signals assigned to DACs 1 and 2 will be audible if the board is connected "
                                   "to an audio amplifier using a standard 3.5-mm stereo cable."));
    label1->setWordWrap(true);

    QLabel *label2 = new QLabel(tr("The selected amplifier waveforms are routed directly through the FPGA on the "
                                   "USB interface board to avoid delays associated with the USB interface and software.  "
                                   "The typical latency from amplifier input to DAC output is less than 200 microseconds. "
                                   ));
    label2->setWordWrap(true);

    QLabel *label3 = new QLabel(tr("The diagram below shows a simplified signal path from the SPI interface cable "
                                   "to the DACs."));
    label3->setWordWrap(true);


    QLabel *label4 = new QLabel(tr("The FPGA also includes optional 'noise slicer' signal processing that can be used "
                                   "to enhance the audibility of low-amplitude neural spikes in a noisy waveform.  "
                                   "The operation of the noise slicer algorithm is described in the 'Analog Waveform "
                                   "Reconstruction and Audio Output' section of the <b>RHD2000 evaluation "
                                   "system datasheet</b> found on the Downloads page of the Intan Technologies website."
                                   ));
    label4->setWordWrap(true);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(label1);
    mainLayout->addWidget(label2);
    mainLayout->addWidget(label3);
    mainLayout->addWidget(imageLabel);
    mainLayout->addWidget(label4);

    setLayout(mainLayout);
}

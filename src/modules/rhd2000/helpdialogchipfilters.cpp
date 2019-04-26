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

#include "helpdialogchipfilters.h"

// RHD2000 chip filters help dialog.

HelpDialogChipFilters::HelpDialogChipFilters(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(tr("RHD2000 On-Chip Filters"));

    QPixmap image;
    image.load(":/images/help_diagram_chip_filters.png", "PNG");
    QLabel *imageLabel = new QLabel();
    imageLabel->setPixmap(image);

    QLabel *label1 = new QLabel(tr("Each amplifier on an RHD2000 chip has a pass band defined by analog circuitry "
                                   "that includes a high-pass filter and a low-pass filter.  The lower end of the pass "
                                   "band has a first-order high-pass characteristic.  The upper end of the pass "
                                   "band is set by a third-order Butterworth low-pass filter."));
    label1->setWordWrap(true);

    QLabel *label2 = new QLabel(tr("Each RHD2000 includes an on-chip module that performs digital signal processing "
                                   "(DSP) to implement an additional first-order high-pass filter on each digitized amplifier "
                                   "waveform.   This feature is used to remove the residual DC offset voltages associated "
                                   "with the analog amplifiers."));
    label2->setWordWrap(true);

    QLabel *label3 = new QLabel(tr("The diagram below shows a simplified functional diagram of one channel in an "
                                   "RHD2000 chip.  For more information, consult the <b>RHD2000 series digital "
                                   "physiology interface chip datasheet</b>, "
                                   "which can be found on the Downloads page of the Intan Technologies website."));
    label3->setWordWrap(true);

    QLabel *label4 = new QLabel(tr("The general recommendation for best linearity is to set the DSP cutoff frequency to "
                                   "the desired low-frequency cutoff and to set the amplifier lower bandwidth 2x to 10x "
                                   "lower than this frequency.  Note that the DSP cutoff frequency has a limited frequency "
                                   "resolution (stepping in powers of two), so if a precise value of low-frequency cutoff "
                                   "is required, the amplifier lower bandwidth could be used to define this and the DSP "
                                   "cutoff frequency set 2x to 10x below this point.  If both the DSP cutoff frequency and "
                                   "the amplifier lower bandwidth are set to the same (or similar) frequencies, the actual "
                                   "3-dB cutoff frequency will be higher than either frequency due to the combined effect of "
                                   "the two filters."));
    label4->setWordWrap(true);

    QLabel *label5 = new QLabel(tr("For a detailed mathematical description of all three on-chip filters, visit the "
                                   "Downloads page on the Intan Technologies website and consult the document <b>FAQ: "
                                   "RHD2000 amplifier filter characteristics</b>."
                                   ));
    label5->setWordWrap(true);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(label1);
    mainLayout->addWidget(label2);
    mainLayout->addWidget(label3);
    mainLayout->addWidget(imageLabel);
    mainLayout->addWidget(label4);
    mainLayout->addWidget(label5);

    setLayout(mainLayout);
}

//  ------------------------------------------------------------------------
//
//  This file is part of the Intan Technologies RHD2000 Interface
//  Version 1.4
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
#include <iostream>

#include "qtincludes.h"

#include "helpdialogfastsettle.h"

using namespace std;

// Fast settle (blanking) help dialog.

HelpDialogFastSettle::HelpDialogFastSettle(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(tr("Amplifier Fast Settle (Blanking)"));

    QLabel *label1 = new QLabel(tr("All RHD2000 chips have a hardware 'fast settle' function that rapidly "
                                   "resets the analog signal path of each amplifier channel to zero to prevent "
                                   "(or recover from) saturation caused by large transient input signals such as those "
                                   "due to nearby stimulation.  Recovery from amplifier saturation can be slow when "
                                   "the lower bandwidth is set to a low frequency (e.g., 1 Hz)."));
    label1->setWordWrap(true);

    QLabel *label2 = new QLabel(tr("This fast settle or 'blanking' function may be enabled manually by clicking the "
                                   "<b>Manual</b> check box.  The amplifier signals will be held at zero until the box "
                                   "is unchecked."));
    label2->setWordWrap(true);

    QLabel *label3 = new QLabel(tr("Real-time control of the fast settle function is enabled by checking the <b>Realtime "
                                   "Settle Control</b> box and selecting a digital input on the USB interface board that will "
                                   "be used to activate blanking.  If this box is checked, a logic high signal on the selected "
                                   "digital input will enable amplifier fast settling with a latency of 4-5 amplifier sampling "
                                   "periods.  For example, if the sampling frequency is 20 kS/s, the control latency will be "
                                   "200-250 microseconds."));
    label3->setWordWrap(true);

    QLabel *label4 = new QLabel(tr("By applying a digital pulse coincident with (or slightly overlapping) nearby stimulation "
                                   "pulses, amplifier saturation and the resulting slow amplifier recovery can be mitigated."));
    label4->setWordWrap(true);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(label1);
    mainLayout->addWidget(label2);
    mainLayout->addWidget(label3);
    mainLayout->addWidget(label4);

    setLayout(mainLayout);
}

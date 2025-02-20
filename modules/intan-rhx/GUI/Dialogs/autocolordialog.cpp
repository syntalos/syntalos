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

#include "autocolordialog.h"

AutoColorDialog::AutoColorDialog(QWidget *parent) :
    QDialog(parent)
{
    numColorsSpinBox = new QSpinBox(this);
    numColorsSpinBox->setRange(1, 128);
    numColorsSpinBox->setValue(16);

    numChannelsSameColorSpinBox = new QSpinBox(this);
    numChannelsSameColorSpinBox->setRange(1, 128);
    numChannelsSameColorSpinBox->setValue(1);

    QHBoxLayout *numColorsLayout = new QHBoxLayout;
    numColorsLayout->addWidget(new QLabel(tr("Total number of colors:"), this));
    numColorsLayout->addWidget(numColorsSpinBox);
    numColorsLayout->addStretch();

    QHBoxLayout *numChannelsSameColorLayout = new QHBoxLayout;
    numChannelsSameColorLayout->addWidget(new QLabel(tr("Number of adjacent channels with same color:"), this));
    numChannelsSameColorLayout->addWidget(numChannelsSameColorSpinBox);
    numChannelsSameColorLayout->addStretch();

    QHBoxLayout *infoLayout = new QHBoxLayout;
    infoLayout->addWidget(new QLabel(tr("<b>Note:</b> You can undo this operation."), this));
    infoLayout->addStretch();

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(numColorsLayout);
    mainLayout->addLayout(numChannelsSameColorLayout);
    mainLayout->addLayout(infoLayout);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
    setWindowTitle(tr("Color Amplifier Channels"));
}

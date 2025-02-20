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

#include "displaylistmanager.h"
#include "autogroupdialog.h"

AutoGroupDialog::AutoGroupDialog(QWidget *parent) :
    QDialog(parent)
{
    groupSizeSpinBox = new QSpinBox(this);
    groupSizeSpinBox->setRange(2, MaxNumWaveformsInGroup);
    groupSizeSpinBox->setValue(4);

    QHBoxLayout *groupSizeLayout = new QHBoxLayout;
    groupSizeLayout->addWidget(new QLabel(tr("Number of amplifier channels per group:"), this));
    groupSizeLayout->addWidget(groupSizeSpinBox);
    groupSizeLayout->addStretch();

    QHBoxLayout *infoLayout1 = new QHBoxLayout;
    infoLayout1->addWidget(new QLabel(tr("Any existing groups will be ungrouped, and all disabled channels will be moved to the bottom."), this));
    infoLayout1->addStretch();

    QHBoxLayout *infoLayout2 = new QHBoxLayout;
    infoLayout2->addWidget(new QLabel(tr("<b>Note:</b> You can undo this operation."), this));
    infoLayout2->addStretch();

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(groupSizeLayout);
    mainLayout->addLayout(infoLayout1);
    mainLayout->addLayout(infoLayout2);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
    setWindowTitle(tr("Group Amplifier Channels"));
}
